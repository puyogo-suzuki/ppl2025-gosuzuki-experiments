/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "soc/soc.h"
#include "soc/soc_caps.h"

#include "soc/interrupt_reg.h"
#include "soc/interrupt_matrix_reg.h"

#include "riscv/encoding.h"
#include "riscv/rvruntime-frames.h"

#include "esp_tee.h"
#include "sdkconfig.h"

    .equ SAVE_REGS, 32
    .equ CONTEXT_SIZE, (SAVE_REGS * 4)
    .equ panic_from_exception, tee_panic_from_exc
    .equ panic_from_isr, tee_panic_from_isr
    .equ MAGIC, 0x1f
    .equ RTNVAL, 0xc0de
    .equ ECALL_U_MODE, 0x8
    .equ ECALL_M_MODE, 0xb
    .equ TEE_APM_INTR_MASK_0, 0x00300000
    .equ TEE_APM_INTR_MASK_1, 0x000000F8

    .global esp_tee_global_interrupt_handler

/* Macro which first allocates space on the stack to save general
 * purpose registers, and then save them. GP register is excluded.
 * The default size allocated on the stack is CONTEXT_SIZE, but it
 * can be overridden. */
.macro save_general_regs cxt_size=CONTEXT_SIZE
    addi sp, sp, -\cxt_size
    sw   ra, RV_STK_RA(sp)
    sw   tp, RV_STK_TP(sp)
    sw   t0, RV_STK_T0(sp)
    sw   t1, RV_STK_T1(sp)
    sw   t2, RV_STK_T2(sp)
    sw   s0, RV_STK_S0(sp)
    sw   s1, RV_STK_S1(sp)
    sw   a0, RV_STK_A0(sp)
    sw   a1, RV_STK_A1(sp)
    sw   a2, RV_STK_A2(sp)
    sw   a3, RV_STK_A3(sp)
    sw   a4, RV_STK_A4(sp)
    sw   a5, RV_STK_A5(sp)
    sw   a6, RV_STK_A6(sp)
    sw   a7, RV_STK_A7(sp)
    sw   s2, RV_STK_S2(sp)
    sw   s3, RV_STK_S3(sp)
    sw   s4, RV_STK_S4(sp)
    sw   s5, RV_STK_S5(sp)
    sw   s6, RV_STK_S6(sp)
    sw   s7, RV_STK_S7(sp)
    sw   s8, RV_STK_S8(sp)
    sw   s9, RV_STK_S9(sp)
    sw   s10, RV_STK_S10(sp)
    sw   s11, RV_STK_S11(sp)
    sw   t3, RV_STK_T3(sp)
    sw   t4, RV_STK_T4(sp)
    sw   t5, RV_STK_T5(sp)
    sw   t6, RV_STK_T6(sp)
.endm

.macro save_mepc
    csrr    t0, mepc
    sw      t0, RV_STK_MEPC(sp)
.endm

.macro save_mcsr
    csrr  t0, mstatus
    sw    t0, RV_STK_MSTATUS(sp)
    csrr  t0, mtvec
    sw    t0, RV_STK_MTVEC(sp)
    csrr  t0, mtval
    sw    t0, RV_STK_MTVAL(sp)
    csrr  t0, mhartid
    sw    t0, RV_STK_MHARTID(sp)
.endm

/* Restore the general purpose registers (excluding gp) from the context on
 * the stack. The context is then deallocated. The default size is CONTEXT_SIZE
 * but it can be overridden. */
.macro restore_general_regs cxt_size=CONTEXT_SIZE
    lw   ra, RV_STK_RA(sp)
    lw   tp, RV_STK_TP(sp)
    lw   t0, RV_STK_T0(sp)
    lw   t1, RV_STK_T1(sp)
    lw   t2, RV_STK_T2(sp)
    lw   s0, RV_STK_S0(sp)
    lw   s1, RV_STK_S1(sp)
    lw   a0, RV_STK_A0(sp)
    lw   a1, RV_STK_A1(sp)
    lw   a2, RV_STK_A2(sp)
    lw   a3, RV_STK_A3(sp)
    lw   a4, RV_STK_A4(sp)
    lw   a5, RV_STK_A5(sp)
    lw   a6, RV_STK_A6(sp)
    lw   a7, RV_STK_A7(sp)
    lw   s2, RV_STK_S2(sp)
    lw   s3, RV_STK_S3(sp)
    lw   s4, RV_STK_S4(sp)
    lw   s5, RV_STK_S5(sp)
    lw   s6, RV_STK_S6(sp)
    lw   s7, RV_STK_S7(sp)
    lw   s8, RV_STK_S8(sp)
    lw   s9, RV_STK_S9(sp)
    lw   s10, RV_STK_S10(sp)
    lw   s11, RV_STK_S11(sp)
    lw   t3, RV_STK_T3(sp)
    lw   t4, RV_STK_T4(sp)
    lw   t5, RV_STK_T5(sp)
    lw   t6, RV_STK_T6(sp)
    addi sp,sp, \cxt_size
.endm

.macro restore_mepc
    lw      t0, RV_STK_MEPC(sp)
    csrw    mepc, t0
.endm

.macro store_magic_general_regs
    lui     ra, MAGIC
    lui     tp, MAGIC
    lui     t0, MAGIC
    lui     t1, MAGIC
    lui     t2, MAGIC
    lui     s0, MAGIC
    lui     s1, MAGIC
    lui     a0, MAGIC
    lui     a1, MAGIC
    lui     a2, MAGIC
    lui     a3, MAGIC
    lui     a4, MAGIC
    lui     a5, MAGIC
    lui     a6, MAGIC
    lui     a7, MAGIC
    lui     s2, MAGIC
    lui     s3, MAGIC
    lui     s4, MAGIC
    lui     s5, MAGIC
    lui     s6, MAGIC
    lui     s7, MAGIC
    lui     s8, MAGIC
    lui     s9, MAGIC
    lui     s10, MAGIC
    lui     s11, MAGIC
    lui     t3, MAGIC
    lui     t4, MAGIC
    lui     t5, MAGIC
    lui     t6, MAGIC
.endm

    .section .exception_vectors.text, "ax"

    /* Exception handler. */
    .global _panic_handler
    .type _panic_handler, @function
_panic_handler:
    /* Backup t0 on the stack before using it */
    addi    sp, sp, -16
    sw      t0, 0(sp)

    // MODIFIED!
    // MODIFIED!
    // MODIFIED!
    // Test for load/store exception
    csrr t0, mcause
    andi t0,t0,-3
    xori t0,t0, 5
    bnez t0,_not_access_exception
    lw      t0, 0(sp)
    addi    sp, sp, 16
    
    save_general_regs RV_STK_FRMSZ
    sw      gp, RV_STK_GP(sp)
    save_mepc
    mv      a0, sp
    csrr    a1, mcause
    sw      a1, RV_STK_MCAUSE(sp)
    /* exception_from_panic never returns */
    jal     load_store_error
    /* We arrive here if the exception handler has returned. */
    j       _return_from_exception
_not_access_exception:
    // END OF MODIFIED!

    /* Check whether the exception is an M-mode ecall */
    csrr    t0, mcause
    xori    t0, t0, ECALL_M_MODE
    beqz    t0, _machine_ecall

    /* Check whether the exception is an U-mode ecall */
    csrr    t0, mcause
    xori    t0, t0, ECALL_U_MODE
    beqz    t0, _user_ecall

    /* Restore t0 from the stack */
    lw      t0, 0(sp)
    addi    sp, sp, 16

    /* Not an ecall, proceed to the panic handler */
    /* Allocate space on the stack and store general purpose registers */
    save_general_regs RV_STK_FRMSZ

    /* As gp register is not saved by the macro, save it here */
    sw      gp, RV_STK_GP(sp)

    /* Same goes for the SP value before trapping */
    addi    t0, sp, RV_STK_FRMSZ /* restore sp with the value when trap happened */

    /* Save CSRs */
    sw      t0, RV_STK_SP(sp)
    save_mepc
    save_mcsr

    /* Call panic_from_exception(sp) or panic_from_isr(sp)
     * depending on whether we have a pseudo excause or not.
     * If mcause's highest bit is 1, then an interrupt called this routine,
     * so we have a pseudo excause. Else, it is due to a exception, we don't
     * have an pseudo excause */
    mv      a0, sp
    csrr    a1, mcause
    /* Branches instructions don't accept immediates values, so use t1 to
     * store our comparator */
    li      t0, 0x80000000
    bgeu    a1, t0, _call_panic_handler
    sw      a1, RV_STK_MCAUSE(sp)
    /* exception_from_panic never returns */
    jal     panic_from_exception
    /* We arrive here if the exception handler has returned. */
    j       _return_from_exception

_call_panic_handler:
    /* Remove highest bit from mcause (a1) register and save it in the
     * structure */
    not     t0, t0
    and     a1, a1, t0
    sw      a1, RV_STK_MCAUSE(sp)
    jal     panic_from_isr

    /* We arrive here if the exception handler has returned. This means that
     * the exception was handled, and the execution flow should resume.
     * Restore the registers and return from the exception.
     */
_return_from_exception:
    restore_mepc
    /* MTVEC and SP are assumed to be unmodified.
     * MSTATUS, MHARTID, MTVAL are read-only and not restored. */
    lw      gp, RV_STK_GP(sp)
    restore_general_regs RV_STK_FRMSZ
    mret


_machine_ecall:
    /* Check whether this is the first M-mode ecall (see esp_tee_init) and skip context restoration */
    lui     t0, ESP_TEE_M2U_SWITCH_MAGIC
    beq     a1, t0, _skip_ctx_restore

    /* Switching back to the saved REE stack */
    la      t0, _ns_sp
    lw      sp, 0(t0)
    fence

    /* Backup the A0 register
     * This point is reached after an ecall is triggered after executing the secure service.
     * The A0 register contains the return value of the corresponding service.
     * After restoring the entire register context, we assign A0 the value back to the return value. */
    csrw    mscratch, a0
    restore_general_regs RV_STK_FRMSZ
    csrrw   a0, mscratch, zero

    /* This point is reached only after the first M-mode ecall, never again (see esp_tee_init) */
_skip_ctx_restore:
    /* Copy the ra register to mepc which contains the user app entry point (i.e. call_start_cpu0) */
    csrw    mepc, ra

    /* Set the privilege mode to transition to after mret to U-mode  */
    li      t3, MSTATUS_MPP
    csrc    mstatus, t3

    /* Jump to the REE */
    mret

    /* U-mode ecall handler */
_user_ecall:
    /* Check whether we are returning after servicing an U-mode interrupt */
    lui     t0, RTNVAL
    csrr    t1, mscratch
    beq     t0, t1, _rtn_from_ns_int
    csrwi   mscratch, 0

    /* Restore t0 from the stack */
    lw      t0, 0(sp)
    addi    sp, sp, 16

    /* This point is reached after a secure service call is issued from the REE */
    /* Save register context and the mepc */
    save_general_regs RV_STK_FRMSZ
    save_mepc

    /* Saving the U-mode (i.e. REE) stack pointer */
    la      t0, _ns_sp
    sw      sp, 0(t0)

    /* Switching to the M-mode (i.e. TEE) stack */
    la      sp, _tee_stack

    /* Load the TEE entry point (see sec_world_entry) in the mepc */
    la      t2, esp_tee_app_config
    lw      t2, ESP_TEE_CFG_OFFS_S_ENTRY_ADDR(t2)
    csrw    mepc, t2

    /* Set the privilege mode to transition to after mret to M-mode  */
    li      t3, MSTATUS_MPP
    csrs    mstatus, t3

    mret

    /* This point is reached after servicing a U-mode interrupt occurred
     * while executing a secure service */
_rtn_from_ns_int:
    /* Disable the U-mode interrupt delegation */
    csrwi    mideleg, 0

    /* Restore the secure stack pointer */
    la      t0, _s_sp
    lw      sp, 0(t0)

    /* Clear the flag set marking the completion of interrupt service */
    csrwi    mscratch, 0

    /* Set the privilege mode to transition to after mret to M-mode */
    li      t0, MSTATUS_MPP
    csrs    mstatus, t0

    /* Restore register context and resume the secure service */
    restore_mepc
    restore_general_regs

    mret

.size  _panic_handler, .-_panic_handler

    .global _imalive
    .type _imalive, @function
_imalive:
    ret