# PreGA

This repository contains **PreGA**, a pre-filtering group authentication framework for defending IoT edge systems against asymmetric denial-of-service (ADoS) attacks. It includes the Adaptive Time-Window Bloom Filter (ATWBF), PreGA protocol code, Verilog accelerator code, embedded benchmark code, real-world attack-test scripts, and demo videos.

This README is written for ACM CCS artifact evaluation. It separates directly executable checks from experiments that require the physical testbed used in the paper.

## 1. Overview

The artifact is organized around the main implementation and evaluation components:

| Component | Artifact Evidence |
| --- | --- |
| ATWBF software implementation | `BF/ATWBF_design.c`, `BF/ATWBF_window_expand.c`, and `BF/other/` for functional behavior, adaptive-window dynamics, and Bloom-filter baselines. |
| Hardware acceleration | `Hardware/` for Vivado 2020.x simulation/synthesis of the ATWBF accelerator on Zynq-7000 XC7Z020. |
| PreGA embedded protocol | `Scheme/PreGA/` for initialization, signing, and verification timing on CH32V307VCT6. |
| Cryptographic baselines | `Scheme/Compare/` for ECC/pairing-heavy timing using MIRACL Core BN254. |
| Real-world ADoS tests | `Real_Attack_test/` scripts and `video/` demos for DTLS+MAVLink and PreGA-style defense experiments. |

Repository layout:

| Directory | Purpose |
| --- | --- |
| `BF/` | Host-side C implementations and tests for ATWBF. |
| `BF/other/` | Baseline Bloom-filter variants: Standard BF, Robust BF, E-BF, MBF, and helper code. |
| `Hardware/` | Verilog RTL and Vivado simulation files for the ATWBF accelerator, targeting Xilinx Zynq-7000 XC7Z020 with Vivado 2020.x. |
| `Scheme/PreGA/` | Embedded C implementation of PreGA using SM3 and ATWBF logic on CH32V307VCT6. |
| `Scheme/Compare/` | Embedded C comparison benchmarks for ECC/pairing-heavy operations using MIRACL Core BN254. |
| `Real_Attack_test/` | Python scripts for MAVLink/UDP ADoS experiments. |
| `video/` | Real-world demonstration videos. |

## 2. Artifact Scope

| Tier | What Reviewers Can Check | Time | Special Hardware |
| --- | --- | ---: | --- |
| Quick sanity | Compile and run host ATWBF tests. | 5-10 min | No |
| Main software evaluation | Reproduce ATWBF functional behavior and adaptive-window trend. | 10-30 min | No |
| Hardware evaluation | Run Vivado 2020.x behavioral simulation; optionally synthesize for Zynq-7000 XC7Z020 resource use. | 20-60 min | Vivado required; physical FPGA not required for simulation |
| Embedded protocol timing | Run PreGA initialization/signing/verification timing and MIRACL baseline timing. | 30-90 min after setup | CH32V307VCT6 board |
| Real-world ADoS demo | Configure and run DTLS+MAVLink and PreGA ADoS scripts in an isolated network. | Experiment-dependent | Orange Pi 5, RadioLink PIX6 or MAVLink serial sources |

The host-only and Vivado simulation tiers are the most portable parts of the artifact. Embedded and real-world experiments depend on the same hardware assumptions as the paper.

## 3. System Requirements

### Hardware

Minimum host machine:

- 4-core x86_64 CPU
- 8 GB RAM
- 2 GB free disk space for source/build outputs, plus the Vivado installation if hardware simulation is run

Special hardware for full reproduction:

- Xilinx Zynq-7000 XC7Z020 platform for the ATWBF accelerator evaluation. The default hardware flow uses Vivado simulation, so a physical FPGA board is not required for the basic hardware test.
- CH32V307VCT6 / WCH CH32V30x board for embedded PreGA and comparison timing.
- Real-world ADoS testbed: Orange Pi 5 or comparable Linux edge host, two RadioLink PIX6 controllers or equivalent MAVLink serial sources, an attacker PC, and an isolated local network.

Serial and network defaults used by the scripts:

- Linux serial ports: `/dev/ttyACM0` and `/dev/ttyACM2`
- Windows serial ports: `COM8` and `COM9`
- UDP port: `9999`

### Software

Recommended OS:

- Ubuntu 22.04 LTS for host tests and Python experiments.
- Windows or Linux with Xilinx Vivado 2020.x for the Zynq-7000 XC7Z020 hardware flow.

Core tools:

- GCC 11 or newer
- Python 3.10 or newer
- `pip`
- Xilinx Vivado 2020.x
- WCH CH32V30x SDK/toolchain for embedded firmware
- MIRACL Core configured for BN254 for `Scheme/Compare/`

Python packages:

```bash
python3 -m pip install pyserial pymavlink cryptography
```

Estimated time and space:

| Evaluation | Estimated Time | Disk Space |
| --- | ---: | ---: |
| ATWBF sanity test | 1-5 min | <100 MB |
| ATWBF adaptive-window test | 1-5 min | <100 MB |
| Vivado behavioral simulation | 5-20 min | Vivado installation plus <500 MB project output |
| Vivado synthesis/resource check | 10-40 min | Vivado installation plus generated project output |
| CH32V embedded timing | 30-90 min after toolchain setup | toolchain-dependent |
| Real-world ADoS test | experiment-dependent | CSV logs usually <100 MB |

## 4. Setup

Get the source:

```bash
git clone <artifact-repository-url> PreGA-main
cd PreGA-main
```

Or unpack the artifact archive:

```bash
unzip PreGA-main.zip
cd PreGA-main
```

Host-only Docker setup:

```bash
docker run --rm -it \
  -v "$PWD":/workspace \
  -w /workspace \
  ubuntu:22.04 bash
```

Inside the container:

```bash
apt-get update
DEBIAN_FRONTEND=noninteractive apt-get install -y \
  build-essential \
  gcc \
  python3 \
  python3-pip

python3 -m pip install pyserial pymavlink cryptography
```

Native Ubuntu setup:

```bash
sudo apt-get update
sudo apt-get install -y build-essential gcc python3 python3-pip
python3 -m pip install pyserial pymavlink cryptography
```

Install Xilinx Vivado 2020.x separately for the hardware flow. In Vivado, select a Zynq-7000 XC7Z020 part, for example `xc7z020clg400-1`, unless your board uses a different package or speed grade.

## 5. Basic Test / Sanity Check

Build and run the host ATWBF tests:

```bash
gcc -std=c11 -O2 -Wall -Wextra BF/ATWBF_design.c -o BF/atwbf_design
gcc -std=c11 -O2 -Wall -Wextra BF/ATWBF_window_expand.c -o BF/atwbf_window_expand

./BF/atwbf_design
./BF/atwbf_window_expand
```

On Windows with MinGW:

```powershell
gcc -std=c11 -O2 -Wall -Wextra BF\ATWBF_design.c -o BF\atwbf_design.exe
gcc -std=c11 -O2 -Wall -Wextra BF\ATWBF_window_expand.c -o BF\atwbf_window_expand.exe

.\BF\atwbf_design.exe
.\BF\atwbf_window_expand.exe
```

Expected output fragments from `ATWBF_design`:

```text
Time Bloom Filter Status:
Total Window Size (T): 30
Is Expanded:
Total Available Filters:
Filter Usage:
Windows age:
```

Expected output fragments from `ATWBF_window_expand`:

```text
==== Burst Traffic Window Dynamics Test ====
Settings: BloomSize=50000, MaxWindows=20, TimeRange=1
Time(T) Msgs(N) WinCount Trend
```

The burst test uses randomized traffic, so exact message counts differ. A successful run shows `WinCount` changing over time and trend labels such as `EXPAND`, `SHRINK`, or `FORCE CYCLE`.

## 6. Detailed Evaluation / Paper Mapping

| Paper Result | Claim / Expected Value | Artifact Path | Reproducibility |
| --- | --- | --- | --- |
| Figure 2 | DTLS+MAVLink ADoS drops parsing from 10 Hz to about 1 Hz and needs recovery time. | `Real_Attack_test/edge_server_linux.py`, `Real_Attack_test/DOS_dtls.py`, `video/` | Requires real testbed; video/log inspection otherwise. |
| Figure 9 | PreGA keeps attitude-packet parsing near 10 Hz during a 10 s attack starting at 20 s. | `Real_Attack_test/our_scheme.py`, `Real_Attack_test/our_scheme_DOS.py` | Requires real testbed. |
| Figure 10 | PreGA remains around 9 Hz under high attack rates; jitter appears at higher rates. | `Real_Attack_test/our_scheme.py`, `Real_Attack_test/our_scheme_DOS.py` | Requires isolated high-rate network. |
| Table 1 | Zynq-7000 accelerator resource use reported in the paper: 457 LUT, 44 LUTRAM, 413 FF, 1 BRAM, 4 DSP. | `Hardware/rtl/` in Vivado 2020.x | Recheck with Vivado synthesis targeting XC7Z020; exact counts depend on settings. |
| Table 2 | ATWBF software and hardware operation-time comparison reported in the paper. | `BF/`, `Hardware/` | Operation implementations are included; exact timing requires the original embedded/hardware timing flow and sample-size settings. |
| Figure 11 | PreGA is faster than Ring/P2BA/BLS/PIC-BI across batch sizes. | `Scheme/PreGA/`, `Scheme/Compare/` | Requires CH32V307VCT6 and MIRACL Core. |
| Figure 12 | Active ATWBF window expands under sustained high load. | `BF/ATWBF_window_expand.c` | Host-reproducible trend; randomized exact values. |
| Figure 13 | ATWBF maintains lower FPR than static Bloom-filter baselines. | `BF/ATWBF_design.c`, `BF/other/` | Requires running or extending BF baseline drivers. |
| Figure 14 | ATWBF has far fewer false positives than static baselines under high load. | `BF/`, `Real_Attack_test/` | Partially reproducible with current source; full plot requires a data-collection driver. |
| Section 6.5 | Malicious batches are rejected much faster than legitimate batches. | `Real_Attack_test/our_scheme.py`, `Real_Attack_test/our_scheme_DOS.py` | Requires real testbed and isolated attack network. |

### Experiment 1: ATWBF Functional Correctness

Corresponds to: ATWBF functional validation and reliability experiments.

```bash
gcc -std=c11 -O2 -Wall -Wextra BF/ATWBF_design.c -o BF/atwbf_design
./BF/atwbf_design
```

Expected result: the terminal prints status blocks for insertion, query, expansion, deletion, and window sliding. `Is Expanded`, `Filter Usage`, and `Total Available Filters` should change across phases.

### Experiment 2: ATWBF Adaptive Window Dynamics

Corresponds to: Figure 12.

```bash
gcc -std=c11 -O2 -Wall -Wextra BF/ATWBF_window_expand.c -o BF/atwbf_window_expand
./BF/atwbf_window_expand
```

Expected result: a 30-step log with message count, active window count, and an ASCII trend bar. Exact values vary because the local test uses randomized bursts.

### Experiment 3: Hardware ATWBF Accelerator on Zynq-7000 XC7Z020

Corresponds to: Table 1 and the hardware part of Table 2.

Required software/platform:

- Xilinx Vivado 2020.x
- Target device/platform: Zynq-7000 XC7Z020, for example `xc7z020clg400-1`
- Simulation top module: `bf_accel_wrapper_tb`

Vivado GUI flow:

1. Open Vivado 2020.x and create a new RTL project.
2. Select the Zynq-7000 XC7Z020 device or matching Zynq-7020 board preset.
3. Add all RTL files from `Hardware/rtl/` as design sources.
4. Add simulation files from `Hardware/sim/` as simulation sources:
   - `bf_accel_wrapper_tb.v`
   - `simple_bus_bfm.v`
   - `memory_model.v`
   - `test_sequences.vh`
   - `test_data.mem`
5. Set `bf_accel_wrapper_tb` as the simulation top.
6. Run behavioral simulation.
7. Optional: run synthesis and inspect the utilization report.

Optional Vivado Tcl flow:

```tcl
create_project prega_hw_sim ./vivado_prega_hw_sim -part xc7z020clg400-1
add_files [glob ./Hardware/rtl/*.v]
add_files ./Hardware/rtl/bf_defines.vh
set_property include_dirs ./Hardware/rtl [current_fileset]
add_files -fileset sim_1 [glob ./Hardware/sim/*.v]
add_files -fileset sim_1 ./Hardware/sim/test_sequences.vh
add_files -fileset sim_1 ./Hardware/sim/test_data.mem
set_property top bf_accel_wrapper_tb [get_filesets sim_1]
launch_simulation
run all

# Optional synthesis/resource check
set_property top bf_accel_wrapper [current_fileset]
synth_design -top bf_accel_wrapper -part xc7z020clg400-1
report_utilization
```

Expected simulation result: the Vivado simulator prints `BFAccel Wrapper Testbench Starting`, initializes DUT memories, and reports ADD/CHECK/REMOVE performance phases. Key log fragments include `[SEQ]`, `[PERF] Phase 1: ADD`, `[PERF] Phase 2: CHECK`, and `[PERF] Phase 3: REMOVE`.

### Experiment 4: Embedded PreGA Protocol Timing

Corresponds to: Figure 11.

Required hardware/software:

- CH32V307VCT6 board
- WCH CH32V30x SDK/toolchain
- Serial console at 115200 baud

Entry point:

```text
Scheme/PreGA/main.c
```

Expected serial output includes:

```text
Scheme simulation started (using SM3 on CH32V307VCT6)...
[TA] 1. System initialization...
Initialization need :<cycles>
[Device] Devices preparing to sign and send messages...
Message Signing need :<cycles>
[ES] Edge Node received a batch of 5 messages, starting batch verification...
Result: Batch verification successful!
Batch Verification need :<cycles>
```

### Experiment 5: Cryptographic Baseline Timing

Corresponds to: Figure 11 and Table 3 comparison context.

Required hardware/software:

- CH32V307VCT6 board
- WCH CH32V30x SDK/toolchain
- MIRACL Core with BN254 enabled

Entry point:

```text
Scheme/Compare/main.c
```

Expected serial output includes:

```text
MIRACL Core Exponentiation Timing (BN254)
Platform: CH32V307VCT6
Time Cost (G1): <us> us
Time Cost (G2): <us> us
Time Cost (GT): <us> us
--- Exponentiation Timing Summary (BN254) ---
```

### Experiment 6: Real-World ADoS Defense

Corresponds to: Figure 2, Figure 9, Figure 10, and Section 6.5.

Safety requirement: run these scripts only inside an isolated local laboratory network. They intentionally generate UDP attack traffic.

Before running the attack scripts:

- Set `TARGET_IP` in `Real_Attack_test/our_scheme_DOS.py`.
- Set `TARGET_IP` in `Real_Attack_test/DOS_dtls.py`. In the current artifact this is an unfilled placeholder and must be changed to a quoted IP string, for example `TARGET_IP = '192.168.1.10'`.

Install Python dependencies:

```bash
python3 -m pip install pyserial pymavlink cryptography
```

Run the PreGA-side edge process:

```bash
python3 Real_Attack_test/our_scheme.py
```

After setting `TARGET_IP`, run the PreGA flooding script in another terminal:

```bash
python3 Real_Attack_test/our_scheme_DOS.py
```

For the DTLS-style baseline, run the edge process and the flooding script in separate terminals:

```bash
python3 Real_Attack_test/edge_server_linux.py
python3 Real_Attack_test/DOS_dtls.py
```

Expected paper-level trends:

- DTLS+MAVLink baseline: attack traffic reduces parsing frequency and requires recovery time.
- PreGA under similar attack: attitude-packet parsing remains near the normal rate.
- Malicious-message rejection is much cheaper than legitimate full processing.

## 7. Reusability

To change ATWBF host workloads:

- Edit `TEST_SIZE`, `DEFAULT_BLOOM_SIZE`, `DEFAULT_TIME_WINDOW`, and `DEFAULT_THRESHOLD_RATIO` in `BF/ATWBF_design.c`.
- Edit traffic generation in `test_burst_traffic()` in `BF/ATWBF_window_expand.c`.

To compare Bloom-filter variants:

- Use `BF/other/bf.c`, `robustbf.c`, `ebf.c`, `mbf.c`, and `st_bf.c` as baseline implementations.
- Add a common driver that inserts the same data stream and queries the same invalid stream for Figure 13/Figure 14-style plots.

To modify PreGA protocol parameters:

- Start with `Scheme/PreGA/scheme.h` for protocol constants and data structures.
- Update protocol operations in `Scheme/PreGA/scheme.c`.
- Update the embedded test driver in `Scheme/PreGA/main.c`.

To change real-world serial/network settings:

- Update `SERIAL_PORT_1`, `SERIAL_PORT_2`, `BAUD_RATE`, and `SERVER_PORT` in `Real_Attack_test/our_scheme.py` or `Real_Attack_test/edge_server_linux.py`.
- Update `TARGET_IP`, `TARGET_PORT`, `PROCESS_COUNT`, and `ATTACK_DURATION` in the DoS scripts.

## 8. Limitations & Notes

- Absolute timing values are platform-dependent. CPU, Vivado version/settings, CH32V board clock(144Hz), compiler flags, serial devices, and network conditions can change measured latency.
- The most portable claims are the ATWBF functional behavior, adaptive-window trend, and Vivado simulation behavior.
- The full Figure 9/Figure 10 real-world reproduction requires MAVLink serial sources and isolated attack traffic.
- The real-world attack scripts intentionally generate high-rate UDP traffic. Do not run them on public or shared networks.
- The embedded firmware depends on WCH CH32V30x platform files and board initialization code.
- `Scheme/Compare/` depends on MIRACL Core configured for BN254.
- The hardware code is written for Vivado 2020.x targeting Zynq-7000 XC7Z020. Board-level deployment requires evaluator-specific constraints and integration.
- Some source comments and terminal strings contain non-ASCII text. Use a UTF-8 terminal when possible.
