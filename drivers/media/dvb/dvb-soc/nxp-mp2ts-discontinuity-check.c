#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/fs.h>
#include <asm/segment.h>
#include <linux/uaccess.h>
#include <linux/buffer_head.h>
#include <linux/syscalls.h>
#include <linux/file.h>

#include "nxp-mp2ts-discontinuity-check.h"

unsigned int debugFVPid;
unsigned int debugFAPid;
unsigned int debugOVPid;
unsigned int debugOAPid;

unsigned int gCntFAudioTs;
unsigned int gCntFVideoTs;
unsigned int gCntOAudioTs;
unsigned int gCntOVideoTs;

void check_ts_reset(void)
{
	Totals = 0;

	gCntFAudioTs = 0;
	gCntFVideoTs = 0;
	gCntOAudioTs = 0;
	gCntOVideoTs = 0;

	memset(Stats, 0xFF, sizeof(Stats));
}

void check_ts_print(void)
{
	unsigned int i;
	pr_err("Total : %u\n\n", Totals);
	for (i = 0; i < MAX_PIDS; i++) {
		if (Stats[i].PID == 0xFFFF)
			break;

		/*	if (Stats[i].Discontinuities)	*/
			pr_err("PID:%04X  %u  %u\n",
					Stats[i].PID,
					Stats[i].Counts,
					Stats[i].Discontinuities);
	}
}

int check_ts(unsigned char *buf, unsigned int len, unsigned int packet_size)
{
	unsigned int i;
	unsigned char *packet = buf;

	while (len) {
		unsigned int PID = (((packet[1] & 0x1F) << 8) | (packet[2]));
		unsigned int CC = (packet[3] & 0x0F);

		Totals++;
		for (i = 0; i < MAX_PIDS; i++) {
			if (Stats[i].PID == PID) {
				Stats[i].Counts++;
				if (debugFVPid == Stats[i].PID)
					++gCntFVideoTs;
				else if (debugFAPid == Stats[i].PID)
					++gCntFAudioTs;
				else if (debugOVPid == Stats[i].PID)
					++gCntOVideoTs;
				else if (debugOAPid == Stats[i].PID)
					++gCntOAudioTs;

				if (((Stats[i].Last_CC + 1) % 16) != CC) {
					if (PID != 0x1FFF) {
						pr_err("Total : (%u) : PID : 0x%04X- (CC : %d/ Last CC : %d)\n",
								Totals,
								PID,
								CC,
							Stats[i].Last_CC);

						Stats[i].Discontinuities++;
					}
				}

				Stats[i].Last_CC = CC;
				break;
			} else if (Stats[i].PID == 0xFFFF) {
				Stats[i].PID = PID;
				Stats[i].Counts = 1;
				Stats[i].Discontinuities = 0;
				Stats[i].Last_CC = CC;
				break;
			}
		}

		packet += packet_size;
		len -= packet_size;
	}

	return 0;
}
