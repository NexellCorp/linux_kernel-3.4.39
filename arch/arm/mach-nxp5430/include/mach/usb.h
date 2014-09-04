#ifndef __PLAT_USB_HW_H__
#define __PLAT_USB_HW_H__

/*
 *
 * 	USB PHY
 *
 */
enum nxp_usb_phy_type {
	NXP_USB_PHY_OTG,
	NXP_USB_PHY_OHCI,
	NXP_USB_PHY_EHCI,
	NXP_USB_PHY_HSIC,
};

extern int nxp_soc_usb_phy_init(struct platform_device *pdev, int type);
extern int nxp_soc_usb_phy_exit(struct platform_device *pdev, int type);

#endif /* __PLAT_USB_HW_H__ */
