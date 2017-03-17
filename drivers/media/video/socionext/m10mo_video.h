
#ifndef MEDIA_M10MO_H
#define MEDIA_M10MO_H

#define M10MO_DRIVER_NAME		"m10mo"

#define JC_ISP_TIMEOUT			5000
#define JC_LOAD_FW_MAIN			1
#define JC_DUMP_FW				1
#define JC_CHECK_FW				1
#define JC_MEM_READ				1
#define ISP_DEBUG_LOG			1
#define JC_SPI_WRITE
#define FW_WRITE_SIZE			262144 /*2097152*/
#define VERIFY_CHIP_ERASED		32
#define ISP_FROM_ERASED			1

struct m10mo_platform_data {
	unsigned int default_width;
	unsigned int default_height;
	unsigned int pixelformat;
	int freq;	/* MCLK in Hz */

	/* This SoC supports Parallel & CSI-2 */
	int is_mipi;

	int (*flash_onoff)(int);
	int (*af_assist_onoff)(int);
	int (*torch_onoff)(int);

	/* ISP interrupt */
	int (*config_isp_irq)(void);
	int irq;

	int (*set_power)(struct device *dev, int on);
	int	gpio_rst;
	bool	enable_rst;

};

extern int m10mo_init4(struct v4l2_subdev *sd);

/**
* struct m10mo_platform_data - platform data for M5MOLS driver
* @irq:   GPIO getting the irq pin of M10MO
* @gpio_rst:  GPIO driving the reset pin of M10MO
 * @enable_rst:	the pin state when reset pin is enabled
* @set_power:	an additional callback to a board setup code
 *		to be called after enabling and before disabling
*		the sensor device supply regulators
 */

#endif	/* MEDIA_M10MO_H */
