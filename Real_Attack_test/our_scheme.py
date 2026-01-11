import socket
import threading
import serial
import time
import queue
import os
import sys
import math
import struct
import hashlib
import csv
from pymavlink import mavutil


SERIAL_PORT_1 = '/dev/ttyACM0'
SERIAL_PORT_2 = '/dev/ttyACM2'

if os.name == 'nt':
    SERIAL_PORT_1 = 'COM8'
    SERIAL_PORT_2 = 'COM9'

print(f"[Config] Drone 1: {SERIAL_PORT_1}")
print(f"[Config] Drone 2: {SERIAL_PORT_2}")

BAUD_RATE = 115200
SERVER_PORT = 9999


MAX_QUEUE_SIZE = 2000
packet_queue = queue.Queue(maxsize=MAX_QUEUE_SIZE)

DIGEST_SIZE = 32
NUM_SUBKEYS = 16

RANGE_SUBKEYS = range(NUM_SUBKEYS)

STRUCT_Q = struct.Struct('!Q')

BLAKE2S = hashlib.blake2s

class ExperimentLogger:
    def __init__(self, filename="data_optimized.csv"):
        self.filename = filename
        self.start_time = time.time()
        try:
            with open(self.filename, 'w', newline='') as f:
                writer = csv.writer(f)
                writer.writerow(["Time", "Frequency", "Latency", "Attack_Flag"])
            print(f"[System] Logging data to {self.filename}")
        except Exception as e:
            print(f"[Error] Failed to init logger: {e}")

    def log(self, freq, latency, is_attack):
        rel_time = time.time() - self.start_time
        try:
            with open(self.filename, 'a', newline='') as f:
                writer = csv.writer(f)
                writer.writerow([f"{rel_time:.2f}", f"{freq:.2f}", f"{latency:.2f}", int(is_attack)])
        except Exception:
            pass

class TimeBloomFilter:
    __slots__ = ['window_size', 'time_range', 'windows', 'all_items', 'current_idx', 'last_rotate_time']

    def __init__(self, window_size=100, time_range=60):
        self.window_size = window_size
        self.time_range = time_range

        self.windows = [set() for _ in range(window_size)]

        self.all_items = set()

        self.current_idx = 0
        self.last_rotate_time = time.time()

    def check_rotate(self):
        now = time.time()

        if now - self.last_rotate_time > self.time_range:
            next_idx = (self.current_idx + 1) % self.window_size

            expired_set = self.windows[next_idx]
            if expired_set:
                self.all_items.difference_update(expired_set)
                expired_set.clear()

            self.current_idx = next_idx
            self.last_rotate_time = now

    def insert(self, item_bytes):
        self.check_rotate()
        self.windows[self.current_idx].add(item_bytes)
        self.all_items.add(item_bytes)

    def query(self, item_bytes):
        self.check_rotate()
        return item_bytes in self.all_items


class IoTDevice:
    __slots__ = ['id', 'master_key', 'sk', 'time_vec']

    def __init__(self, dev_id, master_key):
        self.id = dev_id
        self.master_key = master_key
        self.sk = [os.urandom(DIGEST_SIZE) for _ in range(NUM_SUBKEYS)]
        self.time_vec = [os.urandom(DIGEST_SIZE) for _ in range(NUM_SUBKEYS)]

    def evolve_keys(self):
        _hash = BLAKE2S
        _mkey = self.master_key
        _sk = self.sk
        _time_vec = self.time_vec

        for j in RANGE_SUBKEYS:
            h_ctx = _hash(digest_size=DIGEST_SIZE)
            h_ctx.update(_mkey)
            h_ctx.update(_time_vec[j])
            _time_vec[j] = h_ctx.digest()

            h_ctx = _hash(digest_size=DIGEST_SIZE)
            h_ctx.update(_sk[j])
            h_ctx.update(_time_vec[j])
            _sk[j] = h_ctx.digest()

    def get_contribution(self):
        buffer = bytearray()
        _hash = BLAKE2S
        _sk = self.sk

        for key in _sk:
            buffer.extend(_hash(key, digest_size=DIGEST_SIZE).digest())

        return _hash(buffer, digest_size=DIGEST_SIZE).digest()

    def sign_message(self, message_bytes):
        self.evolve_keys()

        timestamp = STRUCT_Q.pack(int(time.perf_counter() * 1000000))
        nonce = os.urandom(4)

        ctx = BLAKE2S(digest_size=DIGEST_SIZE)
        ctx.update(message_bytes)
        ctx.update(timestamp)
        ctx.update(nonce)
        t_hash = ctx.digest()

        signature = []
        _hash = BLAKE2S
        _sk = self.sk

        for j in RANGE_SUBKEYS:
            bit = (t_hash[j] >> 7) & 1

            if bit == 1:
                signature.append(_hash(_sk[j], digest_size=DIGEST_SIZE).digest())
            else:
                signature.append(_sk[j])

        return {
            'msg': message_bytes,
            'ts': timestamp,
            'nonce': nonce,
            'sig': signature,
            'contribution': self.get_contribution()
        }


class EdgeNodeVerifier:
    __slots__ = ['tbf']

    def __init__(self, tbf):
        self.tbf = tbf

    def verify(self, packet_struct):
        claimed_contrib = packet_struct['contribution']

        if not self.tbf.query(claimed_contrib):
            return False, None

        msg = packet_struct['msg']
        ts = packet_struct['ts']
        nonce = packet_struct['nonce']
        sig = packet_struct['sig']

        ctx = BLAKE2S(digest_size=DIGEST_SIZE)
        ctx.update(msg)
        ctx.update(ts)
        ctx.update(nonce)
        t_hash_prime = ctx.digest()

        recovered_buffer = bytearray()
        _hash = BLAKE2S

        try:
            for j in RANGE_SUBKEYS:
                bit = (t_hash_prime[j] >> 7) & 1

                if bit == 1:
                    recovered_buffer.extend(sig[j])
                else:
                    recovered_buffer.extend(_hash(sig[j], digest_size=DIGEST_SIZE).digest())

            reconstructed_contribution = _hash(recovered_buffer, digest_size=DIGEST_SIZE).digest()

            if reconstructed_contribution == claimed_contrib:
                return True, msg
            else:
                return False, None
        except Exception:
            return False, None

global_tbf = TimeBloomFilter(window_size=100, time_range=60)

SYSTEM_MASTER_KEY = os.urandom(32)
device_registry = {
    'drone1': IoTDevice('drone1', SYSTEM_MASTER_KEY),
    'drone2': IoTDevice('drone2', SYSTEM_MASTER_KEY)
}

print("[TA] Pre-computing Group Verification Points (Optimized BLAKE2s)...")
for d_id, dev in device_registry.items():
    original_sk = list(dev.sk)
    original_time = list(dev.time_vec)

    PRE_COMPUTE_STEPS = 50000
    print(f"   - Processing {d_id} ({PRE_COMPUTE_STEPS} steps)...")

    for _ in range(PRE_COMPUTE_STEPS):
        dev.evolve_keys()
        c_val = dev.get_contribution()
        global_tbf.insert(c_val)

    # 恢复状态
    dev.sk = original_sk
    dev.time_vec = original_time
print("[TA] Pre-computation done.")


def generic_drone_receiver(port, source_id):
    print(f"[System] Connecting to {source_id} on {port}...")
    device = device_registry[source_id]

    try:
        mav_conn = mavutil.mavlink_connection(port, baud=BAUD_RATE)
        mav_conn.wait_heartbeat()
        print(f"[{source_id}] Heartbeat received!")

        mav_conn.mav.request_data_stream_send(
            mav_conn.target_system, mav_conn.target_component,
            mavutil.mavlink.MAV_DATA_STREAM_ALL, 4, 1
        )
        time.sleep(0.5)

        print(f"[{source_id}] Requesting ATTITUDE at 10Hz...")
        mav_conn.mav.command_long_send(
            mav_conn.target_system, mav_conn.target_component,
            mavutil.mavlink.MAV_CMD_SET_MESSAGE_INTERVAL,
            0, 30, 100000, 0, 0, 0, 0, 0
        )

        while True:
            msg = mav_conn.recv_match(blocking=True, timeout=0.1)

            if msg:
                raw_msg_bytes = msg.get_msgbuf()
                signed_packet = device.sign_message(raw_msg_bytes)

                try:
                    packet_queue.put({
                        'source': source_id,
                        'data': signed_packet,
                        'recv_time': time.perf_counter()
                    }, block=False)
                except queue.Full:
                    pass

    except Exception as e:
        print(f"[Error] {source_id} Serial Error ({port}): {e}")

def attack_receiver_thread():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1024 * 1024)
    sock.bind(('0.0.0.0', SERVER_PORT))
    print(f"[System] Attack Listener Ready on UDP {SERVER_PORT}")

    while True:
        try:
            data, _ = sock.recvfrom(2048)

            # 生成假数据
            fake_sig = [os.urandom(DIGEST_SIZE) for _ in range(NUM_SUBKEYS)]
            fake_contrib = os.urandom(DIGEST_SIZE)

            fake_packet = {
                'msg': data,
                'ts': STRUCT_Q.pack(int(time.perf_counter() * 1000000)),
                'nonce': os.urandom(4),
                'sig': fake_sig,
                'contribution': fake_contrib
            }

            try:
                packet_queue.put({
                    'source': 'attack',
                    'data': fake_packet,
                    'recv_time': time.perf_counter()
                }, block=False)
            except queue.Full:
                pass
        except Exception:
            pass


class DroneStats:
    def __init__(self, name, logger=None):
        self.name = name
        self.logger = logger
        self.att_count = 0
        self.freq_start_time = time.time()
        self.last_valid_time = time.perf_counter()
        self.current_freq = 0.0
        self.accumulated_cost = 0.0
        self.last_roll = 0.0
        self.display_line = f"\033[90m[{name.upper()}] Initializing...\033[0m"

        self.window_latency_sum = 0.0
        self.window_pkt_count = 0

    def update(self, packet_cost_ms, msgs=None, is_attack_active=False):
        now = time.perf_counter()

        if msgs:
            current_latency = (now - self.last_valid_time) * 1000 + self.accumulated_cost
            self.window_latency_sum += current_latency
            self.window_pkt_count += 1

        if time.time() - self.freq_start_time >= 0.5:
            dt = time.time() - self.freq_start_time
            if dt > 0:
                self.current_freq = self.att_count / dt

            if self.logger and self.name == 'drone1':
                avg_lat = 0
                if self.window_pkt_count > 0:
                    avg_lat = self.window_latency_sum / self.window_pkt_count
                self.logger.log(self.current_freq, avg_lat, is_attack_active)

            self.window_latency_sum = 0.0
            self.window_pkt_count = 0

            self.att_count = 0
            self.freq_start_time = time.time()

        if msgs:
            for msg in msgs:
                if msg.get_type() == 'ATTITUDE':
                    self.att_count += 1
                    self.last_roll = math.degrees(msg.roll)
                    self.last_valid_time = now

        latency = (now - self.last_valid_time) * 1000 + self.accumulated_cost

        GREEN = "\033[92m"
        RED = "\033[91m"
        RESET = "\033[0m"
        status_color = RED if latency > 200 else GREEN

        self.display_line = (
            f"{status_color}[{self.name.upper()}]{RESET} "
            f"Roll: {self.last_roll:5.1f}° | "
            f"Freq: {self.current_freq:3.0f}Hz | "
            f"Lat: {latency:4.0f}ms"
        )
        self.accumulated_cost = 0.0


def scheme_processor():
    print("[System] Scheme Verification Engine Started (Ultra-Optimized)...")

    csv_logger = ExperimentLogger("data_optimized.csv")

    verifier = EdgeNodeVerifier(global_tbf)
    mav_parser = mavutil.mavlink.MAVLink(None)

    drones = {
        'drone1': DroneStats('drone1', csv_logger),
        'drone2': DroneStats('drone2', csv_logger)
    }

    total_attack_count = 0
    total_verify_time = 0.0
    total_verify_count = 0
    total_attack_reject_time = 0.0
    avg_attack_reject_ms = 0.0


    last_attack_time = 0

    RED = "\033[91m"
    GREEN = "\033[92m"
    YELLOW = "\033[93m"
    RESET = "\033[0m"
    CLEAR_LINE = "\033[K"

    print("\n\n")

    while True:
        try:
            packet_container = packet_queue.get(timeout=1.0)
        except queue.Empty:
            continue

        comp_start = time.perf_counter()

        source = packet_container['source']
        is_attack = (source == 'attack')
        packet_data = packet_container['data']

        if is_attack:
            total_attack_count += 1
            last_attack_time = time.time()

        is_attack_active = (time.time() - last_attack_time < 1.0)

        is_valid, payload = verifier.verify(packet_data)

        comp_end = time.perf_counter()
        cost_ms = (comp_end - comp_start) * 1000


        total_verify_time += cost_ms
        total_verify_count += 1
        avg_all_ms = total_verify_time / total_verify_count

        if is_attack:
            total_attack_reject_time += cost_ms
            avg_attack_reject_ms = total_attack_reject_time / total_attack_count

        if not is_valid:
            for d in drones.values():
                d.accumulated_cost += cost_ms
        elif source in drones:
            try:
                msgs = mav_parser.parse_buffer(payload)
                if msgs:
                    drones[source].update(cost_ms, msgs, is_attack_active)
            except Exception:
                pass

        # UI 刷新
        cost_color = RED if (is_attack or not is_valid) else GREEN
        sec_line = (
            f"{RED}[SCHEME]{RESET} "
            f"Blk: {total_attack_count:<5} | "
            f"Avg(All): {avg_all_ms:4.3f}ms | "
            f"{YELLOW}Rej(Att): {avg_attack_reject_ms:4.4f}ms{RESET} | "
            f"Now: {cost_color}{cost_ms:4.3f}ms{RESET}"
        )

        output = (
            f"\r{drones['drone1'].display_line}{CLEAR_LINE}\n"
            f"{drones['drone2'].display_line}{CLEAR_LINE}\n"
            f"{sec_line}{CLEAR_LINE}\033[2A"
        )
        sys.stdout.write(output)
        sys.stdout.flush()


if __name__ == '__main__':
    t_d1 = threading.Thread(target=generic_drone_receiver, args=(SERIAL_PORT_1, 'drone1'), daemon=True)
    t_d2 = threading.Thread(target=generic_drone_receiver, args=(SERIAL_PORT_2, 'drone2'), daemon=True)
    t_net = threading.Thread(target=attack_receiver_thread, daemon=True)
    t_main = threading.Thread(target=scheme_processor, daemon=True)

    t_d1.start()
    t_d2.start()
    t_net.start()
    t_main.start()

    while True:
        time.sleep(1)