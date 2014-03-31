/*
 * (C) Copyright 2009
 * jung hyun kim, Nexell Co, <jhkim@nexell.co.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef __BOARD_PM_H__
#define __BOARD_PM_H__

#include <linux/suspend.h>

#define	SCR_ARM_SECOND_BOOT		IO_ADDRESS(0xC0010C1C)	// RTC scratch

#define	SCR_ALIVE_BASE			IO_ADDRESS(PHY_BASEADDR_ALIVE)
#define	SCR_SIGNAGURE_RESET		(SCR_ALIVE_BASE + 0x68)
#define	SCR_SIGNAGURE_SET		(SCR_ALIVE_BASE + 0x6C)
#define	SCR_SIGNAGURE_READ		(SCR_ALIVE_BASE + 0x70)
#define	SCR_WAKE_FN_RESET		(SCR_ALIVE_BASE + 0xAC)	// ALIVESCRATCHRST1
#define	SCR_WAKE_FN_SET			(SCR_ALIVE_BASE + 0xB0)
#define	SCR_WAKE_FN_READ		(SCR_ALIVE_BASE + 0xB4)
#define	SCR_CRC_RET_RESET		(SCR_ALIVE_BASE + 0xB8)	// ALIVESCRATCHRST2
#define	SCR_CRC_RET_SET			(SCR_ALIVE_BASE + 0xBC)
#define	SCR_CRC_RET_READ		(SCR_ALIVE_BASE + 0xC0)
#define	SCR_CRC_PHY_RESET		(SCR_ALIVE_BASE + 0xC4)	// ALIVESCRATCHRST3
#define	SCR_CRC_PHY_SET			(SCR_ALIVE_BASE + 0xC8)
#define	SCR_CRC_PHY_READ		(SCR_ALIVE_BASE + 0xCC)
#define	SCR_CRC_LEN_RESET		(SCR_ALIVE_BASE + 0xD0)	// ALIVESCRATCHRST4
#define	SCR_CRC_LEN_SET			(SCR_ALIVE_BASE + 0xD4)
#define	SCR_CRC_LEN_READ		(SCR_ALIVE_BASE + 0xD8)

#define SUSPEND_SIGNATURE		(0x50575200)	/* PWR (ASCII) */
#define	SUSPEND_SAVE_SIZE		(1024*1024)		/* (_etext - _stext) */

struct board_suspend_ops {
	int  (*valid)		(suspend_state_t state);	/* before driver suspend */
	int  (*begin)		(suspend_state_t state);	/* before driver suspend */
	int  (*prepare)		(void);						/* after driver suspend */
	int  (*enter) 		(suspend_state_t state);	/* enter suspend */
	int  (*poweroff)	(void);						/* before cpu power off */
	void (*poweron)		(void);						/* after cpu power on */
	void (*finish)		(void);						/* before driver resume */
	void (*end)			(void);						/* after driver resume */
};

struct suspend_mark_up {
	unsigned int resume_fn;
	unsigned int signature;
	unsigned int save_phy_addr;
	unsigned int save_phy_len;
	unsigned int save_crc_ret;
};

extern void nxp_board_suspend_register(struct board_suspend_ops *ops);

/* Implement */
extern void (*nxp_board_shutdown)(void);
extern void (*nxp_board_pre_shutdown)(void);
extern void (*nxp_board_suspend_mark)(struct suspend_mark_up *mark, int suspend);
extern void (*nxp_board_reset)(char str, const char *cmd);

#endif /* __BOARD_PM_H__ */