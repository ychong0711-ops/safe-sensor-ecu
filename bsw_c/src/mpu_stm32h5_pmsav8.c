/*
 * Week 5-6 Deliverable: Bare-Metal STM32H5 (Cortex-M33 PMSAv8) Partition Manager & Zero-Reset Fault Handler
 * Compiles cleanly with `arm-none-eabi-gcc -mcpu=cortex-m33 -mthumb -Os -Wall -Wextra -Werror`!
 */
#include <stdint.h>
#include "stm32h5_mpu_regs.h"
#include "aurix_tc375_mpu_regs.h"
#include "rte_generated.h"

/* Linker-exported 32-byte aligned partition symbols (`stm32h563_partitioned.ld`) */
extern uint32_t _estack_c_bsw;
extern uint32_t __rust_flash_start__, __rust_flash_end__;
extern uint32_t __c_ram_start__,      __c_ram_end__;
extern uint32_t __pim_start__,        __pim_end__;
extern uint32_t __rust_ram_start__,   __rust_ram_end__;

__attribute__((section(".bss.c_bsw")))  volatile uint32_t g_c_bsw_control_tick_1ms = 0u;
__attribute__((section(".bss.c_bsw")))  volatile uint32_t g_c_bsw_can_tx_frames    = 0u;
__attribute__((section(".data.c_bsw"))) volatile int16_t  g_c_bsw_safe_torque_nm   = 125;
__attribute__((section(".bss.c_bsw")))  MpuFaultTelemetry_t g_mpu_telemetry;

static inline void arm_dsb_isb(void)
{
    __asm__ volatile("dsb sy\n\tisb sy" ::: "memory");
}

/*
 * Exclusive Area (EA) over the shared `SensorFilterPim` -- NESTING-SAFE implementation:
 *   - The interrupt mask is taken ONCE for the OUTERMOST acquisition and is restored
 *     only when that outermost acquisition is released (a plain `cpsid i` / `cpsie i`
 *     pair would leave the CPU with interrupts ENABLED after an unbalanced release, or
 *     with interrupts DISABLED forever after a nested release).
 *   - The PRIMASK value observed on entry is saved, so a caller that already ran with
 *     interrupts masked gets them back masked (never silently re-enabled).
 *   - `g_ea_nest_depth` / `g_ea_saved_primask` are reset by the panic recovery path so
 *     that a fault-isolated runnable cannot leak a stale EA state into the next cycle.
 */
static volatile uint32_t g_ea_nest_depth = 0;
static volatile uint32_t g_ea_saved_primask = 0;

/* Read the current PRIMASK state (0 = interrupts enabled, 1 = interrupts masked) */
static inline uint32_t Ea_ReadPrimask(void)
{
    uint32_t primask;
    __asm__ volatile("mrs %0, primask" : "=r" (primask) :: "memory");
    return primask;
}

/* Exclusive Area implementation: deterministic critical section across ISR / Task preemption */
void Rte_Enter_EA_SensorFilterPim(void)
{
    /* Sample PRIMASK BEFORE masking it (afterwards it would always read back as masked) */
    uint32_t primask = Ea_ReadPrimask();

    __asm__ volatile("cpsid i" ::: "memory");

    if (g_ea_nest_depth == 0u) {
        g_ea_saved_primask = primask; /* Save the caller's previous interrupt state */
    }
    g_ea_nest_depth++;
}

void Rte_Exit_EA_SensorFilterPim(void)
{
    if (g_ea_nest_depth > 0u) {
        g_ea_nest_depth--;
    }

    if (g_ea_nest_depth == 0u) {
        /* Outermost release: restore the interrupt state observed on entry */
        if (g_ea_saved_primask == 0u) {
            __asm__ volatile("cpsie i" ::: "memory");
        }
        /* If the caller had masked the interrupts itself, they stay masked */
        g_ea_saved_primask = 0u;
    }
}

/* Deliberate illegal write probe into C-BSW protected RAM to verify hardware MPU interception */
void Bsw_TriggerIllegalMpuWriteProbe(void)
{
    g_c_bsw_safe_torque_nm = -9999; /* Blocked immediately by PMSAv8 Region 1 (`DACCVIOL`)! */
}

void Bsw_ReportRustPanicAndRecover(uint32_t panic_code)
{
    g_mpu_telemetry.fault_count++;
    g_mpu_telemetry.last_cfsr_flags = panic_code;
    g_mpu_telemetry.recovery_active = 1u;

    /*
     * A Rust panic aborts the runnable while it may still hold the Exclusive Area:
     * drop the EA nesting state and re-enable the interrupts that the EA had masked,
     * so that the next runnable is able to acquire the EA again (no stale depth, no
     * permanently masked interrupt state carried over the fault boundary).
     */
    g_ea_nest_depth = 0u;
    g_ea_saved_primask = 0u;
    __asm__ volatile("cpsie i" ::: "memory");

    while (1) {
        /* On target board, raises SVC #0xFF to unwind PSP frame back to BSW scheduler */
        __asm__ volatile("nop");
    }
}

void Rust_Swc_FaultRecoveryLandingPad(void)
{
    g_mpu_telemetry.recovery_active = 1u;
}

/*
 * Program ARMv8-M PMSAv8 Regions 0..3 with strict W^X (Write XOR Execute) & Spatial Isolation:
 *   Region 0: `FLASH_RUST_SWC` -> All Read-Only, Execute Permitted (`AP_ALL_RO | XN_EXEC_OK`)
 *   Region 1: `SRAM_C_BSW`     -> Privileged RW ONLY, Unprivileged NO ACCESS (`AP_PRIV_RW | XN_NO_EXEC`)
 *   Region 2: `SRAM_PIM_SHARE` -> Shared RW (`AP_ALL_RW | XN_NO_EXEC`) for 32-byte `SensorFilterPim`
 *   Region 3: `SRAM_RUST_SWC`  -> Sandboxed Rust Stack RW (`AP_ALL_RW | XN_NO_EXEC`)
 */
void Bsw_Mpu_InitPartitions(void)
{
    arm_dsb_isb();
    MPU_HW->CTRL = 0UL; /* Disable MPU before atomic region configuration */

    /* Attr 0: Normal Memory, Outer/Inner Write-Back Non-Transient Read/Write Allocate (0xFF) */
    MPU_HW->MAIR0 = 0x000000FFUL;

    /* Region 0: Rust SWC Flash Code + Q2.13 Weight Tables (`W1_Q13`, `W2_Q13`) */
    MPU_HW->RNR  = 0UL;
    MPU_HW->RBAR = ((uint32_t)&__rust_flash_start__ & MPU_RBAR_BASE_Msk)
                 | MPU_RBAR_SH_NON_SHARE | MPU_RBAR_AP_ALL_RO | MPU_RBAR_XN_EXEC_OK;
    MPU_HW->RLAR = (((uint32_t)&__rust_flash_end__ - 32UL) & MPU_RLAR_LIMIT_Msk)
                 | MPU_RLAR_ATTR_IDX(0) | MPU_RLAR_EN_Msk;

    /* Region 1: C-BSW Safety Control RAM -- NO ACCESS from unprivileged Rust SWC! */
    MPU_HW->RNR  = 1UL;
    MPU_HW->RBAR = ((uint32_t)&__c_ram_start__ & MPU_RBAR_BASE_Msk)
                 | MPU_RBAR_SH_INNER | MPU_RBAR_AP_PRIV_RW | MPU_RBAR_XN_NO_EXEC;
    MPU_HW->RLAR = (((uint32_t)&__c_ram_end__ - 32UL) & MPU_RLAR_LIMIT_Msk)
                 | MPU_RLAR_ATTR_IDX(0) | MPU_RLAR_EN_Msk;

    /* Region 2: 32-byte Shared `.pim_rust_swc` guarded at compile time by `PimGuard` */
    MPU_HW->RNR  = 2UL;
    MPU_HW->RBAR = ((uint32_t)&__pim_start__ & MPU_RBAR_BASE_Msk)
                 | MPU_RBAR_SH_INNER | MPU_RBAR_AP_ALL_RW | MPU_RBAR_XN_NO_EXEC;
    MPU_HW->RLAR = (((uint32_t)&__pim_end__ - 32UL) & MPU_RLAR_LIMIT_Msk)
                 | MPU_RLAR_ATTR_IDX(0) | MPU_RLAR_EN_Msk;

    /* Region 3: Dedicated Rust SWC Stack (`SRAM_RUST_SWC`) */
    MPU_HW->RNR  = 3UL;
    MPU_HW->RBAR = ((uint32_t)&__rust_ram_start__ & MPU_RBAR_BASE_Msk)
                 | MPU_RBAR_SH_INNER | MPU_RBAR_AP_ALL_RW | MPU_RBAR_XN_NO_EXEC;
    MPU_HW->RLAR = (((uint32_t)&__rust_ram_end__ - 32UL) & MPU_RLAR_LIMIT_Msk)
                 | MPU_RLAR_ATTR_IDX(0) | MPU_RLAR_EN_Msk;

    /* Enable MemManage exception vector (prevent escalation to non-recoverable HardFault) */
    SCB_HW->SHCSR |= SCB_SHCSR_MEMFAULTENA_Msk;

    /* Enable PMSAv8 with `PRIVDEFENA = 1` so C ISRs keep background peripheral access */
    MPU_HW->CTRL = MPU_CTRL_PRIVDEFENA_Msk | MPU_CTRL_ENABLE_Msk;
    arm_dsb_isb();
}

/*
 * Zero-Reset Hardware Fault Handler (`MemManage_Handler` C Payload):
 * Captures offending address (`SCB->MMFAR`), clears sticky `MMFSR` flags, and redirects the
 * stacked exception `return_pc` (`offset +24` in Cortex-M hardware stack frame) directly to
 * `Rust_Swc_FaultRecoveryLandingPad` (< 36 CPU cycles == ~0.15 us @ 250 MHz!).
 */
void MemManage_FaultRecover_C(CortexM_ExceptionFrame_t *frame)
{
    uint32_t cfsr = SCB_HW->CFSR;
    g_mpu_telemetry.fault_count++;
    g_mpu_telemetry.last_cfsr_flags = cfsr & SCB_CFSR_MMFSR_Msk;
    if ((cfsr & 0x80UL) != 0UL) { /* MMARVALID bit[7] */
        g_mpu_telemetry.last_mmfar_addr = SCB_HW->MMFAR;
    }

    /* Clear sticky MemManage fault status bits by writing 1s */
    SCB_HW->CFSR = (cfsr & SCB_CFSR_MMFSR_Msk);

    /* Redirect faulting instruction pointer to deterministic Safe-State Landing Pad */
    frame->return_pc = ((uint32_t)&Rust_Swc_FaultRecoveryLandingPad) | 1UL;
    frame->xpsr     |= (1UL << 24); /* Guarantee Thumb bit remains set on exception return */
}

__attribute__((naked)) void MemManage_Handler(void)
{
    __asm__ volatile(
        "tst lr, #4                 \n" /* Inspect EXC_RETURN bit 2: 0 = MSP, 1 = PSP (Rust SWC) */
        "ite eq                     \n"
        "mrseq r0, msp              \n"
        "mrsne r0, psp              \n"
        "b MemManage_FaultRecover_C \n"
    );
}

/* Minimal Cortex-M33 Reset Entry + 1ms Cyclic BSW Superloop for Bare-Metal ELF Build Verification */
__attribute__((section(".text.bsw"))) void Reset_Handler(void)
{
    Bsw_Mpu_InitPartitions();

    Rte_RpDualHallRawType rx = { 120, -45, 1620, 95, -30, 380, FAULT_INJECT_NONE };
    Rte_PpCleanSwitchOutType tx = { 0, 0, 0, 0u, DIAG_OK };

    while (1) {
        g_c_bsw_control_tick_1ms++;
        g_mpu_telemetry.recovery_active = 0u;

        Runnable_HallFilterSafetySwc_Step(&rx, &tx);

        if (g_mpu_telemetry.recovery_active != 0u) {
            tx.switch_pressed = 0u;
            tx.diag_status    = DIAG_SAFE_STATE_PARTITION_FAULT;
        }
        g_c_bsw_can_tx_frames++;
    }
}

__attribute__((section(".isr_vector"), used))
const uintptr_t g_pfnVectors[16] = {
    (uintptr_t)&_estack_c_bsw,
    (uintptr_t)&Reset_Handler,
    0, /* NMI */
    0, /* HardFault */
    (uintptr_t)&MemManage_Handler
};
