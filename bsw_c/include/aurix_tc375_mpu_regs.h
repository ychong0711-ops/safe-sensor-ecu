/*
 * Week 6 Deliverable (Part B): Infineon AURIX TriCore TC375 Hardware Protection Cross-Reference
 * Reference: Infineon AURIX TC3xx Architecture Manual Vol. 1 (TriCore TC1.6.2P Protection System)
 *
 * Architectural Equivalence for Thesis / Interview at TUM LIS, KIT ITIV, Infineon, ETAS, Vector:
 *   - Whereas ARM Cortex-M33 uses `MPU_RBAR[i] / MPU_RLAR[i]` + `CONTROL.nPRIV`,
 *   - Infineon AURIX TriCore TC375 uses 18 Data Protection Ranges (`DPR0..DPR17`, 8-byte granularity!)
 *     + 10 Code Protection Ranges (`CPR0..CPR9`) + 6 hardware Protection Register Sets (`PSW.PRS = 0..5`)!
 *   - Switching isolation domains between C MICROSAR BSW (`PRS = 0`) and Rust SWC (`PRS = 1`) requires
 *     ZERO MPU reprogramming at runtime -- ONLY changing `PSW.PRS` across the call boundary (`< 4 cycles`)!
 */
#ifndef AURIX_TC375_MPU_REGS_H
#define AURIX_TC375_MPU_REGS_H

#include <stdint.h>

/* TriCore TC1.6.2P Core Special Function Register (CSFR) 16-bit Offsets for `MTCR` / `MFCR` */
#define AURIX_CSFR_PSW          (0xFE04U) /* Program Status Word (`PRS` bits select active Set 0..5) */
#define AURIX_CSFR_SYSCON       (0xFE14U) /* System Configuration (`PROTEN` bit[1] = 1 enables MPU) */

/* Data Protection Range 0..3 Lower & Upper Bound CSFRs (8-byte aligned address ranges) */
#define AURIX_CSFR_DPR0_L       (0xC000U) /* Region 0: C-BSW Critical Control RAM [Lower, Upper) */
#define AURIX_CSFR_DPR0_U       (0xC004U)
#define AURIX_CSFR_DPR1_L       (0xC008U) /* Region 1: Shared `.pim_rust_swc` (32 B) */
#define AURIX_CSFR_DPR1_U       (0xC00CU)
#define AURIX_CSFR_DPR2_L       (0xC010U) /* Region 2: Rust SWC Private Stack & Scratchpad */
#define AURIX_CSFR_DPR2_U       (0xC014U)
#define AURIX_CSFR_DPR3_L       (0xC018U) /* Region 3: Read-Only Sensor DMA Ring Buffer */
#define AURIX_CSFR_DPR3_U       (0xC01CU)

/* Code Protection Range 0..1 Lower & Upper Bound CSFRs */
#define AURIX_CSFR_CPR0_L       (0xD000U) /* Code Region 0: C BSW & Interrupt Vectors */
#define AURIX_CSFR_CPR0_U       (0xD004U)
#define AURIX_CSFR_CPR1_L       (0xD008U) /* Code Region 1: `.flash_rust_swc` (`HallFilterSafetySwc`) */
#define AURIX_CSFR_CPR1_U       (0xD00CU)

/* Protection Set Access Enable Masks (`PRS 0` = Trusted C BSW, `PRS 1` = Sandboxed Rust SWC) */
#define AURIX_CSFR_DPRE_0       (0xE010U) /* Data Read Enable bitmask for Protection Set 0 (C BSW) */
#define AURIX_CSFR_DPRE_1       (0xE014U) /* Data Read Enable bitmask for Protection Set 1 (Rust SWC) */
#define AURIX_CSFR_DPWE_0       (0xE020U) /* Data Write Enable bitmask for Protection Set 0 (C BSW) */
#define AURIX_CSFR_DPWE_1       (0xE024U) /* Data Write Enable bitmask for Protection Set 1 (Rust SWC) */
#define AURIX_CSFR_CPXE_0       (0xE000U) /* Code Execute Enable bitmask for Protection Set 0 */
#define AURIX_CSFR_CPXE_1       (0xE004U) /* Code Execute Enable bitmask for Protection Set 1 */

/* TriCore Class 1 Memory Protection Trap Identification Numbers (`TIN` in `D15` on Trap Entry) */
#define AURIX_TRAP_CLASS1_MPR   (2U)      /* Memory Protection: Read Access Violation */
#define AURIX_TRAP_CLASS1_MPW   (3U)      /* Memory Protection: Write Access Violation -> Zero-Reset Trap! */
#define AURIX_TRAP_CLASS1_MPX   (4U)      /* Memory Protection: Execute Access Violation */

#if defined(__TRICORE__)
/* Real HighTec LLVM / TASKING TriCore intrinsic CSFR write with mandatory `ISYNC` pipeline barrier */
#define AURIX_MTCR(csfr_addr, val)                 \
    do {                                           \
        __asm__ volatile("mtcr %0, %1\n\tisync"    \
                         :                         \
                         : "i"(csfr_addr), "d"(val)\
                         : "memory");              \
    } while (0)

static inline void Aurix_TC375_InitHybridPartitions(
    uint32_t c_ram_lo, uint32_t c_ram_hi,
    uint32_t pim_lo,   uint32_t pim_hi,
    uint32_t rust_sp_lo, uint32_t rust_sp_hi,
    uint32_t c_rom_lo, uint32_t c_rom_hi,
    uint32_t rust_rom_lo, uint32_t rust_rom_hi)
{
    AURIX_MTCR(AURIX_CSFR_DPR0_L, c_ram_lo);
    AURIX_MTCR(AURIX_CSFR_DPR0_U, c_ram_hi);
    AURIX_MTCR(AURIX_CSFR_DPR1_L, pim_lo);
    AURIX_MTCR(AURIX_CSFR_DPR1_U, pim_hi);
    AURIX_MTCR(AURIX_CSFR_DPR2_L, rust_sp_lo);
    AURIX_MTCR(AURIX_CSFR_DPR2_U, rust_sp_hi);

    AURIX_MTCR(AURIX_CSFR_CPR0_L, c_rom_lo);
    AURIX_MTCR(AURIX_CSFR_CPR0_U, c_rom_hi);
    AURIX_MTCR(AURIX_CSFR_CPR1_L, rust_rom_lo);
    AURIX_MTCR(AURIX_CSFR_CPR1_U, rust_rom_hi);

    /* Set 0 (C BSW): full RW access to DPR0|DPR1|DPR2 (0b0111 = 0x7), Exec CPR0|CPR1 (0x3) */
    AURIX_MTCR(AURIX_CSFR_DPRE_0, 0x00000007UL);
    AURIX_MTCR(AURIX_CSFR_DPWE_0, 0x00000007UL);
    AURIX_MTCR(AURIX_CSFR_CPXE_0, 0x00000003UL);

    /* Set 1 (Rust SWC): RW ONLY to DPR1 (.pim_rust_swc) & DPR2 (Rust stack) -> bit 0 (C RAM) = 0! */
    AURIX_MTCR(AURIX_CSFR_DPRE_1, 0x00000006UL);
    AURIX_MTCR(AURIX_CSFR_DPWE_1, 0x00000006UL);
    AURIX_MTCR(AURIX_CSFR_CPXE_1, 0x00000002UL);

    /* Enable TriCore Hardware Memory Protection (`SYSCON.PROTEN = 1`) */
    AURIX_MTCR(AURIX_CSFR_SYSCON, (1UL << 1));
}
#endif /* __TRICORE__ */

#endif /* AURIX_TC375_MPU_REGS_H */
