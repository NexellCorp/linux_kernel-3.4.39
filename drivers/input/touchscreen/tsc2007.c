/*
 * drivers/input/touchscreen/tsc2007.c
 *
 * Copyright (c) 2008 MtekVision Co., Ltd.
 *	Kwangwoo Lee <kwlee@mtekvision.com>
 *
 * Using code from:
 *  - ads7846.c
 *	Copyright (c) 2005 David Brownell
 *	Copyright (c) 2006 Nokia Corporation
 *  - corgi_ts.c
 *	Copyright (C) 2004-2005 Richard Purdie
 *  - omap_ts.[hc], ads7846.h, ts_osk.c
 *	Copyright (C) 2002 MontaVista Software
 *	Copyright (C) 2004 Texas Instruments
 *	Copyright (C) 2005 Dirk Behme
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/i2c/tsc2007.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/irq.h>

#include <mach/platform.h>
#include <mach/devices.h>
#include <mach/soc.h>


#define TSC2007_MEASURE_TEMP0	(0x0 << 4)
#define TSC2007_MEASURE_AUX		(0x2 << 4)
#define TSC2007_MEASURE_TEMP1	(0x4 << 4)
#define TSC2007_ACTIVATE_XN		(0x8 << 4)
#define TSC2007_ACTIVATE_YN		(0x9 << 4)
#define TSC2007_ACTIVATE_YP_XN		(0xa << 4)
#define TSC2007_SETUP			(0xb << 4)
#define TSC2007_MEASURE_X		(0xc << 4)
#define TSC2007_MEASURE_Y		(0xd << 4)
#define TSC2007_MEASURE_Z1		(0xe << 4)
#define TSC2007_MEASURE_Z2		(0xf << 4)

#define TSC2007_POWER_OFF_IRQ_EN	(0x0 << 2)
#define TSC2007_ADC_ON_IRQ_DIS0		(0x1 << 2)
#define TSC2007_ADC_OFF_IRQ_EN		(0x2 << 2)
#define TSC2007_ADC_ON_IRQ_DIS1		(0x3 << 2)

#define TSC2007_12BIT			(0x0 << 1)
#define TSC2007_8BIT			(0x1 << 1)

#define	MAX_12BIT			((1 << 12) - 1)

#define ADC_ON_12BIT	(TSC2007_12BIT | TSC2007_ADC_ON_IRQ_DIS0)

#define READ_Y		(ADC_ON_12BIT | TSC2007_MEASURE_Y)
#define READ_Z1		(ADC_ON_12BIT | TSC2007_MEASURE_Z1)
#define READ_Z2		(ADC_ON_12BIT | TSC2007_MEASURE_Z2)
#define READ_X		(ADC_ON_12BIT | TSC2007_MEASURE_X)
#define PWRDOWN		(TSC2007_12BIT | TSC2007_POWER_OFF_IRQ_EN)

struct ts_event {
	u16	x;
	u16	y;
	u16	z1, z2;
};

struct tsc2007 {
	struct input_dev	*input;
	char			phys[32];

	struct i2c_client	*client;

	u16			model;
	u16			x_plate_ohms;
	u16			max_rt;
	unsigned long		poll_delay;
	unsigned long		poll_period;

	int			irq;

	wait_queue_head_t	wait;
	bool			stopped;

	int			(*get_pendown_state)(void);
	void			(*clear_penirq)(void);

	long	xp_old;	
	long	yp_old;
	
};


//Nexell allan.park: 20150520
int elements[20] = {24, 47, 991, 43, 977, 565, 39, 567, 511, 295, 
					50, 50, 974, 50, 974, 550, 50, 550, 512, 300};

//int elements[20] = {3677, 3294, 409, 3294, 398,665, 3677, 665, 2038, 1970,
//					50, 50, 974, 50, 974, 550, 50, 550, 512, 300};

typedef struct {
    int x[5], xfb[5];
    int y[5], yfb[5];
    int a[7];
} calibration;

calibration cal; //initial the default value
static struct kobject   * g_ts_kobj = NULL;

// finedigital:jhhong:20150507
bool gbFirst = true;
#ifdef CFG_GPIO_LCD_UPDOWN
bool gbLCD_updown = false;
#endif


//Nexell allan.park: 20150520
int perform_calibration(calibration *cal) {
    int j;
    float n, x, y, x2, y2, xy, z, zx, zy;
    float det, a, b, c, e, f, i;
    float scaling = 65536.0;

// Get sums for matrix
    n = x = y = x2 = y2 = xy = 0;
    for(j=0;j<5;j++) {
        n += 1.0;
        x += (float)cal->x[j];
        y += (float)cal->y[j];
        x2 += (float)(cal->x[j]*cal->x[j]);
        y2 += (float)(cal->y[j]*cal->y[j]);
        xy += (float)(cal->x[j]*cal->y[j]);
    }

// Get determinant of matrix -- check if determinant is too small
    det = n*(x2*y2 - xy*xy) + x*(xy*y - x*y2) + y*(x*xy - y*x2);
    if(det < 0.1 && det > -0.1) {
        printk("ts_calibrate: determinant is too small -- %f\n",det);
        return 0;
    }

// Get elements of inverse matrix
    a = (x2*y2 - xy*xy)/det;
    b = (xy*y - x*y2)/det;
    c = (x*xy - y*x2)/det;
    e = (n*y2 - y*y)/det;
    f = (x*y - n*xy)/det;
    i = (n*x2 - x*x)/det;

// Get sums for x calibration
    z = zx = zy = 0;
    for(j=0;j<5;j++) {
        z += (float)cal->xfb[j];
        zx += (float)(cal->xfb[j]*cal->x[j]);
        zy += (float)(cal->xfb[j]*cal->y[j]);
    }

// Now multiply out to get the calibration for framebuffer x coord
    cal->a[2] = (int)((a*z + b*zx + c*zy)*(scaling));
    cal->a[0] = (int)((b*z + e*zx + f*zy)*(scaling));
    cal->a[1] = (int)((c*z + f*zx + i*zy)*(scaling));

    //printk("%lf %lf %lf \n",(a*z + b*zx + c*zy), (b*z + e*zx + f*zy), (c*z + f*zx + i*zy));

// Get sums for y calibration
    z = zx = zy = 0;
    for(j=0;j<5;j++) {
        z += (float)cal->yfb[j];
        zx += (float)(cal->yfb[j]*cal->x[j]);
        zy += (float)(cal->yfb[j]*cal->y[j]);
    }
// Now multiply out to get the calibration for framebuffer y coord
    cal->a[5] = (int)((a*z + b*zx + c*zy)*(scaling));
    cal->a[3] = (int)((b*z + e*zx + f*zy)*(scaling));
    cal->a[4] = (int)((c*z + f*zx + i*zy)*(scaling));

    //printk( "%lf %lf %lf\n",(a*z + b*zx + c*zy), (b*z + e*zx + f*zy), (c*z + f*zx + i*zy));

// If we got here, we're OK, so assign scaling to a[6] and return
    cal->a[6] = (int)scaling;

	#if 1
	printk("\n CAL Factor = %d %d %d %d %d %d %d \n\n",
			cal->a[0], cal->a[1], cal->a[2],
			cal->a[3], cal->a[4], cal->a[5], cal->a[6]
			);
	#endif
    return 1;
}

static void set_sample(calibration *cal)
{
	int index=0, elements_index=0;
	
	for(index=0, elements_index=0; index < 5; index++, elements_index+=2) {
		cal->x[index] = elements[elements_index];
		cal->y[index] = elements[elements_index+1];
		cal->xfb[index]	= elements[elements_index+10];
		cal->yfb[index]	= elements[elements_index+11];
	}
	#if 1	
	for(index=0; index < 5; index++, elements_index+=2) {
		printk("x=%d, y=%d xfb=%d, yfb=%d \n",
			cal->x[index], cal->y[index], cal->xfb[index], cal->yfb[index]);
	}
	#endif
}
static ssize_t ts_show_calibration(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    char *s = buf;

    printk("%s: Enter ++ \n", __FUNCTION__);

    s += sprintf(s, "%d %d %d %d %d %d %d\n",
        cal.a[0], cal.a[1],
        cal.a[2], cal.a[3],
        cal.a[4], cal.a[5],
        cal.a[6]);

    if (s != buf)
        *(s-1) = '\n';

    return (s - buf);
}

static ssize_t ts_store_calibration(struct kobject *kobj,
             struct kobj_attribute *attr, const char *buf, size_t n)
{
	
    printk("\n %s: Enter ++ \n", __FUNCTION__);

    if (NULL == buf)
        return n;

    sscanf(buf,"%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
		        &elements[0], &elements[1],
		        &elements[2], &elements[3],
		        &elements[4], &elements[5],
		        &elements[6], &elements[7],
		        &elements[8], &elements[9],
		        &elements[10], &elements[11],
		        &elements[12], &elements[13],
		        &elements[14], &elements[15],
		        &elements[16], &elements[17],
		        &elements[18], &elements[19]);

	set_sample(&cal); //init the default value
	perform_calibration(&cal);

    return n;
}


static struct kobj_attribute attr_calibration = {
    .attr   = {
        .name = __stringify(calibration),
        .mode = 0666,
    },
    .show   = ts_show_calibration,
    .store  = ts_store_calibration,
};


static struct attribute *ts_attrs[] = {
    &attr_calibration.attr,
    NULL
};

static struct attribute_group ts_attr_group = {
    .attrs = ts_attrs,
};

#ifdef CFG_GPIO_LCD_UPDOWN
static irqreturn_t bf700_lcd_updown_irq(int irq, void *data)
{
	//printk("LCD_UPDOWN_STATE = 0x%x \n", nxp_soc_gpio_get_in_value(CFG_GPIO_LCD_UPDOWN));
	gbLCD_updown = nxp_soc_gpio_get_in_value(CFG_GPIO_LCD_UPDOWN);
	printk("LCD_UPDOWN_STATE = 0x%x \n", gbLCD_updown);

    return IRQ_HANDLED;
}


static int bf700_lcd_updown_detect_init(void)
{
	int ret;
	nxp_soc_gpio_set_int_enable(CFG_GPIO_LCD_UPDOWN, 0);

	ret = request_irq(gpio_to_irq(CFG_GPIO_LCD_UPDOWN), bf700_lcd_updown_irq,
            IRQ_TYPE_EDGE_BOTH, "lcd_ud", 0);

    if (ret)
    {
        printk("request_irq ID failed: %d\n", ret);
    }
    else
    {
        nxp_soc_gpio_set_int_enable(CFG_GPIO_LCD_UPDOWN, 1);
        nxp_soc_gpio_clr_int_pend(CFG_GPIO_LCD_UPDOWN);
    }
	
}
#endif



//finedigital:jhhong:20150507
static void tsc2007_Reset(void)
{
	//finedigital:jhhong:20150507
	printk("[TOUCH] tsc2007 Reset \n");
	mdelay(100);
	NX_GPIO_SetOutputValue(PAD_GET_GROUP(CFG_IO_TOUCH_PWR_EN), PAD_GET_BITNO(CFG_IO_TOUCH_PWR_EN), CFALSE);
	mdelay(320);
	NX_GPIO_SetOutputValue(PAD_GET_GROUP(CFG_IO_TOUCH_PWR_EN), PAD_GET_BITNO(CFG_IO_TOUCH_PWR_EN), CTRUE);
}

static inline int tsc2007_xfer(struct tsc2007 *tsc, u8 cmd)
{
	s32 data;
	u16 val;

	data = i2c_smbus_read_word_data(tsc->client, cmd);
	if (data < 0) {
		dev_err(&tsc->client->dev, "i2c io error: %d\n", data);
		return data;
	}

	/* The protocol and raw data format from i2c interface:
	 * S Addr Wr [A] Comm [A] S Addr Rd [A] [DataLow] A [DataHigh] NA P
	 * Where DataLow has [D11-D4], DataHigh has [D3-D0 << 4 | Dummy 4bit].
	 */
	val = swab16(data) >> 4;

	dev_dbg(&tsc->client->dev, "data: 0x%x, val: 0x%x\n", data, val);

	return val;
}

static void tsc2007_read_values(struct tsc2007 *tsc, struct ts_event *tc)
{
	/* y- still on; turn on only y+ (and ADC) */
	tc->y = tsc2007_xfer(tsc, READ_Y);

	/* turn y- off, x+ on, then leave in lowpower */
	tc->x = tsc2007_xfer(tsc, READ_X);

	/* turn y+ off, x- on; we'll use formula #1 */
	tc->z1 = tsc2007_xfer(tsc, READ_Z1);
	tc->z2 = tsc2007_xfer(tsc, READ_Z2);

	/* Prepare for next touch reading - power down ADC, enable PENIRQ */
	tsc2007_xfer(tsc, PWRDOWN);
}

static u32 tsc2007_calculate_pressure(struct tsc2007 *tsc, struct ts_event *tc)
{
	u32 rt = 0;

	/* range filtering */
	if (tc->x == MAX_12BIT)
		tc->x = 0;

	if (likely(tc->x && tc->z1)) {
		/* compute touch pressure resistance using equation #1 */
#if 1
		rt = tc->z2 - tc->z1;
		rt *= tc->x;
		rt *= tsc->x_plate_ohms;
		rt /= tc->z1;
		rt = (rt + 2047) >> 12;
#else
		rt = (float)(tsc->x_plate_ohms) * (float)((float)tc->x/4096) * ((float)((float)tc->z2/(float)tc->z1) - 1);
#endif
	}

	return rt;
}

static bool tsc2007_is_pen_down(struct tsc2007 *ts)
{
	/*
	 * NOTE: We can't rely on the pressure to determine the pen down
	 * state, even though this controller has a pressure sensor.
	 * The pressure value can fluctuate for quite a while after
	 * lifting the pen and in some cases may not even settle at the
	 * expected value.
	 *
	 * The only safe way to check for the pen up condition is in the
	 * work function by reading the pen signal state (it's a GPIO
	 * and IRQ). Unfortunately such callback is not always available,
	 * in that case we assume that the pen is down and expect caller
	 * to fall back on the pressure reading.
	 */
	bool val, prev_val;
	int i;
#define DEBOUNCE_COUNT	5

	do {
		prev_val = nxp_soc_gpio_get_in_value(CFG_IO_TOUCH_PENDOWN_DETECT);	
		for (i = 0 ; i < DEBOUNCE_COUNT; i++)
		{
			val = nxp_soc_gpio_get_in_value(CFG_IO_TOUCH_PENDOWN_DETECT);	
			if (prev_val != val)
				break;
			mdelay(4);
		}	
	} while (i < DEBOUNCE_COUNT);

	return ((val == 0) ? 1 : 0);
}

// gylee start
 #define ADCVAL_X_MIN		280 //150 
 #define ADCVAL_X_MAX		3783 //3950 
 #define ADCVAL_Y_MIN		500 //280 
 #define ADCVAL_Y_MAX		3520 //3850 

 #define LCD_SCREEN_X_PIXEL	CFG_DISP_PRI_RESOL_WIDTH 
 #define LCD_SCREEN_Y_PIXEL	CFG_DISP_PRI_RESOL_HEIGHT 
// gylee end 

static irqreturn_t tsc2007_soft_irq(int irq, void *handle)
{
#if 0 
	struct tsc2007 *ts = handle;
	struct input_dev *input = ts->input;
	struct ts_event tc;
	u32 rt;
	int x, y;
	int val;

	while (!ts->stopped && tsc2007_is_pen_down(ts)) {

		/* pen is down, continue with the measurement */
		val = nxp_soc_gpio_get_in_value(CFG_IO_TOUCH_PENDOWN_DETECT);
		if(!val)
			tsc2007_read_values(ts, &tc);
		rt = tsc2007_calculate_pressure(ts, &tc);
		
		if (rt == 0 && !ts->get_pendown_state) {
			/*
			 * If pressure reported is 0 and we don't have
			 * callback to check pendown state, we have to
			 * assume that pen was lifted up.
			 */
			break;
		}

		if ((rt <= ts->max_rt)&&(!val)) {
			dev_dbg(&ts->client->dev,
				"DOWN point(%4d,%4d), pressure (%4u)\n",
				tc.x, tc.y, rt);
			
			printk(" %d  %d  %d  %d  %d \n", tc.x, tc.y, val, rt, ts->max_rt);

			//if( tc.x > ADCVAL_X_MAX ) tc.x = ADCVAL_X_MAX;
			//if( tc.x < ADCVAL_X_MIN ) tc.x = ADCVAL_X_MIN;
			//if( tc.y > ADCVAL_Y_MAX ) tc.y = ADCVAL_Y_MAX;
			//if( tc.y < ADCVAL_Y_MIN ) tc.y = ADCVAL_Y_MIN;
			
			x = tc.x-ADCVAL_X_MIN;
			if( x < 0 ) x = 1;
			y = tc.y-ADCVAL_Y_MIN;
			if( y < 0 ) y = 1;
			
			 	
			tc.x = LCD_SCREEN_X_PIXEL*(x)/(ADCVAL_X_MAX-ADCVAL_X_MIN); 
	 		tc.y = LCD_SCREEN_Y_PIXEL*(y)/(ADCVAL_Y_MAX-ADCVAL_Y_MIN);
			tc.x = LCD_SCREEN_X_PIXEL - tc.x;
			tc.y = LCD_SCREEN_Y_PIXEL - tc.y;

			//printk(" %d %d \n", tc.x, tc.y);
			
			input_report_key(input, BTN_TOUCH, 1);
			input_report_abs(input, ABS_X, tc.x);
			input_report_abs(input, ABS_Y, tc.y);
			input_report_abs(input, ABS_PRESSURE, rt);

			input_sync(input);

			ts->xp_old = tc.x;			
			ts->yp_old = tc.y;

		} else {
			/*
			 * Sample found inconsistent by debouncing or pressure is
			 * beyond the maximum. Don't report it to user space,
			 * repeat at least once more the measurement.
			 */
			dev_dbg(&ts->client->dev, "ignored pressure %d\n", rt);
			ts->xp_old = 0;			
			ts->yp_old = 0;
		}

		wait_event_timeout(ts->wait, ts->stopped,
				   msecs_to_jiffies(ts->poll_period));
	}

	printk("UP\n");
	dev_dbg(&ts->client->dev, "UP\n");

	input_report_key(input, BTN_TOUCH, 0);
	input_report_abs(input, ABS_PRESSURE, 0);
	input_sync(input);

	if (ts->clear_penirq)
		ts->clear_penirq();

	return IRQ_HANDLED;
#else
	struct tsc2007 *ts = handle;
	struct input_dev *input = ts->input;
	struct ts_event tc;
	u32 rt;
	int x, y;
	//int val;

	while (tsc2007_is_pen_down(ts)) {

		/* pen is down, continue with the measurement */
	
		tsc2007_read_values(ts, &tc);
		rt = tsc2007_calculate_pressure(ts, &tc);
		

		if (rt <= ts->max_rt) {
			dev_dbg(&ts->client->dev,
				"DOWN point(%4d,%4d), pressure (%4u)\n",
				tc.x, tc.y, rt);
			//printk("1: %d  %d  %d  %d  %d \n", tc.x, tc.y, val, rt, ts->max_rt);
			//printk("1: %X  %X  %X  %X  %X \n", tc.x, tc.y, val, rt, ts->max_rt);
		//#if 0
			x = tc.x-ADCVAL_X_MIN;
			if( x < 0 ) x = 1;
			y = tc.y-ADCVAL_Y_MIN;
			if( y < 0 ) y = 1;
			
			 	
			tc.x = LCD_SCREEN_X_PIXEL*(x)/(ADCVAL_X_MAX-ADCVAL_X_MIN); 
	 		tc.y = LCD_SCREEN_Y_PIXEL*(y)/(ADCVAL_Y_MAX-ADCVAL_Y_MIN);
			tc.x = LCD_SCREEN_X_PIXEL - tc.x;
			tc.y = LCD_SCREEN_Y_PIXEL - tc.y;

			//printk("2: %d  %d  %d  %d  %d \n", tc.x, tc.y, val, rt, ts->max_rt);
		//#else
			/*
			printk("\n CAL Factor = %d %d %d %d %d %d %d \n\n",
			cal.a[0], cal.a[1], cal.a[2],
			cal.a[3], cal.a[4], cal.a[5], cal.a[6]);
			*/
			//apply the ts calibrate data
			x = tc.x;
			y = tc.y;
			tc.x = (cal.a[2] + cal.a[0]*(int) x + cal.a[1]*(int) y) / cal.a[6];
			tc.y = (cal.a[5] + cal.a[3]*(int) x + cal.a[4]*(int) y) / cal.a[6];

#ifdef CFG_GPIO_LCD_UPDOWN
			if(gbLCD_updown) { //LCD DOWN
				tc.x = LCD_SCREEN_X_PIXEL - tc.x;
				tc.y = LCD_SCREEN_Y_PIXEL - tc.y;
			}
#endif
				
		//#endif
			
			//printk("3: %d  %d  %d  %d  %d \n", tc.x, tc.y, val, rt, ts->max_rt);
			//printk("3: %X  %X  %X  %X  %X \n", tc.x, tc.y, val, rt, ts->max_rt);

			if(tc.x != ts->xp_old || tc.y != ts->yp_old) {
				input_report_key(input, BTN_TOUCH, 1);
				input_report_abs(input, ABS_X, tc.x);
				input_report_abs(input, ABS_Y, tc.y);
				input_report_abs(input, ABS_PRESSURE, 1);

				input_sync(input);
			}

			ts->xp_old = tc.x;			
			ts->yp_old = tc.y;

		} else {
			/*
			 * Sample found inconsistent by debouncing or pressure is
			 * beyond the maximum. Don't report it to user space,
			 * repeat at least once more the measurement.
			 */
			dev_dbg(&ts->client->dev, "ignored pressure %d\n", rt);
			ts->xp_old = 0;			
			ts->yp_old = 0;
		}

		wait_event_timeout(ts->wait, ts->stopped,
				   msecs_to_jiffies(ts->poll_period));
	}

	dev_dbg(&ts->client->dev, "UP\n");
	//printk("UP\n");

	input_report_key(input, BTN_TOUCH, 0);
	input_report_abs(input, ABS_PRESSURE, 0);
	input_sync(input);

	if (ts->clear_penirq)
		ts->clear_penirq();

	return IRQ_HANDLED;

#endif
}

static irqreturn_t tsc2007_hard_irq(int irq, void *handle)
{
#if 0 
	struct tsc2007 *ts = handle;

	if (!ts->get_pendown_state || likely(ts->get_pendown_state()))
		return IRQ_WAKE_THREAD;

	if (ts->clear_penirq)
		ts->clear_penirq();

	return IRQ_HANDLED;
#else 
	struct tsc2007 *ts = handle;
	int val;

	if (ts->stopped == false) {
		val = nxp_soc_gpio_get_in_value(CFG_IO_TOUCH_PENDOWN_DETECT);
		//printk(" TESTESTSESTE ");
		if (val == 0) {
			nxp_soc_gpio_set_int_enable(CFG_IO_TOUCH_PENDOWN_DETECT, 0);
			return IRQ_WAKE_THREAD;
		}
		else {
			nxp_soc_gpio_clr_int_pend(CFG_IO_TOUCH_PENDOWN_DETECT);
			return IRQ_HANDLED;
		}
	}
	else {
		nxp_soc_gpio_clr_int_pend(CFG_IO_TOUCH_PENDOWN_DETECT);
		return IRQ_HANDLED;
	}
	
	return IRQ_HANDLED;
#endif
}

static void tsc2007_stop(struct tsc2007 *ts)
{
	ts->stopped = true;
	mb();
	wake_up(&ts->wait);

	disable_irq(ts->irq);
}


static int tsc2007_open(struct input_dev *input_dev)
{
#if 0
	struct tsc2007 *ts = input_get_drvdata(input_dev);
	int err;

	ts->stopped = false;
	mb();

	enable_irq(ts->irq);
	/* Prepare for touch readings - power down ADC and enable PENIRQ */
	err = tsc2007_xfer(ts, PWRDOWN);
	if (err < 0) {
		tsc2007_stop(ts);
		return err;
	}

	return 0;
#else
	struct tsc2007 *ts = input_get_drvdata(input_dev);
	int err;

	ts->stopped = false;

	//nxp_soc_gpio_set_int_enable(CFG_IO_TOUCH_PENDOWN_DETECT, 1);
	enable_irq(ts->irq);
	err = tsc2007_xfer(ts, PWRDOWN);
	if (err < 0) {
		tsc2007_stop(ts);
		return err;
	}
	
	mb();


   return 0;

#endif
}

static void tsc2007_close(struct input_dev *input_dev)
{
	struct tsc2007 *ts = input_get_drvdata(input_dev);

	tsc2007_stop(ts);
}

static int __devinit tsc2007_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
#if 0
	struct tsc2007 *ts;
	struct tsc2007_platform_data *pdata = client->dev.platform_data;
	struct input_dev *input_dev;
	int err;
	
	if (!pdata) {
		dev_err(&client->dev, "platform data is required!\n");
		return -EINVAL;
	}

	
	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_READ_WORD_DATA))
		return -EIO;

	
	ts = kzalloc(sizeof(struct tsc2007), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!ts || !input_dev) {
		err = -ENOMEM;
		goto err_free_mem;
	}


	ts->client = client;
	ts->irq = client->irq;
	ts->input = input_dev;
	init_waitqueue_head(&ts->wait);

	ts->model             = pdata->model;
	ts->x_plate_ohms      = pdata->x_plate_ohms;
	ts->max_rt            = pdata->max_rt ? : MAX_12BIT;
	ts->poll_delay        = pdata->poll_delay ? : 1;
	ts->poll_period       = pdata->poll_period ? : 1;
	ts->get_pendown_state = pdata->get_pendown_state;
	ts->clear_penirq      = pdata->clear_penirq;

	if (pdata->x_plate_ohms == 0) {
		dev_err(&client->dev, "x_plate_ohms is not set up in platform data");
		err = -EINVAL;
		goto err_free_mem;
	}

	
	snprintf(ts->phys, sizeof(ts->phys),
		 "%s/input0", dev_name(&client->dev));

	input_dev->name = "TSC2007 Touchscreen";
	//input_dev->name = "tsc2007";
	input_dev->phys = ts->phys;
	input_dev->id.bustype = BUS_I2C;

	input_dev->open = tsc2007_open;
	input_dev->close = tsc2007_close;

	input_set_drvdata(input_dev, ts);

	input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);

	//input_set_abs_params(input_dev, ABS_X, 0, MAX_12BIT, pdata->fuzzx, 0);
	//input_set_abs_params(input_dev, ABS_Y, 0, MAX_12BIT, pdata->fuzzy, 0);
	//input_set_abs_params(input_dev, ABS_PRESSURE, 0, MAX_12BIT,
	//		pdata->fuzzz, 0);

	input_set_abs_params(input_dev, ABS_X, 0, LCD_SCREEN_X_PIXEL , 0, 0);
	input_set_abs_params(input_dev, ABS_Y, 0, LCD_SCREEN_Y_PIXEL , 0, 0);
	input_set_abs_params(input_dev, ABS_PRESSURE, 0, ts->max_rt, 0, 0);

	if (pdata->init_platform_hw)
		pdata->init_platform_hw();

	
	
	err = request_threaded_irq(ts->irq, tsc2007_hard_irq, tsc2007_soft_irq,
				   IRQF_ONESHOT, client->dev.driver->name, ts);
	if (err < 0) {
		dev_err(&client->dev, "irq %d busy?\n", ts->irq);
		goto err_free_mem;
	}

	
	tsc2007_stop(ts);

	err = input_register_device(input_dev);
	if (err)
		goto err_free_irq;


	i2c_set_clientdata(client, ts);

	
	return 0;

 err_free_irq:
	free_irq(ts->irq, ts);
	if (pdata->exit_platform_hw)
		pdata->exit_platform_hw();
 err_free_mem:
	input_free_device(input_dev);
	kfree(ts);
	return err;
#else
	struct tsc2007 *ts;
	struct tsc2007_platform_data *pdata = client->dev.platform_data;
	struct input_dev *input_dev;
	int err = 0;
	int ret = 0;
	struct kobject   * kobj = NULL;
	

	// finedigital:jhhong:20150507
	// tcs2007 chips reset
	tsc2007_Reset();

	if (!pdata) {
		dev_err(&client->dev, "platform data is required!\n");
		return -EINVAL;
	}

	
	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_READ_WORD_DATA))
		return -EIO;

	
	ts = kzalloc(sizeof(struct tsc2007), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!ts || !input_dev) {
		err = -ENOMEM;
		goto err_free_mem;
	}

	// ts->stopped = true;
	nxp_soc_gpio_set_int_enable(CFG_IO_TOUCH_PENDOWN_DETECT, 0);
	
	ts->client = client;
	ts->irq = client->irq;
	ts->input = input_dev;
	init_waitqueue_head(&ts->wait);

	ts->model             = pdata->model;
	ts->x_plate_ohms      = pdata->x_plate_ohms;
	ts->max_rt            = pdata->max_rt ? : MAX_12BIT;
	ts->poll_delay        = pdata->poll_delay ? : 1;
	ts->poll_period       = pdata->poll_period ? : 1;
	ts->get_pendown_state = pdata->get_pendown_state;
	ts->clear_penirq      = pdata->clear_penirq;

	if (pdata->x_plate_ohms == 0) {
		dev_err(&client->dev, "x_plate_ohms is not set up in platform data");
		err = -EINVAL;
		goto err_free_mem;
	}

	snprintf(ts->phys, sizeof(ts->phys),
		 "%s/input0", dev_name(&client->dev));

	//input_dev->name = "TSC2007 Touchscreen";
	input_dev->name = "tsc2007";
	input_dev->phys = ts->phys;
	input_dev->id.bustype = BUS_I2C;

	input_dev->open = tsc2007_open;
	input_dev->close = tsc2007_close;

	input_set_drvdata(input_dev, ts);

	input_set_drvdata(input_dev, ts);
	set_bit(ABS_X, input_dev->absbit);
	set_bit(ABS_Y, input_dev->absbit);
	set_bit(ABS_PRESSURE, input_dev->absbit);
	set_bit(BTN_TOUCH, input_dev->keybit);

	input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);

	//input_set_abs_params(input_dev, ABS_X, 0, MAX_12BIT, pdata->fuzzx, 0);
	//input_set_abs_params(input_dev, ABS_Y, 0, MAX_12BIT, pdata->fuzzy, 0);
	//input_set_abs_params(input_dev, ABS_PRESSURE, 0, MAX_12BIT,
	//		pdata->fuzzz, 0);

	input_set_abs_params(input_dev, ABS_X, 0, LCD_SCREEN_X_PIXEL , 0, 0);
	input_set_abs_params(input_dev, ABS_Y, 0, LCD_SCREEN_Y_PIXEL , 0, 0);
	input_set_abs_params(input_dev, ABS_PRESSURE, 0, 1, 0, 0);

	//Nexell allan.park add the tscalibrate routine	
	set_sample(&cal); //init the default value
	perform_calibration(&cal);
	
#ifdef CFG_GPIO_LCD_UPDOWN
	bf700_lcd_updown_detect_init(); //to detect the LCD UP_DOWN
	gbLCD_updown = nxp_soc_gpio_get_in_value(CFG_GPIO_LCD_UPDOWN); //set init state //0 : UP, 1: DOWN
#endif

    /* create attribute interface */
    kobj = kobject_create_and_add("touch", &platform_bus.kobj);
    if (! kobj) {
        printk(KERN_ERR "fail, create touch kobject ...\n");
        goto __err;
    }

    ret = sysfs_create_group(kobj, &ts_attr_group);
    if (ret) {
        printk(KERN_ERR "fail, create touch sysfs group ...\n");
        goto __err;
    }
    g_ts_kobj = kobj;


	if (pdata->init_platform_hw)
		pdata->init_platform_hw();

	
	
	err = request_threaded_irq(ts->irq, tsc2007_hard_irq, tsc2007_soft_irq,
				   IRQF_ONESHOT, client->dev.driver->name, ts);
	if (err < 0) {
		dev_err(&client->dev, "irq %d busy?\n", ts->irq);
		goto err_free_mem;
	}
	
	tsc2007_stop(ts);

	err = input_register_device(input_dev);
	if (err)
		goto err_free_irq;


	i2c_set_clientdata(client, ts);

	return 0;

 err_free_irq:
	free_irq(ts->irq, ts);
	if (pdata->exit_platform_hw)
		pdata->exit_platform_hw();
 err_free_mem:
	input_free_device(input_dev);
 __err:
	kfree(ts);
	return err;

#endif
}

static int __devexit tsc2007_remove(struct i2c_client *client)
{
	struct tsc2007	*ts = i2c_get_clientdata(client);
	struct tsc2007_platform_data *pdata = client->dev.platform_data;

	free_irq(ts->irq, ts);

	if (pdata->exit_platform_hw)
		pdata->exit_platform_hw();

	input_unregister_device(ts->input);
	kobject_del(g_ts_kobj);
	kfree(ts);

	return 0;
}

static const struct i2c_device_id tsc2007_idtable[] = {
	{ "tsc2007", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, tsc2007_idtable);

static struct i2c_driver tsc2007_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "tsc2007"
	},
	.id_table	= tsc2007_idtable,
	.probe		= tsc2007_probe,
	.remove		= __devexit_p(tsc2007_remove),
};

module_i2c_driver(tsc2007_driver);

MODULE_AUTHOR("Kwangwoo Lee <kwlee@mtekvision.com>");
MODULE_DESCRIPTION("TSC2007 TouchScreen Driver");
MODULE_LICENSE("GPL");
