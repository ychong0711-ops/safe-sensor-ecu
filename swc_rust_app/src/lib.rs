//! Week 3-5 Deliverable: Safety-Critical Application Software Component (`HallFilterSafetySwc`)
//!
//! KEY ARCHITECTURAL GUARANTEES (ISO 26262 Part 6 + ISO/IEC 27090):
//!   1. Strict `#![no_std]` + `#![forbid(unsafe_code)]` enforced by the compiler:
//!      Zero raw pointer dereferences, zero heap allocations, zero FPU instructions.
//!   2. Exclusive access to `PerInstanceMemory` (`SensorFilterPim`) is impossible without holding
//!      the compile-time RAII `PimGuard` token (`acquire_pim()` -> `Rte_Enter_EA` ... `Drop` -> `Rte_Exit_EA`).
//!   3. Combines deterministic Q2.13 integer neural filtering (288 MACs, < 3.5 us on 250 MHz Cortex-M33)
//!      with a `1oo2` physics-informed gradient & correction-magnitude plausibility monitor.

#![no_std]
#![forbid(unsafe_code)]

pub mod weights_generated;

use swc_rust_ffi::{
    acquire_pim, diag_code, safety_cfg, trigger_mpu_violation_probe_if_requested,
    RtePpCleanSwitchOut, RteRpDualHallRaw,
};
use weights_generated::{B1_Q13, B2_Q13, HIDDEN_DIM, Q_SHIFT, W1_Q13, W2_Q13};

#[inline(always)]
fn clamp_i16(val: i32) -> i16 {
    if val > i16::MAX as i32 {
        i16::MAX
    } else if val < i16::MIN as i32 {
        i16::MIN
    } else {
        val as i16
    }
}

/// Deterministic fixed-point Q2.13 6 -> 32 (ReLU) -> 3 inference (exact constant WCET, 288 MACs)
#[inline(always)]
pub fn run_q13_fnn_filter(x_centimt: &[i16; 6]) -> [i16; 3] {
    let mut hidden = [0i32; HIDDEN_DIM];

    let mut j = 0;
    while j < HIDDEN_DIM {
        let mut acc: i64 = B1_Q13[j] as i64;
        let row = &W1_Q13[j];
        acc += (row[0] as i64) * (x_centimt[0] as i64);
        acc += (row[1] as i64) * (x_centimt[1] as i64);
        acc += (row[2] as i64) * (x_centimt[2] as i64);
        acc += (row[3] as i64) * (x_centimt[3] as i64);
        acc += (row[4] as i64) * (x_centimt[4] as i64);
        acc += (row[5] as i64) * (x_centimt[5] as i64);

        let shifted = (acc >> Q_SHIFT) as i32;
        hidden[j] = if shifted > 0 { shifted } else { 0 };
        j += 1;
    }

    let mut out = [0i16; 3];
    let mut k = 0;
    while k < 3 {
        let mut acc2: i64 = B2_Q13[k] as i64;
        let row2 = &W2_Q13[k];
        let mut m = 0;
        while m < HIDDEN_DIM {
            acc2 += (row2[m] as i64) * (hidden[m] as i64);
            m += 1;
        }
        out[k] = clamp_i16((acc2 >> Q_SHIFT) as i32);
        k += 1;
    }
    out
}

/// `1oo2` Maxwell Dipole Plausibility Monitor (ISO 26262 + ISO/IEC 27090 Physical Evasion Defense):
/// Detects unmodeled near-field magnetic spoofing probes (< 18 mm from housing) or saturated sensors
/// whose asymmetric spatial gradient across the 11 mm dual-Hall baseline violates the certified training envelope.
#[inline(always)]
pub fn check_1oo2_physics_plausibility(x_centimt: &[i16; 6], y_pred: &[i16; 3]) -> (bool, i32) {
    let dx = (x_centimt[0] as i32 - x_centimt[3] as i32).abs();
    let dy = (x_centimt[1] as i32 - x_centimt[4] as i32).abs();
    let dz = (x_centimt[2] as i32 - x_centimt[5] as i32).abs();
    let l1_spatial_grad = dx + dy + dz;

    // Witness sensor B magnitude directly measures ambient external magnetic disturbance
    let witness_b_l1 =
        (x_centimt[3] as i32).abs() + (x_centimt[4] as i32).abs() + (x_centimt[5] as i32).abs();

    // Correction applied by FNN relative to primary plunger Sensor A
    let corr_bz = (x_centimt[2] as i32 - y_pred[2] as i32).abs();

    let is_plausible = l1_spatial_grad <= safety_cfg::MAX_ASYMMETRIC_GRADIENT_CENTIMT
        && corr_bz <= safety_cfg::MAX_CERTIFIED_DISTURBANCE_CENTIMT;

    (is_plausible, witness_b_l1)
}

/// Pure Safe-Rust AUTOSAR Classic / Eclipse S-CORE 1ms Periodic Runnable Logic
/// Note: Exported to C via auto-generated `swc_rust_rte_entry` crate because `rustc >= 1.85`
/// forbids `#[no_mangle]` symbol overrides directly inside `#![forbid(unsafe_code)]` crates!
pub fn runnable_hall_filter_safety_swc_step(
    rx_in: &RteRpDualHallRaw,
    tx_out: &mut RtePpCleanSwitchOut,
) {
    // Fault-injection hooks for Week 7-8 Hardware Freedom-from-Interference Verification
    if rx_in.fault_inject_cmd == 1 {
        panic!("Injected deterministic Rust no_std panic inside HallFilterSafetySwc");
    } else if rx_in.fault_inject_cmd == 2 {
        trigger_mpu_violation_probe_if_requested();
    }

    let raw_vec: [i16; 6] = [
        rx_in.bx_a_centimt,
        rx_in.by_a_centimt,
        rx_in.bz_a_centimt,
        rx_in.bx_b_centimt,
        rx_in.by_b_centimt,
        rx_in.bz_b_centimt,
    ];

    // Step 1: Deterministic Q2.13 Integer FNN Inference
    let clean_vec = run_q13_fnn_filter(&raw_vec);

    // Step 2: 1oo2 Physics Plausibility & Magnetic Spoofing Verification
    let (plausible, witness_l1) = check_1oo2_physics_plausibility(&raw_vec, &clean_vec);

    // Step 3: Acquire Exclusive Area lock via compile-time RAII `PimGuard`
    let mut pim = acquire_pim();
    pim.runnable_cycle_count = pim.runnable_cycle_count.wrapping_add(1);

    if !plausible {
        pim.consecutive_ood_counter = pim.consecutive_ood_counter.saturating_add(1);
        // Immediate Safe State Transition (`SwitchState = SafeNeutral / 0`) per ISO 26262
        tx_out.bx_clean_centimt = 0;
        tx_out.by_clean_centimt = 0;
        tx_out.bz_clean_centimt = 0;
        tx_out.switch_pressed = 0;
        tx_out.diag_status = diag_code::SAFE_STATE_SPOOFING_OOD;
        return; // `pim` automatically calls `Rte_Exit_EA_SensorFilterPim()` here via `Drop`!
    }

    pim.consecutive_ood_counter = 0;
    // 1-pole integer glitch smoothing with previous validated clean reading (alpha = 3/4 new + 1/4 prev)
    // Smoothed with alpha = 7/8 (tuned 2026-10-26; previous alpha = 3/4 dropped
    // too much of the clean signal and pushed the deployed switch-error rate up).
    let filtered_bz = ((7 * (clean_vec[2] as i32) + (pim.prev_clean_bz_centimt as i32)) >> 3) as i16;
    pim.prev_clean_bz_centimt = filtered_bz;

    tx_out.bx_clean_centimt = clean_vec[0];
    tx_out.by_clean_centimt = clean_vec[1];
    tx_out.bz_clean_centimt = filtered_bz;
    tx_out.switch_pressed = if (filtered_bz as i32) >= safety_cfg::SWITCH_THRESHOLD_BZ_CENTIMT {
        1
    } else {
        0
    };
    tx_out.diag_status = if witness_l1 > 450 {
        diag_code::WARN_DISTURBANCE_COMP
    } else {
        diag_code::OK
    };
}
