/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Western Digital Corporation or its affiliates.
 *
 * Authors:
 *   Anup Patel <anup.patel@wdc.com>
 *   Atish Patra <atish.patra@wdc.com>
 */

#include <sbi/riscv_io.h>
#include <sbi/riscv_asm.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_ecall.h>
#include <sbi/sbi_ecall_interface.h>
#include <sbi/sbi_trap.h>
#include <sbi/sbi_ipi.h>

/*
 * Special handling for Hart 0 IPI.
 * Hart 0 runs RTOS directly without OpenSBI warmboot.
 *
 * VisionFive 2 uses ACLINT MSWI for IPI delivery.
 * Hart 0's MSIP register is at fixed address 0x02000000.
 * Writing 1 to this address triggers Hart 0's machine mode software interrupt.
 *
 * MSIP register layout:
 *   - 32-bit WARL register (only bit 0 is writable)
 *   - Writing 1 triggers MSIP interrupt
 *   - Address: 0x02000000
 */
#define HART0_MSIP_ADDR	0x02000000UL

static int hart0_send_ipi(void)
{
	u32 *msip = (void *)HART0_MSIP_ADDR;

	/* Write 1 to Hart 0's MSIP register to trigger interrupt */
	writel_relaxed(1, msip);

	return 0;
}

static int sbi_ecall_ipi_handler(unsigned long extid, unsigned long funcid,
				 struct sbi_trap_regs *regs,
				 struct sbi_ecall_return *out)
{
	int ret = 0;

	if (funcid == SBI_EXT_IPI_SEND_IPI) {
		ulong hmask = regs->a0;
		ulong hbase = regs->a1;

		/*
		 * Check if target is Hart 0 only (hbase=0, hmask=0x1).
		 * Use direct IMSIC write for Hart 0 since its IMSIC
		 * data is not initialized. Works for any sending hart.
		 */
		if (hbase == 0 && hmask == 0x1) {
			ret = hart0_send_ipi();
		} else {
			/* Normal IPI path for other harts */
			ret = sbi_ipi_send_smode(hmask, hbase);
		}
	} else {
		ret = SBI_ENOTSUPP;
	}

	return ret;
}

struct sbi_ecall_extension ecall_ipi;

static int sbi_ecall_ipi_register_extensions(void)
{
	return sbi_ecall_register_extension(&ecall_ipi);
}

struct sbi_ecall_extension ecall_ipi = {
	.name			= "ipi",
	.extid_start		= SBI_EXT_IPI,
	.extid_end		= SBI_EXT_IPI,
	.register_extensions	= sbi_ecall_ipi_register_extensions,
	.handle			= sbi_ecall_ipi_handler,
};
