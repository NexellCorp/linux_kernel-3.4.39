
#if defined (CONFIG_ARCH_S5P4418)

#define	CPU_ID_S5P4418		(0xE4418000)
#define	VOLTAGE_STEP_UV		(12500)	/* 12.5 mV */

#define ASV_TABLE_COND(id)	(id == CPU_ID_S5P4418)

/*
 *	=================================================================
 * 	|	ASV Group	|	ASV0	|	ASV1	|	ASV2	|	ASV3	|
 *	-----------------------------------------------------------------
 * 	|	ARM_IDS		|	<= 10mA	|	<= 15mA	|	<= 20mA	|	<= 20mA	|
 *	-----------------------------------------------------------------
 * 	|	ARM_RO		|	<= 110	|	<= 130	|	<= 140	|	140 >	|
 *	=================================================================
 * 	|  0: 1400 MHZ	|	1,350 mV|	1,300 mV|	1,250 mV|	1,200 mV|
 *	-----------------------------------------------------------------
 * 	|  1: 1300 MHZ	|	1,300 mV|	1,250 mV|	1,200 mV|	1,150 mV|
 *	-----------------------------------------------------------------
 * 	|  2: 1200 MHZ	|	1,250 mV|	1,200 mV|	1,150 mV|	1,100 mV|
 *	-----------------------------------------------------------------
 * 	|  3: 1100 MHZ	|	1,200 mV|	1,150 mV|	1,100 mV|	1,050 mV|
 *	-----------------------------------------------------------------
 * 	|  4: 1000 MHZ	|	1,175 mV|	1,125 mV|	1,075 mV|	1,025 mV|
 *	-----------------------------------------------------------------
 * 	|  5: 900 MHZ	|	1,150 mV|	1,100 mV|	1,050 mV|	1,000 mV|
 *	-----------------------------------------------------------------
 * 	|  6: 800 MHZ	|	1,125 mV|	1,075 mV|	1,025 mV|	1,000 mV|
 *	-----------------------------------------------------------------
 * 	|  7: 700 MHZ	|	1,100 mV|	1,050 mV|	1,000 mV|	1,000 mV|
 *	-----------------------------------------------------------------
 * 	|  8: 6500 MHZ	|	1,075 mV|	1,025 mV|	1,000 mV|	1,000 mV|
 *	-----------------------------------------------------------------
 * 	|  9: 500 MHZ	|	1,075 mV|	1,025 mV|	1,000 mV|	1,000 mV|
 *	-----------------------------------------------------------------
 * 	| 10: 400 MHZ	|	1,075 mV|	1,025 mV|	1,000 mV|	1,000 mV|
  *	=================================================================
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

struct asv_tb_info asv_tables[] = {
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
	[3] = {	.ids = 20, .ro = 140,
			.Mhz = ASB_FREQ_MHZ,
			.uV  = { UV(1200), UV(1150), UV(1100), UV(1050), UV(1025), UV(1000),
					 UV(1000), UV(1000), UV(1000), UV(1000), UV(1000), },
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

extern void nxp_cpu_id_ecid(u32 ecid[4]);
extern void nxp_cpu_id_string(u32 string[12]);

static struct asv_tb_info *current_asvtb = NULL;
static int nxp_cpufreq_asv_table(unsigned long (*freq_tables)[2])
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
		current_asvtb = &asv_tables[i];
		if (current_asvtb->ids >= ids)
			break;
	}
	idslv = ASV_ARRAY_SIZE != i ? i: (ASV_ARRAY_SIZE-1);

	/* find RO Level */
	for (i=0; (ASV_ARRAY_SIZE-1) > i; i++) {
		current_asvtb = &asv_tables[i];
		if (current_asvtb->ro >= ro)
			break;
	}
	rolv = ASV_ARRAY_SIZE != i ? i: (ASV_ARRAY_SIZE-1);

	/* find Lowest ASV Level */
	asvlv = idslv > rolv ? rolv: idslv;
	current_asvtb = &asv_tables[asvlv];

	for (i=0; FREQ_ARRAY_SIZE > i; i++) {
		freq_tables[i][0] = current_asvtb->Mhz[i] * 1000;	/* frequency */
		freq_tables[i][1] = current_asvtb->uV [i];			/* voltage */
	}

	printk("DVFS: ASV[%d] IDS[%dmA] RO[%d]\n", asvlv, ids, ro);
	return FREQ_ARRAY_SIZE;
}

static void nxp_cpufreq_asv_change_vol(unsigned long (*freq_tables)[2],
				long value, bool down, bool percent)
{
	long step_align = VOLTAGE_STEP_UV;
	long uV, dv, new;
	int i = 0;

	if (NULL == current_asvtb || (0 > value))
		return;

	/* restore voltage */
	if (0 == value) {
		for (i=0; FREQ_ARRAY_SIZE > i; i++)
			freq_tables[i][1] = current_asvtb->uV[i];
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

#else
#define	nxp_cpufreq_asv_table(t)				(-1)
#define	nxp_cpufreq_asv_change_vol(t, v, d, p)	do { } while(0)
#endif

