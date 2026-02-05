/**********************************************************************
 * Copyright (c) 2026 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 *********************************************************************/

#include <asm_macros.S>
#include <common/bl_common.h>
#include <context.h>

	.globl	plat_handle_el3_sync_sp_el0
	.globl	plat_handle_el3_irq_sp_el0
	.globl	plat_handle_el3_fiq_sp_el0
	.globl	plat_handle_el3_ea_sp_el0
	.globl	plat_handle_el3_sync_sp_elx
	.globl	plat_handle_el3_irq_sp_elx
	.globl	plat_handle_el3_fiq_sp_elx
	.globl	plat_handle_el3_ea_sp_elx
	.globl	plat_handle_el3_ea_lel_aa64
	.globl	plat_handle_el3_ea_lel_aa32
	.globl	plat_handle_rt_svc_fw_critical_error
	.globl	plat_handle_imp_def_el3_handler

	.globl	plat_asm_assert_handler

.macro prepare_for_plat_panic_handler _err_code
    msr     TPIDRRO_EL0, x0  // save x0 into TPIDRRO_EL0 (Thread ID) used as scratch
    msr     TPIDR_EL3, x30   // save x30 link register
    mov     x0, #\_err_code  // X0 is the reason for fatal
.endm

.macro restore_x30
	ldr	x30, [sp, #CTX_GPREGS_OFFSET + CTX_GPREG_LR]
.endm

/* ---------------------------------------------------------------------
 * Current EL with SP_EL0 : 0x0 - 0x200
 * ---------------------------------------------------------------------
 */
	/* -----------------------------------------------------
	 * Platform handler for EL3 Sync SP_EL0 Exception.
	 * -----------------------------------------------------
	 */
func plat_handle_el3_sync_sp_el0
	prepare_for_plat_panic_handler SYNC_EXCEPTION_SP_EL0
	b	report_unhandled_exception
endfunc plat_handle_el3_sync_sp_el0

	/* -----------------------------------------------------
	 * Platform handler for EL3 IRQ SP_EL0 Exception.
	 * -----------------------------------------------------
	 */
func plat_handle_el3_irq_sp_el0
	prepare_for_plat_panic_handler IRQ_SP_EL0
	b	report_unhandled_interrupt
endfunc plat_handle_el3_irq_sp_el0

	/* -----------------------------------------------------
	 * Platform handler for EL3 FIQ SP_EL0 Exception.
	 * -----------------------------------------------------
	 */
func plat_handle_el3_fiq_sp_el0
	prepare_for_plat_panic_handler FIQ_SP_EL0
	b	report_unhandled_interrupt
endfunc plat_handle_el3_fiq_sp_el0

	/* -----------------------------------------------------
	 * Platform handler for EL3 External Abort SP_EL0.
	 * -----------------------------------------------------
	 */
func plat_handle_el3_ea_sp_el0
	prepare_for_plat_panic_handler SERROR_SP_EL0
	b	report_unhandled_exception
endfunc plat_handle_el3_ea_sp_el0

/* ---------------------------------------------------------------------
 * Current EL with SP_ELx: 0x200 - 0x400
 * ---------------------------------------------------------------------
 */
	/* -----------------------------------------------------
	 * Platform handler for EL3 Sync SP_ELx Exception.
	 * -----------------------------------------------------
	 */
func plat_handle_el3_sync_sp_elx
	prepare_for_plat_panic_handler SYNC_EXCEPTION_SP_ELX
	b	report_unhandled_exception
endfunc plat_handle_el3_sync_sp_elx

	/* -----------------------------------------------------
	 * Platform handler for EL3 IRQ SP_ELx Exception.
	 * -----------------------------------------------------
	 */
func plat_handle_el3_irq_sp_elx
	prepare_for_plat_panic_handler IRQ_SP_ELX
	b	report_unhandled_interrupt
endfunc plat_handle_el3_irq_sp_elx

	/* -----------------------------------------------------
	 * Platform handler for EL3 FIQ SP_ELx Exception.
	 * -----------------------------------------------------
	 */
func plat_handle_el3_fiq_sp_elx
	prepare_for_plat_panic_handler FIQ_SP_ELX
	b	report_unhandled_interrupt
endfunc plat_handle_el3_fiq_sp_elx

	/* -----------------------------------------------------
	 * Platform handler for EL3 External Abort SP_ELx.
	 * -----------------------------------------------------
	 */
func plat_handle_el3_ea_sp_elx
	prepare_for_plat_panic_handler SERROR_SP_ELX
	b	report_unhandled_exception
endfunc plat_handle_el3_ea_sp_elx

/* ---------------------------------------------------------------------
 * Lower EL using AArch64 : 0x400 - 0x600
 * ---------------------------------------------------------------------
 */

 	/* -----------------------------------------------------
	 * Platform handler for EL3 External Abort aarch64.
	 * -----------------------------------------------------
	 */
func plat_handle_el3_ea_lel_aa64
	// Restore x30 as it was saved in sync_exception_aarch64
	restore_x30
	prepare_for_plat_panic_handler SERROR_AARCH64
	b	report_unhandled_exception
endfunc plat_handle_el3_ea_lel_aa64


/* ---------------------------------------------------------------------
 * Lower EL using AArch32 : 0x600 - 0x800
 * ---------------------------------------------------------------------
 */
 	/* -----------------------------------------------------
	 * Platform handler for EL3 External Abort aarch32.
	 * -----------------------------------------------------
	 */
func plat_handle_el3_ea_lel_aa32
	// Restore x30 as it was saved in sync_exception_aarch32
	restore_x30
	prepare_for_plat_panic_handler SERROR_AARCH32
	b	report_unhandled_exception
endfunc plat_handle_el3_ea_lel_aa32


 	/* -----------------------------------------------------
	 * Platform handler for EL3 RT SVC Critical Error.
	 * -----------------------------------------------------
	 */
func plat_handle_rt_svc_fw_critical_error
	prepare_for_plat_panic_handler PANIC_ASM
	b	report_unhandled_exception
endfunc plat_handle_rt_svc_fw_critical_error

	/* -----------------------------------------------------
	 * Platform handler for imp_def_el3_handler.
	 * -----------------------------------------------------
	 */
func plat_handle_imp_def_el3_handler
	prepare_for_plat_panic_handler IMPL_DEF_EL3_ERR
	b	report_unhandled_exception
endfunc plat_handle_imp_def_el3_handler

	/* -----------------------------------------------------
	 * void plat_asm_assert_handler(char *filename, unsigned int line_num) __dead2;
	 * -----------------------------------------------------
	 */
func plat_asm_assert_handler
	prepare_for_plat_panic_handler PANIC_ASM
	b	plat_panic_handler
endfunc plat_asm_assert_handler
