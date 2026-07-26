# PreGA

PreGA is a lightweight pre-filtering framework for protecting group authentication at the IoT edge against asymmetric denial-of-service (ADoS) attacks.

This repository provides:

- the Adaptive Time-Window Bloom Filter (ATWBF);
- embedded PreGA protocol code;
- Verilog code for the ATWBF accelerator;
- Bloom-filter and cryptographic baselines;
- real-world ADoS test scripts; and
- demonstration videos and reproduction instructions.

## 1. Overview

PreGA targets task-oriented IoT groups whose membership remains relatively stable during a mission or operating period. The system contains a Trusted Authority (TA), Endpoint Devices (EDs), and an Edge Server (ES). The attacker can inject large volumes of syntactically valid but cryptographically invalid requests in order to force the ES to perform expensive authentication operations.

PreGA restructures group authentication into a staged workflow. Invalid group states are rejected by a lightweight pre-filter before costly verification is performed.

### Camera-Ready Protocol Workflow

At a high level, the final protocol follows:

```text
message reception
    -> ATWBF pre-filtering
    -> exact GAP verification in ROM
    -> one-time commitment verification
    -> replay-state checking and consumption
    -> application delivery
```

An ATWBF miss is rejected immediately. An ATWBF hit is only an admission result and does not constitute successful authentication. The ES subsequently performs an exact lookup against the trusted Group Authentication Point (GAP) table stored in ROM and verifies the corresponding one-time commitment. Therefore, a Bloom-filter false positive can cause an additional ROM lookup, but it cannot independently authenticate a forged request.

Each ED receives only its own device-specific key material from the TA. The final protocol uses:

- a task- or session-scoped pseudonym `PID_i`, so the ES does not learn the real device index;
- a shared group-state epoch `e` for admissible group-state construction;
- an independently increasing message sequence number `t_i` for each ED;
- single-use evolved signing states; and
- a replay state indexed by the task, subgroup, pseudonym, and message sequence number.

The TA and ES aggregate device contributions using the same canonical pseudonym order. Large groups are divided into independently authenticated subgroups, avoiding enumeration of the Cartesian product of subgroup states. Cross-subgroup policy constraints, when present, are checked by the upper decision layer.

The complete protocol specification is provided in Section 5.1 and Appendix D of the camera-ready paper.

### Paper Architecture and Artifact Mapping

| Paper Layer | Purpose | Main Artifact Paths |
| --- | --- | --- |
| Protocol layer | TA initialization, ED signing, staged ES verification, trusted GAP/commitment checks, and replay protection. | `Scheme/PreGA/` |
| System layer | ATWBF insertion, lookup, removal, adaptive expansion, shrinking, and sliding. | `Scheme/PreGA/bf.c`, `Scheme/PreGA/bf.h`, `BF/` |
| Hardware layer | Hardware-parallel ATWBF acceleration and Bloom-filter memory operations. | `Hardware/rtl/`, `Hardware/sim/` |
| Evaluation layer | Embedded timing, Bloom-filter comparisons, cryptographic baselines, and real-world ADoS tests. | `Scheme/Compare/`, `BF/other/`, `Real_Attack_test/`, `video/` |

### Main Roles

| Role | Description |
| --- | --- |
| TA | Trusted and resource-rich initializer. It provisions device-specific secrets and task-scoped pseudonyms, precomputes admissible GAPs and one-time commitments, and prepares the trusted verification state. |
| ED | Resource-constrained legitimate endpoint. It evolves single-use signing states and sends authenticated messages under its task-scoped pseudonym. |
| ES | Edge verifier and ADoS target. It performs ATWBF pre-filtering, exact GAP verification, one-time commitment verification, replay-state checking, and final message delivery. |
| ADoS attacker | A return-routable network adversary that sends forged but well-formed traffic without compromising the TA or legitimate EDs or breaking the underlying hash primitive. |

### Trusted Verification State

The camera-ready protocol uses trusted read-only state at the ES:

| Trusted State | Purpose |
| --- | --- |
| `GAPTable[sid, gid, m, e]` | Stores the exact GAP associated with a session, subgroup, admissible membership state, and group-state epoch. |
| `PKCommitTable[sid, PID_i, t_i]` | Stores the trusted one-time commitment used to verify the signing state of an ED. |
| Replay state `(sid, gid, PID_i, t_i)` | Prevents an accepted signing state from being used for repeated application execution. |

Identical retransmissions may reuse the cached verification result, but they do not trigger repeated application processing. A different message carrying an already consumed sequence number is rejected.

## 2. Repository Structure

| Path | Description | Paper Mapping |
| --- | --- | --- |
| `Scheme/PreGA/main.c` | Embedded entry point for TA, ED, and ES timing. | Protocol evaluation and Figure 11 |
| `Scheme/PreGA/scheme.c` | Protocol initialization, signing, and verification operations. | Section 5.1 and Appendix D |
| `Scheme/PreGA/scheme.h` | Protocol data structures and parameters. | Section 5.1 and Appendix D |
| `Scheme/PreGA/bf.c`, `bf.h` | Embedded ATWBF implementation used by the ES. | Section 5.2 |
| `Scheme/PreGA/sm3.c`, `sm3.h` | SM3 implementation used by the embedded protocol. | Cryptographic configuration |
| `Scheme/PreGA/risc_time.c`, `risc_time.h` | CH32 timing helpers. | Embedded timing evaluation |
| `BF/ATWBF_design.c` | Host reference for ATWBF operations. | Algorithms 1 and 2 |
| `BF/ATWBF_window_expand.c` | Adaptive-window workload and reliability test. | Long-term reliability evaluation |
| `BF/other/` | Standard BF, RobustBF, E-BF, MBF, and helper implementations. | Bloom-filter comparisons |
| `Hardware/rtl/` | Verilog RTL for the ATWBF accelerator. | Section 5.3, Figures 7–8, Tables 1–2 |
| `Hardware/sim/` | Vivado-oriented testbench and test data. | Hardware simulation |
| `Scheme/Compare/` | ECC, pairing, and exponentiation comparison benchmarks. | Cryptographic overhead comparison |
| `Real_Attack_test/our_scheme.py` | PreGA-style ES-side real-world defense pipeline. | Real-world ADoS evaluation |
| `Real_Attack_test/our_scheme_DOS.py` | PreGA-shaped UDP ADoS traffic generator. | ADoS workload |
| `Real_Attack_test/edge_server_linux.py` | DTLS + MAVLink baseline server. | Baseline evaluation |
| `Real_Attack_test/DOS_dtls.py` | DTLS-shaped attack generator. | Baseline workload |
| `video/` | Demonstration videos. | Demo artifact |

## 3. System Requirements

### 3.1 Host Requirements

Minimum recommended host:

- 4-core x86-64 CPU;
- 8 GB RAM;
- 2 GB free disk space, excluding Vivado;
- Ubuntu 22.04 LTS, Windows, or another environment supported by the selected toolchain.

### 3.2 Software

- GCC 11 or newer;
- Python 3.10 or newer;
- Xilinx Vivado 2020.x for hardware simulation and synthesis;
- MounRiver Studio and the WCH CH32V30x SDK for CH32V307VCT6;
- MIRACL Core for the cryptographic comparison programs.

Install the Python dependencies with:

```bash
python3 -m pip install pyserial pymavlink cryptography
```

### 3.3 Hardware for Full Reproduction

- Xilinx Zynq-7000 XC7Z020 target for the accelerator evaluation;
- CH32V307VCT6 board and WCH-Link-compatible programmer;
- Orange Pi 5 or a comparable Linux edge host;
- two RadioLink PIX6 controllers or equivalent MAVLink serial sources;
- a separate attacker host;
- an isolated local test network.

The hardware testbench is designed for an isolated laboratory environment. Do not run the traffic-generation scripts against public or production networks.

## 4. Setup

### 4.1 Clone the Repository

```bash
git clone https://github.com/xqx17/PreGA.git
cd PreGA
```

### 4.2 Host ATWBF Software

Build the host-side ATWBF programs:

```bash
gcc -std=c11 -O2 -Wall -Wextra BF/ATWBF_design.c -o BF/atwbf_design
gcc -std=c11 -O2 -Wall -Wextra BF/ATWBF_window_expand.c -o BF/atwbf_window_expand
```

On Windows PowerShell:

```powershell
gcc -std=c11 -O2 -Wall -Wextra BF\ATWBF_design.c -o BF\atwbf_design.exe
gcc -std=c11 -O2 -Wall -Wextra BF\ATWBF_window_expand.c -o BF\atwbf_window_expand.exe
```

Run:

```bash
./BF/atwbf_design
./BF/atwbf_window_expand
```

The functional test exercises insertion, query, expansion, deletion, and window sliding. The adaptive-window test produces a multi-step traffic log containing expansion and shrinking events.

### 4.3 Bloom-Filter Baselines

Build the baseline modules:

```bash
gcc -std=c11 -O2 -Wall -Wextra -c BF/other/bf.c -o other_bf.o
gcc -std=c11 -O2 -Wall -Wextra -c BF/other/st_bf.c -o st_bf.o
gcc -std=c11 -O2 -Wall -Wextra -c BF/other/robustbf.c -o robustbf.o
gcc -std=c11 -O2 -Wall -Wextra -c BF/other/ebf.c -o ebf.o
gcc -std=c11 -O2 -Wall -Wextra -c BF/other/mbf.c -o mbf.o
```

These files can be linked into a common driver to reproduce the Bloom-filter comparison experiments with the same insertion and invalid-query streams.

### 4.4 Hardware ATWBF Accelerator

The default target is:

```text
xc7z020clg400-1
```

A minimal Vivado flow is:

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
report_timing_summary
```

Before simulation, check `Hardware/sim/memory_model.v`. Replace any machine-specific absolute `MEM_INIT_FILE` path with:

```text
test_data.mem
```

or the correct absolute path to `Hardware/sim/test_data.mem`.

For a Table 2-style batch workload, set `PERF_MSGS_DEFAULT` in `Hardware/sim/test_sequences.vh` to the intended sample count, such as `1000`.

### 4.5 Embedded PreGA Protocol

Required environment:

- CH32V307VCT6 board;
- MounRiver Studio with the WCH CH32V30x SDK;
- WCH-Link-compatible programmer/debugger;
- WCH Serial Port Debugging Tool or another 115200-baud serial terminal;
- platform headers and startup files, including `ch32v30x.h`, `debug.h`, and `ch32v30x_rng.h`.

Create a CH32V307VCT6 project and add:

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

Configure the system clock for 144 MHz:

1. Open the generated `system_ch32v30x.c`.
2. Select the 144 MHz HSE clock path.
3. Confirm that `SystemCoreClock` is set to `144000000`.
4. Keep `HSE_VALUE` consistent with the board crystal.
5. Initialize the timing helper after the standard clock and debug initialization.

Build the firmware, flash it to the board, open a 115200-baud serial terminal, reset the board, and collect the initialization, signing, and verification timing output.

The camera-ready verification path is:

```text
ATWBF pre-filtering
    -> exact GAP verification in ROM
    -> one-time commitment verification
    -> replay-state consumption
```

The end-to-end protocol measurement should include the trusted exact GAP lookup. Filter-only measurements should be reported separately from end-to-end authentication or rejection latency.

### 4.6 Cryptographic Baselines

The comparison programs are:

```text
Scheme/Compare/main.c
Scheme/Compare/main.c.ecsm
Scheme/Compare/main.c.pairing
```

Build each entry point as a separate firmware image to avoid multiple `main()` definitions.

Add the MIRACL Core include path:

```text
Scheme/Compare/include/lib
```

Link:

```text
Scheme/Compare/lib/libcore.a
```

In the linker settings, use:

```text
Libraries (-l): core
Library search path (-L): <repo>/Scheme/Compare/lib
```

Collect the timing output from the serial terminal after flashing each benchmark image.

### 4.7 Real-World ADoS Test

Install dependencies:

```bash
python3 -m pip install pyserial pymavlink cryptography
```

Configure:

- `TARGET_IP` in `Real_Attack_test/our_scheme_DOS.py`;
- `TARGET_IP` in `Real_Attack_test/DOS_dtls.py`;
- serial ports in `our_scheme.py` and `edge_server_linux.py`;
- the UDP server port, if it differs from the default `9999`.

Run the PreGA test:

```bash
python3 Real_Attack_test/our_scheme.py
python3 Real_Attack_test/our_scheme_DOS.py
```

Run the DTLS + MAVLink baseline:

```bash
python3 Real_Attack_test/edge_server_linux.py
python3 Real_Attack_test/DOS_dtls.py
```

The attacker is assumed to be return-routable and able to pass the initial DTLS cookie exchange. The evaluated attack targets the subsequent record-processing and authentication path rather than source-address spoofing alone.

## 5. Reproduction Guide

### 5.1 ATWBF Functional Correctness

Run:

```bash
./BF/atwbf_design
```

Expected behavior:

- successful insertion and membership queries;
- counter-aware deletion;
- adaptive row expansion;
- window sliding and reduction.

### 5.2 Adaptive-Window Behavior

Run:

```bash
./BF/atwbf_window_expand
```

The active-window count should change in response to the number of valid GAP entries retained in the active interval. Adversarial query traffic does not insert entries into ATWBF or directly modify its state.

### 5.3 Hardware Resource Check

Synthesize `bf_accel_wrapper` for `xc7z020clg400-1` and compare the resulting utilization report with Table 1 of the paper.

Table 1 reports the on-chip accelerator logic. The trusted GAP/commitment tables and Bloom-filter bit arrays used by the integrated system are stored outside the reported coprocessor logic resources.

### 5.4 Embedded Protocol Timing

The embedded program reports:

- TA initialization time;
- average ED signing time; and
- ES batch-verification time.

Timing values depend on clock configuration, compiler optimization, serial/debug settings, and the enabled verification stages.

### 5.5 Real-World ADoS Evaluation

Use the isolated testbed to compare:

- the PreGA pre-filtering path; and
- the DTLS + MAVLink baseline.

Record application-message processing, rejection latency, CPU load, and packet statistics under the same attack rate.

Do not interpret local packet-construction microbenchmarks as a replacement for the complete isolated-network experiment.

## 6. Protocol and Evaluation Notes

### 6.1 False-Positive Experiments

The paper reports two different false-positive experiments:

1. a fixed-state PC batch simulation over five million invalid queries; and
2. a long-running experiment with continuous valid insertions and adaptive-window updates.

These experiments use different workloads and should not be interpreted as the same statistical population.

### 6.2 Complexity

ATWBF lookup requires `O(k * T_a)` operations, where `k` is the number of hash functions and `T_a` is the number of active windows. It behaves as constant time in the implementation only when `k` is fixed and `T_a` is bounded.

The complete group-authentication cost also includes exact trusted-state lookup and per-device one-time commitment verification.

### 6.3 One-Time Signing States

Each evolved signing vector is used for at most one new message. After signing, the device irreversibly advances its state and erases the previous vector.

A retransmission reuses the already generated tuple. It does not produce a second signature with the same signing vector.

### 6.4 Pseudonymity

The ES processes task-scoped pseudonyms rather than real device indices. A pseudonym may remain linkable within one task or session, but the ES does not receive the TA's mapping from that pseudonym to the real device identity.

### 6.5 Scope

PreGA is designed for computation-oriented ADoS attacks against group authentication in task-oriented, relatively stable IoT groups.

The current scope does not include:

- physical-layer jamming;
- complete network-bandwidth saturation;
- compromise of the TA or trusted ROM;
- valid-request flooding from a compromised legitimate ED; or
- fully open groups with unrestricted dynamic joins and leaves.

## 7. Reusability

To modify ATWBF workloads:

- edit `TEST_SIZE`, `DEFAULT_BLOOM_SIZE`, `DEFAULT_TIME_WINDOW`, and `DEFAULT_THRESHOLD_RATIO` in `BF/ATWBF_design.c`;
- edit the traffic generator in `test_burst_traffic()` in `BF/ATWBF_window_expand.c`.

To compare Bloom-filter variants:

- use the implementations in `BF/other/`;
- drive all variants with the same valid insertion stream and invalid query stream.

To modify protocol parameters:

- edit the constants and data structures in `Scheme/PreGA/scheme.h`;
- update the protocol operations in `Scheme/PreGA/scheme.c`;
- update the embedded driver in `Scheme/PreGA/main.c`;
- keep the trusted GAP table, one-time commitment table, and replay-state logic consistent with Appendix D.

To change network or serial settings:

- edit `SERIAL_PORT_1`, `SERIAL_PORT_2`, `BAUD_RATE`, and `SERVER_PORT` in the ES scripts;
- edit `TARGET_IP`, `TARGET_PORT`, `PROCESS_COUNT`, and `ATTACK_DURATION` in the traffic generators.

## 8. Artifact Availability

- GitHub repository: https://github.com/xqx17/PreGA
- Archival release: https://doi.org/10.5281/zenodo.20594460
- Demo videos: see the `video/` directory and the archival release.

The GitHub repository is the development entry point. The Zenodo record is the archival release intended for long-term citation. Before publishing a new archival version, verify that the paper, README, source code, scripts, video links, and version identifier describe the same protocol and experiment configuration.

## 9. Citation

Please cite the final PreGA paper when using this artifact. Replace the placeholder metadata below with the final ACM Digital Library metadata after publication.

```bibtex
@inproceedings{prega2026,
  title     = {An Ounce of Prevention Is Worth a Pound of Cure:
               A Pre-filtering Framework Against Asymmetric DoS
               in Group Authentication at the IoT Edge},
  author    = {Xu, Qingxiang and others},
  booktitle = {Proceedings of the 2026 ACM SIGSAC Conference
               on Computer and Communications Security},
  year      = {2026},
  note      = {Artifact available at https://github.com/xqx17/PreGA}
}
```

## 10. License

Use of this repository is subject to the license file included in the project. Third-party components retain their respective licenses.
