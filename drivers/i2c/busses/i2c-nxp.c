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

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/time.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/gpio.h>

#include <mach/platform.h>
#include <mach/devices.h>
#include <mach/soc.h>

/*
#define pr_debug(msg...)
*/

#if (1)
#define ERROUT(msg...)		{ printk(KERN_ERR "i2c: " msg); }
#else
#define ERROUT(msg...)		do {} while (0)
#endif

#define	DEF_I2C_RATE		(100000)	/* wait 50 msec */
#define DEF_RETRY_COUNT		(1)
#define	DEF_WAIT_ACK		(200)		/* wait 50 msec */

const static int i2c_gpio [][2] = {
	{ (PAD_GPIO_D + 2), (PAD_GPIO_D + 3) },
	{ (PAD_GPIO_D + 4), (PAD_GPIO_D + 5) },
	{ (PAD_GPIO_D + 6), (PAD_GPIO_D + 7) },
};
const static int i2c_reset[3] = {RESET_ID_I2C0, RESET_ID_I2C1, RESET_ID_I2C2};

struct i2c_register {
	unsigned int ICCR;    	///< 0x00 : I2C Control Register
    unsigned int ICSR;      ///< 0x04 : I2C Status Register
    unsigned int IAR;       ///< 0x08 : I2C Address Register
    unsigned int IDSR;      ///< 0x0C : I2C Data Register
    unsigned int STOPCON;   ///< 0x10 : I2C Stop Control Register
};

/*
 * 	local data and macro
 */
struct nxp_i2c_hw {
	int port;
	int irqno;
	int scl_io;
	int sda_io;
	int clksrc;
	int clkscale;
	/* Register */
	void *base_addr;
};

struct nxp_i2c_param {
	struct nxp_i2c_hw hw;
	struct mutex  lock;
	wait_queue_head_t wait_q;
	unsigned int condition;	
	unsigned long rate;
	int	no_stop_mod;
	u8	pre_data;
	int	request_ack;
	int	timeout;
	/* i2c trans data */
	struct i2c_adapter adapter;
	struct i2c_msg *msg;
	struct clk *clk;
	int	trans_count;
	int	trans_done;
	int	trans_mode;
	int run_state;
	int preempt_flag;
};

/*
 * I2C control macro
 */
#define I2C_TXRXMODE_SLAVE_RX		0	///< Slave Receive Mode
#define I2C_TXRXMODE_SLAVE_TX		1	///< Slave Transmit Mode
#define I2C_TXRXMODE_MASTER_RX		2	///< Master Receive Mode
#define I2C_TXRXMODE_MASTER_TX		3	///< Master Transmit Mode

#define I2C_ICCR_OFFS		0x00
#define I2C_ICSR_OFFS		0x04
#define I2C_IDSR_OFFS		0x0C
#define I2C_STOP_OFFS		0x10

#define ICCR_IRQ_CLR_POS	8
#define ICCR_ACK_ENB_POS	7
#define ICCR_IRQ_ENB_POS	5
#define ICCR_IRQ_PND_POS	4
#define ICCR_CLK_SRC_POS 	6
#define ICCR_CLK_VAL_POS 	0

#define ICSR_MOD_SEL_POS	6
#define ICSR_SIG_GEN_POS	5
#define ICSR_BUS_BUSY_POS	5
#define ICSR_OUT_ENB_POS	4
#define ICSR_ARI_STA_POS	3 /* Arbitration */
#define ICSR_ACK_REV_POS	0 /* ACK */

#define STOP_ACK_GEM_POS	2
#define STOP_DAT_REL_POS	1  /* only slave transmode */
#define STOP_CLK_REL_POS	0  /* only master transmode */

static inline void i2c_start_dev(struct nxp_i2c_param *par)
{
	unsigned int base = (unsigned int)par->hw.base_addr;
	unsigned int ICSR = 0, ICCR = 0;
	
	ICSR = readl(base+I2C_ICSR_OFFS);
	ICSR  =  (1<<ICSR_OUT_ENB_POS);	
	writel(ICSR, (base+I2C_ICSR_OFFS));

	writel(par->pre_data, (base+I2C_IDSR_OFFS));				

	ICCR = readl(base+I2C_ICCR_OFFS);
	ICCR &= ~(1<<ICCR_ACK_ENB_POS);	
	ICCR |=  (1<<ICCR_IRQ_ENB_POS);			
	writel(ICCR, (base+I2C_ICCR_OFFS));
	ICSR  =  ( par->trans_mode << ICSR_MOD_SEL_POS) | (1<<ICSR_SIG_GEN_POS) | (1<<ICSR_OUT_ENB_POS);	
	writel(ICSR, (base+I2C_ICSR_OFFS));
}

static inline void i2c_trans_data(unsigned int base, unsigned int ack, int last)
{
	unsigned int ICCR = 0, STOP = 0;
	
	ICCR = readl(base+I2C_ICCR_OFFS);
	ICCR &= ~(1<<ICCR_ACK_ENB_POS);
	ICCR |= ack << ICCR_ACK_ENB_POS;
	
	writel(ICCR, (base+I2C_ICCR_OFFS));
	if(last)
	{
		STOP  = readl(base+I2C_STOP_OFFS);
		STOP |= 1<<STOP_ACK_GEM_POS;
		writel(STOP, base+I2C_STOP_OFFS);
	}

	ICCR  = readl((base+I2C_ICCR_OFFS));
	ICCR  &=  ( ~ (1 <<ICCR_IRQ_PND_POS ) | (~ (1 << ICCR_IRQ_PND_POS)));
    ICCR  |=  1<<ICCR_IRQ_CLR_POS;
    ICCR  |=  1<<ICCR_IRQ_ENB_POS;
	writel(ICCR, (base+I2C_ICCR_OFFS));	
}

//#define	I2C_STOP_IN_ISR

static inline void i2c_stop_dev(struct nxp_i2c_param *par, int nostop, int read)
{
	unsigned int base = (unsigned int)par->hw.base_addr;
	unsigned int ICSR = 0, ICCR = 0, STOP = 0;

	if(!nostop) {
		gpio_request(par->hw.sda_io,NULL);		 //gpio_Request
		gpio_direction_output(par->hw.sda_io,0); //SDA LOW
		udelay(1);
		
		STOP = (1<<STOP_CLK_REL_POS);
		writel(STOP, (base+I2C_STOP_OFFS));	
		ICSR= readl(base+I2C_ICSR_OFFS);
		ICSR &= ~(1<<ICSR_OUT_ENB_POS);	
		ICSR = par->trans_mode << ICSR_MOD_SEL_POS;	
		writel(ICSR, (base+I2C_ICSR_OFFS));
	
		ICCR = (1<<ICCR_IRQ_CLR_POS);
		writel(ICCR, (base+I2C_ICCR_OFFS));	
		udelay(1);
		gpio_set_value(par->hw.sda_io,1);			//STOP Signal Gen
		
		nxp_soc_gpio_set_io_func(par->hw.sda_io, 1);
	} else {
//		ICSR= readl(base+I2C_ICSR_OFFS);
		ICSR &= ~(1<<ICSR_OUT_ENB_POS);	
		ICSR = par->trans_mode << ICSR_MOD_SEL_POS;	
		writel(ICSR, (base+I2C_ICSR_OFFS));
	
		ICCR = (1<<ICCR_IRQ_CLR_POS);
		writel(ICCR, (base+I2C_ICCR_OFFS));	
	}
}

static inline void i2c_wait_dev(struct nxp_i2c_param *par, int wait)
{
	unsigned int base = (unsigned int)par->hw.base_addr;
	unsigned int ICSR = 0;

	do {
		ICSR = readl(base+I2C_ICSR_OFFS);
		if ( !(ICSR & (1<<ICSR_BUS_BUSY_POS)) &&  !(ICSR & (1<<ICSR_ARI_STA_POS)) )
			break;
	    mdelay(1);
	}while(wait-- > 0);
}

static inline void i2c_bus_off(struct nxp_i2c_param *par)
{
	unsigned int base = (unsigned int)par->hw.base_addr;
	unsigned int ICSR = 0;

	ICSR &= ~(1<<ICSR_OUT_ENB_POS);	
	writel(ICSR, (base+I2C_ICSR_OFFS));
}


#define I2C_SET_DATA(p,dat) (((struct i2c_register *)p)->IDSR = dat)
#define I2C_GET_DATA(p)	(((struct i2c_register *)p)->IDSR)
#define I2C_BUS_OFF(p)	(((struct i2c_register *)p)->ICSR & ~(1<<ICSR_OUT_ENB_POS))
#define I2C_ACK_STAT(p)	(((struct i2c_register *)p)->ICSR & (1<<ICSR_ACK_REV_POS))
#define I2C_ARB_STAT(p)	(((struct i2c_register *)p)->ICSR & (1<<ICSR_ARI_STA_POS))
#define I2C_INT_STAT(p)	(((struct i2c_register *)p)->ICCR & (1<<ICCR_IRQ_PND_POS))

/*
 * 	Hardware I2C
 */

static inline void	i2c_set_clock(struct nxp_i2c_param *par, int enable)
{
	int cksrc = (par->hw.clksrc == 16) ?  0 : 1;
	int ckscl = par->hw.clkscale;
	unsigned int base = (unsigned int)par->hw.base_addr;
	unsigned int ICCR = 0, ICSR = 0;
	
	pr_debug("%s: i2c.%d, src:%d, scale:%d, %s\n",
		__func__,  par->hw.port, cksrc, ckscl, enable?"on":"off");
	
	if (enable)		
	{
		ICCR  = readl(base+I2C_ICCR_OFFS);
		ICCR &=  ~( 0x0f | 1 << ICCR_CLK_SRC_POS);
		ICCR |=  ((cksrc << ICCR_CLK_SRC_POS) | (ckscl-1));

		writel(ICCR,(base+I2C_ICCR_OFFS));
	}
	else
	{
		ICSR  = readl(base+I2C_ICSR_OFFS);
		ICSR &= ~(1<<ICSR_OUT_ENB_POS);
		writel(ICSR, (base+I2C_ICSR_OFFS));
	}
}

static inline int i2c_wait_busy(struct nxp_i2c_param *par)
{
	int wait = 500;
	int ret = 0;
	void *base = par->hw.base_addr;

	pr_debug("%s(i2c.%d, nostop:%d)\n", __func__, par->hw.port, par->no_stop_mod);

	/* busy status check*/
	i2c_wait_dev(par, wait);


	if (0 > wait) {
		printk(KERN_ERR "Fail, i2c.%d is busy, arbitration %s ...\n",
			par->hw.port, I2C_ARB_STAT(base)?"busy":"free");
		ret = -1;
	}
	return ret;
}

static irqreturn_t i2c_trans_irq(int irqno, void *dev_id)
{
	struct nxp_i2c_param *par = dev_id;
	struct i2c_msg *msg = par->msg;
	unsigned int base = (unsigned int)par->hw.base_addr;
	u16 flags = par->msg->flags;
	int len = msg->len;
	int cnt = par->trans_count;

	/* Arbitration Check. */
	if((I2C_ARB_STAT((void *)base) != 0 ) ) {
		ERROUT("Fail,i2c.%d  addr [0x%02x] Arbitraion [0x%02x], trans %2d:%2d\n",
			par->hw.port,(msg->addr<<1), par->pre_data, cnt, len);
		par->trans_done = 0;
		goto __end_trans;
	}

	if (par->request_ack && (I2C_ACK_STAT ((void *)base))) {
		ERROUT("Fail, i2c.%d addr [0x%02x] no ack data [0x%02x], trans %2d:%2d \n",
			par->hw.port, (msg->addr<<1), par->pre_data, cnt, len);
		goto __end_trans;
	}

	if (!par->trans_done) {
		if (flags & I2C_M_RD) {
			int ack  = (len == cnt + 1) ? 0: 1;
			int last = (len == cnt + 1) ? 1: 0;

			par->request_ack = 0;
			if (0 == cnt) {
				i2c_trans_data(base, ack, 0);	/* first: address */ 
				par->trans_count += 1;
				return IRQ_HANDLED;	/* next: data read */
				}

			msg->buf[cnt - 1] = I2C_GET_DATA((void *)base);

			if (len == par->trans_count) {
				par->trans_done = 1;
				goto __end_trans;
			} else {
				i2c_trans_data(base, ack, last);
				par->trans_count += 1;

				return IRQ_HANDLED;
			}
		} else {
			par->pre_data = msg->buf[cnt];
			par->request_ack = (msg->flags & I2C_M_IGNORE_NAK) ? 0 : 1;
			par->trans_count += 1;

			if (len == par->trans_count) 
				par->trans_done = 1;

			I2C_SET_DATA((void *)base, msg->buf[cnt]);
			i2c_trans_data(base, 0, par->trans_done?1:0);

			return IRQ_HANDLED;
		}
	}

__end_trans:
	i2c_stop_dev(par, par->no_stop_mod, (flags&I2C_M_RD ? 0 : 1));
	
	par->condition = 1;
	wake_up(&par->wait_q);
	return IRQ_HANDLED;
}
static irqreturn_t i2c_irq(int irqno, void *dev_id)
{
	struct nxp_i2c_param *par = dev_id;
    unsigned int ICCR = 0;
	unsigned int base = (unsigned int)par->hw.base_addr;
	if (!par->run_state)
		return IRQ_NONE;
	if( par->preempt_flag == 1){
		i2c_trans_irq( irqno,dev_id);
		return IRQ_HANDLED;
	}

	ICCR  = readl((base+I2C_ICCR_OFFS));
	ICCR  &=  ( ~ (1 <<ICCR_IRQ_PND_POS ) | (~ (1 << ICCR_IRQ_ENB_POS)));
	ICCR  |=  1<<ICCR_IRQ_CLR_POS;
	writel(ICCR, (base+I2C_ICCR_OFFS));

	return IRQ_WAKE_THREAD;
}
static int i2c_wait_end(struct nxp_i2c_param *par)
{
	void *base = par->hw.base_addr;
	int tout, ret = -1;
	struct i2c_msg *msg = par->msg;

	if(!preempt_count()){
		tout = wait_event_timeout(par->wait_q, par->condition,
					msecs_to_jiffies(par->timeout));\
	} else {
	tout = par->timeout;
	while(tout--)
	{
		mdelay(1);
		if(par->trans_done)
		break;
		}
	}

	if (par->condition)
		ret = 0;

	if (0 > ret) {
		ERROUT("Fail, i2c.%d addr=0x%02x , %02x irq condition (%d), pend (%s), arbitration (%s)\n",
			par->hw.port, (par->msg->addr<<1), msg->buf[0],par->condition, I2C_INT_STAT(base)?"yes":"no",
		I2C_ARB_STAT(base)?"busy":"free");
		i2c_stop_dev(par, 0, 0);
	}

	if (!par->trans_done)
		i2c_stop_dev(par, 0, 0);
	
	par->condition = 0;
	if (0 >ret )
		return -1;

	return (par->trans_done ? 0 : -1);
}

static inline int i2c_trans_run(struct nxp_i2c_param *par, struct i2c_msg *msg)
{
	u32 mode;
	u8  addr;

	if (msg->flags & I2C_M_TEN) {
		printk(KERN_ERR "Fail, i2c.%d not support ten bit addr:0x%02x, flags:0x%x \n",
			par->hw.port, (msg->addr<<1), msg->flags);
		return -1;
	}

	if (msg->flags & I2C_M_RD) {
		addr =  msg->addr << 1 | 1;
		mode = I2C_TXRXMODE_MASTER_RX;
	} else {
		addr = msg->addr << 1;
		mode = I2C_TXRXMODE_MASTER_TX;
	}

	pr_debug("%s(i2c.%d, addr:0x%02x, %s)\n",
		__func__, par->hw.port, addr, msg->flags&I2C_M_RD?"R":"W");

 	/* clear irq condition */
	par->msg = msg;
	par->condition = 0;
	par->pre_data = addr;
	par->request_ack = (msg->flags & I2C_M_IGNORE_NAK ) ? 0 : 1;
	par->trans_count = 0;
	par->trans_done  = 0;
	par->trans_mode  = mode;

	i2c_start_dev(par);
	/* wait for end transfer */
	return i2c_wait_end(par);
}

static int nxp_i2c_transfer_hw(struct nxp_i2c_param *par, struct i2c_msg *msg, int num)
{
	int ret = -EAGAIN;

	pr_debug("\n%s(flags:0x%x, %c)\n",
		__func__, msg->flags, msg->flags&I2C_M_RD?'R':'W');

	i2c_set_clock(par, 1);

	if (0 > i2c_wait_busy(par))
		goto err_i2c;

	/* transfer */
	if (0 > i2c_trans_run(par, msg))
		goto err_i2c;

	ret = msg->len;

err_i2c:
	pr_debug("%s : Err  ", __func__);
	if (ret != msg->len)
		msg->flags &= ~I2C_M_NOSTART;

	i2c_wait_busy(par);
	i2c_set_clock(par, 0);

	return ret;
}

static int nxp_i2c_algo_xfer(struct i2c_adapter *adapter, struct i2c_msg *msgs, int num)
{
	struct nxp_i2c_param *par  = adapter->algo_data;
	struct i2c_msg  *tmsg = msgs;
	int i = adapter->retries;
	int j = num;
	int ret = -EAGAIN;
	int len = 0;
	int  (*transfer_i2c)(struct nxp_i2c_param *, struct i2c_msg *, int) = NULL;

	/* lock par */
	if(!preempt_count())
		mutex_lock(&par->lock);
	else 
		par->preempt_flag = 1;

	transfer_i2c = nxp_i2c_transfer_hw;
	par->run_state = 1;
	pr_debug("\n %s(msg num:%d)\n", __func__, num);

	par->no_stop_mod = 1;
	for ( ; j > 0; j--, tmsg++) {
		len = tmsg->len;
		if (1 == j)
			par->no_stop_mod = 0;

		/* transfer */
		for (i = adapter->retries; i > 0; i--) {
			ret = transfer_i2c(par, tmsg, num);
			if (ret == len)
				break;

			ERROUT("i2c.%d addr 0x%02x (try:%d)\n",
				par->hw.port, tmsg->addr<<1, adapter->retries-i+1);
		}

		/* Error */
		if (ret != len)
			break;
	}

	par->run_state = 0;
	par->preempt_flag = 0;
	/* unlock par */
	if(!preempt_count())
		mutex_unlock(&par->lock);

	/* ok */
	if (ret == len)
		return num;

	pr_err("Error: i2c.%d, addr:%02x, trans len:%d(%d), try:%d\n",
		par->hw.port, (msgs->addr<<1), ret, len, adapter->retries);
	return ret;
}

static u32 nxp_i2c_algo_fn(struct i2c_adapter *adapter)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static struct i2c_algorithm nxp_i2c_algo = {
	.master_xfer 	= nxp_i2c_algo_xfer,
	.functionality 	= nxp_i2c_algo_fn,
};

static int	nxp_i2c_set_param(struct nxp_i2c_param *par, struct platform_device *pdev)
{
	struct nxp_i2c_plat_data *plat = pdev->dev.platform_data;
	unsigned long rate = 0;
	int ret = 0;

	unsigned long get_real_clk = 0, req_rate =0 ;
	unsigned long calc_clk , t_clk = 0;
	unsigned int t_src = 0,  t_div = 0;
	int div = 0 ;
	struct clk *clk;
	unsigned int i=0, src = 0;

	/* set par hardware */
	par->hw.port = plat->port;
	par->hw.irqno = plat->irq;
	par->hw.base_addr = (void*)IO_ADDRESS(plat->base_addr);
	par->hw.scl_io = plat->gpio->scl_pin ? plat->gpio->scl_pin : i2c_gpio[plat->port][0];
	par->hw.sda_io = plat->gpio->sda_pin ? plat->gpio->sda_pin : i2c_gpio[plat->port][1];
	par->no_stop_mod = 0;
	par->timeout = DEF_WAIT_ACK;
	par->rate =	plat->rate ? plat->rate : DEF_I2C_RATE;
	par->preempt_flag = 0;
	nxp_soc_gpio_set_io_func(par->hw.scl_io, 1);
	nxp_soc_gpio_set_io_func(par->hw.sda_io, 1);

	clk = clk_get(&pdev->dev, NULL);
	if (IS_ERR(clk)) {
		ret = PTR_ERR(clk);
		return ret;
	}

	rate = clk_get_rate(clk);
	req_rate = par->rate;
	t_clk = rate/16/3;

	for (i = 0; i < 2; i ++) {
		src	= (i== 0) ? 16: 256;
		for (div = 3 ; div < 16; div++) {
			get_real_clk = rate/src/div;
			if (get_real_clk > req_rate )
				calc_clk = get_real_clk - req_rate;
			else
				calc_clk = req_rate - get_real_clk ;

			if (calc_clk < t_clk) {
				t_clk = calc_clk;
				t_div = div;
				t_src = src;
			} else if (calc_clk == 0) {
				t_div = div;
				t_src = src;
				break;
			}
		}
		if (calc_clk == 0)
			break;
	}

	par->hw.clksrc = t_src;
	par->hw.clkscale = t_div;
	par->clk = clk;

	nxp_soc_rsc_reset(i2c_reset[plat->port]);
	/* init par resource */
	mutex_init(&par->lock);
	init_waitqueue_head(&par->wait_q);

	printk("%s.%d: %8ld hz [pclk=%ld, clk = %3d, scale=%2d, timeout=%4d ms]\n",
		DEV_NAME_I2C, par->hw.port, rate/t_src/t_div,
		rate, par->hw.clksrc, par->hw.clkscale-1, par->timeout);

	ret = request_threaded_irq(par->hw.irqno, i2c_irq,i2c_trans_irq, IRQF_DISABLED|IRQF_SHARED , DEV_NAME_I2C, par);
	if (ret)
		printk(KERN_ERR "Fail, i2c.%d request irq %d ...\n", par->hw.port, par->hw.irqno);

	i2c_bus_off(par);
	
	clk_enable(clk);

	return ret;
}

static int nxp_i2c_probe(struct platform_device *pdev)
{
	struct nxp_i2c_param *par = NULL;
	int ret = 0;
	pr_debug("%s name:%s, id:%d\n", __func__, pdev->name, pdev->id);

	/*	allocate nxp_i2c_param data */
	par = kzalloc(sizeof(struct nxp_i2c_param), GFP_KERNEL);
	if (!par) {
		printk(KERN_ERR "Fail, %s allocate driver info ...\n", pdev->name);
		return -ENOMEM;
	}

	/* init par data struct */
	ret = nxp_i2c_set_param(par, pdev);
	if (0 > ret)
		goto err_mem;

	/*	init par adapter */
	strlcpy(par->adapter.name, DEV_NAME_I2C, I2C_NAME_SIZE);

	par->adapter.owner 	= THIS_MODULE;
	par->adapter.nr 		= par->hw.port;
	par->adapter.class 	= I2C_CLASS_HWMON | I2C_CLASS_SPD;
	par->adapter.algo 		= &nxp_i2c_algo;
	par->adapter.algo_data = par;
	par->adapter.dev.parent= &pdev->dev;
	par->adapter.retries 	= DEF_RETRY_COUNT;

	ret = i2c_add_numbered_adapter(&par->adapter);
	if (ret) {
		printk(KERN_ERR "Fail, i2c.%d add to adapter ...\n", par->hw.port);
		goto err_irq;
	}

	/* set driver data */
	platform_set_drvdata(pdev, par);
	return ret;

err_irq:
	free_irq(par->hw.irqno, par);

err_mem:
	kfree(par);
	return ret;
}

static int nxp_i2c_remove(struct platform_device *pdev)
{
	struct nxp_i2c_param *par = platform_get_drvdata(pdev);
	int rsc = i2c_reset[par->hw.port];
	int irq = par->hw.irqno;

	nxp_soc_rsc_enter(rsc);
	clk_disable(par->clk);

	free_irq(irq, par);
	i2c_del_adapter(&par->adapter);
	kfree(par);

	platform_set_drvdata(pdev, NULL);
	return 0;
}

#ifdef CONFIG_PM
static int nxp_i2c_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct nxp_i2c_param *par = platform_get_drvdata(pdev);
	int rsc = i2c_reset[par->hw.port];
	nxp_soc_rsc_enter(rsc);
	PM_DBGOUT("%s \n", __func__);
	return 0;
}

static int nxp_i2c_resume(struct platform_device *pdev)
{
	struct nxp_i2c_param *par = platform_get_drvdata(pdev);
	int sda = par->hw.sda_io;
	int scl = par->hw.scl_io;
	int rsc = i2c_reset[par->hw.port];

	PM_DBGOUT("%s\n", __func__);
	nxp_soc_gpio_set_io_func(scl, 1);
	nxp_soc_gpio_set_io_func(sda, 1);
	nxp_soc_rsc_reset(rsc);

	i2c_bus_off(par);
	clk_enable(par->clk);
	return 0;
}

#else
#define nxp_i2c_suspend		NULL
#define nxp_i2c_resume		NULL
#endif

static struct platform_driver i2c_plat_driver = {
	.probe	 = nxp_i2c_probe,
	.remove	 = nxp_i2c_remove,
    .suspend = nxp_i2c_suspend,
    .resume	 = nxp_i2c_resume,
	.driver	 = {
	.owner	 = THIS_MODULE,
	.name	 = DEV_NAME_I2C,
	},
};

static int __init nxp_i2c_init(void)
{
	return platform_driver_register(&i2c_plat_driver);
}

static void __exit nxp_i2c_exit(void)
{
	platform_driver_unregister(&i2c_plat_driver);
}

subsys_initcall(nxp_i2c_init);
module_exit(nxp_i2c_exit);

MODULE_DESCRIPTION("I2C driver for the Nexell");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform: nexell par");

