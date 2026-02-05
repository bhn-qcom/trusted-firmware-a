/*
 * Copyright (c) 2018, ARM Limited and Contributors. All rights reserved.
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 * Portions copyright (c) 2026, Qualcomm Technologies, Inc. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <arch_helpers.h>
#include <common/debug.h>
#include <common/par.h>
#include <lib/mmio.h>
#include <lib/smccc.h>
#include <lib/xlat_tables/xlat_tables_v2.h>
#include <services/arm_arch_svc.h>

#include <platform_def.h>
#include <qti_plat.h>

#include <bl31qtilib_interface.h>

int qti_ns_va_to_pa(uintptr_t va, unsigned int client_mode, uintptr_t *pa_out)
{
	sysreg_t par;
	u_register_t scr_el3;

	assert((client_mode == MODE_EL2) || (client_mode == MODE_EL1));
	assert(pa_out != NULL);

	/* Force NS/Normal-World context so the ATS uses NS stage-2 tables. */
	scr_el3 = read_scr_el3();
	write_scr_el3((scr_el3 | SCR_NS_BIT) & ~SCR_NSE_BIT);
	isb();

	/* Issue the appropriate ATS instruction for the caller's EL. */
	if (client_mode == MODE_EL2) {
		ats1e2r(va);        /* stage-1 EL2 read */
	} else {
		AT(ats12e1r, va);   /* stage-1+2 EL1 read */
	}
	isb();

	/* Capture PAR_EL1 before restoring SCR_EL3. */
	par = read_par_el1();
	write_scr_el3(scr_el3);
	isb();

	/* PAR_EL1.F (bit 0) set means the translation faulted. */
	if ((par & PAR_F_MASK) != 0) {
		return -1;
	}

	*pa_out = (uintptr_t)get_par_el1_pa(par);
	return 0;
}

bool qti_is_overlap_atf_rg(unsigned long long addr, size_t size)
{
	if (addr > addr + size ||
	    (BL31_BASE < addr + size && BL31_LIMIT > addr)) {
		return true;
	}
	return false;
}

/*
 * Set up the page tables for the generic and platform-specific memory regions.
 * The extents of the generic memory regions are specified by the function
 * arguments and consist of:
 * - Trusted SRAM seen by the BL image;
 * - Code section;
 * - Read-only data section;
 * - Coherent memory region, if applicable.
 */

void qti_setup_coherent_page_tables(
			   uintptr_t total_base,
			   size_t total_size,
			   uintptr_t code_start,
			   uintptr_t code_limit,
			   uintptr_t rodata_start,
			   uintptr_t rodata_limit,
			   uintptr_t coherent_ram_start,
			   uintptr_t coherent_ram_limit)
{
	qti_setup_page_tables(total_base, total_size, code_start, code_limit,
			      rodata_start, rodata_limit);

#if USE_COHERENT_MEM
	/* Re-map the coherent memory region */
	if ((coherent_ram_start != 0U) && (coherent_ram_limit != 0U)) {
		mmap_add_region(coherent_ram_start, coherent_ram_start,
				coherent_ram_limit - coherent_ram_start,
				MT_DEVICE | MT_RW | MT_SECURE);
	}
#else
	(void)coherent_ram_start;
	(void)coherent_ram_limit;
#endif // USE_COHERENT_MEM

	/* Add Wildcat-specific MMU mappings that are not present in common */
	/* TME fuse controller (RO, secure device) */
	mmap_add_region(QTI_TME_FUSE_CONTROLLER_BASE, QTI_TME_FUSE_CONTROLLER_BASE,
			QTI_TME_FUSE_CONTROLLER_LENGTH, MT_DEVICE | MT_RO | MT_SECURE);

	/* Secure PRNG (RO, secure device) */
	mmap_add_region(QTI_SEC_PRNG_BASE, QTI_SEC_PRNG_BASE,
			QTI_PRNG_LENGTH, MT_DEVICE | MT_RO | MT_SECURE);

	/* Shared memory used by TF-A/BL31 (RW, secure memory, non-exec) */
	mmap_add_region(TFA_SHARED_MEMORY_BASE, TFA_SHARED_MEMORY_BASE,
			TFA_SHARED_MEMORY_SIZE, MT_MEMORY | MT_SECURE | MT_RW | MT_EXECUTE_NEVER);

	/* Create the page tables to reflect the above mappings */
	init_xlat_tables();
}

static inline void qti_align_mem_region(uintptr_t addr, size_t size,
					uintptr_t *aligned_addr,
					size_t *aligned_size)
{
	*aligned_addr = round_down(addr, PAGE_SIZE);
	*aligned_size = round_up(addr - *aligned_addr + size, PAGE_SIZE);
}

int qti_mmap_add_dynamic_region(uintptr_t base_pa, size_t size,
				unsigned int attr)
{
	uintptr_t aligned_pa;
	size_t aligned_size;

	qti_align_mem_region(base_pa, size, &aligned_pa, &aligned_size);

	if (qti_is_overlap_atf_rg(base_pa, size)) {
		/* Memory shouldn't overlap with TF-A range. */
		return -EPERM;
	}

	return mmap_add_dynamic_region(aligned_pa, aligned_pa, aligned_size,
				       attr);
}

int qti_mmap_remove_dynamic_region(uintptr_t base_va, size_t size)
{
	qti_align_mem_region(base_va, size, &base_va, &size);
	return mmap_remove_dynamic_region(base_va, size);
}


/*
 * This function returns soc version which mainly consist of below fields
 *
 * soc_version[30:24] = JEP-106 continuation code for the SiP
 * soc_version[23:16] = JEP-106 identification code with parity bit for the SiP
 * soc_version[0:15]  = Implementation defined SoC ID
 */
int32_t plat_get_soc_version(void)
{
	/* soc_version will be 0 if no associated chip id could be found, or if called
	   before Chipinfo is initialized
	 */
	uint32_t soc_version = (bl31qtilib_get_chip_id() & SOC_ID_IMPL_DEF_MASK);

	uint32_t jep106az_code =
		(JEDEC_QTI_BKID << QTI_SOC_CONTINUATION_SHIFT) |
		(JEDEC_QTI_MFID << QTI_SOC_IDENTIFICATION_SHIFT);
	return (int32_t)(jep106az_code | soc_version);
}

/*
 * This function returns soc revision in below format
 *
 *   soc_revision[0:30] = SOC revision of specific SOC
 *	   [15:8] = Major Revision
 *	   [7:0]  = Minor Revision
 */
int32_t plat_get_soc_revision(void)
{
	return bl31qtilib_get_soc_revision() & SOC_ID_REV_MASK;
}

#if PLAT_RUNTIME_DEBUG_CFG

bool plat_trace_enabled(int security_state)
{
	switch (security_state) {
		case SECURE:
			return bl31qtilib_is_invasive_debug_enabled(SECURE) ||
				bl31qtilib_is_non_invasive_debug_enabled(SECURE);
		case NON_SECURE:
			return bl31qtilib_is_non_invasive_debug_enabled(NON_SECURE);
		default:
			return false;
	}
}

bool plat_external_debug_access_enabled()
{
	return bl31qtilib_is_invasive_debug_enabled(NON_SECURE);
}

bool plat_perfmon_enabled(int security_state)
{
	switch (security_state) {
		case SECURE:
			return bl31qtilib_is_non_invasive_debug_enabled(SECURE);
		case NON_SECURE:
			return bl31qtilib_is_non_invasive_debug_enabled(SECURE);
		default:
			return false;
	}
}

#endif /* PLAT_RUNTIME_DEBUG_CFG */
