/*
 * MELFAS MIP4 Touchscreen
 *
 * Copyright (C) 2016 MELFAS Inc.
 *
 *
 * mip4_ts.h
 *
 * Version : 2016.09.06
 */

//Debug mode : Disable for production builds
#if 1	// 0 = Disable, 1 = Enable
#define DEBUG
#endif

//Include
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/sysfs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif
#ifdef CONFIG_ACPI
#include <linux/acpi.h>
#endif
#ifdef CONFIG_REGULATOR
#include <linux/regulator/consumer.h>
#endif
#ifdef CONFIG_ANDROID
#include <linux/wakelock.h>
#endif

//Include platform data
#include <linux/input/melfas_mip4_ts.h>

//Include register map
#include "mip4_reg.h"

//Device info
#ifdef CONFIG_ACPI
#define ACPI_ID			"MLFS0000"
#endif

//Chip info
#define CHIP_NONE		000000
#define CHIP_MMS427	104270
#define CHIP_MMS438	104380
#define CHIP_MMS438S	104381
#define CHIP_MMS449	104490
#define CHIP_MMS458	104580
#define CHIP_MMS500	105000
#define CHIP_MMS600	106000
#define CHIP_MCS6000	260000
#define CHIP_MCS8040L	280401
#define CHIP_MIT200		302000
#define CHIP_MIT300		303000
#define CHIP_MIT400		304000
#define CHIP_MIT401		304010
#define CHIP_MIT410		304100
#define CHIP_MFS10		400100
#define CHIP_MFS10VE	400101

#ifdef CONFIG_TOUCHSCREEN_MELFAS_MIP4
#define CHIP_NAME		"MIP4"
#define CHIP_MODEL		CHIP_NONE
#endif
#ifdef CONFIG_TOUCHSCREEN_MELFAS_MMS427
#define CHIP_NAME		"MMS427"
#define CHIP_MODEL		CHIP_MMS427
#endif
#ifdef CONFIG_TOUCHSCREEN_MELFAS_MMS438
#define CHIP_NAME		"MMS438"
#define CHIP_MODEL		CHIP_MMS438
#endif
#ifdef CONFIG_TOUCHSCREEN_MELFAS_MMS438S
#define CHIP_NAME		"MMS438S"
#define CHIP_MODEL		CHIP_MMS438S
#endif
#ifdef CONFIG_TOUCHSCREEN_MELFAS_MMS449
#define CHIP_NAME		"MMS449"
#define CHIP_MODEL		CHIP_MMS449
#endif
#ifdef CONFIG_TOUCHSCREEN_MELFAS_MMS458
#define CHIP_NAME		"MMS458"
#define CHIP_MODEL		CHIP_MMS458
#endif
#ifdef CONFIG_TOUCHSCREEN_MELFAS_MMS500
#define CHIP_NAME		"MMS500"
#define CHIP_MODEL		CHIP_MMS500
#endif
#ifdef CONFIG_TOUCHSCREEN_MELFAS_MMS600
#define CHIP_NAME		"MMS600"
#define CHIP_MODEL		CHIP_MMS600
#endif
#ifdef CONFIG_TOUCHSCREEN_MELFAS_MFS10
#define CHIP_NAME		"MFS10"
#define CHIP_MODEL		CHIP_MFS10
#endif
#ifdef CONFIG_TOUCHSCREEN_MELFAS_MFS10VE
#define CHIP_NAME		"MFS10VE"
#define CHIP_MODEL		CHIP_MFS10VE
#endif
#ifdef CONFIG_TOUCHSCREEN_MELFAS_MCS6000
#define CHIP_NAME		"MCS6000"
#define CHIP_MODEL		CHIP_MCS6000
#endif
#ifdef CONFIG_TOUCHSCREEN_MELFAS_MCS8040L
#define CHIP_NAME		"MCS8040L"
#define CHIP_MODEL		CHIP_MCS8040L
#endif
#ifdef CONFIG_TOUCHSCREEN_MELFAS_MIT200
#define CHIP_NAME		"MIT200"
#define CHIP_MODEL		CHIP_MIT200
#endif
#ifdef CONFIG_TOUCHSCREEN_MELFAS_MIT300
#define CHIP_NAME		"MIT300"
#define CHIP_MODEL		CHIP_MIT300
#endif
#ifdef CONFIG_TOUCHSCREEN_MELFAS_MIT400
#define CHIP_NAME		"MIT400"
#define CHIP_MODEL		CHIP_MIT400
#endif
#ifdef CONFIG_TOUCHSCREEN_MELFAS_MIT401
#define CHIP_NAME		"MIT401"
#define CHIP_MODEL		CHIP_MIT401
#endif
#ifdef CONFIG_TOUCHSCREEN_MELFAS_MIT410
#define CHIP_NAME		"MIT410"
#define CHIP_MODEL		CHIP_MIT410
#endif

//Config driver
#define USE_INPUT_OPEN_CLOSE		0	// 0 (default) or 1
#define INPUT_REPORT_TYPE			1 	// 0 or 1 (default)
#define I2C_RETRY_COUNT				3	// 2~
#define RESET_ON_I2C_ERROR			1	// 0 or 1 (default)
#define RESET_ON_EVENT_ERROR		0	// 0 (default) or 1
#define ESD_COUNT_FOR_DISABLE		7	// 7~

//Driver features
#define USE_DEV			1	// 0 or 1 (default) : Optional - Required for development
#define USE_SYS			1	// 0 or 1 (default) : Optional - Required for development
#define USE_CMD			0	// 0 (default) or 1 : Optional
#define USE_LPWG		0	// 0 (default) or 1 : Optional

//Module features
#define USE_WAKEUP_GESTURE	0	// 0 (default) or 1

//Input value
#define MAX_FINGER_NUM 			10
#define MAX_KEY_NUM				4
#define INPUT_TOUCH_MAJOR_MIN 	0
#define INPUT_TOUCH_MAJOR_MAX 	255
#define INPUT_TOUCH_MINOR_MIN 	0
#define INPUT_TOUCH_MINOR_MAX 	255
#define INPUT_PRESSURE_MIN 		0
#define INPUT_PRESSURE_MAX 		255

//Firmware update
#define FW_PATH_INTERNAL	"melfas/melfas_mip4_ts.fw"		//path of firmware included in the kernel image (/firmware)
#define FW_PATH_EXTERNAL	"/melfas_mip4_ts.fw"	//path of firmware in external storage

#define FW_MAX_SECT_NUM		4
#define USE_AUTO_FW_UPDATE	0	// 0 (default) or 1
#define FW_UPDATE_DEBUG		0	// 0 (default) or 1
#define FW_UPDATE_SECTION		0	// 0 (default) or 1
#define EXT_FW_FORCE_UPDATE	1	// 0 or 1 (default)

/*
* Firmware update error code
*/
enum fw_update_errno{
	fw_err_file_read = -4,
	fw_err_file_open = -3,
	fw_err_file_type = -2,
	fw_err_download = -1,
	fw_err_none = 0,
	fw_err_uptodate = 1,
};

/*
* Firmware file location
*/
enum fw_bin_source{
	fw_bin_source_kernel = 1,
	fw_bin_source_external = 2,
};

//Command function
#if USE_CMD
#define CMD_LEN 			32
#define CMD_RESULT_LEN 	512
#define CMD_PARAM_NUM 	8
#endif

/*
* Device info structure
*/
struct mip4_ts_info {
	struct i2c_client *client;
	struct i2c_client *client_download;
	struct input_dev *input_dev;
	struct mip4_ts_platform_data *pdata;
	char phys[32];
	dev_t mip4_ts_dev;
	struct class *class;
	struct mutex lock;
	int irq;

	int gpio_intr;
	int gpio_ce;
	struct regulator *regulator_vd33;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_enable;
	struct pinctrl_state *pins_disable;

	bool init;
	bool enabled;
	bool irq_enabled;
	int power;

	char *fw_name;
	char *fw_path_ext;

	u8 product_name[16];
	int max_x;
	int max_y;
	unsigned int ppm_x;
	unsigned int ppm_y;
	u8 node_x;
	u8 node_y;
	u8 node_key;
	u8 fw_version[8];

	u8 event_size;
	int event_format;

	u8 touch_state[MAX_FINGER_NUM];
	u8 wakeup_gesture_code;
	int pressure;
	int pressure_stage;
	int proximity;

	bool key_enable;
	int key_num;
	int key_code[MAX_KEY_NUM];

	u8 gesture_wakeup_mode;
	u8 glove_mode;
	u8 charger_mode;
	u8 cover_mode;

	u8 esd_cnt;
	bool disable_esd;

	u8 *print_buf;
	u8 *debug_buf;
	int *image_buf;

	bool test_busy;
	bool cmd_busy;
	bool dev_busy;

#if USE_CMD
	dev_t cmd_dev_t;
	struct device *cmd_dev;
	struct class *cmd_class;
	struct list_head cmd_list_head;
	u8 cmd_state;
	char cmd[CMD_LEN];
	char cmd_result[CMD_RESULT_LEN];
	int cmd_param[CMD_PARAM_NUM];
	int cmd_buffer_size;
	struct device *key_dev;
#endif

#if USE_DEV
	struct cdev cdev;
	u8 *dev_fs_buf;
#endif
};

/*
* Function declarations
*/
//main
void mip4_ts_reboot(struct mip4_ts_info *info);
int mip4_ts_i2c_read(struct mip4_ts_info *info, char *write_buf, unsigned int write_len, char *read_buf, unsigned int read_len);
int mip4_ts_i2c_write(struct mip4_ts_info *info, char *write_buf, unsigned int write_len);
int mip4_ts_enable(struct mip4_ts_info *info);
int mip4_ts_disable(struct mip4_ts_info *info);
int mip4_ts_get_ready_status(struct mip4_ts_info *info);
int mip4_ts_get_fw_version(struct mip4_ts_info *info, u8 *ver_buf);
int mip4_ts_get_fw_version_u16(struct mip4_ts_info *info, u16 *ver_buf_u16);
int mip4_ts_get_fw_version_from_bin(struct mip4_ts_info *info, u8 *ver_buf);
int mip4_ts_set_power_state(struct mip4_ts_info *info, u8 mode);
int mip4_ts_set_wakeup_gesture_type(struct mip4_ts_info *info, u32 type);
int mip4_ts_disable_esd_alert(struct mip4_ts_info *info);
int mip4_ts_config(struct mip4_ts_info *info);
int mip4_ts_fw_update_from_kernel(struct mip4_ts_info *info);
int mip4_ts_fw_update_from_storage(struct mip4_ts_info *info, char *path, bool force);
int mip4_ts_suspend(struct device *dev);
int mip4_ts_resume(struct device *dev);

//mod
int mip4_ts_config_regulator(struct mip4_ts_info *info);
int mip4_ts_regulator_control(struct mip4_ts_info *info, int enable);
int mip4_ts_power_on(struct mip4_ts_info *info);
int mip4_ts_power_off(struct mip4_ts_info *info);
void mip4_ts_clear_input(struct mip4_ts_info *info);
int mip4_ts_gesture_wakeup_event_handler(struct mip4_ts_info *info, int gesture_code);
void mip4_ts_input_event_handler(struct mip4_ts_info *info, u8 sz, u8 *buf);
#ifdef CONFIG_OF
int mip4_ts_parse_devicetree(struct device *dev, struct mip4_ts_info *info);
#endif
void mip4_ts_config_input(struct mip4_ts_info *info);

//firmware
#if (CHIP_MODEL != CHIP_NONE)
#if ((CHIP_MODEL == CHIP_MFS10) || (CHIP_MODEL == CHIP_MFS10VE))
int mip4_ts_flash_cal_table(struct mip4_ts_info *info);
#endif
int mip4_ts_flash_fw(struct mip4_ts_info *info, const u8 *fw_data, size_t fw_size, bool force, bool section);
int mip4_ts_bin_fw_version(struct mip4_ts_info *info, const u8 *fw_data, size_t fw_size, u8 *ver_buf);
#endif

//dev
#if USE_DEV
int mip4_ts_dev_create(struct mip4_ts_info *info);
int mip4_ts_get_log(struct mip4_ts_info *info);
#endif
#if (USE_SYS || USE_CMD)
int mip4_ts_run_test(struct mip4_ts_info *info, u8 test_type);
int mip4_ts_get_image(struct mip4_ts_info *info, u8 image_type);
#endif
#if USE_SYS
int mip4_ts_sysfs_create(struct mip4_ts_info *info);
void mip4_ts_sysfs_remove(struct mip4_ts_info *info);
static const struct attribute_group mip4_ts_test_attr_group;
#endif

//cmd
#if USE_CMD
int mip4_ts_sysfs_cmd_create(struct mip4_ts_info *info);
void mip4_ts_sysfs_cmd_remove(struct mip4_ts_info *info);
static const struct attribute_group mip4_ts_cmd_attr_group;
#endif

//lpwg
#if USE_LPWG
int mip4_ts_lpwg_event_handler(struct mip4_ts_info *info, u8 *rbuf, u8 size);
#endif

