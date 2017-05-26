/* ==========================================================================
 * $File: //dwh/usb_iip/dev/software/otg/linux/drivers/dwc_otg_hcd_intr.c $
 * $Revision: #89 $
 * $Date: 2011/10/20 $
 * $Change: 1869487 $
 *
 * Synopsys HS OTG Linux Software Driver and documentation (hereinafter,
 * "Software") is an Unsupported proprietary work of Synopsys, Inc. unless
 * otherwise expressly agreed to in writing between Synopsys and you.
 *
 * The Software IS NOT an item of Licensed Software or Licensed Product under
 * any End User Software License Agreement or Agreement for Licensed Product
 * with Synopsys or any supplement thereto. You are permitted to use and
 * redistribute this Software in source and binary forms, with or without
 * modification, provided that redistributions of source code must retain this
 * notice. You may not view, use, disclose, copy or distribute this file or
 * any information contained herein except pursuant to this license grant from
 * Synopsys. If you do not agree with this notice, including the disclaimer
 * below, then you are not authorized to use the Software.
 *
 * THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS" BASIS
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * ========================================================================== */
#ifndef DWC_DEVICE_ONLY

#include "dwc_otg_hcd.h"
#include "dwc_otg_regs.h"
#include "dwc_otg_mphi_fix.h"

#include <linux/jiffies.h>
//#include <mach/hardware.h>
#include <asm/fiq.h>


extern bool microframe_schedule;

/** @file
 * This file contains the implementation of the HCD Interrupt handlers.
 */

/*
 * Some globals to communicate between the FIQ and INTERRUPT
 */

void * dummy_send;
mphi_regs_t c_mphi_regs;
volatile void *dwc_regs_base;
int fiq_done, int_done;

gintsts_data_t  gintsts_saved = {.d32 = 0};
hcint_data_t    hcint_saved[MAX_EPS_CHANNELS];
hcintmsk_data_t hcintmsk_saved[MAX_EPS_CHANNELS];
int             split_out_xfersize[MAX_EPS_CHANNELS];
haint_data_t    haint_saved;

int g_next_sched_frame, g_np_count, g_np_sent;
static int mphi_int_count = 0 ;

hcchar_data_t nak_hcchar;
hctsiz_data_t nak_hctsiz;
hcsplt_data_t nak_hcsplt;
int nak_count;

int complete_sched[MAX_EPS_CHANNELS] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
int split_start_frame[MAX_EPS_CHANNELS];
int queued_port[MAX_EPS_CHANNELS];

#ifdef FIQ_DEBUG
char buffer[1000*16];
int wptr;
void notrace _fiq_print(FIQDBG_T dbg_lvl, char *fmt, ...)
{
	FIQDBG_T dbg_lvl_req = FIQDBG_PORTHUB;
	va_list args;
	char text[17];
	hfnum_data_t hfnum = { .d32 = FIQ_READ(dwc_regs_base + 0x408) };
	unsigned long flags;

	local_irq_save(flags);
	local_fiq_disable();
	if(dbg_lvl & dbg_lvl_req || dbg_lvl == FIQDBG_ERR)
	{
		snprintf(text, 9, "%4d%d:%d ", hfnum.b.frnum/8, hfnum.b.frnum%8, 8 - hfnum.b.frrem/937);
		va_start(args, fmt);
		vsnprintf(text+8, 9, fmt, args);
		va_end(args);

		memcpy(buffer + wptr, text, 16);
		wptr = (wptr + 16) % sizeof(buffer);
	}
	local_irq_restore(flags);
}
#endif

void notrace fiq_queue_request(int channel, int odd_frame)
{
	hcchar_data_t   hcchar   = { .d32 = FIQ_READ(dwc_regs_base + 0x500 + (channel * 0x20) + 0x0)  };
	hcsplt_data_t   hcsplt   = { .d32 = FIQ_READ(dwc_regs_base + 0x500 + (channel * 0x20) + 0x4)  };
	hctsiz_data_t   hctsiz   = { .d32 = FIQ_READ(dwc_regs_base + 0x500 + (channel * 0x20) + 0x10) };

	if(hcsplt.b.spltena	== 0)
	{
		fiq_print(FIQDBG_ERR, "SPLTENA ");
		BUG();
	}

	if(hcchar.b.epdir == 1)
	{
		fiq_print(FIQDBG_SCHED, "IN  Ch %d", channel);
	}
	else
	{
		hctsiz.b.xfersize = 0;
		fiq_print(FIQDBG_SCHED, "OUT Ch %d", channel);
	}
	FIQ_WRITE((dwc_regs_base + 0x500 + (channel * 0x20) + 0x10), hctsiz.d32);

	hcsplt.b.compsplt = 1;
	FIQ_WRITE((dwc_regs_base + 0x500 + (channel * 0x20) + 0x4), hcsplt.d32);

	// Send the Split complete
	hcchar.b.chen = 1;
	hcchar.b.oddfrm = odd_frame ? 1 : 0;

	// Post this for transmit on the next frame for periodic or this frame for non-periodic
	fiq_print(FIQDBG_SCHED, "SND_%s", odd_frame ? "ODD " : "EVEN");

	FIQ_WRITE((dwc_regs_base + 0x500 + (channel * 0x20) + 0x0), hcchar.d32);
}

static int last_sof = -1;

/*
** Function to handle the start of frame interrupt, choose whether we need to do anything and
** therefore trigger the main interrupt
**
** returns int != 0 - interrupt has been handled
*/
int diff;

int notrace fiq_sof_handle(hfnum_data_t hfnum)
{
	int handled = 0;
	int i;

	// Just check that once we're running we don't miss a SOF
	/*if(last_sof != -1 && (hfnum.b.frnum != ((last_sof + 1) & 0x3fff)))
	{
		fiq_print(FIQDBG_ERR, "LASTSOF ");
		fiq_print(FIQDBG_ERR, "%4d%d   ", last_sof / 8, last_sof & 7);
		fiq_print(FIQDBG_ERR, "%4d%d   ", hfnum.b.frnum / 8, hfnum.b.frnum & 7);
		BUG();
	}*/

	// Only start remembering the last sof when the interrupt has been
	// enabled (we don't check the mask to come in here...)
	if(last_sof != -1 || FIQ_READ(dwc_regs_base + 0x18) & (1<<3))
		last_sof = hfnum.b.frnum;

	for(i = 0; i < MAX_EPS_CHANNELS; i++)
	{
		if(complete_sched[i] != -1)
		{
			if(complete_sched[i] <= hfnum.b.frnum || (complete_sched[i] > 0x3f00 && hfnum.b.frnum < 0xf0))
			{
				fiq_queue_request(i, hfnum.b.frnum & 1);
				complete_sched[i] = -1;
			}
		}

		if(complete_sched[i] != -1)
		{
			// This is because we've seen a split complete occur with no start...
			// most likely because missed the complete 0x3fff frames ago!

			diff = (hfnum.b.frnum + 0x3fff - complete_sched[i]) & 0x3fff ;
			if(diff > 32 && diff < 0x3f00)
			{
				fiq_print(FIQDBG_ERR, "SPLTMISS");
				BUG();
			}
		}
	}

	if(g_np_count == g_np_sent && dwc_frame_num_gt(g_next_sched_frame, hfnum.b.frnum))
	{
		/*
		 * If np_count != np_sent that means we need to queue non-periodic (bulk) packets this packet
		 * g_next_sched_frame is the next frame we have periodic packets for
		 *
		 * if neither of these are required for this frame then just clear the interrupt
		 */
		handled = 1;

	}

	return handled;
}

int notrace port_id(hcsplt_data_t hcsplt)
{
	return hcsplt.b.prtaddr + (hcsplt.b.hubaddr << 8);
}

int notrace fiq_hcintr_handle(int channel, hfnum_data_t hfnum)
{
	hcchar_data_t   hcchar   = { .d32 = FIQ_READ(dwc_regs_base + 0x500 + (channel * 0x20) + 0x0) };
	hcsplt_data_t   hcsplt   = { .d32 = FIQ_READ(dwc_regs_base + 0x500 + (channel * 0x20) + 0x4) };
	hcint_data_t    hcint    = { .d32 = FIQ_READ(dwc_regs_base + 0x500 + (channel * 0x20) + 0x8) };
	hcintmsk_data_t hcintmsk = { .d32 = FIQ_READ(dwc_regs_base + 0x500 + (channel * 0x20) + 0xc) };
	hctsiz_data_t   hctsiz   = { .d32 = FIQ_READ(dwc_regs_base + 0x500 + (channel * 0x20) + 0x10)};

	hcint_saved[channel].d32 |= hcint.d32;
	hcintmsk_saved[channel].d32 =  hcintmsk.d32;

	if(hcsplt.b.spltena)
	{
		fiq_print(FIQDBG_PORTHUB, "ph: %4x", port_id(hcsplt));
		if(hcint.b.chhltd)
		{
			fiq_print(FIQDBG_SCHED, "CH HLT %d", channel);
			fiq_print(FIQDBG_SCHED, "%08x", hcint_saved[channel]);
		}
		if(hcint.b.stall || hcint.b.xacterr || hcint.b.bblerr || hcint.b.frmovrun || hcint.b.datatglerr)
		{
			queued_port[channel] = 0;
			fiq_print(FIQDBG_ERR, "CHAN ERR");
		}
		if(hcint.b.xfercomp)
		{
			// Clear the port allocation and transmit anything also on this port
			queued_port[channel] = 0;
			fiq_print(FIQDBG_SCHED, "XFERCOMP");
		}
		if(hcint.b.nak)
		{
			queued_port[channel] = 0;
			fiq_print(FIQDBG_SCHED, "NAK");
		}
		if(hcint.b.ack && !hcsplt.b.compsplt)
		{
			int i;

			// Do not complete isochronous out transactions
			if(hcchar.b.eptype == 1 && hcchar.b.epdir == 0)
			{
				queued_port[channel] = 0;
				fiq_print(FIQDBG_SCHED, "ISOC_OUT");
			}
			else
			{
				// Make sure we check the port / hub combination that we sent this split on.
				// Do not queue a second request to the same port
				for(i = 0; i < MAX_EPS_CHANNELS; i++)
				{
					if(port_id(hcsplt) == queued_port[i])
					{
						fiq_print(FIQDBG_ERR, "PORTERR ");
						//BUG();
					}
				}

				split_start_frame[channel] = (hfnum.b.frnum + 1) & ~7;

				// Note, the size of an OUT is in the start split phase, not
				// the complete split
				split_out_xfersize[channel] = hctsiz.b.xfersize;

				hcint_saved[channel].b.chhltd = 0;
				hcint_saved[channel].b.ack = 0;

				queued_port[channel] = port_id(hcsplt);

				if(hcchar.b.eptype & 1)
				{
					// Send the periodic complete in the same oddness frame as the ACK went...
					fiq_queue_request(channel, !(hfnum.b.frnum & 1));
	//				complete_sched[channel] = dwc_frame_num_inc(hfnum.b.frnum, 1);
				}
				else
				{
					// Schedule the split complete to occur later
					complete_sched[channel] = dwc_frame_num_inc(hfnum.b.frnum, 2);
					fiq_print(FIQDBG_SCHED, "ACK%04d%d", complete_sched[channel]/8, complete_sched[channel]%8);
				}
			}
		}
		if(hcint.b.nyet)
		{
			fiq_print(FIQDBG_ERR, "NYETERR1");
			//BUG();
			// Can transmit a split complete up to uframe .0 of the next frame
			if(hfnum.b.frnum <= dwc_frame_num_inc(split_start_frame[channel], 8))
			{
				// Send it next frame
				if(hcchar.b.eptype & 1) // type 1 & 3 are interrupt & isoc
				{
					fiq_print(FIQDBG_SCHED, "NYT:SEND");
					fiq_queue_request(channel, !(hfnum.b.frnum & 1));
				}
				else
				{
					// Schedule non-periodic access for next frame (the odd-even bit doesn't effect NP)
					complete_sched[channel] = dwc_frame_num_inc(hfnum.b.frnum, 1);
					fiq_print(FIQDBG_SCHED, "NYT%04d%d", complete_sched[channel]/8, complete_sched[channel]%8);
				}
				hcint_saved[channel].b.chhltd = 0;
				hcint_saved[channel].b.nyet = 0;
			}
			else
			{
				queued_port[channel] = 0;
				fiq_print(FIQDBG_ERR, "NYETERR2");
				//BUG();
			}
		}
	}
	else
	{
		/*
		 * If we have any of NAK, ACK, Datatlgerr active on a
		 * non-split channel, the sole reason is to reset error
		 * counts for a previously broken transaction. The FIQ
		 * will thrash on NAK IN and ACK OUT in particular so
		 * handle it "once" and allow the IRQ to do the rest.
		 */
		hcint.d32 &= hcintmsk.d32;
		if(hcint.b.nak)
		{
			hcintmsk.b.nak = 0;
			FIQ_WRITE((dwc_regs_base + 0x500 + (channel * 0x20) + 0xc), hcintmsk.d32);
		}
		if (hcint.b.ack)
		{
			hcintmsk.b.ack = 0;
			FIQ_WRITE((dwc_regs_base + 0x500 + (channel * 0x20) + 0xc), hcintmsk.d32);
		}
	}

	// Clear the interrupt, this will also clear the HAINT bit
	FIQ_WRITE((dwc_regs_base + 0x500 + (channel * 0x20) + 0x8), hcint.d32);
	return hcint_saved[channel].d32 == 0;
}

gintsts_data_t gintsts;
gintmsk_data_t gintmsk;
// triggered: The set of interrupts that were triggered
// handled:   The set of interrupts that have been handled (no IRQ is
//            required)
// keep:      The set of interrupts we want to keep unmasked even though we
//            want to trigger an IRQ to handle it (SOF and HCINTR)
gintsts_data_t triggered, handled, keep;
hfnum_data_t hfnum;

void __attribute__ ((naked)) notrace dwc_otg_hcd_handle_fiq(void)
{

	/* entry takes care to store registers we will be treading on here */
	asm __volatile__ (
		"mov     ip, sp ;"
		/* stash FIQ and normal regs */
		"stmdb	sp!, {r0-r12,  lr};"
		/* !! THIS SETS THE FRAME, adjust to > sizeof locals */
		"sub     fp, ip, #512 ;"
		);

	// Cannot put local variables at the beginning of the function
	// because otherwise 'C' will play with the stack pointer. any locals
	// need to be inside the following block
	do
	{
		fiq_done++;
		gintsts.d32 = FIQ_READ(dwc_regs_base + 0x14);
		gintmsk.d32 = FIQ_READ(dwc_regs_base + 0x18);
		hfnum.d32 =   FIQ_READ(dwc_regs_base + 0x408);
		triggered.d32 = gintsts.d32 & gintmsk.d32;
		handled.d32 = 0;
		keep.d32 = 0;
		fiq_print(FIQDBG_INT, "FIQ     ");
		fiq_print(FIQDBG_INT, "%08x", gintsts.d32);
		fiq_print(FIQDBG_INT, "%08x", gintmsk.d32);
		if(gintsts.d32)
		{
			// If port enabled
			if((FIQ_READ(dwc_regs_base + 0x440) & 0xf) == 0x5)
			{
				if(gintsts.b.sofintr)
				{
					if(fiq_sof_handle(hfnum))
					{
						handled.b.sofintr = 1; /* Handled in FIQ */
					}
					else
					{
						/* Keer interrupt unmasked */
						keep.b.sofintr = 1;
					}
					{
						// Need to make sure the read and clearing of the SOF interrupt is as close as possible to avoid the possibility of missing
						// a start of frame interrupt
						gintsts_data_t gintsts = { .b.sofintr = 1 };
						FIQ_WRITE((dwc_regs_base + 0x14), gintsts.d32);
					}
				}

				if(fiq_split_enable && gintsts.b.hcintr)
				{
					int i;
					haint_data_t    haint;
					haintmsk_data_t haintmsk;

					haint.d32 = FIQ_READ(dwc_regs_base + 0x414);
					haintmsk.d32 = FIQ_READ(dwc_regs_base + 0x418);
					haint.d32 &= haintmsk.d32;
					haint_saved.d32 |= haint.d32;

					fiq_print(FIQDBG_INT, "hcintr");
					fiq_print(FIQDBG_INT, "%08x", FIQ_READ(dwc_regs_base + 0x414));

					// Go through each channel that has an enabled interrupt
					for(i = 0; i < 16; i++)
						if((haint.d32 >> i) & 1)
							if(fiq_hcintr_handle(i, hfnum))
								haint_saved.d32 &= ~(1 << i); /* this was handled */

					/* If we've handled all host channel interrupts then don't trigger the interrupt */
					if(haint_saved.d32 == 0)
					{
						handled.b.hcintr = 1;
					}
					else
					{
						/* Make sure we keep the channel interrupt unmasked when triggering the IRQ */
						keep.b.hcintr = 1;
					}

					{
						gintsts_data_t gintsts = { .b.hcintr = 1 };

						// Always clear the channel interrupt
						FIQ_WRITE((dwc_regs_base + 0x14), gintsts.d32);
					}
				}
			}
			else
			{
				last_sof = -1;
			}
		}

		// Mask out the interrupts triggered - those handled - don't mask out the ones we want to keep
		gintmsk.d32 = keep.d32 | (gintmsk.d32 & ~(triggered.d32 & ~handled.d32));
		// Save those that were triggered but not handled
		gintsts_saved.d32 |= triggered.d32 & ~handled.d32;
		FIQ_WRITE(dwc_regs_base + 0x18, gintmsk.d32);

		// Clear and save any unhandled interrupts and trigger the interrupt
		if(gintsts_saved.d32)
		{
			/* To enable the MPHI interrupt  (INT 32)
			 */
			FIQ_WRITE( c_mphi_regs.outdda, (int) dummy_send);
			FIQ_WRITE( c_mphi_regs.outddb, (1 << 29));

			mphi_int_count++;
		}
	}
	while(0);

	mb();

	/* exit back to normal mode restoring everything */
	asm __volatile__ (
		/* return FIQ regs back to pristine state
		 * and get normal regs back
		 */
		"ldmia	sp!, {r0-r12, lr};"

		/* return */
		"subs	pc, lr, #4;"
	);
}

/** This function handles interrupts for the HCD. */
int32_t dwc_otg_hcd_handle_intr(dwc_otg_hcd_t * dwc_otg_hcd)
{
	int retval = 0;
	static int last_time;

	dwc_otg_core_if_t *core_if = dwc_otg_hcd->core_if;
	gintsts_data_t gintsts;
	gintmsk_data_t gintmsk;
	hfnum_data_t hfnum;

#ifdef DEBUG
	dwc_otg_core_global_regs_t *global_regs = core_if->core_global_regs;

#endif

	gintsts.d32 = DWC_READ_REG32(&core_if->core_global_regs->gintsts);
	gintmsk.d32 = DWC_READ_REG32(&core_if->core_global_regs->gintmsk);

	/* Exit from ISR if core is hibernated */
	if (core_if->hibernation_suspend == 1) {
		goto exit_handler_routine;
	}

	if (core_if->op_state == A_PERIPHERAL) {
		return retval;
	}

//	DWC_SPINLOCK(dwc_otg_hcd->lock);
	/* Check if HOST Mode */
	if (dwc_otg_is_host_mode(core_if)) {
	DWC_SPINLOCK(dwc_otg_hcd->lock);
		local_fiq_disable();
		gintmsk.d32 |= gintsts_saved.d32;
		gintsts.d32 |= gintsts_saved.d32;
		gintsts_saved.d32 = 0;
		local_fiq_enable();
		if (!gintsts.d32) {
			goto exit_handler_routine;
		}
		gintsts.d32 &= gintmsk.d32;

#ifdef DEBUG
		// We should be OK doing this because the common interrupts should already have been serviced
		/* Don't print debug message in the interrupt handler on SOF */
#ifndef DEBUG_SOF
		if (gintsts.d32 != DWC_SOF_INTR_MASK)
#endif
			DWC_DEBUGPL(DBG_HCDI, "\n");
#endif

#ifdef DEBUG
#ifndef DEBUG_SOF
		if (gintsts.d32 != DWC_SOF_INTR_MASK)
#endif
			DWC_DEBUGPL(DBG_HCDI,
				    "DWC OTG HCD Interrupt Detected gintsts&gintmsk=0x%08x core_if=%p\n",
				    gintsts.d32, core_if);
#endif
		hfnum.d32 = DWC_READ_REG32(&dwc_otg_hcd->core_if->host_if->host_global_regs->hfnum);
		if (gintsts.b.sofintr && g_np_count == g_np_sent && dwc_frame_num_gt(g_next_sched_frame, hfnum.b.frnum))
		{
			/* Note, we should never get here if the FIQ is doing it's job properly*/
			retval |= dwc_otg_hcd_handle_sof_intr(dwc_otg_hcd);
		}
		else if (gintsts.b.sofintr) {
			retval |= dwc_otg_hcd_handle_sof_intr(dwc_otg_hcd);
		}

		if (gintsts.b.rxstsqlvl) {
			retval |=
			    dwc_otg_hcd_handle_rx_status_q_level_intr
			    (dwc_otg_hcd);
		}
		if (gintsts.b.nptxfempty) {
			retval |=
			    dwc_otg_hcd_handle_np_tx_fifo_empty_intr
			    (dwc_otg_hcd);
		}
		if (gintsts.b.i2cintr) {
			/** @todo Implement i2cintr handler. */
		}
		if (gintsts.b.portintr) {

			gintmsk_data_t gintmsk = { .b.portintr = 1};
			retval |= dwc_otg_hcd_handle_port_intr(dwc_otg_hcd);
			DWC_MODIFY_REG32(&core_if->core_global_regs->gintmsk, 0, gintmsk.d32);
		}
		if (gintsts.b.hcintr) {
			retval |= dwc_otg_hcd_handle_hc_intr(dwc_otg_hcd);
		}
		if (gintsts.b.ptxfempty) {
			retval |=
			    dwc_otg_hcd_handle_perio_tx_fifo_empty_intr
			    (dwc_otg_hcd);
		}
#ifdef DEBUG
#ifndef DEBUG_SOF
		if (gintsts.d32 != DWC_SOF_INTR_MASK)
#endif
		{
			DWC_DEBUGPL(DBG_HCDI,
				    "DWC OTG HCD Finished Servicing Interrupts\n");
			DWC_DEBUGPL(DBG_HCDV, "DWC OTG HCD gintsts=0x%08x\n",
				    DWC_READ_REG32(&global_regs->gintsts));
			DWC_DEBUGPL(DBG_HCDV, "DWC OTG HCD gintmsk=0x%08x\n",
				    DWC_READ_REG32(&global_regs->gintmsk));
		}
#endif

#ifdef DEBUG
#ifndef DEBUG_SOF
		if (gintsts.d32 != DWC_SOF_INTR_MASK)
#endif
			DWC_DEBUGPL(DBG_HCDI, "\n");
#endif
	DWC_SPINUNLOCK(dwc_otg_hcd->lock);
	}

exit_handler_routine:

	if (fiq_fix_enable)
	{
		local_fiq_disable();
		// Make sure that we don't clear the interrupt if we've still got pending work to do
		if(gintsts_saved.d32 == 0)
		{
			/* Clear the MPHI interrupt */
			DWC_WRITE_REG32(c_mphi_regs.intstat, (1<<16));
			if (mphi_int_count >= 60)
			{
				DWC_WRITE_REG32(c_mphi_regs.ctrl, ((1<<31) + (1<<16)));
				while(!(DWC_READ_REG32(c_mphi_regs.ctrl) & (1 << 17)))
					;
				DWC_WRITE_REG32(c_mphi_regs.ctrl, (1<<31));
				mphi_int_count = 0;
			}
			int_done++;
		}

		// Unmask handled interrupts
		FIQ_WRITE(dwc_regs_base + 0x18, gintmsk.d32);
		//DWC_MODIFY_REG32((uint32_t *)IO_ADDRESS(USB_BASE + 0x8), 0 , 1);

		local_fiq_enable();

		if((jiffies / HZ) > last_time)
		{
			/* Once a second output the fiq and irq numbers, useful for debug */
			last_time = jiffies / HZ;
			DWC_DEBUGPL(DBG_USER, "int_done = %d fiq_done = %d\n", int_done, fiq_done);
		}
	}

//	DWC_SPINUNLOCK(dwc_otg_hcd->lock);
	return retval;
}

#ifdef DWC_TRACK_MISSED_SOFS

#warning Compiling code to track missed SOFs
#define FRAME_NUM_ARRAY_SIZE 1000
/**
 * This function is for debug only.
 */
static inline void track_missed_sofs(uint16_t curr_frame_number)
{
	static uint16_t frame_num_array[FRAME_NUM_ARRAY_SIZE];
	static uint16_t last_frame_num_array[FRAME_NUM_ARRAY_SIZE];
	static int frame_num_idx = 0;
	static uint16_t last_frame_num = DWC_HFNUM_MAX_FRNUM;
	static int dumped_frame_num_array = 0;

	if (frame_num_idx < FRAME_NUM_ARRAY_SIZE) {
		if (((last_frame_num + 1) & DWC_HFNUM_MAX_FRNUM) !=
		    curr_frame_number) {
			frame_num_array[frame_num_idx] = curr_frame_number;
			last_frame_num_array[frame_num_idx++] = last_frame_num;
		}
	} else if (!dumped_frame_num_array) {
		int i;
		DWC_PRINTF("Frame     Last Frame\n");
		DWC_PRINTF("-----     ----------\n");
		for (i = 0; i < FRAME_NUM_ARRAY_SIZE; i++) {
			DWC_PRINTF("0x%04x    0x%04x\n",
				   frame_num_array[i], last_frame_num_array[i]);
		}
		dumped_frame_num_array = 1;
	}
	last_frame_num = curr_frame_number;
}
#endif

/**
 * Handles the start-of-frame interrupt in host mode. Non-periodic
 * transactions may be queued to the DWC_otg controller for the current
 * (micro)frame. Periodic transactions may be queued to the controller for the
 * next (micro)frame.
 */
int32_t dwc_otg_hcd_handle_sof_intr(dwc_otg_hcd_t * hcd)
{
	hfnum_data_t hfnum;
	dwc_list_link_t *qh_entry;
	dwc_otg_qh_t *qh;
	dwc_otg_transaction_type_e tr_type;
	int did_something = 0;
	int32_t next_sched_frame = -1;

	hfnum.d32 =
	    DWC_READ_REG32(&hcd->core_if->host_if->host_global_regs->hfnum);

#ifdef DEBUG_SOF
	DWC_DEBUGPL(DBG_HCD, "--Start of Frame Interrupt--\n");
#endif
	hcd->frame_number = hfnum.b.frnum;

#ifdef DEBUG
	hcd->frrem_accum += hfnum.b.frrem;
	hcd->frrem_samples++;
#endif

#ifdef DWC_TRACK_MISSED_SOFS
	track_missed_sofs(hcd->frame_number);
#endif
	/* Determine whether any periodic QHs should be executed. */
	qh_entry = DWC_LIST_FIRST(&hcd->periodic_sched_inactive);
	while (qh_entry != &hcd->periodic_sched_inactive) {
		qh = DWC_LIST_ENTRY(qh_entry, dwc_otg_qh_t, qh_list_entry);
		qh_entry = qh_entry->next;
		if (dwc_frame_num_le(qh->sched_frame, hcd->frame_number)) {

			/*
			 * Move QH to the ready list to be executed next
			 * (micro)frame.
			 */
			DWC_LIST_MOVE_HEAD(&hcd->periodic_sched_ready,
					   &qh->qh_list_entry);

			did_something = 1;
		}
		else
		{
			if(next_sched_frame < 0 || dwc_frame_num_le(qh->sched_frame, next_sched_frame))
			{
				next_sched_frame = qh->sched_frame;
			}
		}
	}

	g_next_sched_frame = next_sched_frame;

	tr_type = dwc_otg_hcd_select_transactions(hcd);
	if (tr_type != DWC_OTG_TRANSACTION_NONE) {
		dwc_otg_hcd_queue_transactions(hcd, tr_type);
		did_something = 1;
	}

	/* Clear interrupt */
	gintsts.b.sofintr = 1;
	DWC_WRITE_REG32(&hcd->core_if->core_global_regs->gintsts, gintsts.d32);

	return 1;
}

/** Handles the Rx Status Queue Level Interrupt, which indicates that there is at
 * least one packet in the Rx FIFO.  The packets are moved from the FIFO to
 * memory if the DWC_otg controller is operating in Slave mode. */
int32_t dwc_otg_hcd_handle_rx_status_q_level_intr(dwc_otg_hcd_t * dwc_otg_hcd)
{
	host_grxsts_data_t grxsts;
	dwc_hc_t *hc = NULL;

	DWC_DEBUGPL(DBG_HCD, "--RxStsQ Level Interrupt--\n");

	grxsts.d32 =
	    DWC_READ_REG32(&dwc_otg_hcd->core_if->core_global_regs->grxstsp);

	hc = dwc_otg_hcd->hc_ptr_array[grxsts.b.chnum];
	if (!hc) {
		DWC_ERROR("Unable to get corresponding channel\n");
		return 0;
	}

	/* Packet Status */
	DWC_DEBUGPL(DBG_HCDV, "    Ch num = %d\n", grxsts.b.chnum);
	DWC_DEBUGPL(DBG_HCDV, "    Count = %d\n", grxsts.b.bcnt);
	DWC_DEBUGPL(DBG_HCDV, "    DPID = %d, hc.dpid = %d\n", grxsts.b.dpid,
		    hc->data_pid_start);
	DWC_DEBUGPL(DBG_HCDV, "    PStatus = %d\n", grxsts.b.pktsts);

	switch (grxsts.b.pktsts) {
	case DWC_GRXSTS_PKTSTS_IN:
		/* Read the data into the host buffer. */
		if (grxsts.b.bcnt > 0) {
			dwc_otg_read_packet(dwc_otg_hcd->core_if,
					    hc->xfer_buff, grxsts.b.bcnt);

			/* Update the HC fields for the next packet received. */
			hc->xfer_count += grxsts.b.bcnt;
			hc->xfer_buff += grxsts.b.bcnt;
		}

	case DWC_GRXSTS_PKTSTS_IN_XFER_COMP:
	case DWC_GRXSTS_PKTSTS_DATA_TOGGLE_ERR:
	case DWC_GRXSTS_PKTSTS_CH_HALTED:
		/* Handled in interrupt, just ignore data */
		break;
	default:
		DWC_ERROR("RX_STS_Q Interrupt: Unknown status %d\n",
			  grxsts.b.pktsts);
		break;
	}

	return 1;
}

/** This interrupt occurs when the non-periodic Tx FIFO is half-empty. More
 * data packets may be written to the FIFO for OUT transfers. More requests
 * may be written to the non-periodic request queue for IN transfers. This
 * interrupt is enabled only in Slave mode. */
int32_t dwc_otg_hcd_handle_np_tx_fifo_empty_intr(dwc_otg_hcd_t * dwc_otg_hcd)
{
	DWC_DEBUGPL(DBG_HCD, "--Non-Periodic TxFIFO Empty Interrupt--\n");
	dwc_otg_hcd_queue_transactions(dwc_otg_hcd,
				       DWC_OTG_TRANSACTION_NON_PERIODIC);
	return 1;
}

/** This interrupt occurs when the periodic Tx FIFO is half-empty. More data
 * packets may be written to the FIFO for OUT transfers. More requests may be
 * written to the periodic request queue for IN transfers. This interrupt is
 * enabled only in Slave mode. */
int32_t dwc_otg_hcd_handle_perio_tx_fifo_empty_intr(dwc_otg_hcd_t * dwc_otg_hcd)
{
	DWC_DEBUGPL(DBG_HCD, "--Periodic TxFIFO Empty Interrupt--\n");
	dwc_otg_hcd_queue_transactions(dwc_otg_hcd,
				       DWC_OTG_TRANSACTION_PERIODIC);
	return 1;
}

/** There are multiple conditions that can cause a port interrupt. This function
 * determines which interrupt conditions have occurred and handles them
 * appropriately. */
int32_t dwc_otg_hcd_handle_port_intr(dwc_otg_hcd_t * dwc_otg_hcd)
{
	int retval = 0;
	hprt0_data_t hprt0;
	hprt0_data_t hprt0_modify;

	hprt0.d32 = DWC_READ_REG32(dwc_otg_hcd->core_if->host_if->hprt0);
	hprt0_modify.d32 = DWC_READ_REG32(dwc_otg_hcd->core_if->host_if->hprt0);

	/* Clear appropriate bits in HPRT0 to clear the interrupt bit in
	 * GINTSTS */

	hprt0_modify.b.prtena = 0;
	hprt0_modify.b.prtconndet = 0;
	hprt0_modify.b.prtenchng = 0;
	hprt0_modify.b.prtovrcurrchng = 0;

	/* Port Connect Detected
	 * Set flag and clear if detected */
	if (dwc_otg_hcd->core_if->hibernation_suspend == 1) {
		// Dont modify port status if we are in hibernation state
		hprt0_modify.b.prtconndet = 1;
		hprt0_modify.b.prtenchng = 1;
		DWC_WRITE_REG32(dwc_otg_hcd->core_if->host_if->hprt0, hprt0_modify.d32);
		hprt0.d32 = DWC_READ_REG32(dwc_otg_hcd->core_if->host_if->hprt0);
		return retval;
	}

	if (hprt0.b.prtconndet) {
		/** @todo - check if steps performed in 'else' block should be perfromed regardles adp */
		if (dwc_otg_hcd->core_if->adp_enable && 	
				dwc_otg_hcd->core_if->adp.vbuson_timer_started == 1) {
			DWC_PRINTF("PORT CONNECT DETECTED ----------------\n");
			DWC_TIMER_CANCEL(dwc_otg_hcd->core_if->adp.vbuson_timer);
			dwc_otg_hcd->core_if->adp.vbuson_timer_started = 0;
			/* TODO - check if this is required, as
			 * host initialization was already performed
			 * after initial ADP probing
			 */
			/*dwc_otg_hcd->core_if->adp.vbuson_timer_started = 0;
			dwc_otg_core_init(dwc_otg_hcd->core_if);
			dwc_otg_enable_global_interrupts(dwc_otg_hcd->core_if);
			cil_hcd_start(dwc_otg_hcd->core_if);*/
		} else {
		
			DWC_DEBUGPL(DBG_HCD, "--Port Interrupt HPRT0=0x%08x "
				    "Port Connect Detected--\n", hprt0.d32);
			dwc_otg_hcd->flags.b.port_connect_status_change = 1;
			dwc_otg_hcd->flags.b.port_connect_status = 1;
			hprt0_modify.b.prtconndet = 1;
	
			/* B-Device has connected, Delete the connection timer. */
			DWC_TIMER_CANCEL(dwc_otg_hcd->conn_timer);
		}
		/* The Hub driver asserts a reset when it sees port connect
		 * status change flag */
		retval |= 1;
	}

	/* Port Enable Changed
	 * Clear if detected - Set internal flag if disabled */
	if (hprt0.b.prtenchng) {
		DWC_DEBUGPL(DBG_HCD, "  --Port Interrupt HPRT0=0x%08x "
			    "Port Enable Changed--\n", hprt0.d32);
		hprt0_modify.b.prtenchng = 1;
		if (hprt0.b.prtena == 1) {
			hfir_data_t hfir;
			int do_reset = 0;
			dwc_otg_core_params_t *params =
			    dwc_otg_hcd->core_if->core_params;
			dwc_otg_core_global_regs_t *global_regs =
			    dwc_otg_hcd->core_if->core_global_regs;
			dwc_otg_host_if_t *host_if =
			    dwc_otg_hcd->core_if->host_if;
			    
			/* Every time when port enables calculate
			 * HFIR.FrInterval
			 */
			hfir.d32 = DWC_READ_REG32(&host_if->host_global_regs->hfir);
			hfir.b.frint = calc_frame_interval(dwc_otg_hcd->core_if);
			DWC_WRITE_REG32(&host_if->host_global_regs->hfir, hfir.d32);

			/* Check if we need to adjust the PHY clock speed for
			 * low power and adjust it */
			if (params->host_support_fs_ls_low_power) {
				gusbcfg_data_t usbcfg;

				usbcfg.d32 =
				    DWC_READ_REG32(&global_regs->gusbcfg);

				if (hprt0.b.prtspd == DWC_HPRT0_PRTSPD_LOW_SPEED
				    || hprt0.b.prtspd ==
				    DWC_HPRT0_PRTSPD_FULL_SPEED) {
					/*
					 * Low power
					 */
					hcfg_data_t hcfg;
					if (usbcfg.b.phylpwrclksel == 0) {
						/* Set PHY low power clock select for FS/LS devices */
						usbcfg.b.phylpwrclksel = 1;
						DWC_WRITE_REG32
						    (&global_regs->gusbcfg,
						     usbcfg.d32);
						do_reset = 1;
					}

					hcfg.d32 =
					    DWC_READ_REG32
					    (&host_if->host_global_regs->hcfg);

					if (hprt0.b.prtspd ==
					    DWC_HPRT0_PRTSPD_LOW_SPEED
					    && params->host_ls_low_power_phy_clk
					    ==
					    DWC_HOST_LS_LOW_POWER_PHY_CLK_PARAM_6MHZ)
					{
						/* 6 MHZ */
						DWC_DEBUGPL(DBG_CIL,
							    "FS_PHY programming HCFG to 6 MHz (Low Power)\n");
						if (hcfg.b.fslspclksel !=
						    DWC_HCFG_6_MHZ) {
							hcfg.b.fslspclksel =
							    DWC_HCFG_6_MHZ;
							DWC_WRITE_REG32
							    (&host_if->host_global_regs->hcfg,
							     hcfg.d32);
							do_reset = 1;
						}
					} else {
						/* 48 MHZ */
						DWC_DEBUGPL(DBG_CIL,
							    "FS_PHY programming HCFG to 48 MHz ()\n");
						if (hcfg.b.fslspclksel !=
						    DWC_HCFG_48_MHZ) {
							hcfg.b.fslspclksel =
							    DWC_HCFG_48_MHZ;
							DWC_WRITE_REG32
							    (&host_if->host_global_regs->hcfg,
							     hcfg.d32);
							do_reset = 1;
						}
					}
				} else {
					/*
					 * Not low power
					 */
					if (usbcfg.b.phylpwrclksel == 1) {
						usbcfg.b.phylpwrclksel = 0;
						DWC_WRITE_REG32
						    (&global_regs->gusbcfg,
						     usbcfg.d32);
						do_reset = 1;
					}
				}

				if (do_reset) {
					DWC_TASK_SCHEDULE(dwc_otg_hcd->reset_tasklet);
				}
			}

			if (!do_reset) {
				/* Port has been enabled set the reset change flag */
				dwc_otg_hcd->flags.b.port_reset_change = 1;
			}
		} else {
			dwc_otg_hcd->flags.b.port_enable_change = 1;
		}
		retval |= 1;
	}

	/** Overcurrent Change Interrupt */
	if (hprt0.b.prtovrcurrchng) {
		DWC_DEBUGPL(DBG_HCD, "  --Port Interrupt HPRT0=0x%08x "
			    "Port Overcurrent Changed--\n", hprt0.d32);
		dwc_otg_hcd->flags.b.port_over_current_change = 1;
		hprt0_modify.b.prtovrcurrchng = 1;
		retval |= 1;
	}

	/* Clear Port Interrupts */
	DWC_WRITE_REG32(dwc_otg_hcd->core_if->host_if->hprt0, hprt0_modify.d32);

	return retval;
}

/** This interrupt indicates that one or more host channels has a pending
 * interrupt. There are multiple conditions that can cause each host channel
 * interrupt. This function determines which conditions have occurred for each
 * host channel interrupt and handles them appropriately. */
int32_t dwc_otg_hcd_handle_hc_intr(dwc_otg_hcd_t * dwc_otg_hcd)
{
	int i;
	int retval = 0;
	haint_data_t haint;

	/* Clear appropriate bits in HCINTn to clear the interrupt bit in
	 * GINTSTS */

	haint.d32 = dwc_otg_read_host_all_channels_intr(dwc_otg_hcd->core_if);

	// Overwrite with saved interrupts from fiq handler
	if(fiq_split_enable)
	{
		local_fiq_disable();
		haint.d32 = haint_saved.d32;
		haint_saved.d32 = 0;
		local_fiq_enable();
	}

	for (i = 0; i < dwc_otg_hcd->core_if->core_params->host_channels; i++) {
		if (haint.b2.chint & (1 << i)) {
			retval |= dwc_otg_hcd_handle_hc_n_intr(dwc_otg_hcd, i);
		}
	}

	return retval;
}

/**
 * Gets the actual length of a transfer after the transfer halts. _halt_status
 * holds the reason for the halt.
 *
 * For IN transfers where halt_status is DWC_OTG_HC_XFER_COMPLETE,
 * *short_read is set to 1 upon return if less than the requested
 * number of bytes were transferred. Otherwise, *short_read is set to 0 upon
 * return. short_read may also be NULL on entry, in which case it remains
 * unchanged.
 */
static uint32_t get_actual_xfer_length(dwc_hc_t * hc,
				       dwc_otg_hc_regs_t * hc_regs,
				       dwc_otg_qtd_t * qtd,
				       dwc_otg_halt_status_e halt_status,
				       int *short_read)
{
	hctsiz_data_t hctsiz;
	uint32_t length;

	if (short_read != NULL) {
		*short_read = 0;
	}
	hctsiz.d32 = DWC_READ_REG32(&hc_regs->hctsiz);

	if (halt_status == DWC_OTG_HC_XFER_COMPLETE) {
		if (hc->ep_is_in) {
			length = hc->xfer_len - hctsiz.b.xfersize;
			if (short_read != NULL) {
				*short_read = (hctsiz.b.xfersize != 0);
			}
		} else if (hc->qh->do_split) {
			if(fiq_split_enable)
				length = split_out_xfersize[hc->hc_num];
			else
				length = qtd->ssplit_out_xfer_count;
		} else {
			length = hc->xfer_len;
		}
	} else {
		/*
		 * Must use the hctsiz.pktcnt field to determine how much data
		 * has been transferred. This field reflects the number of
		 * packets that have been transferred via the USB. This is
		 * always an integral number of packets if the transfer was
		 * halted before its normal completion. (Can't use the
		 * hctsiz.xfersize field because that reflects the number of
		 * bytes transferred via the AHB, not the USB).
		 */
		length =
		    (hc->start_pkt_count - hctsiz.b.pktcnt) * hc->max_packet;
	}

	return length;
}

/**
 * Updates the state of the URB after a Transfer Complete interrupt on the
 * host channel. Updates the actual_length field of the URB based on the
 * number of bytes transferred via the host channel. Sets the URB status
 * if the data transfer is finished.
 *
 * @return 1 if the data transfer specified by the URB is completely finished,
 * 0 otherwise.
 */
static int update_urb_state_xfer_comp(dwc_hc_t * hc,
				      dwc_otg_hc_regs_t * hc_regs,
				      dwc_otg_hcd_urb_t * urb,
				      dwc_otg_qtd_t * qtd)
{
	int xfer_done = 0;
	int short_read = 0;

	int xfer_length;

	xfer_length = get_actual_xfer_length(hc, hc_regs, qtd,
					     DWC_OTG_HC_XFER_COMPLETE,
					     &short_read);

	/* non DWORD-aligned buffer case handling. */
	if (hc->align_buff && xfer_length && hc->ep_is_in) {
		dwc_memcpy(urb->buf + urb->actual_length, hc->qh->dw_align_buf,
			   xfer_length);
	}

	urb->actual_length += xfer_length;

	if (xfer_length && (hc->ep_type == DWC_OTG_EP_TYPE_BULK) &&
	    (urb->flags & URB_SEND_ZERO_PACKET)
	    && (urb->actual_length == urb->length)
	    && !(urb->length % hc->max_packet)) {
		xfer_done = 0;
	} else if (short_read || urb->actual_length >= urb->length) {
		xfer_done = 1;
		urb->status = 0;
	}
	
#ifdef DEBUG
	{
		hctsiz_data_t hctsiz;
		hctsiz.d32 = DWC_READ_REG32(&hc_regs->hctsiz);
		DWC_DEBUGPL(DBG_HCDV, "DWC_otg: %s: %s, channel %d\n",
			    __func__, (hc->ep_is_in ? "IN" : "OUT"),
			    hc->hc_num);
		DWC_DEBUGPL(DBG_HCDV, "  hc->xfer_len %d\n", hc->xfer_len);
		DWC_DEBUGPL(DBG_HCDV, "  hctsiz.xfersize %d\n",
			    hctsiz.b.xfersize);
		DWC_DEBUGPL(DBG_HCDV, "  urb->transfer_buffer_length %d\n",
			    urb->length);
		DWC_DEBUGPL(DBG_HCDV, "  urb->actual_length %d\n",
			    urb->actual_length);
		DWC_DEBUGPL(DBG_HCDV, "  short_read %d, xfer_done %d\n",
			    short_read, xfer_done);
	}
#endif

	return xfer_done;
}

/*
 * Save the starting data toggle for the next transfer. The data toggle is
 * saved in the QH for non-control transfers and it's saved in the QTD for
 * control transfers.
 */
void dwc_otg_hcd_save_data_toggle(dwc_hc_t * hc,
			     dwc_otg_hc_regs_t * hc_regs, dwc_otg_qtd_t * qtd)
{
	hctsiz_data_t hctsiz;
	hctsiz.d32 = DWC_READ_REG32(&hc_regs->hctsiz);

	if (hc->ep_type != DWC_OTG_EP_TYPE_CONTROL) {
		dwc_otg_qh_t *qh = hc->qh;
		if (hctsiz.b.pid == DWC_HCTSIZ_DATA0) {
			qh->data_toggle = DWC_OTG_HC_PID_DATA0;
		} else {
			qh->data_toggle = DWC_OTG_HC_PID_DATA1;
		}
	} else {
		if (hctsiz.b.pid == DWC_HCTSIZ_DATA0) {
			qtd->data_toggle = DWC_OTG_HC_PID_DATA0;
		} else {
			qtd->data_toggle = DWC_OTG_HC_PID_DATA1;
		}
	}
}

/**
 * Updates the state of an Isochronous URB when the transfer is stopped for
 * any reason. The fields of the current entry in the frame descriptor array
 * are set based on the transfer state and the input _halt_status. Completes
 * the Isochronous URB if all the URB frames have been completed.
 *
 * @return DWC_OTG_HC_XFER_COMPLETE if there are more frames remaining to be
 * transferred in the URB. Otherwise return DWC_OTG_HC_XFER_URB_COMPLETE.
 */
static dwc_otg_halt_status_e
update_isoc_urb_state(dwc_otg_hcd_t * hcd,
		      dwc_hc_t * hc,
		      dwc_otg_hc_regs_t * hc_regs,
		      dwc_otg_qtd_t * qtd, dwc_otg_halt_status_e halt_status)
{
	dwc_otg_hcd_urb_t *urb = qtd->urb;
	dwc_otg_halt_status_e ret_val = halt_status;
	struct dwc_otg_hcd_iso_packet_desc *frame_desc;

	frame_desc = &urb->iso_descs[qtd->isoc_frame_index];
	switch (halt_status) {
	case DWC_OTG_HC_XFER_COMPLETE:
		frame_desc->status = 0;
		frame_desc->actual_length =
		    get_actual_xfer_length(hc, hc_regs, qtd, halt_status, NULL);

		/* non DWORD-aligned buffer case handling. */
		if (hc->align_buff && frame_desc->actual_length && hc->ep_is_in) {
			dwc_memcpy(urb->buf + frame_desc->offset + qtd->isoc_split_offset,
				   hc->qh->dw_align_buf, frame_desc->actual_length);
		}
		
		break;
	case DWC_OTG_HC_XFER_FRAME_OVERRUN:
		urb->error_count++;
		if (hc->ep_is_in) {
			frame_desc->status = -DWC_E_NO_STREAM_RES;
		} else {
			frame_desc->status = -DWC_E_COMMUNICATION;
		}
		frame_desc->actual_length = 0;
		break;
	case DWC_OTG_HC_XFER_BABBLE_ERR:
		urb->error_count++;
		frame_desc->status = -DWC_E_OVERFLOW;
		/* Don't need to update actual_length in this case. */
		break;
	case DWC_OTG_HC_XFER_XACT_ERR:
		urb->error_count++;
		frame_desc->status = -DWC_E_PROTOCOL;
		frame_desc->actual_length =
		    get_actual_xfer_length(hc, hc_regs, qtd, halt_status, NULL);

		/* non DWORD-aligned buffer case handling. */
		if (hc->align_buff && frame_desc->actual_length && hc->ep_is_in) {
			dwc_memcpy(urb->buf + frame_desc->offset + qtd->isoc_split_offset,
				   hc->qh->dw_align_buf, frame_desc->actual_length);
		}
		/* Skip whole frame */
		if (hc->qh->do_split && (hc->ep_type == DWC_OTG_EP_TYPE_ISOC) &&
		    hc->ep_is_in && hcd->core_if->dma_enable) {
			qtd->complete_split = 0;
			qtd->isoc_split_offset = 0;
		}

		break;
	default:
		DWC_ASSERT(1, "Unhandled _halt_status (%d)\n", halt_status);
		break;
	}
	if (++qtd->isoc_frame_index == urb->packet_count) {
		/*
		 * urb->status is not used for isoc transfers.
		 * The individual frame_desc statuses are used instead.
		 */
		hcd->fops->complete(hcd, urb->priv, urb, 0);
		ret_val = DWC_OTG_HC_XFER_URB_COMPLETE;
	} else {
		ret_val = DWC_OTG_HC_XFER_COMPLETE;
	}
	return ret_val;
}

/**
 * Frees the first QTD in the QH's list if free_qtd is 1. For non-periodic
 * QHs, removes the QH from the active non-periodic schedule. If any QTDs are
 * still linked to the QH, the QH is added to the end of the inactive
 * non-periodic schedule. For periodic QHs, removes the QH from the periodic
 * schedule if no more QTDs are linked to the QH.
 */
static void deactivate_qh(dwc_otg_hcd_t * hcd, dwc_otg_qh_t * qh, int free_qtd)
{
	int continue_split = 0;
	dwc_otg_qtd_t *qtd;

	DWC_DEBUGPL(DBG_HCDV, "  %s(%p,%p,%d)\n", __func__, hcd, qh, free_qtd);

	qtd = DWC_CIRCLEQ_FIRST(&qh->qtd_list);

	if (qtd->complete_split) {
		continue_split = 1;
	} else if (qtd->isoc_split_pos == DWC_HCSPLIT_XACTPOS_MID ||
		   qtd->isoc_split_pos == DWC_HCSPLIT_XACTPOS_END) {
		continue_split = 1;
	}

	if (free_qtd) {
		dwc_otg_hcd_qtd_remove_and_free(hcd, qtd, qh);
		continue_split = 0;
	}

	qh->channel = NULL;
	dwc_otg_hcd_qh_deactivate(hcd, qh, continue_split);
}

/**
 * Releases a host channel for use by other transfers. Attempts to select and
 * queue more transactions since at least one host channel is available.
 *
 * @param hcd The HCD state structure.
 * @param hc The host channel to release.
 * @param qtd The QTD associated with the host channel. This QTD may be freed
 * if the transfer is complete or an error has occurred.
 * @param halt_status Reason the channel is being released. This status
 * determines the actions taken by this function.
 */
static void release_channel(dwc_otg_hcd_t * hcd,
			    dwc_hc_t * hc,
			    dwc_otg_qtd_t * qtd,
			    dwc_otg_halt_status_e halt_status)
{
	dwc_otg_transaction_type_e tr_type;
	int free_qtd;
	dwc_irqflags_t flags;
	dwc_spinlock_t *channel_lock = hcd->channel_lock;
#ifdef FIQ_DEBUG
	int endp = qtd->urb ? qtd->urb->pipe_info.ep_num : 0;
#endif
	int hog_port = 0;

	DWC_DEBUGPL(DBG_HCDV, "  %s: channel %d, halt_status %d, xfer_len %d\n",
		    __func__, hc->hc_num, halt_status, hc->xfer_len);

	if(fiq_split_enable && hc->do_split) {
		if(!hc->ep_is_in && hc->ep_type == UE_ISOCHRONOUS) {
			if(hc->xact_pos == DWC_HCSPLIT_XACTPOS_MID || 
					hc->xact_pos == DWC_HCSPLIT_XACTPOS_BEGIN) {
				hog_port = 1;
			}
		}
	}

	switch (halt_status) {
	case DWC_OTG_HC_XFER_URB_COMPLETE:
		free_qtd = 1;
		break;
	case DWC_OTG_HC_XFER_AHB_ERR:
	case DWC_OTG_HC_XFER_STALL:
	case DWC_OTG_HC_XFER_BABBLE_ERR:
		free_qtd = 1;
		break;
	case DWC_OTG_HC_XFER_XACT_ERR:
		if (qtd->error_count >= 3) {
			DWC_DEBUGPL(DBG_HCDV,
				    "  Complete URB with transaction error\n");
			free_qtd = 1;
			qtd->urb->status = -DWC_E_PROTOCOL;
			hcd->fops->complete(hcd, qtd->urb->priv,
					    qtd->urb, -DWC_E_PROTOCOL);
		} else {
			free_qtd = 0;
		}
		break;
	case DWC_OTG_HC_XFER_URB_DEQUEUE:
		/*
		 * The QTD has already been removed and the QH has been
		 * deactivated. Don't want to do anything except release the
		 * host channel and try to queue more transfers.
		 */
		goto cleanup;
	case DWC_OTG_HC_XFER_NO_HALT_STATUS:
		free_qtd = 0;
		break;
	case DWC_OTG_HC_XFER_PERIODIC_INCOMPLETE:
		DWC_DEBUGPL(DBG_HCDV,
			"  Complete URB with I/O error\n");
		free_qtd = 1;
		qtd->urb->status = -DWC_E_IO;
		hcd->fops->complete(hcd, qtd->urb->priv,
			qtd->urb, -DWC_E_IO);
		break;
	default:
		free_qtd = 0;
		break;
	}

	deactivate_qh(hcd, hc->qh, free_qtd);

cleanup:
	/*
	 * Release the host channel for use by other transfers. The cleanup
	 * function clears the channel interrupt enables and conditions, so
	 * there's no need to clear the Channel Halted interrupt separately.
	 */
	dwc_otg_hc_cleanup(hcd->core_if, hc);
	DWC_CIRCLEQ_INSERT_TAIL(&hcd->free_hc_list, hc, hc_list_entry);

	if (!microframe_schedule) {
		switch (hc->ep_type) {
		case DWC_OTG_EP_TYPE_CONTROL:
		case DWC_OTG_EP_TYPE_BULK:
			hcd->non_periodic_channels--;
			break;

		default:
			/*
			 * Don't release reservations for periodic channels here.
			 * That's done when a periodic transfer is descheduled (i.e.
			 * when the QH is removed from the periodic schedule).
			 */
			break;
		}
	} else {

		DWC_SPINLOCK_IRQSAVE(channel_lock, &flags);
		hcd->available_host_channels++;
		fiq_print(FIQDBG_PORTHUB, "AHC = %d ", hcd->available_host_channels);
		DWC_SPINUNLOCK_IRQRESTORE(channel_lock, flags);
	}

	if(fiq_split_enable && hc->do_split)
	{
		if(!(hcd->hub_port[hc->hub_addr] & (1 << hc->port_addr)))
		{
			fiq_print(FIQDBG_ERR, "PRTNOTAL");
			//BUG();
		}
		if(!hog_port && (hc->ep_type == DWC_OTG_EP_TYPE_ISOC ||
				hc->ep_type == DWC_OTG_EP_TYPE_INTR)) {
			hcd->hub_port[hc->hub_addr] &= ~(1 << hc->port_addr);
#ifdef FIQ_DEBUG
			hcd->hub_port_alloc[hc->hub_addr * 16 + hc->port_addr] = -1;
#endif
			fiq_print(FIQDBG_PORTHUB, "H%dP%d:RR%d", hc->hub_addr, hc->port_addr, endp);
		}
	}

	/* Try to queue more transfers now that there's a free channel. */
	tr_type = dwc_otg_hcd_select_transactions(hcd);
	if (tr_type != DWC_OTG_TRANSACTION_NONE) {
		dwc_otg_hcd_queue_transactions(hcd, tr_type);
	}
}

/**
 * Halts a host channel. If the channel cannot be halted immediately because
 * the request queue is full, this function ensures that the FIFO empty
 * interrupt for the appropriate queue is enabled so that the halt request can
 * be queued when there is space in the request queue.
 *
 * This function may also be called in DMA mode. In that case, the channel is
 * simply released since the core always halts the channel automatically in
 * DMA mode.
 */
static void halt_channel(dwc_otg_hcd_t * hcd,
			 dwc_hc_t * hc,
			 dwc_otg_qtd_t * qtd, dwc_otg_halt_status_e halt_status)
{
	if (hcd->core_if->dma_enable) {
		release_channel(hcd, hc, qtd, halt_status);
		return;
	}

	/* Slave mode processing... */
	dwc_otg_hc_halt(hcd->core_if, hc, halt_status);

	if (hc->halt_on_queue) {
		gintmsk_data_t gintmsk = {.d32 = 0 };
		dwc_otg_core_global_regs_t *global_regs;
		global_regs = hcd->core_if->core_global_regs;

		if (hc->ep_type == DWC_OTG_EP_TYPE_CONTROL ||
		    hc->ep_type == DWC_OTG_EP_TYPE_BULK) {
			/*
			 * Make sure the Non-periodic Tx FIFO empty interrupt
			 * is enabled so that the non-periodic schedule will
			 * be processed.
			 */
			gintmsk.b.nptxfempty = 1;
			DWC_MODIFY_REG32(&global_regs->gintmsk, 0, gintmsk.d32);
		} else {
			/*
			 * Move the QH from the periodic queued schedule to
			 * the periodic assigned schedule. This allows the
			 * halt to be queued when the periodic schedule is
			 * processed.
			 */
			DWC_LIST_MOVE_HEAD(&hcd->periodic_sched_assigned,
					   &hc->qh->qh_list_entry);

			/*
			 * Make sure the Periodic Tx FIFO Empty interrupt is
			 * enabled so that the periodic schedule will be
			 * processed.
			 */
			gintmsk.b.ptxfempty = 1;
			DWC_MODIFY_REG32(&global_regs->gintmsk, 0, gintmsk.d32);
		}
	}
}

/**
 * Performs common cleanup for non-periodic transfers after a Transfer
 * Complete interrupt. This function should be called after any endpoint type
 * specific handling is finished to release the host channel.
 */
static void complete_non_periodic_xfer(dwc_otg_hcd_t * hcd,
				       dwc_hc_t * hc,
				       dwc_otg_hc_regs_t * hc_regs,
				       dwc_otg_qtd_t * qtd,
				       dwc_otg_halt_status_e halt_status)
{
	hcint_data_t hcint;

	qtd->error_count = 0;

	hcint.d32 = DWC_READ_REG32(&hc_regs->hcint);
	if (hcint.b.nyet) {
		/*
		 * Got a NYET on the last transaction of the transfer. This
		 * means that the endpoint should be in the PING state at the
		 * beginning of the next transfer.
		 */
		hc->qh->ping_state = 1;
		clear_hc_int(hc_regs, nyet);
	}

	/*
	 * Always halt and release the host channel to make it available for
	 * more transfers. There may still be more phases for a control
	 * transfer or more data packets for a bulk transfer at this point,
	 * but the host channel is still halted. A channel will be reassigned
	 * to the transfer when the non-periodic schedule is processed after
	 * the channel is released. This allows transactions to be queued
	 * properly via dwc_otg_hcd_queue_transactions, which also enables the
	 * Tx FIFO Empty interrupt if necessary.
	 */
	if (hc->ep_is_in) {
		/*
		 * IN transfers in Slave mode require an explicit disable to
		 * halt the channel. (In DMA mode, this call simply releases
		 * the channel.)
		 */
		halt_channel(hcd, hc, qtd, halt_status);
	} else {
		/*
		 * The channel is automatically disabled by the core for OUT
		 * transfers in Slave mode.
		 */
		release_channel(hcd, hc, qtd, halt_status);
	}
}

/**
 * Performs common cleanup for periodic transfers after a Transfer Complete
 * interrupt. This function should be called after any endpoint type specific
 * handling is finished to release the host channel.
 */
static void complete_periodic_xfer(dwc_otg_hcd_t * hcd,
				   dwc_hc_t * hc,
				   dwc_otg_hc_regs_t * hc_regs,
				   dwc_otg_qtd_t * qtd,
				   dwc_otg_halt_status_e halt_status)
{
	hctsiz_data_t hctsiz;
	qtd->error_count = 0;

	hctsiz.d32 = DWC_READ_REG32(&hc_regs->hctsiz);
	if (!hc->ep_is_in || hctsiz.b.pktcnt == 0) {
		/* Core halts channel in these cases. */
		release_channel(hcd, hc, qtd, halt_status);
	} else {
		/* Flush any outstanding requests from the Tx queue. */
		halt_channel(hcd, hc, qtd, halt_status);
	}
}

static int32_t handle_xfercomp_isoc_split_in(dwc_otg_hcd_t * hcd,
					     dwc_hc_t * hc,
					     dwc_otg_hc_regs_t * hc_regs,
					     dwc_otg_qtd_t * qtd)
{
	uint32_t len;
	struct dwc_otg_hcd_iso_packet_desc *frame_desc;
	frame_desc = &qtd->urb->iso_descs[qtd->isoc_frame_index];

	len = get_actual_xfer_length(hc, hc_regs, qtd,
				     DWC_OTG_HC_XFER_COMPLETE, NULL);

	if (!len) {
		qtd->complete_split = 0;
		qtd->isoc_split_offset = 0;
		return 0;
	}
	frame_desc->actual_length += len;

	if (hc->align_buff && len)
		dwc_memcpy(qtd->urb->buf + frame_desc->offset +
			   qtd->isoc_split_offset, hc->qh->dw_align_buf, len);
	qtd->isoc_split_offset += len;

	if (frame_desc->length == frame_desc->actual_length) {
		frame_desc->status = 0;
		qtd->isoc_frame_index++;
		qtd->complete_split = 0;
		qtd->isoc_split_offset = 0;
	}

	if (qtd->isoc_frame_index == qtd->urb->packet_count) {
		hcd->fops->complete(hcd, qtd->urb->priv, qtd->urb, 0);
		release_channel(hcd, hc, qtd, DWC_OTG_HC_XFER_URB_COMPLETE);
	} else {
		release_channel(hcd, hc, qtd, DWC_OTG_HC_XFER_NO_HALT_STATUS);
	}

	return 1;		/* Indicates that channel released */
}

/**
 * Handles a host channel Transfer Complete interrupt. This handler may be
 * called in either DMA mode or Slave mode.
 */
static int32_t handle_hc_xfercomp_intr(dwc_otg_hcd_t * hcd,
				       dwc_hc_t * hc,
				       dwc_otg_hc_regs_t * hc_regs,
				       dwc_otg_qtd_t * qtd)
{
	int urb_xfer_done;
	dwc_otg_halt_status_e halt_status = DWC_OTG_HC_XFER_COMPLETE;
	dwc_otg_hcd_urb_t *urb = qtd->urb;
	int pipe_type = dwc_otg_hcd_get_pipe_type(&urb->pipe_info);

	DWC_DEBUGPL(DBG_HCDI, "--Host Channel %d Interrupt: "
		    "Transfer Complete--\n", hc->hc_num);

	if (hcd->core_if->dma_desc_enable) {
		dwc_otg_hcd_complete_xfer_ddma(hcd, hc, hc_regs, halt_status);
		if (pipe_type == UE_ISOCHRONOUS) {
			/* Do not disable the interrupt, just clear it */
			clear_hc_int(hc_regs, xfercomp);
			return 1;
		}
		goto handle_xfercomp_done;
	}

	/*
	 * Handle xfer complete on CSPLIT.
	 */

	if (hc->qh->do_split) {
		if ((hc->ep_type == DWC_OTG_EP_TYPE_ISOC) && hc->ep_is_in
		    && hcd->core_if->dma_enable) {
			if (qtd->complete_split
			    && handle_xfercomp_isoc_split_in(hcd, hc, hc_regs,
							     qtd))
				goto handle_xfercomp_done;
		} else {
			qtd->complete_split = 0;
		}
	}

	/* Update the QTD and URB states. */
	switch (pipe_type) {
	case UE_CONTROL:
		switch (qtd->control_phase) {
		case DWC_OTG_CONTROL_SETUP:
			if (urb->length > 0) {
				qtd->control_phase = DWC_OTG_CONTROL_DATA;
			} else {
				qtd->control_phase = DWC_OTG_CONTROL_STATUS;
			}
			DWC_DEBUGPL(DBG_HCDV,
				    "  Control setup transaction done\n");
			halt_status = DWC_OTG_HC_XFER_COMPLETE;
			break;
		case DWC_OTG_CONTROL_DATA:{
				urb_xfer_done =
				    update_urb_state_xfer_comp(hc, hc_regs, urb,
							       qtd);
				if (urb_xfer_done) {
					qtd->control_phase =
					    DWC_OTG_CONTROL_STATUS;
					DWC_DEBUGPL(DBG_HCDV,
						    "  Control data transfer done\n");
				} else {
					dwc_otg_hcd_save_data_toggle(hc, hc_regs, qtd);
				}
				halt_status = DWC_OTG_HC_XFER_COMPLETE;
				break;
			}
		case DWC_OTG_CONTROL_STATUS:
			DWC_DEBUGPL(DBG_HCDV, "  Control transfer complete\n");
			if (urb->status == -DWC_E_IN_PROGRESS) {
				urb->status = 0;
			}
			hcd->fops->complete(hcd, urb->priv, urb, urb->status);
			halt_status = DWC_OTG_HC_XFER_URB_COMPLETE;
			break;
		}

		complete_non_periodic_xfer(hcd, hc, hc_regs, qtd, halt_status);
		break;
	case UE_BULK:
		DWC_DEBUGPL(DBG_HCDV, "  Bulk transfer complete\n");
		urb_xfer_done =
		    update_urb_state_xfer_comp(hc, hc_regs, urb, qtd);
		if (urb_xfer_done) {
			hcd->fops->complete(hcd, urb->priv, urb, urb->status);
			halt_status = DWC_OTG_HC_XFER_URB_COMPLETE;
		} else {
			halt_status = DWC_OTG_HC_XFER_COMPLETE;
		}

		dwc_otg_hcd_save_data_toggle(hc, hc_regs, qtd);
		complete_non_periodic_xfer(hcd, hc, hc_regs, qtd, halt_status);
		break;
	case UE_INTERRUPT:
		DWC_DEBUGPL(DBG_HCDV, "  Interrupt transfer complete\n");
		urb_xfer_done =
			update_urb_state_xfer_comp(hc, hc_regs, urb, qtd);

		/*
		 * Interrupt URB is done on the first transfer complete
		 * interrupt.
		 */
		if (urb_xfer_done) {
				hcd->fops->complete(hcd, urb->priv, urb, urb->status);
				halt_status = DWC_OTG_HC_XFER_URB_COMPLETE;
		} else {
				halt_status = DWC_OTG_HC_XFER_COMPLETE;
		}

		dwc_otg_hcd_save_data_toggle(hc, hc_regs, qtd);
		complete_periodic_xfer(hcd, hc, hc_regs, qtd, halt_status);
		break;
	case UE_ISOCHRONOUS:
		DWC_DEBUGPL(DBG_HCDV, "  Isochronous transfer complete\n");
		if (qtd->isoc_split_pos == DWC_HCSPLIT_XACTPOS_ALL) {
			halt_status =
			    update_isoc_urb_state(hcd, hc, hc_regs, qtd,
						  DWC_OTG_HC_XFER_COMPLETE);
		}
		complete_periodic_xfer(hcd, hc, hc_regs, qtd, halt_status);
		break;
	}

handle_xfercomp_done:
	disable_hc_int(hc_regs, xfercompl);

	return 1;
}

/**
 * Handles a host channel STALL interrupt. This handler may be called in
 * either DMA mode or Slave mode.
 */
static int32_t handle_hc_stall_intr(dwc_otg_hcd_t * hcd,
				    dwc_hc_t * hc,
				    dwc_otg_hc_regs_t * hc_regs,
				    dwc_otg_qtd_t * qtd)
{
	dwc_otg_hcd_urb_t *urb = qtd->urb;
	int pipe_type = dwc_otg_hcd_get_pipe_type(&urb->pipe_info);

	DWC_DEBUGPL(DBG_HCD, "--Host Channel %d Interrupt: "
		    "STALL Received--\n", hc->hc_num);

	if (hcd->core_if->dma_desc_enable) {
		dwc_otg_hcd_complete_xfer_ddma(hcd, hc, hc_regs, DWC_OTG_HC_XFER_STALL);
		goto handle_stall_done;
	}

	if (pipe_type == UE_CONTROL) {
		hcd->fops->complete(hcd, urb->priv, urb, -DWC_E_PIPE);
	}

	if (pipe_type == UE_BULK || pipe_type == UE_INTERRUPT) {
		hcd->fops->complete(hcd, urb->priv, urb, -DWC_E_PIPE);
		/*
		 * USB protocol requires resetting the data toggle for bulk
		 * and interrupt endpoints when a CLEAR_FEATURE(ENDPOINT_HALT)
		 * setup command is issued to the endpoint. Anticipate the
		 * CLEAR_FEATURE command since a STALL has occurred and reset
		 * the data toggle now.
		 */
		hc->qh->data_toggle = 0;
	}

	halt_channel(hcd, hc, qtd, DWC_OTG_HC_XFER_STALL);

handle_stall_done:
	disable_hc_int(hc_regs, stall);

	return 1;
}

/*
 * Updates the state of the URB when a transfer has been stopped due to an
 * abnormal condition before the transfer completes. Modifies the
 * actual_length field of the URB to reflect the number of bytes that have
 * actually been transferred via the host channel.
 */
static void update_urb_state_xfer_intr(dwc_hc_t * hc,
				       dwc_otg_hc_regs_t * hc_regs,
				       dwc_otg_hcd_urb_t * urb,
				       dwc_otg_qtd_t * qtd,
				       dwc_otg_halt_status_e halt_status)
{
	uint32_t bytes_transferred = get_actual_xfer_length(hc, hc_regs, qtd,
							    halt_status, NULL);
	/* non DWORD-aligned buffer case handling. */
	if (hc->align_buff && bytes_transferred && hc->ep_is_in) {
		dwc_memcpy(urb->buf + urb->actual_length, hc->qh->dw_align_buf,
			   bytes_transferred);
	}

	urb->actual_length += bytes_transferred;

#ifdef DEBUG
	{
		hctsiz_data_t hctsiz;
		hctsiz.d32 = DWC_READ_REG32(&hc_regs->hctsiz);
		DWC_DEBUGPL(DBG_HCDV, "DWC_otg: %s: %s, channel %d\n",
			    __func__, (hc->ep_is_in ? "IN" : "OUT"),
			    hc->hc_num);
		DWC_DEBUGPL(DBG_HCDV, "  hc->start_pkt_count %d\n",
			    hc->start_pkt_count);
		DWC_DEBUGPL(DBG_HCDV, "  hctsiz.pktcnt %d\n", hctsiz.b.pktcnt);
		DWC_DEBUGPL(DBG_HCDV, "  hc->max_packet %d\n", hc->max_packet);
		DWC_DEBUGPL(DBG_HCDV, "  bytes_transferred %d\n",
			    bytes_transferred);
		DWC_DEBUGPL(DBG_HCDV, "  urb->actual_length %d\n",
			    urb->actual_length);
		DWC_DEBUGPL(DBG_HCDV, "  urb->transfer_buffer_length %d\n",
			    urb->length);
	}
#endif
}

/**
 * Handles a host channel NAK interrupt. This handler may be called in either
 * DMA mode or Slave mode.
 */
static int32_t handle_hc_nak_intr(dwc_otg_hcd_t * hcd,
				  dwc_hc_t * hc,
				  dwc_otg_hc_regs_t * hc_regs,
				  dwc_otg_qtd_t * qtd)
{
	DWC_DEBUGPL(DBG_HCDI, "--Host Channel %d Interrupt: "
		    "NAK Received--\n", hc->hc_num);

	/*
	 * When we get bulk NAKs then remember this so we holdoff on this qh until
	 * the beginning of the next frame
	 */
	switch(dwc_otg_hcd_get_pipe_type(&qtd->urb->pipe_info)) {
		case UE_BULK:
		case UE_CONTROL:
		if (nak_holdoff_enable)
			hc->qh->nak_frame = dwc_otg_hcd_get_frame_number(hcd);
	}

	/*
	 * Handle NAK for IN/OUT SSPLIT/CSPLIT transfers, bulk, control, and
	 * interrupt.  Re-start the SSPLIT transfer.
	 */
	if (hc->do_split) {
		if (hc->complete_split) {
			qtd->error_count = 0;
		}
		qtd->complete_split = 0;
		halt_channel(hcd, hc, qtd, DWC_OTG_HC_XFER_NAK);
		goto handle_nak_done;
	}

	switch (dwc_otg_hcd_get_pipe_type(&qtd->urb->pipe_info)) {
	case UE_CONTROL:
	case UE_BULK:
		if (hcd->core_if->dma_enable && hc->ep_is_in) {
			/*
			 * NAK interrupts are enabled on bulk/control IN
			 * transfers in DMA mode for the sole purpose of
			 * resetting the error count after a transaction error
			 * occurs. The core will continue transferring data.
			 * Disable other interrupts unmasked for the same
			 * reason.
			 */
			disable_hc_int(hc_regs, datatglerr);
			disable_hc_int(hc_regs, ack);
			qtd->error_count = 0;
			goto handle_nak_done;
		}

		/*
		 * NAK interrupts normally occur during OUT transfers in DMA
		 * or Slave mode. For IN transfers, more requests will be
		 * queued as request queue space is available.
		 */
		qtd->error_count = 0;

		if (!hc->qh->ping_state) {
			update_urb_state_xfer_intr(hc, hc_regs,
						   qtd->urb, qtd,
						   DWC_OTG_HC_XFER_NAK);
			dwc_otg_hcd_save_data_toggle(hc, hc_regs, qtd);

			if (hc->speed == DWC_OTG_EP_SPEED_HIGH)
				hc->qh->ping_state = 1;
		}

		/*
		 * Halt the channel so the transfer can be re-started from
		 * the appropriate point or the PING protocol will
		 * start/continue.
		 */
		halt_channel(hcd, hc, qtd, DWC_OTG_HC_XFER_NAK);
		break;
	case UE_INTERRUPT:
		qtd->error_count = 0;
		halt_channel(hcd, hc, qtd, DWC_OTG_HC_XFER_NAK);
		break;
	case UE_ISOCHRONOUS:
		/* Should never get called for isochronous transfers. */
		DWC_ASSERT(1, "NACK interrupt for ISOC transfer\n");
		break;
	}

handle_nak_done:
	disable_hc_int(hc_regs, nak);

	return 1;
}

/**
 * Handles a host channel ACK interrupt. This interrupt is enabled when
 * performing the PING protocol in Slave mode, when errors occur during
 * either Slave mode or DMA mode, and during Start Split transactions.
 */
static int32_t handle_hc_ack_intr(dwc_otg_hcd_t * hcd,
				  dwc_hc_t * hc,
				  dwc_otg_hc_regs_t * hc_regs,
				  dwc_otg_qtd_t * qtd)
{
	DWC_DEBUGPL(DBG_HCDI, "--Host Channel %d Interrupt: "
		    "ACK Received--\n", hc->hc_num);

	if (hc->do_split) {
		/*
		 * Handle ACK on SSPLIT.
		 * ACK should not occur in CSPLIT.
		 */
		if (!hc->ep_is_in && hc->data_pid_start != DWC_OTG_HC_PID_SETUP) {
			qtd->ssplit_out_xfer_count = hc->xfer_len;
		}
		if (!(hc->ep_type == DWC_OTG_EP_TYPE_ISOC && !hc->ep_is_in)) {
			/* Don't need complete for isochronous out transfers. */
			qtd->complete_split = 1;
		}

		/* ISOC OUT */
		if (hc->ep_type == DWC_OTG_EP_TYPE_ISOC && !hc->ep_is_in) {
			switch (hc->xact_pos) {
			case DWC_HCSPLIT_XACTPOS_ALL:
				break;
			case DWC_HCSPLIT_XACTPOS_END:
				qtd->isoc_split_pos = DWC_HCSPLIT_XACTPOS_ALL;
				qtd->isoc_split_offset = 0;
				break;
			case DWC_HCSPLIT_XACTPOS_BEGIN:
			case DWC_HCSPLIT_XACTPOS_MID:
				/*
				 * For BEGIN or MID, calculate the length for
				 * the next microframe to determine the correct
				 * SSPLIT token, either MID or END.
				 */
				{
					struct dwc_otg_hcd_iso_packet_desc
					*frame_desc;

					frame_desc =
					    &qtd->urb->
					    iso_descs[qtd->isoc_frame_index];
					qtd->isoc_split_offset += 188;

					if ((frame_desc->length -
					     qtd->isoc_split_offset) <= 188) {
						qtd->isoc_split_pos =
						    DWC_HCSPLIT_XACTPOS_END;
					} else {
						qtd->isoc_split_pos =
						    DWC_HCSPLIT_XACTPOS_MID;
					}

				}
				break;
			}
		} else {
			halt_channel(hcd, hc, qtd, DWC_OTG_HC_XFER_ACK);
		}
	} else {
		/*
		 * An unmasked ACK on a non-split DMA transaction is
		 * for the sole purpose of resetting error counts. Disable other
		 * interrupts unmasked for the same reason.
		 */
		if(hcd->core_if->dma_enable) {
			disable_hc_int(hc_regs, datatglerr);
			disable_hc_int(hc_regs, nak);
		}
		qtd->error_count = 0;

		if (hc->qh->ping_state) {
			hc->qh->ping_state = 0;
			/*
			 * Halt the channel so the transfer can be re-started
			 * from the appropriate point. This only happens in
			 * Slave mode. In DMA mode, the ping_state is cleared
			 * when the transfer is started because the core
			 * automatically executes the PING, then the transfer.
			 */
			halt_channel(hcd, hc, qtd, DWC_OTG_HC_XFER_ACK);
		}
	}

	/*
	 * If the ACK occurred when _not_ in the PING state, let the channel
	 * continue transferring data after clearing the error count.
	 */

	disable_hc_int(hc_regs, ack);

	return 1;
}

/**
 * Handles a host channel NYET interrupt. This interrupt should only occur on
 * Bulk and Control OUT endpoints and for complete split transactions. If a
 * NYET occurs at the same time as a Transfer Complete interrupt, it is
 * handled in the xfercomp interrupt handler, not here. This handler may be
 * called in either DMA mode or Slave mode.
 */
static int32_t handle_hc_nyet_intr(dwc_otg_hcd_t * hcd,
				   dwc_hc_t * hc,
				   dwc_otg_hc_regs_t * hc_regs,
				   dwc_otg_qtd_t * qtd)
{
	DWC_DEBUGPL(DBG_HCDI, "--Host Channel %d Interrupt: "
		    "NYET Received--\n", hc->hc_num);

	/*
	 * NYET on CSPLIT
	 * re-do the CSPLIT immediately on non-periodic
	 */
	if (hc->do_split && hc->complete_split) {
		if (hc->ep_is_in && (hc->ep_type == DWC_OTG_EP_TYPE_ISOC)
		    && hcd->core_if->dma_enable) {
			qtd->complete_split = 0;
			qtd->isoc_split_offset = 0;
			if (++qtd->isoc_frame_index == qtd->urb->packet_count) {
				hcd->fops->complete(hcd, qtd->urb->priv, qtd->urb, 0);
				release_channel(hcd, hc, qtd, DWC_OTG_HC_XFER_URB_COMPLETE);	
			}
			else
				release_channel(hcd, hc, qtd, DWC_OTG_HC_XFER_NO_HALT_STATUS);	
			goto handle_nyet_done;
		}
		
		if (hc->ep_type == DWC_OTG_EP_TYPE_INTR ||
		    hc->ep_type == DWC_OTG_EP_TYPE_ISOC) {
			int frnum = dwc_otg_hcd_get_frame_number(hcd);

			// With the FIQ running we only ever see the failed NYET
			if (dwc_full_frame_num(frnum) !=
			    dwc_full_frame_num(hc->qh->sched_frame) ||
			    fiq_split_enable) {
				/*
				 * No longer in the same full speed frame.
				 * Treat this as a transaction error.
				 */
#if 0
				/** @todo Fix system performance so this can
				 * be treated as an error. Right now complete
				 * splits cannot be scheduled precisely enough
				 * due to other system activity, so this error
				 * occurs regularly in Slave mode.
				 */
				qtd->error_count++;
#endif
				qtd->complete_split = 0;
				halt_channel(hcd, hc, qtd,
					     DWC_OTG_HC_XFER_XACT_ERR);
				/** @todo add support for isoc release */
				goto handle_nyet_done;
			}
		}

		halt_channel(hcd, hc, qtd, DWC_OTG_HC_XFER_NYET);
		goto handle_nyet_done;
	}

	hc->qh->ping_state = 1;
	qtd->error_count = 0;

	update_urb_state_xfer_intr(hc, hc_regs, qtd->urb, qtd,
				   DWC_OTG_HC_XFER_NYET);
	dwc_otg_hcd_save_data_toggle(hc, hc_regs, qtd);

	/*
	 * Halt the channel and re-start the transfer so the PING
	 * protocol will start.
	 */
	halt_channel(hcd, hc, qtd, DWC_OTG_HC_XFER_NYET);

handle_nyet_done:
	disable_hc_int(hc_regs, nyet);
	return 1;
}

/**
 * Handles a host channel babble interrupt. This handler may be called in
 * either DMA mode or Slave mode.
 */
static int32_t handle_hc_babble_intr(dwc_otg_hcd_t * hcd,
				     dwc_hc_t * hc,
				     dwc_otg_hc_regs_t * hc_regs,
				     dwc_otg_qtd_t * qtd)
{
	DWC_DEBUGPL(DBG_HCDI, "--Host Channel %d Interrupt: "
		    "Babble Error--\n", hc->hc_num);

	if (hcd->core_if->dma_desc_enable) {
		dwc_otg_hcd_complete_xfer_ddma(hcd, hc, hc_regs,
					       DWC_OTG_HC_XFER_BABBLE_ERR);
		goto handle_babble_done;
	}

	if (hc->ep_type != DWC_OTG_EP_TYPE_ISOC) {
		hcd->fops->complete(hcd, qtd->urb->priv,
				    qtd->urb, -DWC_E_OVERFLOW);
		halt_channel(hcd, hc, qtd, DWC_OTG_HC_XFER_BABBLE_ERR);
	} else {
		dwc_otg_halt_status_e halt_status;
		halt_status = update_isoc_urb_state(hcd, hc, hc_regs, qtd,
						    DWC_OTG_HC_XFER_BABBLE_ERR);
		halt_channel(hcd, hc, qtd, halt_status);
	}

handle_babble_done:
	disable_hc_int(hc_regs, bblerr);
	return 1;
}

/**
 * Handles a host channel AHB error interrupt. This handler is only called in
 * DMA mode.
 */
static int32_t handle_hc_ahberr_intr(dwc_otg_hcd_t * hcd,
				     dwc_hc_t * hc,
				     dwc_otg_hc_regs_t * hc_regs,
				     dwc_otg_qtd_t * qtd)
{
	hcchar_data_t hcchar;
	hcsplt_data_t hcsplt;
	hctsiz_data_t hctsiz;
	uint32_t hcdma;
	char *pipetype, *speed;

	dwc_otg_hcd_urb_t *urb = qtd->urb;

	DWC_DEBUGPL(DBG_HCDI, "--Host Channel %d Interrupt: "
		    "AHB Error--\n", hc->hc_num);

	hcchar.d32 = DWC_READ_REG32(&hc_regs->hcchar);
	hcsplt.d32 = DWC_READ_REG32(&hc_regs->hcsplt);
	hctsiz.d32 = DWC_READ_REG32(&hc_regs->hctsiz);
	hcdma = DWC_READ_REG32(&hc_regs->hcdma);

	DWC_ERROR("AHB ERROR, Channel %d\n", hc->hc_num);
	DWC_ERROR("  hcchar 0x%08x, hcsplt 0x%08x\n", hcchar.d32, hcsplt.d32);
	DWC_ERROR("  hctsiz 0x%08x, hcdma 0x%08x\n", hctsiz.d32, hcdma);
	DWC_DEBUGPL(DBG_HCD, "DWC OTG HCD URB Enqueue\n");
	DWC_ERROR("  Device address: %d\n",
		  dwc_otg_hcd_get_dev_addr(&urb->pipe_info));
	DWC_ERROR("  Endpoint: %d, %s\n",
		  dwc_otg_hcd_get_ep_num(&urb->pipe_info),
		  (dwc_otg_hcd_is_pipe_in(&urb->pipe_info) ? "IN" : "OUT"));

	switch (dwc_otg_hcd_get_pipe_type(&urb->pipe_info)) {
	case UE_CONTROL:
		pipetype = "CONTROL";
		break;
	case UE_BULK:
		pipetype = "BULK";
		break;
	case UE_INTERRUPT:
		pipetype = "INTERRUPT";
		break;
	case UE_ISOCHRONOUS:
		pipetype = "ISOCHRONOUS";
		break;
	default:
		pipetype = "UNKNOWN";
		break;
	}

	DWC_ERROR("  Endpoint type: %s\n", pipetype);

	switch (hc->speed) {
	case DWC_OTG_EP_SPEED_HIGH:
		speed = "HIGH";
		break;
	case DWC_OTG_EP_SPEED_FULL:
		speed = "FULL";
		break;
	case DWC_OTG_EP_SPEED_LOW:
		speed = "LOW";
		break;
	default:
		speed = "UNKNOWN";
		break;
	};

	DWC_ERROR("  Speed: %s\n", speed);

	DWC_ERROR("  Max packet size: %d\n",
		  dwc_otg_hcd_get_mps(&urb->pipe_info));
	DWC_ERROR("  Data buffer length: %d\n", urb->length);
	DWC_ERROR("  Transfer buffer: %p, Transfer DMA: %p\n",
		  urb->buf, (void *)urb->dma);
	DWC_ERROR("  Setup buffer: %p, Setup DMA: %p\n",
		  urb->setup_packet, (void *)urb->setup_dma);
	DWC_ERROR("  Interval: %d\n", urb->interval);

	/* Core haltes the channel for Descriptor DMA mode */
	if (hcd->core_if->dma_desc_enable) {
		dwc_otg_hcd_complete_xfer_ddma(hcd, hc, hc_regs,
					       DWC_OTG_HC_XFER_AHB_ERR);
		goto handle_ahberr_done;
	}

	hcd->fops->complete(hcd, urb->priv, urb, -DWC_E_IO);

	/*
	 * Force a channel halt. Don't call halt_channel because that won't
	 * write to the HCCHARn register in DMA mode to force the halt.
	 */
	dwc_otg_hc_halt(hcd->core_if, hc, DWC_OTG_HC_XFER_AHB_ERR);
handle_ahberr_done:
	disable_hc_int(hc_regs, ahberr);
	return 1;
}

/**
 * Handles a host channel transaction error interrupt. This handler may be
 * called in either DMA mode or Slave mode.
 */
static int32_t handle_hc_xacterr_intr(dwc_otg_hcd_t * hcd,
				      dwc_hc_t * hc,
				      dwc_otg_hc_regs_t * hc_regs,
				      dwc_otg_qtd_t * qtd)
{
	DWC_DEBUGPL(DBG_HCDI, "--Host Channel %d Interrupt: "
		    "Transaction Error--\n", hc->hc_num);

	if (hcd->core_if->dma_desc_enable) {
		dwc_otg_hcd_complete_xfer_ddma(hcd, hc, hc_regs,
					       DWC_OTG_HC_XFER_XACT_ERR);
		goto handle_xacterr_done;
	}

	switch (dwc_otg_hcd_get_pipe_type(&qtd->urb->pipe_info)) {
	case UE_CONTROL:
	case UE_BULK:
		qtd->error_count++;
		if (!hc->qh->ping_state) {

			update_urb_state_xfer_intr(hc, hc_regs,
						   qtd->urb, qtd,
						   DWC_OTG_HC_XFER_XACT_ERR);
			dwc_otg_hcd_save_data_toggle(hc, hc_regs, qtd);
			if (!hc->ep_is_in && hc->speed == DWC_OTG_EP_SPEED_HIGH) {
				hc->qh->ping_state = 1;
			}
		}

		/*
		 * Halt the channel so the transfer can be re-started from
		 * the appropriate point or the PING protocol will start.
		 */
		halt_channel(hcd, hc, qtd, DWC_OTG_HC_XFER_XACT_ERR);
		break;
	case UE_INTERRUPT:
		qtd->error_count++;
		if (hc->do_split && hc->complete_split) {
			qtd->complete_split = 0;
		}
		halt_channel(hcd, hc, qtd, DWC_OTG_HC_XFER_XACT_ERR);
		break;
	case UE_ISOCHRONOUS:
		{
			dwc_otg_halt_status_e halt_status;
			halt_status =
			    update_isoc_urb_state(hcd, hc, hc_regs, qtd,
						  DWC_OTG_HC_XFER_XACT_ERR);

			halt_channel(hcd, hc, qtd, halt_status);
		}
		break;
	}
handle_xacterr_done:
	disable_hc_int(hc_regs, xacterr);
#if 0
	/* stop all operation of usb when d+/d- is shorted. */
	dwc_otg_disable_host_interrupts(hcd->core_if);
	dwc_otg_disable_global_interrupts(hcd->core_if);
#endif
	return 1;
}

/**
 * Handles a host channel frame overrun interrupt. This handler may be called
 * in either DMA mode or Slave mode.
 */
static int32_t handle_hc_frmovrun_intr(dwc_otg_hcd_t * hcd,
				       dwc_hc_t * hc,
				       dwc_otg_hc_regs_t * hc_regs,
				       dwc_otg_qtd_t * qtd)
{
	DWC_DEBUGPL(DBG_HCDI, "--Host Channel %d Interrupt: "
		    "Frame Overrun--\n", hc->hc_num);

	switch (dwc_otg_hcd_get_pipe_type(&qtd->urb->pipe_info)) {
	case UE_CONTROL:
	case UE_BULK:
		break;
	case UE_INTERRUPT:
		halt_channel(hcd, hc, qtd, DWC_OTG_HC_XFER_FRAME_OVERRUN);
		break;
	case UE_ISOCHRONOUS:
		{
			dwc_otg_halt_status_e halt_status;
			halt_status =
			    update_isoc_urb_state(hcd, hc, hc_regs, qtd,
						  DWC_OTG_HC_XFER_FRAME_OVERRUN);

			halt_channel(hcd, hc, qtd, halt_status);
		}
		break;
	}

	disable_hc_int(hc_regs, frmovrun);

	return 1;
}

/**
 * Handles a host channel data toggle error interrupt. This handler may be
 * called in either DMA mode or Slave mode.
 */
static int32_t handle_hc_datatglerr_intr(dwc_otg_hcd_t * hcd,
					 dwc_hc_t * hc,
					 dwc_otg_hc_regs_t * hc_regs,
					 dwc_otg_qtd_t * qtd)
{
	DWC_DEBUGPL(DBG_HCDI, "--Host Channel %d Interrupt: "
		"Data Toggle Error on %s transfer--\n",
		hc->hc_num, (hc->ep_is_in ? "IN" : "OUT"));

	/* Data toggles on split transactions cause the hc to halt.
	 * restart transfer */
	if(hc->qh->do_split)
	{
		qtd->error_count++;
		dwc_otg_hcd_save_data_toggle(hc, hc_regs, qtd);
		update_urb_state_xfer_intr(hc, hc_regs,
			qtd->urb, qtd, DWC_OTG_HC_XFER_XACT_ERR);
		halt_channel(hcd, hc, qtd, DWC_OTG_HC_XFER_XACT_ERR);
	} else if (hc->ep_is_in) {
		/* An unmasked data toggle error on a non-split DMA transaction is
		 * for the sole purpose of resetting error counts. Disable other
		 * interrupts unmasked for the same reason.
		 */
		if(hcd->core_if->dma_enable) {
			disable_hc_int(hc_regs, ack);
			disable_hc_int(hc_regs, nak);
		}
		qtd->error_count = 0;
	}

	disable_hc_int(hc_regs, datatglerr);

	return 1;
}

#ifdef DEBUG
/**
 * This function is for debug only. It checks that a valid halt status is set
 * and that HCCHARn.chdis is clear. If there's a problem, corrective action is
 * taken and a warning is issued.
 * @return 1 if halt status is ok, 0 otherwise.
 */
static inline int halt_status_ok(dwc_otg_hcd_t * hcd,
				 dwc_hc_t * hc,
				 dwc_otg_hc_regs_t * hc_regs,
				 dwc_otg_qtd_t * qtd)
{
	hcchar_data_t hcchar;
	hctsiz_data_t hctsiz;
	hcint_data_t hcint;
	hcintmsk_data_t hcintmsk;
	hcsplt_data_t hcsplt;

	if (hc->halt_status == DWC_OTG_HC_XFER_NO_HALT_STATUS) {
		/*
		 * This code is here only as a check. This condition should
		 * never happen. Ignore the halt if it does occur.
		 */
		hcchar.d32 = DWC_READ_REG32(&hc_regs->hcchar);
		hctsiz.d32 = DWC_READ_REG32(&hc_regs->hctsiz);
		hcint.d32 = DWC_READ_REG32(&hc_regs->hcint);
		hcintmsk.d32 = DWC_READ_REG32(&hc_regs->hcintmsk);
		hcsplt.d32 = DWC_READ_REG32(&hc_regs->hcsplt);
		DWC_WARN
		    ("%s: hc->halt_status == DWC_OTG_HC_XFER_NO_HALT_STATUS, "
		     "channel %d, hcchar 0x%08x, hctsiz 0x%08x, "
		     "hcint 0x%08x, hcintmsk 0x%08x, "
		     "hcsplt 0x%08x, qtd->complete_split %d\n", __func__,
		     hc->hc_num, hcchar.d32, hctsiz.d32, hcint.d32,
		     hcintmsk.d32, hcsplt.d32, qtd->complete_split);

		DWC_WARN("%s: no halt status, channel %d, ignoring interrupt\n",
			 __func__, hc->hc_num);
		DWC_WARN("\n");
		clear_hc_int(hc_regs, chhltd);
		return 0;
	}

	/*
	 * This code is here only as a check. hcchar.chdis should
	 * never be set when the halt interrupt occurs. Halt the
	 * channel again if it does occur.
	 */
	hcchar.d32 = DWC_READ_REG32(&hc_regs->hcchar);
	if (hcchar.b.chdis) {
		DWC_WARN("%s: hcchar.chdis set unexpectedly, "
			 "hcchar 0x%08x, trying to halt again\n",
			 __func__, hcchar.d32);
		clear_hc_int(hc_regs, chhltd);
		hc->halt_pending = 0;
		halt_channel(hcd, hc, qtd, hc->halt_status);
		return 0;
	}

	return 1;
}
#endif

/**
 * Handles a host Channel Halted interrupt in DMA mode. This handler
 * determines the reason the channel halted and proceeds accordingly.
 */
static void handle_hc_chhltd_intr_dma(dwc_otg_hcd_t * hcd,
				      dwc_hc_t * hc,
				      dwc_otg_hc_regs_t * hc_regs,
				      dwc_otg_qtd_t * qtd,
				      hcint_data_t hcint,
				      hcintmsk_data_t hcintmsk)
{
	int out_nak_enh = 0;

	/* For core with OUT NAK enhancement, the flow for high-
	 * speed CONTROL/BULK OUT is handled a little differently.
	 */
	if (hcd->core_if->snpsid >= OTG_CORE_REV_2_71a) {
		if (hc->speed == DWC_OTG_EP_SPEED_HIGH && !hc->ep_is_in &&
		    (hc->ep_type == DWC_OTG_EP_TYPE_CONTROL ||
		     hc->ep_type == DWC_OTG_EP_TYPE_BULK)) {
			out_nak_enh = 1;
		}
	}

	if (hc->halt_status == DWC_OTG_HC_XFER_URB_DEQUEUE ||
	    (hc->halt_status == DWC_OTG_HC_XFER_AHB_ERR
	     && !hcd->core_if->dma_desc_enable)) {
		/*
		 * Just release the channel. A dequeue can happen on a
		 * transfer timeout. In the case of an AHB Error, the channel
		 * was forced to halt because there's no way to gracefully
		 * recover.
		 */
		if (hcd->core_if->dma_desc_enable)
			dwc_otg_hcd_complete_xfer_ddma(hcd, hc, hc_regs,
						       hc->halt_status);
		else
			release_channel(hcd, hc, qtd, hc->halt_status);
		return;
	}

	/* Read the HCINTn register to determine the cause for the halt. */
	if(!fiq_split_enable)
	{
		hcint.d32 = DWC_READ_REG32(&hc_regs->hcint);
		hcintmsk.d32 = DWC_READ_REG32(&hc_regs->hcintmsk);
	}

	if (hcint.b.xfercomp) {
		/** @todo This is here because of a possible hardware bug.  Spec
		 * says that on SPLIT-ISOC OUT transfers in DMA mode that a HALT
		 * interrupt w/ACK bit set should occur, but I only see the
		 * XFERCOMP bit, even with it masked out.  This is a workaround
		 * for that behavior.  Should fix this when hardware is fixed.
		 */
		if (hc->ep_type == DWC_OTG_EP_TYPE_ISOC && !hc->ep_is_in) {
			handle_hc_ack_intr(hcd, hc, hc_regs, qtd);
		}
		handle_hc_xfercomp_intr(hcd, hc, hc_regs, qtd);
	} else if (hcint.b.stall) {
		handle_hc_stall_intr(hcd, hc, hc_regs, qtd);
	} else if (hcint.b.xacterr && !hcd->core_if->dma_desc_enable) {
		if (out_nak_enh) {
			if (hcint.b.nyet || hcint.b.nak || hcint.b.ack) {
				DWC_DEBUGPL(DBG_HCD, "XactErr with NYET/NAK/ACK\n");
				qtd->error_count = 0;
			} else {
				DWC_DEBUGPL(DBG_HCD, "XactErr without NYET/NAK/ACK\n");
			}
		}

		/*
		 * Must handle xacterr before nak or ack. Could get a xacterr
		 * at the same time as either of these on a BULK/CONTROL OUT
		 * that started with a PING. The xacterr takes precedence.
		 */
		handle_hc_xacterr_intr(hcd, hc, hc_regs, qtd);
	} else if (hcint.b.xcs_xact && hcd->core_if->dma_desc_enable) {
		handle_hc_xacterr_intr(hcd, hc, hc_regs, qtd);
	} else if (hcint.b.ahberr && hcd->core_if->dma_desc_enable) {
		handle_hc_ahberr_intr(hcd, hc, hc_regs, qtd);
	} else if (hcint.b.bblerr) {
		handle_hc_babble_intr(hcd, hc, hc_regs, qtd);
	} else if (hcint.b.frmovrun) {
		handle_hc_frmovrun_intr(hcd, hc, hc_regs, qtd);
	} else if (hcint.b.datatglerr) {
		handle_hc_datatglerr_intr(hcd, hc, hc_regs, qtd);
	} else if (!out_nak_enh) {
		if (hcint.b.nyet) {
			/*
			 * Must handle nyet before nak or ack. Could get a nyet at the
			 * same time as either of those on a BULK/CONTROL OUT that
			 * started with a PING. The nyet takes precedence.
			 */
			handle_hc_nyet_intr(hcd, hc, hc_regs, qtd);
		} else if (hcint.b.nak && !hcintmsk.b.nak) {
			/*
			 * If nak is not masked, it's because a non-split IN transfer
			 * is in an error state. In that case, the nak is handled by
			 * the nak interrupt handler, not here. Handle nak here for
			 * BULK/CONTROL OUT transfers, which halt on a NAK to allow
			 * rewinding the buffer pointer.
			 */
			handle_hc_nak_intr(hcd, hc, hc_regs, qtd);
		} else if (hcint.b.ack && !hcintmsk.b.ack) {
			/*
			 * If ack is not masked, it's because a non-split IN transfer
			 * is in an error state. In that case, the ack is handled by
			 * the ack interrupt handler, not here. Handle ack here for
			 * split transfers. Start splits halt on ACK.
			 */
			handle_hc_ack_intr(hcd, hc, hc_regs, qtd);
		} else {
			if (hc->ep_type == DWC_OTG_EP_TYPE_INTR ||
			    hc->ep_type == DWC_OTG_EP_TYPE_ISOC) {
				/*
				 * A periodic transfer halted with no other channel
				 * interrupts set. Assume it was halted by the core
				 * because it could not be completed in its scheduled
				 * (micro)frame.
				 */
#ifdef DEBUG
				DWC_PRINTF
				    ("%s: Halt channel %d (assume incomplete periodic transfer)\n",
				     __func__, hc->hc_num);
#endif
				halt_channel(hcd, hc, qtd,
					     DWC_OTG_HC_XFER_PERIODIC_INCOMPLETE);
			} else {
				DWC_ERROR
				    ("%s: Channel %d, DMA Mode -- ChHltd set, but reason "
				     "for halting is unknown, hcint 0x%08x, intsts 0x%08x\n",
				     __func__, hc->hc_num, hcint.d32,
				     DWC_READ_REG32(&hcd->
						    core_if->core_global_regs->
						    gintsts));
				/* Failthrough: use 3-strikes rule */
				qtd->error_count++;
				dwc_otg_hcd_save_data_toggle(hc, hc_regs, qtd);
				update_urb_state_xfer_intr(hc, hc_regs,
					   qtd->urb, qtd, DWC_OTG_HC_XFER_XACT_ERR);
				halt_channel(hcd, hc, qtd, DWC_OTG_HC_XFER_XACT_ERR);
			}

		}
	} else {
		DWC_PRINTF("NYET/NAK/ACK/other in non-error case, 0x%08x\n",
			   hcint.d32);
		/* Failthrough: use 3-strikes rule */
		qtd->error_count++;
		dwc_otg_hcd_save_data_toggle(hc, hc_regs, qtd);
		update_urb_state_xfer_intr(hc, hc_regs,
			   qtd->urb, qtd, DWC_OTG_HC_XFER_XACT_ERR);
		halt_channel(hcd, hc, qtd, DWC_OTG_HC_XFER_XACT_ERR);
	}
}

/**
 * Handles a host channel Channel Halted interrupt.
 *
 * In slave mode, this handler is called only when the driver specifically
 * requests a halt. This occurs during handling other host channel interrupts
 * (e.g. nak, xacterr, stall, nyet, etc.).
 *
 * In DMA mode, this is the interrupt that occurs when the core has finished
 * processing a transfer on a channel. Other host channel interrupts (except
 * ahberr) are disabled in DMA mode.
 */
static int32_t handle_hc_chhltd_intr(dwc_otg_hcd_t * hcd,
				     dwc_hc_t * hc,
				     dwc_otg_hc_regs_t * hc_regs,
				     dwc_otg_qtd_t * qtd,
				     hcint_data_t hcint,
				     hcintmsk_data_t hcintmsk)
{
	DWC_DEBUGPL(DBG_HCDI, "--Host Channel %d Interrupt: "
		    "Channel Halted--\n", hc->hc_num);

	if (hcd->core_if->dma_enable) {
		handle_hc_chhltd_intr_dma(hcd, hc, hc_regs, qtd, hcint, hcintmsk);
	} else {
#ifdef DEBUG
		if (!halt_status_ok(hcd, hc, hc_regs, qtd)) {
			return 1;
		}
#endif
		release_channel(hcd, hc, qtd, hc->halt_status);
	}

	return 1;
}

/** Handles interrupt for a specific Host Channel */
int32_t dwc_otg_hcd_handle_hc_n_intr(dwc_otg_hcd_t * dwc_otg_hcd, uint32_t num)
{
	int retval = 0;
	hcint_data_t hcint, hcint_orig;
	hcintmsk_data_t hcintmsk;
	dwc_hc_t *hc;
	dwc_otg_hc_regs_t *hc_regs;
	dwc_otg_qtd_t *qtd;

	DWC_DEBUGPL(DBG_HCDV, "--Host Channel Interrupt--, Channel %d\n", num);

	hc = dwc_otg_hcd->hc_ptr_array[num];
	hc_regs = dwc_otg_hcd->core_if->host_if->hc_regs[num];
	if(hc->halt_status == DWC_OTG_HC_XFER_URB_DEQUEUE) {
		/* We are responding to a channel disable. Driver
		 * state is cleared - our qtd has gone away.
		 */
		release_channel(dwc_otg_hcd, hc, NULL, hc->halt_status);
		return 1;
	}
	qtd = DWC_CIRCLEQ_FIRST(&hc->qh->qtd_list);

	hcint.d32 = DWC_READ_REG32(&hc_regs->hcint);
	hcint_orig = hcint;
	hcintmsk.d32 = DWC_READ_REG32(&hc_regs->hcintmsk);
	DWC_DEBUGPL(DBG_HCDV,
		    "  hcint 0x%08x, hcintmsk 0x%08x, hcint&hcintmsk 0x%08x\n",
		    hcint.d32, hcintmsk.d32, (hcint.d32 & hcintmsk.d32));
	hcint.d32 = hcint.d32 & hcintmsk.d32;

	if(fiq_split_enable)
	{
		// replace with the saved interrupts from the fiq handler
		local_fiq_disable();
		hcint_orig.d32 = hcint_saved[num].d32;
		hcint.d32 = hcint_orig.d32 & hcintmsk_saved[num].d32;
		hcint_saved[num].d32 = 0;
		local_fiq_enable();
	}

	if (!dwc_otg_hcd->core_if->dma_enable) {
		if (hcint.b.chhltd && hcint.d32 != 0x2) {
			hcint.b.chhltd = 0;
		}
	}

	if (hcint.b.xfercomp) {
		retval |=
		    handle_hc_xfercomp_intr(dwc_otg_hcd, hc, hc_regs, qtd);
		/*
		 * If NYET occurred at same time as Xfer Complete, the NYET is
		 * handled by the Xfer Complete interrupt handler. Don't want
		 * to call the NYET interrupt handler in this case.
		 */
		hcint.b.nyet = 0;
	}
	if (hcint.b.chhltd) {
		retval |= handle_hc_chhltd_intr(dwc_otg_hcd, hc, hc_regs, qtd, hcint_orig, hcintmsk_saved[num]);
	}
	if (hcint.b.ahberr) {
		retval |= handle_hc_ahberr_intr(dwc_otg_hcd, hc, hc_regs, qtd);
	}
	if (hcint.b.stall) {
		retval |= handle_hc_stall_intr(dwc_otg_hcd, hc, hc_regs, qtd);
	}
	if (hcint.b.nak) {
		retval |= handle_hc_nak_intr(dwc_otg_hcd, hc, hc_regs, qtd);
	}
	if (hcint.b.ack) {
		if(!hcint.b.chhltd)
			retval |= handle_hc_ack_intr(dwc_otg_hcd, hc, hc_regs, qtd);
	}
	if (hcint.b.nyet) {
		retval |= handle_hc_nyet_intr(dwc_otg_hcd, hc, hc_regs, qtd);
	}
	if (hcint.b.xacterr) {
		retval |= handle_hc_xacterr_intr(dwc_otg_hcd, hc, hc_regs, qtd);
	}
	if (hcint.b.bblerr) {
		retval |= handle_hc_babble_intr(dwc_otg_hcd, hc, hc_regs, qtd);
	}
	if (hcint.b.frmovrun) {
		retval |=
		    handle_hc_frmovrun_intr(dwc_otg_hcd, hc, hc_regs, qtd);
	}
	if (hcint.b.datatglerr) {
		retval |=
		    handle_hc_datatglerr_intr(dwc_otg_hcd, hc, hc_regs, qtd);
	}

	return retval;
}
#endif /* DWC_DEVICE_ONLY */
