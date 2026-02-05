 /*
 * Copyright (c) 2023 ARM Limited and Contributors. All rights reserved.
 * Copyright (c) 2026 Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <platform_def.h>
#include <common/debug.h>
#include <qti_plat.h>
#include <lib/mmio.h>
#include <assert.h>

/*
 * Function : find_cluster_id
 * This function provides the cluster id to which the core belongs
 * Returns cluster id by reading aff1 bits in mpidr.
 * This function overrides the find_cluster_id definition in common
 * folder. This is because the Affinity bits depicting cluster id in
 * MPIDR for Pakala are different compared to previous chipsets
 */
unsigned int find_cluster_id(void)
{
	unsigned long mpidr, cluster_id;

	mpidr = read_mpidr_el1();
	cluster_id = (mpidr >> MPIDR_AFF2_SHIFT) & MPIDR_AFFLVL_MASK;
	return cluster_id;
}

/*
 * Function : find_cluster_id_by_mpidr
 * This function provides the cluster id to which the provided
 * mpidr belongs. Returns cluster id by reading aff1 bits in mpidr.
 * This function overrides the find_cluster_id definition in common
 * folder. This is because the Affinity bits depicting cluster id in
 * MPIDR for Hamoa are different compared to previous chipsets
 */
unsigned int find_cluster_id_by_mpidr(u_register_t mpidr)
{
	unsigned int cluster_id;

	cluster_id = (mpidr >> MPIDR_AFF2_SHIFT) & MPIDR_AFFLVL_MASK;
	return cluster_id;
}

