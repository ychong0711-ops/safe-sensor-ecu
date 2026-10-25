/*
 * Week 5 Deliverable (Part A): ARMv8-M (Cortex-M33 / STM32H563) Hardware PMSAv8 Registers
 * Reference: ARMv8-M Architecture Reference Manual (DDI 0553B.s) & ST RM0481 (STM32H563/H573)
 *
 * Key Difference vs Legacy ARMv7-M (STM32F4/F7):
 *   - ARMv7-M used power-of-two subregions (`RASR.SIZE`), wasting RAM on odd-sized SWCs.
 *   - ARMv8-M PMSAv8 (`Cortex-M33` / `Cortex-R52`) uses arbitrary 32-byte-aligned base & limit
 *     addresses (`RBAR` & `RLAR`) with ZERO overlap waste -- matching Infineon AURIX DPR ranges!
 */
#ifndef STM32H5_MPU_REGS_H
#define STM32H5_MPU_REGS_H

#include <stdint.h>

/* System Control Space (SCS) Memory-Mapped Register Base Addresses */
#define SCS_BASE_ADDR          (0xE000E000UL)
#define SCB_BASE_ADDR          (SCS_BASE_ADDR + 0x0D00UL)
#define MPU_BASE_ADDR          (SCS_BASE_ADDR + 0x0D90UL)

typedef struct {
    volatile uint32_t CPUID;    /* 0x00: CPUID Base Register */
    volatile uint32_t ICSR;     /* 0x04: Interrupt Control and State Register */
    volatile uint32_t VTOR;     /* 0x08: Vector Table Offset Register */
    volatile uint32_t AIRCR;    /* 0x0C: Application Interrupt and Reset Control */
    volatile uint32_t SCR;      /* 0x10: System Control Register */
    volatile uint32_t CCR;      /* 0x14: Configuration and Control Register */
    volatile uint8_t  SHPR[12]; /* 0x18..0x23: System Handlers Priority Registers (4..15) */
    volatile uint32_t SHCSR;    /* 0x24: System Handler Control and State Register */
    volatile uint32_t CFSR;     /* 0x28: Configurable Fault Status Register (MMFSR|BFSR|UFSR) */
    volatile uint32_t HFSR;     /* 0x2C: HardFault Status Register */
    volatile uint32_t DFSR;     /* 0x30: Debug Fault Status Register */
    volatile uint32_t MMFAR;    /* 0x34: MemManage Fault Address Register */
    volatile uint32_t BFAR;     /* 0x38: BusFault Address Register */
} ARMv8M_SCB_Regs_t;

typedef struct {
    volatile const uint32_t TYPE; /* 0x00 (0xE000ED90): MPU Type Register (DREGION[15:8] = 8 or 16) */
    volatile uint32_t CTRL;       /* 0x04 (0xE000ED94): ENABLE[0], HFNMIENA[1], PRIVDEFENA[2] */
    volatile uint32_t RNR;        /* 0x08 (0xE000ED98): Region Number Register (0..7) */
    volatile uint32_t RBAR;       /* 0x0C (0xE000ED9C): BASE[31:5] | SH[4:3] | AP[2:1] | XN[0] */
    volatile uint32_t RLAR;       /* 0x10 (0xE000EDA0): LIMIT[31:5] | AttrIndx[3:1] | EN[0] */
    volatile uint32_t RBAR_A1;    /* 0x14: Alias 1 fast region programming */
    volatile uint32_t RLAR_A1;    /* 0x18 */
    volatile uint32_t RBAR_A2;    /* 0x1C: Alias 2 */
    volatile uint32_t RLAR_A2;    /* 0x20 */
    volatile uint32_t RBAR_A3;    /* 0x24: Alias 3 */
    volatile uint32_t RLAR_A3;    /* 0x28 */
    uint32_t RESERVED0;           /* 0x2C */
    volatile uint32_t MAIR0;      /* 0x30 (0xE000EDC0): Memory Attribute Indirection 0 (Attr 0..3) */
    volatile uint32_t MAIR1;      /* 0x34 (0xE000EDC4): Memory Attribute Indirection 1 (Attr 4..7) */
} ARMv8M_MPU_Regs_t;

#define SCB_HW                 ((ARMv8M_SCB_Regs_t *)SCB_BASE_ADDR)
#define MPU_HW                 ((ARMv8M_MPU_Regs_t *)MPU_BASE_ADDR)

/* MPU_CTRL Bit Definitions */
#define MPU_CTRL_ENABLE_Msk    (1UL << 0)
#define MPU_CTRL_HFNMIENA_Msk  (1UL << 1)
#define MPU_CTRL_PRIVDEFENA_Msk (1UL << 2) /* Privileged C-BSW gets background map; unpriv Rust does NOT */

/* MPU_RBAR Bit Definitions (32-byte aligned base address) */
#define MPU_RBAR_BASE_Msk      (0xFFFFFFE0UL)
#define MPU_RBAR_SH_NON_SHARE  (0UL << 3)
#define MPU_RBAR_SH_OUTER      (2UL << 3)
#define MPU_RBAR_SH_INNER      (3UL << 3)
#define MPU_RBAR_AP_PRIV_RW    (0UL << 1) /* C-BSW RW, Unprivileged Rust SWC NO ACCESS */
#define MPU_RBAR_AP_ALL_RW     (1UL << 1) /* C-BSW RW, Unprivileged Rust SWC RW */
#define MPU_RBAR_AP_PRIV_RO    (2UL << 1) /* C-BSW RO, Unprivileged Rust SWC NO ACCESS */
#define MPU_RBAR_AP_ALL_RO     (3UL << 1) /* C-BSW RO, Unprivileged Rust SWC RO (Flash constants) */
#define MPU_RBAR_XN_EXEC_OK    (0UL << 0)
#define MPU_RBAR_XN_NO_EXEC    (1UL << 0) /* Execute-Never (W^X defense on all SRAM regions) */

/* MPU_RLAR Bit Definitions (32-byte inclusive upper limit address) */
#define MPU_RLAR_LIMIT_Msk     (0xFFFFFFE0UL)
#define MPU_RLAR_ATTR_IDX(n)   (((uint32_t)(n) & 0x7UL) << 1)
#define MPU_RLAR_EN_Msk        (1UL << 0)

/* SCB Fault Control & Status Masks */
#define SCB_SHCSR_MEMFAULTENA_Msk (1UL << 16)
#define SCB_CFSR_MMFSR_Msk        (0x000000FFUL) /* IACCVIOL[0], DACCVIOL[1], MMARVALID[7] */

/* Hardware-Stacked Cortex-M Exception Frame (8 x 32-bit words pushed automatically on entry) */
typedef struct {
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t return_pc; /* <--- Redirected by MemManage_Handler for Zero-Reset Partition Recovery! */
    uint32_t xpsr;      /* Bit 24 MUST remain 1 (Thumb state) */
} CortexM_ExceptionFrame_t;

/* Diagnostics recorded upon hardware fault interception */
typedef struct {
    volatile uint32_t fault_count;
    volatile uint32_t last_mmfar_addr;
    volatile uint32_t last_cfsr_flags;
    volatile uint32_t recovery_active;
} MpuFaultTelemetry_t;

void Bsw_Mpu_InitPartitions(void);
void Rust_Swc_FaultRecoveryLandingPad(void);

#endif /* STM32H5_MPU_REGS_H */
