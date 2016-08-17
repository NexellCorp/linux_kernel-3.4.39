#include <linux/io.h>

#include "platform.h"
#include "fc8080.h"
#include "fci_oal.h"

#include <mach/platform.h>
#include <mach/devices.h>
#include <mach/soc.h>

void platform_hw_setting(void)
{
	/* FCI POWER On Sequence */
	/*	3.3V IO power enable
		1 msec wait
		1.2V Core Power enable
		1 msec wait
	*/

	// Power enable
	nxp_soc_gpio_set_io_dir(CFG_IO_DMB_1P2V_ON, 1);			// Output mode
	nxp_soc_gpio_set_io_pull_enb(CFG_IO_DMB_1P2V_ON, 0);	// Pull disable
}

void platform_hw_init(u32 power_pin, u32 reset_pin)
{
	nxp_soc_gpio_set_out_value(CFG_IO_DMB_1P2V_ON, CTRUE);
	ms_wait(1);
}

void platform_hw_deinit(u32 power_pin)
{
	nxp_soc_gpio_set_out_value(CFG_IO_DMB_1P2V_ON, CFALSE);
	ms_wait(1);
}

void platform_hw_reset(u32 reset_pin)
{
	
}
