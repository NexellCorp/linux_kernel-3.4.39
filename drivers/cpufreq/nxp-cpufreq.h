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

extern void nxp_cpu_id_ecid(u32 ecid[4]);
extern void nxp_cpu_id_string(u32 string[12]);

#if defined (CONFIG_ARCH_S5P4418)

#define	CPU_ID_S5P4418		(0xE4418000)
#define	VOLTAGE_STEP_UV		(12500)	/* 12.5 mV */

#define ASV_TABLE_COND(id)	(id == CPU_ID_S5P4418)

/*
 *	=============================================================================
 * 	|	ASV Group	|	ASV0	|	ASV1	|	ASV2	|	ASV3	|	ASV4	|
 *	-----------------------------------------------------------------------------
 * 	|	ARM_IDS		|	<= 10mA	|	<= 15mA	|	<= 20mA	|	<= 50mA	|	> 50mA	|
 *	-----------------------------------------------------------------------------
 * 	|	ARM_RO		|	<= 110	|	<= 130	|	<= 140	|	<= 180 	|	180 >	|
 *	=============================================================================
 * 	|  0: 1400 MHZ	|	1,350 mV|	1,300 mV|	1,250 mV|	1,200 mV|	1,200 mV|
 *	-----------------------------------------------------------------------------
 * 	|  1: 1300 MHZ	|	1,300 mV|	1,250 mV|	1,200 mV|	1,150 mV|	1,150 mV|
 *	-----------------------------------------------------------------------------
 * 	|  2: 1200 MHZ	|	1,250 mV|	1,200 mV|	1,150 mV|	1,100 mV|	1,100 mV|
 *	-----------------------------------------------------------------------------
 * 	|  3: 1100 MHZ	|	1,200 mV|	1,150 mV|	1,100 mV|	1,050 mV|	1,050 mV|
 *	-----------------------------------------------------------------------------
 * 	|  4: 1000 MHZ	|	1,175 mV|	1,125 mV|	1,075 mV|	1,025 mV|	1,050 mV|
 *	-----------------------------------------------------------------------------
 * 	|  5: 900 MHZ	|	1,150 mV|	1,100 mV|	1,050 mV|	1,000 mV|	1,050 mV|
 *	-----------------------------------------------------------------------------
 * 	|  6: 800 MHZ	|	1,125 mV|	1,075 mV|	1,025 mV|	1,000 mV|	1,050 mV|
 *	-----------------------------------------------------------------------------
 * 	|  7: 700 MHZ	|	1,100 mV|	1,050 mV|	1,000 mV|	1,000 mV|	1,050 mV|
 *	-----------------------------------------------------------------------------
 * 	|  8: 6500 MHZ	|	1,075 mV|	1,025 mV|	1,000 mV|	1,000 mV|	1,050 mV|
 *	-----------------------------------------------------------------------------
 * 	|  9: 500 MHZ	|	1,075 mV|	1,025 mV|	1,000 mV|	1,000 mV|	1,050 mV|
 *	-----------------------------------------------------------------------------
 * 	| 10: 400 MHZ	|	1,075 mV|	1,025 mV|	1,000 mV|	1,000 mV|	1,050 mV|
 *	=============================================================================
 */
#define	FREQ_ARRAY_SIZE		(11)
#define	UV(v)				(v*1000)

struct asv_tb_info {
	int ids;
	int ro;
	long Mhz[FREQ_ARRAY_SIZE];
	long uV [FREQ_ARRAY_SIZE];
};

#define	ASB_FREQ_MHZ {	\
	[ 0] = 1400,	\
	[ 1] = 1300,	\
	[ 2] = 1200,	\
	[ 3] = 1100,	\
	[ 4] = 1000,	\
	[ 5] =  900,	\
	[ 6] =  800,	\
	[ 7] =  700,	\
	[ 8] =  600,	\
	[ 9] =  500,	\
	[10] =  400,	\
	}

static struct asv_tb_info asv_tables[] = {
	[0] = {	.ids = 10, .ro = 110,
			.Mhz = ASB_FREQ_MHZ,
			.uV  = { UV(1350), UV(1300), UV(1250), UV(1200), UV(1175), UV(1150),
					 UV(1125), UV(1100), UV(1075), UV(1075), UV(1075), },
	},
	[1] = {	.ids = 15, .ro = 130,
			.Mhz = ASB_FREQ_MHZ,
			.uV  = { UV(1300), UV(1250), UV(1200), UV(1150), UV(1125), UV(1100),
					 UV(1075), UV(1050), UV(1025), UV(1025), UV(1025), },
	},
	[2] = {	.ids = 20, .ro = 140,
			.Mhz = ASB_FREQ_MHZ,
			.uV  = { UV(1250), UV(1200), UV(1150), UV(1100), UV(1075), UV(1050),
					 UV(1025), UV(1000), UV(1000), UV(1000), UV(1000), },
	},
	[3] = {	.ids = 50, .ro = 180,
			.Mhz = ASB_FREQ_MHZ,
			.uV  = { UV(1200), UV(1150), UV(1100), UV(1050), UV(1025), UV(1000),
					 UV(1000), UV(1000), UV(1000), UV(1000), UV(1000), },
	},
	[4] = {	.ids = 50, .ro = 180,
			.Mhz = ASB_FREQ_MHZ,
			.uV  = { UV(1200), UV(1150), UV(1100), UV(1050), UV(1050), UV(1050),
					 UV(1050), UV(1050), UV(1050), UV(1050), UV(1050), },
	},
};
#define	ASV_ARRAY_SIZE	ARRAY_SIZE(asv_tables)

static inline unsigned int MtoL(unsigned int data, int bits)
{
	unsigned int result = 0;
	unsigned int mask = 1;
	int i = 0;
	for (i = 0; i<bits ; i++) {
		if (data&(1<<i))
			result |= mask<<(bits-i-1);
	}
	return result;
}

static struct asv_tb_info *__pAsvTB = NULL;
static int Asv_Level = -1;

static int s5p4418_asv_setup_table(unsigned long (*freq_tables)[2])
{
	unsigned int ecid[4] = { 0, };
	unsigned int string[12] = { 0, };
	int i, ids, ro;
	int idslv, rolv, asvlv;

	nxp_cpu_id_string(string);
	nxp_cpu_id_ecid(ecid);

	ids = MtoL((ecid[1]>>16) & 0xFF, 8);
	ro  = MtoL((ecid[1]>>24) & 0xFF, 8);

	if (ASV_TABLE_COND(string[0])) {
		if (!ids || !ro) {
			printk("DVFS: ASV not support (0x%08x, IDS:%d, RO:%d)\n", string[0], ids, ro);
			return -1;
		}
	}

	/* find IDS Level */
	for (i=0; (ASV_ARRAY_SIZE-1) > i; i++) {
		__pAsvTB = &asv_tables[i];
		if (__pAsvTB->ids >= ids)
			break;
	}
	idslv = ASV_ARRAY_SIZE != i ? i: (ASV_ARRAY_SIZE-1);

	/* find RO Level */
	for (i=0; (ASV_ARRAY_SIZE-1) > i; i++) {
		__pAsvTB = &asv_tables[i];
		if (__pAsvTB->ro >= ro)
			break;
	}
	rolv = ASV_ARRAY_SIZE != i ? i: (ASV_ARRAY_SIZE-1);

	/* find Lowest ASV Level */
	asvlv = idslv > rolv ? rolv: idslv;
	__pAsvTB = &asv_tables[asvlv];
	Asv_Level = asvlv;

	for (i=0; FREQ_ARRAY_SIZE > i; i++) {
		freq_tables[i][0] = __pAsvTB->Mhz[i] * 1000;	/* frequency */
		freq_tables[i][1] = __pAsvTB->uV [i];			/* voltage */
	}

	printk("DVFS: ASV[%d] IDS[%dmA] RO[%d]\n", asvlv, ids, ro);
	return FREQ_ARRAY_SIZE;
}

static void s5p4418_asv_modify_vol(unsigned long (*freq_tables)[2],
				long value, bool down, bool percent)
{
	long step_align = VOLTAGE_STEP_UV;
	long uV, dv, new;
	int i = 0;

	if (NULL == __pAsvTB || (0 > value))
		return;

	/* restore voltage */
	if (0 == value) {
		for (i=0; FREQ_ARRAY_SIZE > i; i++)
			freq_tables[i][1] = __pAsvTB->uV[i];
		return;
	}

	/* new voltage */
	for (i=0; FREQ_ARRAY_SIZE > i; i++) {
		uV  = freq_tables[i][1];
		dv  = percent ? ((uV/100) * value) : (value*1000);
		new = down ? uV - dv : uV + dv;

		if ((new % step_align)) {
			new = (new / step_align) * step_align;
			if (down) new += step_align;	/* Upper */
		}

		pr_debug("%7ldkhz, %7ld (%s%ld) align %ld (%s) -> %7ld\n",
			freq_tables[i][0], freq_tables[i][1],
			down?"-":"+", dv, step_align, (new % step_align)?"X":"O", new);

		freq_tables[i][1] = new;
	}
}

static int s5p4418_asv_current_label(char *buf)
{
	char *s = buf;

	if (s && __pAsvTB)
		s += sprintf(s, "%d,%dmA,%d\n", Asv_Level, __pAsvTB->ids, __pAsvTB->ro);

	return (s - buf);
}

static struct cpufreq_asv_ops asv_freq_ops = {
	.setup_table = s5p4418_asv_setup_table,
	.modify_vol = s5p4418_asv_modify_vol,
	.current_label = s5p4418_asv_current_label,
};
#else
static struct cpufreq_asv_ops asv_freq_ops = {
};
#endif


