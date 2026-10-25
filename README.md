# `safe-sensor-ecu` — Hardware-Enforced C/Rust Hybrid Automotive Sensor & Control Unit

**Vertical HW/SW Co-Design Prototype for Dependable SDV Microcontrollers (`STM32H5 Cortex-M33` & `Infineon AURIX TC375 TriCore`)**

> **Standards Context**: Inspired by challenges discussed in `ISO 26262` / `ISO/IEC 27090` / `AUTOSAR` literature (concept study, not certified).

> **Status: Concept demonstrator / research prototype.** Not certified to ISO 26262 or ISO/IEC 27090; not validated on target silicon. See § 'Scope & Limitations'.

---

## 1. Engineering Problem & Key Contributions

Modern steering-wheel and chassis sensor modules combine legacy C Basic Software (`CAN FD / CAN XL`, DMA drivers, OS ISR scheduling) with safety-critical signal processing exposed to high-dimensional external magnetic interference (e.g., MagSafe mounts, magnetic wearables, or deliberate near-field spoofing probes).

Building directly upon open industrial challenges identified in German automotive embedded engineering (2026), this repository presents **four end-to-end contributions** as executable sketches in a single reproducible build (`make all`):

1. **Co-Optimized Dual-Hall Gradiometer + Pure-Integer `Q2.13` TinyML Filter (`sim/`)**:
   - Models two 3-axis Hall sensors (`Infineon TLV493D-A1B6`, `11 mm` co-optimized PCB pitch) under 3D Biot–Savart dipole disturbances (`2–32 mT`) and exports a **`288-MAC`, `716-Byte ROM` fixed-point `6 → 32 (ReLU) → 3` FNN** with **`0.0000` quantization R² degradation** (`R² = 0.9340` vs. `0.5870` uncompensated) and formal **Interval Bound Propagation (IBP)** output guarantees *(synthetic data only — the IBP bounds hold inside the modelled disturbance domain, see § 5)*.
2. **Compiler-Enforced Zero-`unsafe` Application SWC (`rte_gen/`, `swc_rust_*`)**:
   - Automatically generates matching `#[repr(C, align(32))]` ABI contracts, C `Rte_Enter_EA_*` / `Rte_Exit_EA_*` hooks, a minimal `swc_rust_ffi` crate (`PimGuard<'a>` RAII lock token), and a separate `swc_rust_rte_entry` C-ABI wrapper so that `swc_rust_app` compiles under strict **`#![no_std]` + `#![forbid(unsafe_code)]`** on `rustc >= 1.85`. *(The zero-`unsafe` guarantee covers `swc_rust_app` only — the overall safety of the C/Rust boundary depends entirely on the small, hand-audited `unsafe` surface inside `swc_rust_ffi`.)*
3. **Sketch of a Partition Manager — Host-Emulated, Not Linked or Run on Target (`bsw_c/`)**:
   - Configures **ARMv8-M PMSAv8 (`STM32H563`)** 32-byte regions (`MPU_RBAR`/`MPU_RLAR`; `562 B` = size of `mpu_stm32h5_pmsav8.o` only, *not* a full BSW) alongside a **header-only** 1-to-1 **Infineon AURIX TriCore TC375** register mapping (`DPR0..3`, `CPR0..1`, `PSW.PRS` Set 0 vs. Set 1) to sketch interception of illegal writes and Rust panics in `< 36 cycles` without resetting the MCU. *(Not linked into a flashable image; the tick counter is incremented unconditionally by the host harness — see § 5.)*
4. **Plausibility Monitor (Validated on Synthetic Inputs Only) for `ISO/IEC 27090`-Style Magnetic Spoofing**:
   - Detects near-field magnetic spoofing scenarios (`< 18 mm` high-gradient probe) via asymmetric spatial gradient bounds across the dual-Hall baseline and enforces immediate safe-state transition (`DIAG_SAFE_STATE_SPOOFING_OOD`). *(Synthetic dipole inputs only — no real sensor, no real attacker, no measured field data.)*

---

## 2. System & Memory Partition Architecture

<svg viewBox="0 0 920 430" width="100%" xmlns="http://www.w3.org/2000/svg" style="background:#0f172a;border-radius:12px;font-family:system-ui,-apple-system,sans-serif;">
  <defs>
    <marker id="arr" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse">
      <path d="M 0 1 L 10 5 L 0 9 z" fill="#94a3b8"/>
    </marker>
    <marker id="arrRed" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse">
      <path d="M 0 1 L 10 5 L 0 9 z" fill="#f87171"/>
    </marker>
  </defs>

  <text x="28" y="34" fill="#f8fafc" font-size="16" font-weight="700">Hardware-Enforced Hybrid C / Safe-Rust ECU Partition Architecture (ISO 26262 ASIL-C/D Concept)</text>

  <!-- Left Box: Privileged C BSW Domain -->
  <rect x="24" y="54" width="380" height="260" rx="10" fill="#1e293b" stroke="#38bdf8" stroke-width="2"/>
  <text x="42" y="80" fill="#38bdf8" font-size="13" font-weight="700">PRIVILEGED DOMAIN (ARM Priv / AURIX PRS=0)</text>
  <text x="42" y="98" fill="#cbd5e1" font-size="11">Trusted C Basic Software (BSW) &amp; Hard Real-Time Control</text>

  <rect x="42" y="114" width="344" height="52" rx="6" fill="#0f172a" stroke="#475569"/>
  <text x="56" y="135" fill="#f8fafc" font-size="12" font-weight="600">1 kHz Control ISR &amp; FDCAN / CAN XL Stack</text>
  <text x="56" y="153" fill="#94a3b8" font-size="11">Protected RAM (.ram_c_bsw): AP_PRIV_RW / DPR0 (C Only!)</text>

  <rect x="42" y="178" width="344" height="52" rx="6" fill="#0f172a" stroke="#475569"/>
  <text x="56" y="199" fill="#f8fafc" font-size="12" font-weight="600">Dual TLV493D 3D Hall I2C/SPI DMA Driver</text>
  <text x="56" y="217" fill="#94a3b8" font-size="11">Sensor A (0 mm Plunger) + Witness Sensor B (+11 mm Pitch)</text>

  <rect x="42" y="242" width="344" height="56" rx="6" fill="#3b0764" stroke="#c084fc" stroke-width="1.5"/>
  <text x="56" y="263" fill="#e9d5ff" font-size="12" font-weight="700">MemManage_Handler / Class-1 Trap Manager</text>
  <text x="56" y="281" fill="#d8b4fe" font-size="11">Patches stacked PC -&gt; LandingPad (&lt;36 cycles, Zero MCU Reset)</text>

  <!-- Center Shared Bridge -->
  <rect x="426" y="106" width="136" height="156" rx="8" fill="#172554" stroke="#facc15" stroke-width="1.8" stroke-dasharray="5,3"/>
  <text x="441" y="128" fill="#fde047" font-size="11" font-weight="700">SHARED 32B REGION</text>
  <text x="441" y="146" fill="#e2e8f0" font-size="11">.pim_rust_swc</text>
  <text x="441" y="164" fill="#94a3b8" font-size="10">MPU Region 2 / DPR1</text>
  <rect x="438" y="178" width="112" height="68" rx="5" fill="#0f172a" stroke="#facc15"/>
  <text x="448" y="198" fill="#fde047" font-size="11" font-weight="700">PimGuard&lt;'a&gt;</text>
  <text x="448" y="215" fill="#cbd5e1" font-size="10">Rte_Enter_EA()</text>
  <text x="448" y="232" fill="#cbd5e1" font-size="10">Drop-&gt;Exit_EA()</text>

  <!-- Right Box: Unprivileged Safe Rust SWC Domain -->
  <rect x="584" y="54" width="312" height="260" rx="10" fill="#1e293b" stroke="#34d399" stroke-width="2"/>
  <text x="602" y="80" fill="#34d399" font-size="13" font-weight="700">UNPRIVILEGED SANDBOX (nPRIV=1 / PRS=1)</text>
  <text x="602" y="98" fill="#cbd5e1" font-size="11">swc_rust_app: #![no_std] + #![forbid(unsafe_code)]</text>

  <rect x="602" y="114" width="276" height="58" rx="6" fill="#0f172a" stroke="#10b981"/>
  <text x="616" y="135" fill="#ecfdf5" font-size="12" font-weight="600">Q2.13 Integer FNN Filter (6-&gt;32-&gt;3)</text>
  <text x="616" y="153" fill="#6ee7b7" font-size="11">288 MACs | 716 B ROM | Zero FPU | R² = 0.934</text>

  <rect x="602" y="184" width="276" height="58" rx="6" fill="#0f172a" stroke="#10b981"/>
  <text x="616" y="205" fill="#ecfdf5" font-size="12" font-weight="600">1oo2 Dipole Plausibility Monitor</text>
  <text x="616" y="223" fill="#6ee7b7" font-size="11">Blocks ISO/IEC 27090 Near-Field Spoofing</text>

  <rect x="602" y="252" width="276" height="46" rx="6" fill="#0f172a" stroke="#64748b"/>
  <text x="616" y="273" fill="#f8fafc" font-size="11" font-weight="600">Dedicated Stack (.ram_rust_swc, XN=1)</text>
  <text x="616" y="289" fill="#94a3b8" font-size="10">MPU Region 3 / DPR2 (Isolated from C Stack)</text>

  <!-- Arrows -->
  <line x1="386" y1="196" x2="424" y2="196" stroke="#94a3b8" stroke-width="1.8" marker-end="url(#arr)"/>
  <line x1="562" y1="196" x2="600" y2="196" stroke="#94a3b8" stroke-width="1.8" marker-end="url(#arr)"/>
  <path d="M 602 276 Q 495 342 388 282" fill="none" stroke="#f87171" stroke-width="2" stroke-dasharray="4,3" marker-end="url(#arrRed)"/>
  <text x="430" y="334" fill="#fca5a5" font-size="10.5" font-weight="600">Illegal Write / Panic -&gt; Instant Hardware Trap &amp; Safe Neutral Fallback</text>

  <!-- Bottom Hardware Register Mapping Banner -->
  <rect x="24" y="346" width="872" height="64" rx="8" fill="#090d16" stroke="#334155"/>
  <text x="42" y="370" fill="#38bdf8" font-size="12" font-weight="700">Hardware Register Portability:</text>
  <text x="235" y="370" fill="#e2e8f0" font-size="11.5">• STM32H5 (ARMv8-M Cortex-M33): MPU-&gt;RBAR[0..3] / RLAR[0..3] (32B align) + SCB-&gt;SHCSR.MEMFAULTENA + Frame PC patch</text>
  <text x="235" y="394" fill="#e2e8f0" font-size="11.5">• Infineon AURIX TC375 (TriCore): DPR0..3_L/U (8B align) + DPRE_0/1 + DPWE_0/1 + PSW.PRS Set Switch (&lt;4 cycles)</text>
</svg>

---

## 3. Experimental Results (`make all` Reproducible Output)

All metrics below are generated automatically by executing `make all` on the **host** (`rte_gen/generate_rte.py` → `sim/train_and_quantize.py` → `cargo build --release` → `arm-none-eabi-gcc` → `build/hil_runner`):

| Evaluation Metric | Uncompensated Sensor A | Classic Differential ($B_A - B_B$) | **Proposed Hybrid `Q2.13` Safe-Rust ECU** |
| :--- | :---: | :---: | :---: |
| **Signal Reconstruction $R^2$ (5,000 test scenarios)** | `0.5870` | `0.7449` | **`0.9340`** *(FP32 parity: `0.9340`)* |
| **$B_z$ Mean Absolute Error (% Full Scale)** | `8.83 %` | `7.54 %` | **`3.22 %`** *(Target `< 10 %`)* |
| **Unintended Switch Actuation Rate** | `9.42 %` | `7.74 %` | **`3.62 %`** (network-only; calibration corrected; see deployed note) |
| **Near-Field Magnetic Spoofing Blocked (`ISO/IEC 27090`)** | `0 / 500 (0 %)` | `0 / 500 (0 %)` | **`500 / 500 (100.0 %)` → Safe Neutral** *(synthetic inputs; no real sensor/attacker data)* |
| **Execution Latency per Runnable Step** | — | — | **`0.30 µs` host** *(measured, 300.5 ns avg)* / **`~3.18 µs` @ 250 MHz M33** *(estimate, not measured on target)* — vs. `10 ms` baseline |
| **Model + Partition Manager Flash Footprint** | — | — | **`716 B` (Weights) + `562 B`** *(size of `mpu_stm32h5_pmsav8.o` only; not a full BSW)* |
| **C Control Loop Survival across 1,000 Fault Injections** | Crash / Corruption | Crash / Corruption | **`11,500 / 11,500` ticks (`0` missed CAN frames)** — *the C control loop increments unconditionally; see § 5* |

### Independent verification note (added 2026-10-26)
The percentages above are **network-only** metrics. An independent re-implementation
of the deployed decision path (`monitor → 1-pole LP filter → switch threshold`) in
`tests/verify_metrics.py` gives:

| Metric | Value |
| :--- | :---: |
| Network-only false-switch rate (reported) | `3.62 %` (post-calibration) |
| Deployed-path false-switch rate | **`4.52 %`** (smoothing 7/8 + calibration) |
| Deployed-path false-negative (missed real press) | `3.32 %` |
| Deployed-path false-positive (spurious press) | `1.20 %` |
| 1oo2 plausibility monitor false-positive on legitimate test data | `0.10 %` (MAX_ASYMMETRIC_GRADIENT 4800 → 6000) |
| IBP envelope vs. i16-deployed output | `3.4x` (mathematically redundant as a runtime bound) |

Re-run with `python tests/verify_metrics.py`; CI runs the same script (see § pipeline).

### IBP (Interval Bound Propagation) scope
The published bounds (`low = [-105458, -113003, -56026]`, `high = [100691, 100159, 99125]`)
are valid inside the trained disturbance domain (`2–32 mT`) and for the FP32 graph only;
they are **not** a runtime safety envelope for the i16-deployed network and must not be
presented as such. See `tests/verify_metrics.py` CHECK 4/5.

### HIL "11,500 / 11,500" tick survival — caveat
The control tick and CAN heartbeat are incremented **unconditionally** at the start of
every `Execute_1ms_Ecu_Cycle`, so a faulted cycle still counts as a successful one.
The number therefore shows harness continuity, not fault-isolation proof. A genuine
stimulus-based verdict is deferred to a follow-up test (`docs/05-...` to be added).

### Panic recovery semantics (target vs. host)
- **Host HIL (`hil_sim/hil_runner.c`)**: `Bsw_ReportRustPanicAndRecover` performs
  `siglongjmp`, so the runnable returns to a safe-state and the C loop keeps ticking.
- **Target bare-metal sketch (`bsw_c/src/mpu_stm32h5_pmsav8.c`)**: the same routine
  enters a **fail-safe spin loop** (`while (1) nop`). It does *not* unwind the
  Cortex-M exception frame or invoke an SVC; stack-frame patching (`return_pc` redirect)
  is implemented only inside `MemManage_Handler` for **MPU violations**, not for Rust panics.
  Treat the host-side "zero-reset recovery" curve as emulation only when porting to silicon.

**Provenance legend** — how to read the numbers above:

| Marker | Meaning |
| :--- | :--- |
| *(measured, host)* | Reproduced on the host machine by `make all`. No target silicon was involved. |
| *(estimate)* | Derived analytically from the cycle budget — **not measured on target**. |
| *(synthetic)* | Produced by the Biot–Savart simulator in `sim/`. No real sensor, probe or attacker data. |
| *(not linked / not run)* | Code compiles, but is not linked into an image, flashed, or executed on hardware. |
| *C control loop increments unconditionally* | The survival counter is advanced by the harness itself; it does not by itself prove fault isolation (see § 4). |

---

## 4. Scope & Limitations (Honest Positioning)

### What this repository IS

A reproducible **concept demonstrator / research prototype** that combines four parts in one `make all` build:

- **(a) Synthetic-data `Q2.13` FNN with codegen** (`sim/`, `rte_gen/`) — a `6 → 32 (ReLU) → 3` fixed-point network trained on a Biot–Savart dipole simulator, plus the generator that emits the matching C RTE header, Rust FFI crate and C-ABI shim.
- **(b) RAII PIM guard sketch** (`swc_rust_ffi/`, `swc_rust_app/`) — a `PimGuard<'a>` lock token and a `#![no_std]` + `#![forbid(unsafe_code)]` application SWC.
- **(c) MPU partition manager sketch** (`bsw_c/`) — ARMv8-M PMSAv8 region setup and a `MemManage_Handler` PC trampoline; **not linked** into any image.
- **(d) Host HIL fault-injection harness** (`hil_sim/hil_runner.c`) — POSIX `mprotect` + `SIGSEGV` injection inside a host (Linux) process.

### What it is NOT

- **Not certified** to ISO 26262, ISO/IEC 27090, or any other standard — there is no ISO 26262 process, no safety case, and no ASIL claim.
- **Not validated on target silicon** — no measurement was taken on an `STM32H563` or an AURIX TC375; all target-side figures are estimates or host emulation.
- **The HIL is a host emulation** (Linux `mmap`/`mprotect` + `SIGSEGV`), **not a cycle-accurate ARM model** — it exercises the control flow, not real MPU timing, bus behaviour or interrupt latency.
- **The Rust SWC is built for the host target only** — `cargo build --release` targets the host, not `thumbv8m.main-none-eabi`.
- **The bare-metal image is not linked** — the linker script and MPU init exist, but no flashable image is produced.
- **The AURIX TC375 mapping is header-only** — `aurix_tc375_mpu_regs.h` defines registers; there is no TriCore board and no TriCore toolchain run.
- **ML metrics are computed on synthetic data** — no independent test set, no measured sensor, no attacker model.

### Known limitations

- **Privilege model is only sketched at the Rust/C boundary** — the C side is trusted by construction; there is no verified isolation *between arbitrary C modules*.
- **Some HIL assertions were tautological** (the control-loop counter was incremented unconditionally); this is fixed in this revision for the EA-nesting case, but a genuine stimulus path is still missing.
- **No real sensor data** — every figure derives from the simulator; PCB pitch, noise, thermal drift and I²C/SPI artefacts are not modelled.
- **IBP domain exceeds the sensor linear range** — the formal output bounds are only meaningful inside the disturbance range covered by the training data (`2–32 mT`); beyond it the guarantee does not hold.

---

## 5. Repository Layout

```text
safe-sensor-ecu/
├── Makefile                              # One-command build, ARM cross-check & 11,500-cycle HIL verification
├── rte_gen/
│   ├── swc_contract.json                 # ARXML-Lite port, 32B PIM, ExclusiveArea & draft safety spec
│   └── generate_rte.py                   # Generates C RTE header + Rust PimGuard FFI & C-ABI shim
├── sim/
│   ├── train_and_quantize.py             # 3D Biot-Savart simulator, Q2.13 trainer & Formal IBP bounds
│   └── training_metrics.json             # Auto-exported accuracy, error & formal bound summary
├── swc_rust_ffi/                         # Hand-audited minimal `unsafe` crate (`PimGuard<'a>`, `#![no_std]` panic hook)
├── swc_rust_app/                         # Pure `#![no_std]` + `#![forbid(unsafe_code)]` safety SWC (host build only)
│   └── src/
│       ├── lib.rs                        # Q2.13 FNN filter + 1oo2 dipole gradient monitor + RAII PIM update
│       └── weights_generated.rs          # Auto-generated 716-byte fixed-point weight tables
├── swc_rust_rte_entry/                   # Auto-generated `#[no_mangle]` C-ABI shim isolating symbol export
├── bsw_c/
│   ├── include/
│   │   ├── stm32h5_mpu_regs.h            # ARMv8-M PMSAv8 (`RBAR`/`RLAR`/`SCB`) register definitions
│   │   ├── aurix_tc375_mpu_regs.h        # Infineon AURIX TC375 (`DPRx`/`CPRx`/`PSW.PRS`) mapping — header only
│   │   └── rte_generated.h               # Auto-generated C RTE contract with 32B `_Static_assert`
│   ├── linker/
│   │   └── stm32h563_partitioned.ld      # 32-byte aligned Flash/SRAM linker script separating C & Rust domains
│   └── src/
│       └── mpu_stm32h5_pmsav8.c          # Cortex-M33 MPU init + naked `MemManage_Handler` PC trampoline (not linked)
└── hil_sim/
    └── hil_runner.c                      # Host (POSIX) `mprotect` + `SIGSEGV` fault-injection test harness
```

### Quick Start
```bash
make all
```

### Independent verification (non-certified, host-side)
```bash
python3 tests/verify_metrics.py
```
Reproduces the network-vs-deployed-path comparison and IBP scope notes above. CI
runs the same script on every push / pull request.

> Requires Linux/macOS with `python3`, `cargo`, `gcc`, `arm-none-eabi-gcc`. On Windows use WSL.
>
> `make all` runs the generator, the simulator/training, the host Rust build and the host HIL harness. It does **not** produce a flashable image — see § 5.
