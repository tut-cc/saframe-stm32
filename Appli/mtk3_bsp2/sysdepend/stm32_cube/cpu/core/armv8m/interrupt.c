/*
 *----------------------------------------------------------------------
 *    micro T-Kernel 3.0 BSP 2.0
 *
 *    Copyright (C) 2025 by Ken Sakamura.
 *    This software is distributed under the T-License 2.1.
 *----------------------------------------------------------------------
 *
 *    Released by TRON Forum(http://www.tron.org) at 2025/03.
 *
 *----------------------------------------------------------------------
 */

#include <sys/machine.h>
#if defined(MTKBSP_STM32CUBE) && defined(MTKBSP_CPU_CORE_ARMV8M)
/*
 *	interrupt.c (ARMv8-M)
 *	Interrupt control
 */

#include <tk/tkernel.h>
#include <kernel.h>
#include <stm32n6xx.h>
#include "sysdepend.h"
#include "cpu_status.h"

/* Make vector updates visible to exception-vector fetches on cached SRAM. */
LOCAL void knl_publish_exctbl(const void *address, INT size)
{
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
	if ((SCB->CCR & SCB_CCR_DC_Msk) != 0U) {
		SCB_CleanDCache_by_Addr((uint32_t *)address, (int32_t)size);
	}
#else
	(void)address;
	(void)size;
#endif
	__DSB();
	__ISB();
}

/* HLL Interrupt Handler Table */
LOCAL UW hllint_tbl[sizeof(UW)*N_INTVEC];

/* ------------------------------------------------------------------------ */
/*
 * HLL(High level programming language) Interrupt Handler
 */
EXPORT void knl_hll_inthdr(void)
{
	FP	inthdr;
	UW	intno;

	ENTER_TASK_INDEPENDENT;

	intno	= knl_get_ipsr() - 16;
	inthdr	= (FP)hllint_tbl[intno];

	(*inthdr)(intno);

	LEAVE_TASK_INDEPENDENT;
}

/* ------------------------------------------------------------------------ */
/*
 * System-timer Interrupt handler
 */
EXPORT void knl_systim_inthdr(void)
{
	ENTER_TASK_INDEPENDENT;

	knl_timer_handler();

	LEAVE_TASK_INDEPENDENT;
}

/* ----------------------------------------------------------------------- */
/*
 * Set interrupt handler (Used in tk_def_int())
 */
EXPORT ER knl_define_inthdr( INT intno, ATR intatr, FP inthdr )
{
	volatile FP	*intvet;

	if(inthdr != NULL) {
		if ( (intatr & TA_HLNG) != 0 ) {
			hllint_tbl[intno] = (UW)inthdr;
			inthdr = knl_hll_inthdr;
		}		
	} else 	{	/* Clear interrupt handler */
		inthdr = (FP)knl_exctbl_o[N_SYSVEC + intno];
	}
	intvet = (FP*)(knl_exctbl + N_SYSVEC);
	intvet[intno] = inthdr;
	knl_publish_exctbl((const void *)&intvet[intno], sizeof(intvet[intno]));

	return E_OK;
}

/* ----------------------------------------------------------------------- */
/*
 * Return interrupt handler (Used in tk_ret_int())
 */
EXPORT void knl_return_inthdr(void)
{
	/* No processing in ARM. */
	return;
}

void knl_default_handler(void)
{
	tm_printf((UB*)"Default Handler\n");
	while(1);
}

/* ------------------------------------------------------------------------ */
/*
 * Interrupt initialize
 */
EXPORT ER knl_init_interrupt( void )
{
	/* Set Exception handler */
	knl_exctbl[2]	= (UW)knl_nmi_handler;		/* 2: NMI Handler */
	/*
	 * 3-6 (Hard/MPU/Bus/Usage Fault) keep the entries copied from the startup
	 * vector table in knl_start_mtkernel(): the application handlers in
	 * stm32n6xx_it.c, which save the fault into app_fault_record so it is
	 * reported on the next boot. The knl_*fault_handler stubs only spin.
	 */

	knl_exctbl[11]	= (UW)knl_svcall_handler;	/* 11: Svcall */
	knl_exctbl[12]	= (UW)knl_debugmon_handler;	/* 12: Debug Monitor Handler */

	knl_exctbl[14]	= (UW)knl_dispatch_entry;	/* 14: Pend SV */
	knl_exctbl[15]	= (UW)knl_systim_inthdr;	/* 15: Systick */
	knl_publish_exctbl((const void *)knl_exctbl,
		(INT)(sizeof(UW) * N_SYSVEC));

	return E_OK;
}

#endif	/* defined(MTKBSP_STM32CUBE) && defined(MTKBSP_CPU_CORE_ARMV8M) */
