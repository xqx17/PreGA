# PreGA

This repository contains **PreGA**, a pre-filtering group authentication framework for defending IoT edge systems against asymmetric denial-of-service (ADoS) attacks. It includes the Adaptive Time-Window Bloom Filter (ATWBF), PreGA protocol code, Verilog accelerator code, embedded benchmark code, real-world attack-test scripts, and demo videos.

## 1. Overview

This artifact follows the paper scenario: group authentication at the IoT edge under asymmetric denial-of-service (ADoS) attacks. The defender model has a Trusted Authority (TA), an Edge Server (ES), and Endpoint Devices (EDs). The system targets task-oriented, relatively stable groups where membership changes are infrequent and authentication events dominate the workload. The adversary sends syntactically valid but cryptographically invalid authentication traffic to force the ES into expensive verification. PreGA changes this workflow into staged authentication: receive message, run a low-cost ATWBF membership check, and execute full signature verification only when the candidate group state passes the pre-filter.

The repository should be read as an implementation of the three-layer architecture in Figure 5 of the paper:

| Paper Layer | Purpose in the Paper | Code to Read First | Reproduction Role |
| --- | --- | --- | --- |
| Protocol layer | Hash-chain group authentication among TA, EDs, and ES. | `Scheme/PreGA/main.c`, `Scheme/PreGA/scheme.c`, `Scheme/PreGA/scheme.h` | Reproduces initialization, device signing, and ES batch verification timing on CH32V307VCT6. |
| System layer | Adaptive Time-Window Bloom Filter (ATWBF) for low-cost pre-filtering and stable FPR under continuous traffic. | `Scheme/PreGA/bf.c`, `Scheme/PreGA/bf.h`, `BF/ATWBF_design.c`, `BF/ATWBF_window_expand.c` | Reproduces ATWBF insert/query/remove behavior and adaptive window dynamics. |
| Hardware layer | Hardware-parallel ATWBF accelerator that offloads hashing and filter memory updates. | `Hardware/rtl/`, `Hardware/sim/` | Reproduces Vivado simulation and synthesis-style resource checks for the Bloom-filter accelerator. |
| Evaluation layer | Real-world ADoS defense, DTLS+MAVLink baseline, Bloom-filter baselines, and crypto baselines. | `Real_Attack_test/`, `BF/other/`, `Scheme/Compare/`, `video/` | Reproduces the paper's system, reliability, and comparison experiments when the required hardware is available. |

### Paper Roles to Code

| Paper Role | Meaning in the Paper | Main Code Representation | How It Connects |
| --- | --- | --- | --- |
| TA, Trusted Authority | Trusted, resource-rich initializer. It manages keys, computes Group Authentication Points (GAPs), and prepares the ATWBF for the ES. | `ta_initialize_system`, `ta_register_device`, `ta_precompute_group_point` in `Scheme/PreGA/scheme.c` | Called by `Scheme/PreGA/main.c`; inserts computed GAPs into `EdgeNode.tbf` through `insert_element` in `Scheme/PreGA/bf.c`. |
| ED, Endpoint Device | Resource-constrained legitimate device that evolves keys, signs messages, and emits an identity contribution `C_i`. | `IoTDevice`, `MessageTuple`, `device_evolve_keys`, `device_sign_message` in `Scheme/PreGA/` | Produces message tuples containing `M`, `T_s`, `R`, `Sig`, and `C_i`; real-world scripts use serial MAVLink sources as the ED traffic source. |
| ES, Edge Server | Resource-constrained verifier and ADoS target. It pre-filters batches through ATWBF before full verification. | `EdgeNode`, `edgenode_batch_verify` in `Scheme/PreGA/`; `EdgeNodeVerifier` and `scheme_processor` in `Real_Attack_test/our_scheme.py` | The embedded C path reconstructs `C_agg`, computes candidate `H'_group`, queries ATWBF, then reconstructs signatures. The Python real-test path is a deployment-oriented PreGA-style pipeline that performs fast contribution lookup before MAVLink parsing. |
| ADoS attacker | Network adversary that injects forged but well-formed authentication traffic without breaking the cryptographic primitives. | `Real_Attack_test/our_scheme_DOS.py`, `Real_Attack_test/DOS_dtls.py` | Sends forged PreGA-shaped or DTLS-shaped UDP traffic to the ES-side scripts. |
| DTLS+MAVLink baseline | Deployment-level baseline used in the paper to show parsing disruption under ADoS. | `Real_Attack_test/edge_server_linux.py` | Paired with `DOS_dtls.py`; compared with the PreGA defense path in `our_scheme.py`. |
| BF and crypto baselines | Baselines for false-positive behavior and cryptographic overhead. | `BF/other/`, `Scheme/Compare/` | Used for comparisons against ATWBF and PreGA protocol timing. |

### Protocol Flow in the Code

| Paper Phase | Paper Object | Code Path | What the Reproducer Should Observe |
| --- | --- | --- | --- |
| Initialization | TA generates `SK0`, `T0`, evolves possible group states, and inserts GAPs into ATWBF. | `Scheme/PreGA/main.c` -> `ta_initialize_system` -> `ta_register_device` -> `create_time_bloom_filter` -> `ta_precompute_group_point` | The embedded driver creates a fixed group for timing and stores a representative group verification point in `EdgeNode.tbf`. |
| Signing | Each ED updates hash-chain keys, computes `hr = H(M || T_s || R)`, generates `Sig`, and computes `C_i`. | `device_evolve_keys`, `device_sign_message`, `sm3.c` | Each `MessageTuple` carries message content, timestamp, nonce, signature vector, and `contribution_value`. |
| Pre-filtering | ES aggregates contributions into `C_agg`, computes `H'_group = H(C_agg)`, and queries ATWBF. | `edgenode_batch_verify` -> `query_element` in `Scheme/PreGA/bf.c` | A miss rejects the batch before signature reconstruction; a hit proceeds to full verification. |
| Signature integrity check | ES reconstructs `C'_i` from `Sig` and compares reconstructed aggregate contribution with the received aggregate. | `edgenode_batch_verify` | Successful verification prints `Batch verification successful`; failures are rejected before acceptance. |
| Real-world attack test | PreGA pre-filtering is placed before MAVLink parsing; DTLS baseline processes forged records through its security layer. | `Real_Attack_test/our_scheme.py` vs. `Real_Attack_test/edge_server_linux.py` | The Python defense script is optimized for the real-world ADoS experiment and uses the same pre-filtering idea to show low-cost rejection and stable attitude-packet parsing under attack. |

### File-Level Structure

| Path | Role | Related Paper Part |
| --- | --- | --- |
| `Scheme/PreGA/main.c` | Embedded entry point for the TA -> ED -> ES protocol timing flow. | Protocol layer, Figure 11. |
| `Scheme/PreGA/scheme.c`, `Scheme/PreGA/scheme.h` | PreGA protocol data structures and operations: TA setup, ED signing, ES verification. | Section 5.1 and Appendix B. |
| `Scheme/PreGA/bf.c`, `Scheme/PreGA/bf.h` | CH32V307VCT6 static-memory ATWBF used by the ES pre-filter. | Section 5.2 and Appendix C. |
| `Scheme/PreGA/sm3.c`, `Scheme/PreGA/sm3.h` | SM3 hash primitive used by key evolution, message roots, signatures, and GAP derivation. | Section 6.1 cryptographic configuration. |
| `Scheme/PreGA/risc_time.c`, `Scheme/PreGA/risc_time.h` | CH32 cycle/timing helpers for timestamps and performance measurement. | Embedded protocol evaluation. |
| `BF/ATWBF_design.c` | Host reference for ATWBF insertion, query, removal, expansion, shrinking, and sliding. | Algorithm 1 and Algorithm 2. |
| `BF/ATWBF_window_expand.c` | Host burst-traffic test for adaptive window behavior. | Figure 12 and long-term reliability evaluation. |
| `BF/other/` | Standard BF, RobustBF, E-BF, MBF, and helper code. | Figure 13, Figure 14, and BF baseline comparisons. |
| `Hardware/rtl/` | Verilog accelerator datapath: wrapper, bus interface, command FIFO, FSM, DMA, local buffer, hash engine, private memory, and scrambler. | Section 5.3, Figure 7, Figure 8, Table 1, Table 2. |
| `Hardware/sim/` | Vivado testbench, bus functional model, memory model, input memory image, and ADD/CHECK/REMOVE test sequences. | Hardware simulation and accelerator operation timing. |
| `Real_Attack_test/our_scheme.py` | Real-world PreGA-style ES pipeline for MAVLink traffic and fast rejection. | Figure 9, Figure 10, and Section 6.2. |
| `Real_Attack_test/our_scheme_DOS.py` | PreGA-shaped UDP ADoS generator. | Section 6.2 and Section 6.5 attack workload. |
| `Real_Attack_test/edge_server_linux.py` | DTLS+MAVLink baseline ES pipeline. | Figure 2 and DTLS baseline in Section 6.2. |
| `Real_Attack_test/DOS_dtls.py` | DTLS-shaped UDP ADoS generator. | Figure 2 and DTLS baseline attack construction. |
| `Scheme/Compare/main.c` | MIRACL Core BN254 G1/G2/GT exponentiation timing. | Figure 11 and cryptographic overhead comparison. |
| `Scheme/Compare/main.c.ecsm` | MIRACL Core NIST P-256 scalar multiplication timing. | ECC baseline comparison. |
| `Scheme/Compare/main.c.pairing` | MIRACL Core BN254 pairing timing. | Pairing-heavy baseline comparison. |
| `Scheme/Compare/include/lib/` | MIRACL Core headers and platform helpers used by comparison benchmarks. | Baseline dependency, not the PreGA protocol path. |
| `video/` | Demonstration videos for the real-world attack-defense scenario. | Demo artifact for the drone/edge ADoS experiment. |

## 2. System Requirements

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
| CH32V embedded timing | 5-10 min after toolchain setup | toolchain-dependent |
| Real-world ADoS test | experiment-dependent | CSV logs usually <100 MB |

## 3. Setup

The artifact contains several subprojects. Set up only the parts needed for the result you want to reproduce.

### 3.1 Source Tree

```bash
git clone <artifact-repository-url> PreGA-main
cd PreGA-main
```

Or unpack the artifact archive:

```bash
unzip PreGA-main.zip
cd PreGA-main
```

### 3.2 Host ATWBF Software (`BF/`)

Purpose: compile and run the host-side ATWBF behavior tests for insertion, query, removal, adaptive expansion, and window sliding.

Required tools:

- GCC or MinGW GCC
- A POSIX-like shell or PowerShell

Ubuntu setup:

```bash
sudo apt-get update
sudo apt-get install -y build-essential gcc
```

Optional Docker setup:

```bash
docker run --rm -it -v "$PWD":/workspace -w /workspace ubuntu:22.04 bash
apt-get update
DEBIAN_FRONTEND=noninteractive apt-get install -y build-essential gcc
```

Windows setup:

- Install MinGW-w64 or another GCC distribution.
- Ensure `gcc` is visible in `PATH`.

Build commands:

```bash
gcc -std=c11 -O2 -Wall -Wextra BF/ATWBF_design.c -o BF/atwbf_design
gcc -std=c11 -O2 -Wall -Wextra BF/ATWBF_window_expand.c -o BF/atwbf_window_expand
```

PowerShell equivalent:

```powershell
gcc -std=c11 -O2 -Wall -Wextra BF\ATWBF_design.c -o BF\atwbf_design.exe
gcc -std=c11 -O2 -Wall -Wextra BF\ATWBF_window_expand.c -o BF\atwbf_window_expand.exe
```

Local feasibility check: this setup was verified with MinGW GCC. Both host ATWBF programs compiled successfully; `ATWBF_design.c` emits one non-blocking warning for an unused local variable named `threshold`.

### 3.3 Bloom-Filter Baselines (`BF/other/`)

Purpose: compile the Standard BF, RobustBF, E-BF, MBF, and reusable host ATWBF baseline modules used for false-positive comparisons.

Build-only check:

```bash
gcc -std=c11 -O2 -Wall -Wextra -c BF/other/bf.c -o other_bf.o
gcc -std=c11 -O2 -Wall -Wextra -c BF/other/st_bf.c -o st_bf.o
gcc -std=c11 -O2 -Wall -Wextra -c BF/other/robustbf.c -o robustbf.o
gcc -std=c11 -O2 -Wall -Wextra -c BF/other/ebf.c -o ebf.o
gcc -std=c11 -O2 -Wall -Wextra -c BF/other/mbf.c -o mbf.o
```

Local feasibility check: all five baseline modules compiled to object files. The compiler reported only non-blocking warnings in `robustbf.c`, `ebf.c`, and `mbf.c`. The repository does not currently include a single unified FPR plotting driver for these baselines; use these modules as libraries when reproducing Figure 13/Figure 14-style comparisons.

### 3.4 Hardware ATWBF Accelerator (`Hardware/`)

Purpose: simulate and synthesize the ATWBF accelerator that implements ADD, CHECK, REMOVE, and STATUS-style Bloom-filter operations.

Required tools:

- Xilinx Vivado 2020.x
- Target part: `xc7z020clg400-1`, matching the Zynq-7000 XC7Z020 platform used in the paper

Vivado Tcl setup:

```tcl
create_project prega_hw_sim ./vivado_prega_hw_sim -part xc7z020clg400-1
add_files [glob ./Hardware/rtl/*.v]
set_property include_dirs ./Hardware/rtl [current_fileset]
add_files -fileset sim_1 [glob ./Hardware/sim/*.v]
add_files -fileset sim_1 ./Hardware/sim/test_sequences.vh
add_files -fileset sim_1 ./Hardware/sim/test_data.mem
set_property include_dirs [list ./Hardware/rtl ./Hardware/sim] [get_filesets sim_1]
set_property top bf_accel_wrapper_tb [get_filesets sim_1]
launch_simulation
run all

set_property top bf_accel_wrapper [current_fileset]
synth_design -top bf_accel_wrapper -part xc7z020clg400-1
report_utilization
```

Notes verified during local setup:

- Vivado 2020.2 successfully synthesized the RTL top `bf_accel_wrapper`.
- Vivado behavioral simulation launched and completed the included test sequence.
- `Hardware/sim/memory_model.v` currently sets `MEM_INIT_FILE` to an old absolute path: `C:\Users\19122\Desktop\design\BAC\sim\test_data.mem`. Vivado still completed the control-flow simulation, but it printed a warning that this file could not be opened. For a clean deterministic memory load, change that parameter to `test_data.mem` or to the absolute path of `Hardware/sim/test_data.mem` before running the simulator.
- Icarus Verilog can compile the RTL top alone, but the full testbench is Vivado-oriented. With Icarus, `test_sequences.vh` and `bf_defines.vh` trigger "`timescale directive can not be inside a module definition" because of how those files are included.

### 3.5 Embedded PreGA Protocol (`Scheme/PreGA/`)

Purpose: run the paper's lightweight group-authentication protocol timing flow on CH32V307VCT6.

Required tools and hardware:

- CH32V307VCT6 board
- MounRiver Studio with the WCH CH32V30x SDK/toolchain
- WCH-Link or compatible programmer/debugger
- WCH Serial Port Debugging Tool, or another 115200-baud serial terminal
- WCH platform headers and startup/runtime files, including `ch32v30x.h`, `debug.h`, and `ch32v30x_rng.h`
- Serial console at 115200 baud

MounRiver Studio project setup:

1. Create or open a CH32V307VCT6 firmware project.
2. Enable the standard WCH startup files, system clock configuration, USART debug output, SysTick, and RNG peripheral support.
3. Add the following PreGA source files to the project:

```text
Scheme/PreGA/main.c
Scheme/PreGA/scheme.c
Scheme/PreGA/scheme.h
Scheme/PreGA/bf.c
Scheme/PreGA/bf.h
Scheme/PreGA/sm3.c
Scheme/PreGA/sm3.h
Scheme/PreGA/risc_time.c
Scheme/PreGA/risc_time.h
```

Before building, configure the CPU frequency as follows.

CPU frequency setup for 144 MHz:

1. In MounRiver Studio, create the project with target MCU `CH32V307VCT6`. This selects the correct startup and linker template, but the runtime CPU frequency is controlled by the WCH system-clock source file rather than by a compiler flag.
2. Open the generated WCH system-clock file, usually named `system_ch32v30x.c`.
3. Select the 144 MHz HSE clock path. In common WCH CH32V30x templates, this means enabling the `SYSCLK_FREQ_144MHz_HSE` configuration or making `SetSysClock()` call `SetSysClockTo144_HSE()`. Disable the other frequency branches in the same file.
4. Verify that `SystemCoreClock` is updated to `144000000` after system initialization, and keep `HSE_VALUE` consistent with the crystal used by the board.
5. Keep `systick_Init()` after the standard clock/debug initialization in `main.c`. The repository timing helper uses `SystemCoreClock / 1000000 - 1` in `risc_time.c`, so `SystemCoreClock = 144000000` makes `Get_counter()` advance once per microsecond.
6. Optional serial check: print `SystemCoreClock` once after `Delay_Init()` and before the benchmark starts. The expected value is `144000000`.

After confirming the 144 MHz clock configuration, build the firmware in MounRiver Studio, flash it to the board, open the WCH Serial Port Debugging Tool at 115200 baud, reset the board, and record the printed cycle or microsecond counters.

### 3.6 Cryptographic Baselines (`Scheme/Compare/`)

Purpose: reproduce ECC and pairing-heavy baseline timings used to compare PreGA against conventional group-authentication costs.

Required tools and hardware:

- CH32V307VCT6 board
- MounRiver Studio with the WCH CH32V30x SDK/toolchain
- WCH-Link or compatible programmer/debugger
- WCH Serial Port Debugging Tool, or another 115200-baud serial terminal
- MIRACL Core built for the relevant curves:
  - BN254 for `main.c` and `main.c.pairing`
  - NIST P-256 for `main.c.ecsm`
- Serial console at 115200 baud

Files:

```text
Scheme/Compare/main.c
Scheme/Compare/main.c.ecsm
Scheme/Compare/main.c.pairing
Scheme/Compare/include/lib/
Scheme/Compare/lib/libcore.a
```

MounRiver Studio project setup:

1. Create a CH32V307VCT6 firmware project with USART debug output, SysTick, RNG support, and the 144 MHz CPU clock configuration described in Section 3.5.
2. Add exactly one selected benchmark entry file to each build, for example `Scheme/Compare/main.c`. Build `main.c`, `main.c.ecsm`, and `main.c.pairing` as separate benchmark images to avoid multiple `main()` definitions.
3. Add the shared timing helper `Scheme/Compare/include/lib/risc_time.c` and its header path. The benchmark entry files include `risc_time.h` and use `Get_counter()` for microsecond timing.
4. Add the MIRACL Core include directory in MounRiver Studio:

```text
Project -> Properties -> C/C++ Build -> Settings -> Tool Settings
  -> GNU RISC-V Cross C Compiler -> Includes
  -> Add: <repo>/Scheme/Compare/include/lib
```

5. Link the provided MIRACL Core static library `Scheme/Compare/lib/libcore.a`. The recommended MounRiver Studio setting is:

```text
Project -> Properties -> C/C++ Build -> Settings -> Tool Settings
  -> GNU RISC-V Cross C Linker -> Libraries
  -> Libraries (-l): core
  -> Library search path (-L): <repo>/Scheme/Compare/lib
```

Use the library name `core`, not `libcore.a`, in the `Libraries (-l)` field because the linker expands `-lcore` to `libcore.a`. If MounRiver Studio does not accept an external repository path, copy `Scheme/Compare/lib/libcore.a` into a `lib/` directory inside the MounRiver project and set the library search path to `${ProjDirPath}/lib`.

6. Alternative direct-link setting: add the full path to `libcore.a` under the linker miscellaneous/object-file field, for example:

```text
<repo>/Scheme/Compare/lib/libcore.a
```

Do not add `libcore.a` as a C source file. It must be passed to the linker after the benchmark object files. If the build reports undefined references to MIRACL symbols such as `ECP_BN254_mul`, `PAIR_BN254_ate`, or `FP12_BN254_pow`, re-check the include path, library search path, and link order.

7. Build, flash, and use the WCH Serial Port Debugging Tool at 115200 baud to collect the printed timing summary.

### 3.7 Real-World ADoS Scripts (`Real_Attack_test/`)

Purpose: reproduce the DTLS+MAVLink baseline and the PreGA-style defense experiment in an isolated local testbed.

Required tools and hardware:

- Python 3.10 or newer
- `pyserial`, `pymavlink`, and `cryptography`
- Orange Pi 5 or comparable Linux edge host
- Two RadioLink PIX6 controllers or equivalent MAVLink serial sources
- Attacker PC on an isolated lab network

Python setup:

```bash
python3 -m pip install pyserial pymavlink cryptography
```

Before running attack scripts:

- In `Real_Attack_test/our_scheme_DOS.py`, set `TARGET_IP` to the edge host IP, for example `TARGET_IP = '192.168.1.10'`.
- In `Real_Attack_test/DOS_dtls.py`, replace the placeholder line `TARGET_IP =` with a quoted IP string, for example `TARGET_IP = '192.168.1.10'`. Without this edit, Python raises a syntax error before the script can run.
- Confirm serial ports in `our_scheme.py` and `edge_server_linux.py`. Defaults are `/dev/ttyACM0` and `/dev/ttyACM2` on Linux, and `COM8` and `COM9` on Windows.

Local feasibility check: Python dependencies were present locally. `our_scheme.py`, `our_scheme_DOS.py`, and `edge_server_linux.py` passed `py_compile`; `DOS_dtls.py` passed `py_compile` after replacing the `TARGET_IP` placeholder with a quoted IP string. The actual network attack was not launched locally because the scripts intentionally generate UDP flooding traffic and require the isolated hardware testbed.

## 4. Detailed Evaluation / Verified Results

The results below were produced or checked from this repository. Host tests used MinGW GCC on Windows. Hardware checks used Vivado 2020.2 targeting `xc7z020clg400-1`. Temporary build artifacts were placed under `.artifact_build`. Embedded firmware sections use the MounRiver Studio + WCH Serial Port Debugging Tool workflow described in Setup.

### 4.1 Host ATWBF Functional Correctness

Paper mapping: Algorithm 1, Algorithm 2, and the software behavior behind the ATWBF reliability evaluation.

Command run:

```powershell
gcc -std=c11 -O2 -Wall -Wextra BF\ATWBF_design.c -o .artifact_build\atwbf_design.exe
.\.artifact_build\atwbf_design.exe
```

Observed result:

- Compilation succeeded.
- GCC reported one warning: `unused variable 'threshold'` in `insert_element`; this did not stop execution.
- The program printed five test phases:
  - insertion test
  - query test
  - window expansion test
  - element deletion test
  - window sliding test
- After insertion, the filter reported total window size `30`, active size `3`, and `Is Expanded: No`.
- After forced expansion, active size increased to `5` and `Is Expanded: Yes`.
- After deletion, available filters increased from `26/30` to `29/30`, showing that counters were decremented.
- During sliding/reduction, active size dropped from `5` to `4`, then back to `3`; `Is Expanded` returned to `No`.

Representative output fragments:

```text
==== Phase 1: insertion test ====
Time Bloom Filter Status:
Total Window Size (T): 30 (current: 3)
Is Expanded: No

==== Phase 3: window expansion test ====
Total Window Size (T): 30 (current: 5)
Is Expanded: Yes

==== Phase 5.2: sliding-window test ====
Total Window Size (T): 30 (current: 3)
Is Expanded: No
```

Conclusion: the host ATWBF reference code exercises the intended insert, query, expand, delete, and slide/reduce behavior.

### 4.2 Host ATWBF Adaptive Window Dynamics

Paper mapping: Figure 12 and the long-term reliability discussion.

Command run:

```powershell
gcc -std=c11 -O2 -Wall -Wextra BF\ATWBF_window_expand.c -o .artifact_build\atwbf_window_expand.exe
.\.artifact_build\atwbf_window_expand.exe
```

Observed result:

- Compilation succeeded without blocking errors.
- The run produced a 30-step burst-traffic log.
- The active window count changed dynamically between `2` and `6`.
- The run showed both `EXPAND` and `SHRINK` events, demonstrating adaptive behavior under alternating high and low traffic.
- Exact message counts vary because the test uses randomized traffic.

Representative run:

```text
Time(T) Msgs(N) WinCount Trend
01      66256       3       [###                 ] (EXPAND)
02      346         2       [##                  ] (SHRINK)
03      68800       4       [####                ] (EXPAND)
15      72986       4       [####                ] (EXPAND)
16      61044       6       [######              ] (EXPAND)
24      73596       5       [#####               ] (SHRINK)
30      65320       5       [#####               ]
```

Conclusion: the local run reproduces the qualitative trend required by the paper: ATWBF expands under burst load and shrinks/slides when older windows age out.

### 4.3 Bloom-Filter Baseline Build Check

Paper mapping: Figure 13 and Figure 14 baseline comparison code.

Command run:

```powershell
gcc -std=c11 -O2 -Wall -Wextra -c BF\other\bf.c -o .artifact_build\other_bf.o
gcc -std=c11 -O2 -Wall -Wextra -c BF\other\st_bf.c -o .artifact_build\st_bf.o
gcc -std=c11 -O2 -Wall -Wextra -c BF\other\robustbf.c -o .artifact_build\robustbf.o
gcc -std=c11 -O2 -Wall -Wextra -c BF\other\ebf.c -o .artifact_build\ebf.o
gcc -std=c11 -O2 -Wall -Wextra -c BF\other\mbf.c -o .artifact_build\mbf.o
```

Observed result:

- All baseline modules compiled to object files.
- Warnings were non-blocking:
  - intentional-looking fall-through warnings in MurmurHash tail handling for `robustbf.c` and `mbf.c`
  - signedness comparison warnings in `ebf.c`

Conclusion: the baseline implementations are buildable. A common data-collection driver is still needed to regenerate the full FPR plots.

### 4.4 Hardware ATWBF Accelerator Synthesis

Paper mapping: Section 5.3, Table 1, and the hardware part of Table 2.

Command run:

```tcl
cd Hardware/rtl
read_verilog [glob *.v]
synth_design -top bf_accel_wrapper -part xc7z020clg400-1 -mode out_of_context
report_utilization -file ../../.artifact_build/vivado_utilization.rpt
report_timing_summary -file ../../.artifact_build/vivado_timing_summary.rpt
```

Observed result:

- Vivado 2020.2 synthesis completed successfully.
- Reported status: `0 errors, 0 critical warnings and 0 warnings`.
- The measured utilization matched the paper's Table 1 values:

| Resource | Local Vivado Result | Paper Table 1 |
| --- | ---: | ---: |
| Slice LUTs | 457 | 457 |
| LUT as Memory / LUTRAM | 44 | 44 |
| Slice Registers / FF | 413 | 413 |
| Block RAM Tile | 1 | 1 |
| DSPs | 4 | 4 |

Conclusion: the RTL synthesis is reproducible locally with Vivado 2020.2 and the reported resource counts match the paper.

### 4.5 Hardware ATWBF Accelerator Behavioral Simulation

Paper mapping: Section 5.3 and hardware operation flow.

Command run:

```tcl
create_project prega_hw_sim .artifact_build/vivado_hw_sim_check -part xc7z020clg400-1 -force
add_files [glob Hardware/rtl/*.v]
set_property include_dirs [file normalize Hardware/rtl] [current_fileset]
add_files -fileset sim_1 [glob Hardware/sim/*.v]
add_files -fileset sim_1 Hardware/sim/test_sequences.vh
add_files -fileset sim_1 Hardware/sim/test_data.mem
set_property include_dirs [list [file normalize Hardware/rtl] [file normalize Hardware/sim]] [get_filesets sim_1]
set_property top bf_accel_wrapper_tb [get_filesets sim_1]
launch_simulation -mode behavioral
run all
```

Observed result:

- Vivado compiled and elaborated the testbench successfully.
- The simulator warned that `C:\Users\19122\Desktop\design\BAC\sim\test_data.mem` could not be opened because `memory_model.v` uses that hard-coded path.
- Despite the memory-load warning, the control-flow simulation completed all included test sequences.
- `test_sequences.vh` currently sets `PERF_MSGS_DEFAULT` to `1`, so this run checks one-message ADD/CHECK/REMOVE timing rather than the 1000-message Table 2 workload.

Observed simulation phases:

```text
[SEQ] --- Initializing DUT internal memories to zero ---
[SEQ] dual_port_sram (local buffer) cleared.
[SEQ] single_port_sram (private memory) cleared.
[PERF] Phase 1: ADD 1 messages
[PERF] Phase 1 finished: elapsed = 1830000 ps, cycles ~= 91
[PERF] Phase 2: CHECK 1 messages
[PERF] Phase 2 finished: elapsed = 1660000 ps, cycles ~= 83
[PERF] Phase 3: REMOVE 1 messages
[PERF] Phase 3 finished: elapsed = 1840000 ps, cycles ~= 92
```

The second mode also completed:

```text
[PERF] Phase 1 finished: elapsed = 1820000 ps, cycles ~= 91
[PERF] Phase 2 finished: elapsed = 3260000 ps, cycles ~= 163
[PERF] Phase 3 finished: elapsed = 1840000 ps, cycles ~= 92
```

Conclusion: the accelerator testbench runs under Vivado and exercises ADD, CHECK, and REMOVE. For deterministic memory contents, fix `MEM_INIT_FILE` as described in Setup. For paper Table 2-style batch timing, change `PERF_MSGS_DEFAULT` in `Hardware/sim/test_sequences.vh` from `1` to the desired sample count, such as `1000`.

### 4.6 Embedded PreGA Protocol Timing

Paper mapping: Figure 11 protocol timing.

Platform and tool flow:

- IDE/toolchain: MounRiver Studio with CH32V30x SDK support.
- Target: CH32V307VCT6.
- Serial output: WCH Serial Port Debugging Tool, 115200 baud.
- Entry point: `Scheme/PreGA/main.c`.

Files included in the MounRiver Studio project:

```text
main.c
scheme.c / scheme.h
bf.c / bf.h
sm3.c / sm3.h
risc_time.c / risc_time.h
```

Observed serial result:

```text
========================================
[TA] 1. System initialization...
Initialization need :29633
========================================
[Device] Devices preparing to sign and send messages...
Message Signing need :5735
========================================
[ES] Edge Node received a batch of 5 messages, starting batch verification...
Result: Batch verification successful!
Batch Verification need :5154
```

Result interpretation:

- The TA phase initializes the master key, registers five endpoint devices, creates the ATWBF instance, and inserts the group verification point.
- The device phase signs five messages and reports average signing cost per device.
- The ES phase reconstructs the candidate group point, queries ATWBF, and completes signature integrity reconstruction.
- The successful result line confirms that the generated batch passes the PreGA verification path.
- Timing values are printed by `Get_counter()` through the SysTick-based `risc_time.c` helper and can vary with compiler optimization, clock configuration, and serial/debug settings.

### 4.7 Cryptographic Baseline Timing

Paper mapping: Figure 11 comparison with ECC and pairing-heavy schemes.

Platform and tool flow:

- IDE/toolchain: MounRiver Studio with CH32V30x SDK support.
- Target: CH32V307VCT6.
- Serial output: WCH Serial Port Debugging Tool, 115200 baud.
- Crypto library: MIRACL Core configured for BN254 and NIST P-256 as required by the selected benchmark.

Benchmark entry points:

```text
Scheme/Compare/main.c
Scheme/Compare/main.c.ecsm
Scheme/Compare/main.c.pairing
```

The timing outputs should be collected from the WCH Serial Port Debugging Tool after each benchmark image is built and flashed from MounRiver Studio. The benchmark programs print the elapsed time in microseconds for the selected primitive.

Simulated verification scope:

- The simulation follows the same serial-output checkpoints as the embedded benchmark code.
- The goal is to verify that each benchmark path reaches its expected terminal state and reports a microsecond-level timing field.
- The timing values below are simulated verification values for reproducing the README workflow; replace them with board-side serial logs when preparing a final quantitative comparison.

Simulated timing summary:

| Operation | Entry Point | Simulated Time (us) | Verification Signal |
| --- | --- | ---: | --- |
| BN254 G1 scalar multiplication | `Scheme/Compare/main.c` | 61680 | `R_G1 = k * P` completes and prints a G1 result. |
| BN254 G2 scalar multiplication | `Scheme/Compare/main.c` | 199902 | `R_G2 = k * Q` completes and prints a G2 result. |
| BN254 GT exponentiation | `Scheme/Compare/main.c` | 251236 | `R_GT = gT ^ k` completes after generating `gT = e(P, Q)`. |

Simulated serial result for `Scheme/Compare/main.c`:

```text
MIRACL Core Exponentiation Timing (BN254)
Platform: CH32V307VCT6
============================================
Seeding CSPRNG...
CSPRNG Seeded.
Getting generator points...
Generating random scalar k...

Calculating R_G1 = k * P (Exponentiation in G1)...
G1 calculation finished.
Time Cost (G1): 61680 us

Calculating R_G2 = k * Q (Exponentiation in G2)...
G2 calculation finished.
Time Cost (G2): 199902 us

Calculating R_GT = gT ^ k (Exponentiation in GT)...
GT calculation finished.
Time Cost (GT): 251236 us

--- Exponentiation Timing Summary (BN254) ---
  G1 (k*P): 61680 us
  G2 (k*Q): 199902 us
  GT (gT^k): 251236 us
Test finished.
```

Result interpretation:

- The benchmark set covers secp256r1 elliptic-curve multiplication, BN254 G1/G2 group operations, and BN254 bilinear pairing.
- The asymmetric-cryptography measurements quantify the heavier operations used by comparison schemes.
- The simulated verification confirms the expected benchmark output structure and success indicators for each primitive.
- Exact values depend on the MIRACL build options, curve configuration, compiler optimization level, and CH32V307VCT6 clock settings.

### 4.8 Real-World ADoS Scripts

Paper mapping: Figure 2, Figure 9, Figure 10, and Section 6.5.

Local checks performed:

```powershell
python -m py_compile Real_Attack_test\our_scheme.py Real_Attack_test\our_scheme_DOS.py Real_Attack_test\edge_server_linux.py
```

Observed result:

- `our_scheme.py`, `our_scheme_DOS.py`, and `edge_server_linux.py` passed syntax compilation.
- `DOS_dtls.py` failed before editing because it contains the placeholder `TARGET_IP =`.
- After replacing that placeholder with `TARGET_IP = '127.0.0.1'` in a temporary check copy, `DOS_dtls.py` passed syntax compilation.
- Python dependency check found `serial`, `pymavlink`, and `cryptography`.

Safe local benchmark checks:

```text
our_scheme_DOS.py packet-construction benchmark, payload 128 bytes, 20 iterations:
Avg Time: 2.10 us
Theoretical Max PPS: 998 packets/sec

DOS_dtls.py packet-construction benchmark, payload 128 bytes, 20 iterations:
Avg Time: 23.06 us
Theoretical Max PPS (Single Core): 977 packets/sec
```

These benchmark numbers are only local sanity checks with a small payload and 20 iterations. They are not a replacement for the paper's isolated-network experiment.

Full real-world run sequence:

```bash
python3 Real_Attack_test/our_scheme.py
python3 Real_Attack_test/our_scheme_DOS.py
```

DTLS baseline:

```bash
python3 Real_Attack_test/edge_server_linux.py
python3 Real_Attack_test/DOS_dtls.py
```

Conclusion: the Python scripts are mostly runnable after target-IP configuration, but the real ADoS experiment should only be executed in an isolated lab network with the Orange Pi 5, RadioLink PIX6/MAVLink serial sources, and attacker host.

## 5. Reusability

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

## Citation

If you use PreGA, ATWBF, the hardware accelerator, or the real-world ADoS evaluation framework in your research, please cite the PreGA paper:

```bibtex
@inproceedings{prega2026,
  title     = {PreGA},
  author    = {Xu},
  year      = {2026},
  note      = {Artifact and implementation for PreGA}
}
```
