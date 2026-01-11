import socket
import threading
import serial
import time
import queue
import os
import sys
import math
import struct
from pymavlink import mavutil

from cryptography.hazmat.primitives.ciphers.aead import AESGCM
from cryptography.exceptions import InvalidTag

SERIAL_PORT_1 = '/dev/ttyACM0'
SERIAL_PORT_2 = '/dev/ttyACM2'

if os.name == 'nt':
    SERIAL_PORT_1 = 'COM8'
    SERIAL_PORT_2 = 'COM9'

print(f"[Config] Drone 1: {SERIAL_PORT_1}")
print(f"[Config] Drone 2: {SERIAL_PORT_2}")

BAUD_RATE = 115200
SERVER_PORT = 9999


packet_queue = queue.Queue()

class DTLSSecurityLayer:
    VERSION = 0xFEFD
    CONTENT_TYPE_APP_DATA = 23
    HEADER_SIZE = 13

    def __init__(self, session_key, salt):
        self.key = session_key
        self.salt = salt
        self.aesgcm = AESGCM(session_key)

    def pack_record(self, payload, epoch, seq_num):
        explicit_nonce = os.urandom(8)
        nonce = self.salt + explicit_nonce
        encrypted_len = len(explicit_nonce) + len(payload) + 16
        combined_seq = (epoch << 48) | (seq_num & 0xFFFFFFFFFFFF)
        aad_header = struct.pack('!Q B H H', combined_seq, self.CONTENT_TYPE_APP_DATA, self.VERSION, encrypted_len)
        ciphertext_with_tag = self.aesgcm.encrypt(nonce, payload, aad_header)
        seq_bytes = struct.pack('!Q', seq_num)[2:]
        header = struct.pack('!B H H', self.CONTENT_TYPE_APP_DATA, self.VERSION, epoch) + seq_bytes + struct.pack('!H',
                                                                                                                  encrypted_len)
        return header + explicit_nonce + ciphertext_with_tag

    def unpack_record(self, packet):
        if len(packet) < self.HEADER_SIZE:
            return None, None, None
        try:
            header_bytes = packet[:self.HEADER_SIZE]
            content_type, version, epoch = struct.unpack('!B H H', header_bytes[:5])
            seq_num = int.from_bytes(header_bytes[5:11], byteorder='big')
            length = struct.unpack('!H', header_bytes[11:13])[0]

            if content_type != self.CONTENT_TYPE_APP_DATA or version != self.VERSION:
                return None, None, None

            body = packet[self.HEADER_SIZE:]
            if len(body) != length:
                return None, None, None

            explicit_nonce = body[:8]
            ciphertext_with_tag = body[8:]
            nonce = self.salt + explicit_nonce
            combined_seq = (epoch << 48) | seq_num
            aad_header = struct.pack('!Q B H H', combined_seq, content_type, version, length)
            payload = self.aesgcm.decrypt(nonce, ciphertext_with_tag, aad_header)
            return payload, epoch, seq_num
        except (InvalidTag, Exception):
            return None, None, None


SESSION_KEY = b'\xaa' * 16
SALT = b'\xbb' * 4

def generic_drone_receiver(port, source_id):
    print(f"[System] Connecting to {source_id} on {port}...")

    dtls_sender = DTLSSecurityLayer(SESSION_KEY, SALT)
    epoch = 1
    seq_num = 0

    try:
        ser = serial.Serial(port, BAUD_RATE, timeout=0.1)
        mav = mavutil.mavlink.MAVLink(ser)
        mav.srcSystem = 255
        mav.srcComponent = 1

        try:
            msg = mav.request_data_stream_encode(1, 1, mavutil.mavlink.MAV_DATA_STREAM_ALL, 4, 1)
            ser.write(msg.pack(mav))
        except Exception:
            pass
        time.sleep(0.5)

        print(f"[{source_id}] Requesting ATTITUDE at 10Hz...")
        mav.command_long_send(1, 1, mavutil.mavlink.MAV_CMD_SET_MESSAGE_INTERVAL, 0, 30, 100000, 0, 0, 0, 0, 0)
        time.sleep(0.1)

        mav.command_long_send(1, 1, mavutil.mavlink.MAV_CMD_SET_MESSAGE_INTERVAL, 0, 26, 20000, 0, 0, 0, 0, 0)

        while True:
            if ser.in_waiting > 0:
                raw_data = ser.read(ser.in_waiting)
                dtls_packet = dtls_sender.pack_record(raw_data, epoch, seq_num)
                seq_num += 1

                packet_queue.put({
                    'source': source_id,
                    'data': dtls_packet,
                    'recv_time': time.perf_counter()
                })
            else:
                time.sleep(0.002)

    except Exception as e:
        print(f"[Error] {source_id} Serial Error ({port}): {e}")


def attack_receiver_thread():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('0.0.0.0', SERVER_PORT))
    print(f"[System] Attack Listener Ready on UDP {SERVER_PORT}")

    epoch = 1
    seq_num = 999999

    def pack_fake_dtls(payload):
        fake_len = 8 + len(payload) + 16
        header = struct.pack('!B H H', 23, 0xFEFD, epoch) + struct.pack('!Q', seq_num)[2:] + struct.pack('!H', fake_len)
        return header + os.urandom(8) + payload + os.urandom(16)

    while True:
        try:
            data, _ = sock.recvfrom(2048)
            packet_queue.put({
                'source': 'attack',
                'data': pack_fake_dtls(data),
                'recv_time': time.perf_counter()
            })
        except Exception:
            pass

class DroneStats:
    def __init__(self, name):
        self.name = name
        self.att_count = 0
        self.other_count = 0
        self.freq_start_time = time.time()
        self.last_valid_time = time.perf_counter()
        self.current_att_freq = 0.0
        self.current_other_freq = 0.0
        self.accumulated_cost = 0.0
        self.last_roll = 0.0

        self.display_line = f"\033[90m[{name.upper()}] Initializing...\033[0m"

    def update(self, packet_cost_ms, msgs=None):
        now = time.perf_counter()

        if time.time() - self.freq_start_time >= 0.5:
            dt = time.time() - self.freq_start_time
            self.current_att_freq = self.att_count / dt
            self.current_other_freq = self.other_count / dt
            self.att_count = 0
            self.other_count = 0
            self.freq_start_time = time.time()

        if msgs:
            for msg in msgs:
                if msg.get_type() == 'ATTITUDE':
                    self.att_count += 1
                    self.last_roll = math.degrees(msg.roll)
                    self.last_valid_time = now
                else:
                    self.other_count += 1

        latency = (now - self.last_valid_time) * 1000 + self.accumulated_cost

        GREEN = "\033[92m"
        RED = "\033[91m"
        RESET = "\033[0m"

        status_color = RED if latency > 200 else GREEN
        header = f"{status_color}[{self.name.upper()}]{RESET}"

        roll_str = f"{self.last_roll:5.1f}°"

        self.display_line = (
            f"{header} "
            f"Roll: {roll_str} | "
            f"Freq: {self.current_att_freq:3.0f}Hz | "
            f"Lat: {latency:4.0f}ms"
        )

        self.accumulated_cost = 0.0

def dtls_mavlink_processor():
    print("[System] Processor Engine Started...")

    dtls_receiver = DTLSSecurityLayer(SESSION_KEY, SALT)
    mav_parser = mavutil.mavlink.MAVLink(None)

    drones = {
        'drone1': DroneStats('drone1'),
        'drone2': DroneStats('drone2')
    }

    total_attack_count = 0
    total_decrypt_time = 0.0
    total_decrypt_count = 0

    RED = "\033[91m"
    GREEN = "\033[92m"
    RESET = "\033[0m"
    CLEAR_LINE = "\033[K"

    print("\n\n")

    while True:
        packet = packet_queue.get()
        comp_start = time.perf_counter()

        source = packet['source']
        is_attack = (source == 'attack')

        if is_attack:
            total_attack_count += 1

        payload, _, _ = dtls_receiver.unpack_record(packet['data'])
        is_valid = (payload is not None)

        comp_end = time.perf_counter()
        cost_ms = (comp_end - comp_start) * 1000

        total_decrypt_time += cost_ms
        total_decrypt_count += 1
        avg_ms = total_decrypt_time / total_decrypt_count

        if is_attack or not is_valid:
            for d in drones.values():
                d.accumulated_cost += cost_ms

        elif source in drones:
            try:
                msgs = mav_parser.parse_buffer(payload)
                drones[source].update(cost_ms, msgs)
            except Exception:
                pass

        # === UI 刷新 ===
        cost_color = RED if is_attack else GREEN
        sec_line = (
            f"{RED}[SECURITY]{RESET} "
            f"Atk: {total_attack_count:<5} | "
            f"Avg: {avg_ms:4.3f}ms | "
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
    t_main = threading.Thread(target=dtls_mavlink_processor, daemon=True)

    t_d1.start()
    t_d2.start()
    t_net.start()
    t_main.start()

    while True:
        time.sleep(1)