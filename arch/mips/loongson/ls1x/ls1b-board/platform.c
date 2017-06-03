/*
 * Copyright (c) 2012 Tang, Haifeng <tanghaifeng-gz@loongson.cn>
 * Platform device support for Loongson 1B
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/resource.h>
#include <linux/serial_8250.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/delay.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/spi/mmc_spi.h>
#include <linux/spi/spi_gpio.h>
#include <linux/mmc/host.h>
#include <linux/phy.h>
#include <linux/stmmac.h>
#include <linux/i2c.h>
#include <linux/i2c/tsc2007.h>
#include <linux/i2c/ft5x06_ts.h>
#include <linux/videodev2.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/input/matrix_keypad.h>
#include <linux/input/74x165_gpio_keys_polled.h>
#include <linux/rotary_encoder.h>
#include <linux/ssd1305.h>
#include <linux/st7920.h>
#include <linux/clk.h>
#include <linux/jbt6k74.h>

#include <video/ls1xfb.h>

#include <loongson1.h>
#include <irq.h>
#include <asm/gpio.h>
#include <asm-generic/sizes.h>

#ifdef CONFIG_MTD_NAND_LS1X
#include <ls1x_nand.h>
static struct mtd_partition ls1x_nand_partitions[] = {
	{
		.name	= "kernel",
		.offset	= MTDPART_OFS_APPEND,
		.size	= 14*1024*1024,
	}, {
		.name	= "rootfs",
		.offset	= MTDPART_OFS_APPEND,
		.size	= 100*1024*1024,
	}, {
		.name	= "data",
		.offset	= MTDPART_OFS_APPEND,
		.size	= MTDPART_SIZ_FULL,
	},
};

static struct ls1x_nand_platform_data ls1x_nand_parts = {
//	.enable_arbiter	= 1,
//	.keep_config	= 1,
	.parts		= ls1x_nand_partitions,
	.nr_parts	= ARRAY_SIZE(ls1x_nand_partitions),
};

static struct resource ls1x_nand_resources[] = {
	[0] = {
		.start          = LS1X_NAND_BASE,
		.end            = LS1X_NAND_BASE + SZ_16K - 1,
		.flags          = IORESOURCE_MEM,
	},
	[1] = {
//		.start          = LS1X_NAND_IRQ,
//		.end            = LS1X_NAND_IRQ,
		.start          = LS1X_DMA0_IRQ,
        .end            = LS1X_DMA0_IRQ,
		.flags          = IORESOURCE_IRQ,
	},
};

struct platform_device ls1x_nand_device = {
	.name	= "ls1x-nand",
	.id		= -1,
	.dev	= {
		.platform_data = &ls1x_nand_parts,
	},
	.num_resources	= ARRAY_SIZE(ls1x_nand_resources),
	.resource		= ls1x_nand_resources,
};
#endif //CONFIG_MTD_NAND_LS1X

#define LS1X_UART(_id)						\
	{							\
		.mapbase	= LS1X_UART ## _id ## _BASE,	\
		.irq		= LS1X_UART ## _id ## _IRQ,	\
		.iotype		= UPIO_MEM,			\
		.flags		= UPF_IOREMAP | UPF_FIXED_TYPE,	\
		.type		= PORT_16550A,			\
	}

#define LS1X_UART_SHARE(_id, _irq)						\
	{							\
		.mapbase	= LS1X_UART ## _id ## _BASE,	\
		.irq		= LS1X_UART ## _irq ## _IRQ,	\
		.iotype		= UPIO_MEM,			\
		.flags		= UPF_IOREMAP | UPF_FIXED_TYPE | UPF_SHARE_IRQ,	\
		.type		= PORT_16550A,			\
	}

static struct plat_serial8250_port ls1x_serial8250_port[] = {
	LS1X_UART(0),
	LS1X_UART(1),
	LS1X_UART(2),
	LS1X_UART(3),
#ifdef CONFIG_LS1B_MACH
	LS1X_UART(4),
	LS1X_UART(5),
#ifdef CONFIG_MULTIFUNC_CONFIG_SERAIL0
	LS1X_UART_SHARE(6, 0),
	LS1X_UART_SHARE(7, 0),
	LS1X_UART_SHARE(8, 0),
#endif
#ifdef CONFIG_MULTIFUNC_CONFIG_SERAIL1
	LS1X_UART_SHARE(9, 1),
	LS1X_UART_SHARE(10, 1),
	LS1X_UART_SHARE(11, 1),
#endif
#endif
	{},
};

struct platform_device ls1x_uart_device = {
	.name		= "serial8250",
	.id		= PLAT8250_DEV_PLATFORM,
	.dev		= {
		.platform_data = ls1x_serial8250_port,
	},
};

void __init ls1x_serial_setup(void)
{
	struct clk *clk;
	struct plat_serial8250_port *p;

#ifdef CONFIG_MULTIFUNC_CONFIG_SERAIL0
	__raw_writeb(__raw_readb(UART_SPLIT) | 0x01, UART_SPLIT);
#endif
#ifdef CONFIG_MULTIFUNC_CONFIG_SERAIL1
	__raw_writeb(__raw_readb(UART_SPLIT) | 0x02, UART_SPLIT);
	__raw_writel(__raw_readl(LS1X_MUX_CTRL1) | UART1_3_USE_CAN1 | UART1_2_USE_CAN0, 
				LS1X_MUX_CTRL1);
#endif

	clk = clk_get(NULL, "apb");
	if (IS_ERR(clk))
		panic("unable to get apb clock, err=%ld", PTR_ERR(clk));

	for (p = ls1x_serial8250_port; p->flags != 0; ++p)
		p->uartclk = clk_get_rate(clk);
}

/* OHCI */
#ifdef CONFIG_USB_OHCI_HCD_LS1X
static u64 ls1x_ohci_dma_mask = DMA_BIT_MASK(32);
static struct resource ls1x_ohci_resources[] = {
	[0] = {
		.start          = LS1X_OHCI_BASE,
		.end            = LS1X_OHCI_BASE + SZ_32K - 1,
		.flags          = IORESOURCE_MEM,
	},
	[1] = {
		.start          = LS1X_OHCI_IRQ,
		.end            = LS1X_OHCI_IRQ,
		.flags          = IORESOURCE_IRQ,
	},
};

static struct platform_device ls1x_ohci_device = {
	.name           = "ls1x-ohci",
	.id             = 0,
	.dev = {
//		.platform_data = &ls1x_ohci_platform_data,
		.dma_mask = &ls1x_ohci_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
	.num_resources  = ARRAY_SIZE(ls1x_ohci_resources),
	.resource       = ls1x_ohci_resources,
};
#endif //#ifdef CONFIG_USB_OHCI_HCD_LS1X

#ifdef CONFIG_USB_OHCI_HCD_PLATFORM
#include <linux/usb/ohci_pdriver.h>
static u64 ls1x_ohci_dmamask = DMA_BIT_MASK(32);

static struct resource ls1x_ohci_resources[] = {
	[0] = {
		.start	= LS1X_OHCI_BASE,
		.end	= LS1X_OHCI_BASE + SZ_32K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= LS1X_OHCI_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct usb_ohci_pdata ls1x_ohci_pdata = {
};

struct platform_device ls1x_ohci_device = {
	.name		= "ohci-platform",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(ls1x_ohci_resources),
	.resource	= ls1x_ohci_resources,
	.dev		= {
		.dma_mask = &ls1x_ohci_dmamask,
		.platform_data = &ls1x_ohci_pdata,
	},
};
#endif

/* EHCI */
#ifdef CONFIG_USB_EHCI_HCD_LS1X
static u64 ls1x_ehci_dma_mask = DMA_BIT_MASK(32);
static struct resource ls1x_ehci_resources[] = { 
	[0] = {
		.start          = LS1X_EHCI_BASE,
		.end            = LS1X_EHCI_BASE + SZ_32K - 1,
		.flags          = IORESOURCE_MEM,
	},
	[1] = {
		.start          = LS1X_EHCI_IRQ,
		.end            = LS1X_EHCI_IRQ,
		.flags          = IORESOURCE_IRQ,
	},
};

static struct platform_device ls1x_ehci_device = {
	.name           = "ls1x-ehci",
	.id             = 0,
	.dev = {
//		.platform_data = &ls1x_ehci_platform_data,
		.dma_mask = &ls1x_ehci_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
	.num_resources  = ARRAY_SIZE(ls1x_ehci_resources),
	.resource       = ls1x_ehci_resources,
};
#endif //#ifdef CONFIG_USB_EHCI_HCD_LS1X

/*
* watchdog
*/
#ifdef CONFIG_LS1X_WDT
static struct resource ls1x_wdt_resource[] = {
	[0]={
		.start      = LS1X_WDT_BASE,
		.end        = (LS1X_WDT_BASE + 0x8),
		.flags      = IORESOURCE_MEM,
	},
};

static struct platform_device ls1x_wdt_device = {
	.name       = "ls1x-wdt",
	.id         = -1,
	.num_resources  = ARRAY_SIZE(ls1x_wdt_resource),
	.resource   = ls1x_wdt_resource,
};
#endif //#ifdef CONFIG_LS1X_WDT

/* RTC */
#ifdef CONFIG_RTC_DRV_RTC_LOONGSON1
static struct resource ls1x_rtc_resource[] = {
	[0]={
		.start      = LS1X_RTC_BASE,
		.end        = LS1X_RTC_BASE + SZ_16K - 1,
		.flags      = IORESOURCE_MEM,
	},
	[1] = {
		.start      = LS1X_RTC_INT0_IRQ,
		.end        = LS1X_RTC_INT0_IRQ,
		.flags      = IORESOURCE_IRQ,
	},
	[2] = {
		.start      = LS1X_RTC_INT1_IRQ,
		.end        = LS1X_RTC_INT1_IRQ,
		.flags      = IORESOURCE_IRQ,
	},
	[3] = {
		.start      = LS1X_RTC_INT2_IRQ,
		.end        = LS1X_RTC_INT2_IRQ,
		.flags      = IORESOURCE_IRQ,
	},
	[4] = {
		.start      = LS1X_RTC_TICK_IRQ,
		.end        = LS1X_RTC_TICK_IRQ,
		.flags      = IORESOURCE_IRQ,
	},
};

static struct platform_device ls1x_rtc_device = {
	.name       = "ls1x-rtc",
	.id         = 0,
	.num_resources  = ARRAY_SIZE(ls1x_rtc_resource),
	.resource   = ls1x_rtc_resource,
};
#endif //#ifdef CONFIG_RTC_DRV_RTC_LOONGSON1

#ifdef CONFIG_RTC_DRV_TOY_LOONGSON1
static struct platform_device ls1x_toy_device = {
	.name       = "ls1x-toy",
	.id         = 1,
};
#endif //#ifdef CONFIG_RTC_DRV_TOY_LOONGSON1

/*
*I2C
*/
/* I2C devices fitted. */
#ifdef CONFIG_TOUCHSCREEN_TSC2007
#define TSC2007_GPIO_IRQ	60
static int ts_get_pendown_state(void)
{
	return !gpio_get_value(TSC2007_GPIO_IRQ);
}

int ts_init(void)
{
	gpio_request(TSC2007_GPIO_IRQ, "tsc2007 gpio irq");
	gpio_direction_input(TSC2007_GPIO_IRQ);
	return 0;
}

static struct tsc2007_platform_data tsc2007_info = {
	.model				= 2007,
	.x_plate_ohms		= 180,
	.get_pendown_state	= ts_get_pendown_state,
	.init_platform_hw	= ts_init,
};
#endif //#ifdef CONFIG_TOUCHSCREEN_TSC2007

#ifdef CONFIG_TOUCHSCREEN_FT5X0X
#define FT5X0X_GPIO_IRQ		38
#define FT5X0X_GPIO_WAUP	39
int ft5x0x_irq_init(void)
{
	gpio_request(FT5X0X_GPIO_IRQ, "ft5x0x gpio irq");
	gpio_direction_input(FT5X0X_GPIO_IRQ);		/* 输入使能 */
	return 0;
}

int ft5x0x_wake_up(void)
{
	gpio_direction_output(FT5X0X_GPIO_WAUP, 0);		/* 输出使能 */
	msleep(10);
	gpio_set_value(FT5X0X_GPIO_WAUP, 1);
	msleep(10);
	return 0;
}

static struct ft5x0x_ts_platform_data ft5x0x_info = {
	.init_platform_hw	= ft5x0x_irq_init,
	.wake_platform_hw	= ft5x0x_wake_up,
};
#endif //#ifdef CONFIG_TOUCHSCREEN_FT5X0X

#ifdef CONFIG_SOC_CAMERA_LS1B
#include <media/ls1b_camera.h>
static struct ls1b_camera_platform_data gc0307_plat = {
	.bl = 57,
	.ts = 56,
	.hsync = 58,
	.vsync = 59,
};
#endif

#ifdef CONFIG_GPIO_PCA953X
#include <linux/i2c/pca953x.h>
#define PCA9555_GPIO_BASE 170
#define PCA9555_IRQ_BASE 170
#define PCA9555_GPIO_IRQ 8

#define PCA9555_DO0 (PCA9555_GPIO_BASE+0)
#define PCA9555_DO1 (PCA9555_GPIO_BASE+1)
#define PCA9555_DO2 (PCA9555_GPIO_BASE+2)
#define PCA9555_DO3 (PCA9555_GPIO_BASE+3)
#define PCA9555_DO4 (PCA9555_GPIO_BASE+4)
#define PCA9555_DO5 (PCA9555_GPIO_BASE+5)
#define PCA9555_DO6 (PCA9555_GPIO_BASE+6)
#define PCA9555_DO7 (PCA9555_GPIO_BASE+7)

#define PCA9555_EMERG_OFF (PCA9555_GPIO_BASE+10)
#define PCA9555_PWRKEY (PCA9555_GPIO_BASE+11)

#define PCA9555_LED0 (PCA9555_GPIO_BASE+8)
#define PCA9555_LED1 (PCA9555_GPIO_BASE+12)
#define PCA9555_LED2 (PCA9555_GPIO_BASE+13)
#define PCA9555_LED3 (PCA9555_GPIO_BASE+14)
#define PCA9555_LED4 (PCA9555_GPIO_BASE+15)

static int ls1x_pca9555_setup(struct i2c_client *client,
			       unsigned gpio_base, unsigned ngpio,
			       void *context)
{
#if 0
	static int ls1x_gpio_value[] = {
		1, 1, 1, 1, 1, 1, 1, 1, 0, -1, -1, 0, 0, 0, 0, 0
	};
	int n;

	gpio_request(PCA9555_GPIO_IRQ, "pca9555 gpio irq");
	gpio_direction_input(PCA9555_GPIO_IRQ);		/* 输入使能 */

	for (n = 0; n < ARRAY_SIZE(ls1x_gpio_value); ++n) {
		gpio_request(gpio_base + n, "ACS-5000 GPIO Expander");
		if (ls1x_gpio_value[n] < 0)
			gpio_direction_input(gpio_base + n);
		else
			gpio_direction_output(gpio_base + n,
					      ls1x_gpio_value[n]);
		gpio_export(gpio_base + n, 0); /* Export, direction locked down */
	}
#endif
	gpio_request(PCA9555_GPIO_IRQ, "pca9555 gpio irq");
	gpio_direction_input(PCA9555_GPIO_IRQ);
	return 0;
}

static struct pca953x_platform_data ls1x_i2c_pca9555_platdata = {
	.gpio_base	= PCA9555_GPIO_BASE, /* Start directly after the CPU's GPIO */
	.irq_base = PCA9555_IRQ_BASE,
//	.invert		= 0, /* Do not invert */
	.setup		= ls1x_pca9555_setup,
};

#if defined(CONFIG_LEDS_GPIO) || defined(CONFIG_LEDS_GPIO_MODULE)
#include <linux/leds.h>
struct gpio_led pca9555_gpio_leds[] = {
	/* gpio for 7 relay purposes */
	{
		.name			= "DO0",
		.gpio			= PCA9555_DO0,
		.active_low		= 0,
		.default_trigger	= "none",
		.default_state	= LEDS_GPIO_DEFSTATE_OFF,
	},{
		.name			= "DO1",
		.gpio			= PCA9555_DO1,
		.active_low		= 0,
		.default_trigger	= "none",
		.default_state	= LEDS_GPIO_DEFSTATE_OFF,
	},{
		.name			= "DO2",
		.gpio			= PCA9555_DO2,
		.active_low		= 0,
		.default_trigger	= "none",
		.default_state	= LEDS_GPIO_DEFSTATE_OFF,
	},{
		.name			= "DO3",
		.gpio			= PCA9555_DO3,
		.active_low		= 0,
		.default_trigger	= "none",
		.default_state	= LEDS_GPIO_DEFSTATE_OFF,
	},{
		.name			= "DO4",
		.gpio			= PCA9555_DO4,
		.active_low		= 0,
		.default_trigger	= "none",
		.default_state	= LEDS_GPIO_DEFSTATE_OFF,
	}, {
		.name			= "DO5",
		.gpio			= PCA9555_DO5,
		.active_low		= 0,
		.default_trigger	= "none",
		.default_state	= LEDS_GPIO_DEFSTATE_OFF,
	},{
		.name			= "DO6",
		.gpio			= PCA9555_DO6,
		.active_low		= 0,
		.default_trigger	= "none",
		.default_state	= LEDS_GPIO_DEFSTATE_OFF,
	},{
		.name			= "DO7",
		.gpio			= PCA9555_DO7,
		.active_low		= 0,
		.default_trigger	= "none",
		.default_state	= LEDS_GPIO_DEFSTATE_OFF,
	},

	/* ctrl GPRS */
	{
		.name			= "emerg_off",
		.gpio			= PCA9555_EMERG_OFF,
		.active_low		= 0,
		.default_trigger	= "none",
		.default_state	= LEDS_GPIO_DEFSTATE_OFF,
	}, {
		.name			= "pwrkey",
		.gpio			= PCA9555_PWRKEY,
		.active_low		= 0,
		.default_trigger	= "none",
		.default_state	= LEDS_GPIO_DEFSTATE_OFF,
	},

	/* 5 leds */
	{
		.name			= "green:run",
		.gpio			= PCA9555_LED0,
		.active_low		= 1,
		.default_trigger	= "none",
		.default_state	= LEDS_GPIO_DEFSTATE_ON,
	}, {
		.name			= "red:channel1",
		.gpio			= PCA9555_LED1,
		.active_low		= 1,
		.default_trigger	= "none",
		.default_state	= LEDS_GPIO_DEFSTATE_OFF,
	}, {
		.name			= "red:channel2",
		.gpio			= PCA9555_LED2,
		.active_low		= 1,
		.default_trigger	= "none",
		.default_state	= LEDS_GPIO_DEFSTATE_OFF,
	}, {
		.name			= "red:channel3",
		.gpio			= PCA9555_LED3,
		.active_low		= 1,
		.default_trigger	= "none",
		.default_state	= LEDS_GPIO_DEFSTATE_OFF,
	}, {
		.name			= "red:channel4",
		.gpio			= PCA9555_LED4,
		.active_low		= 1,
		.default_trigger	= "none",
		.default_state	= LEDS_GPIO_DEFSTATE_OFF,
	},
};

static struct gpio_led_platform_data pca9555_gpio_led_info = {
	.leds		= pca9555_gpio_leds,
	.num_leds	= ARRAY_SIZE(pca9555_gpio_leds),
};

static struct platform_device pca9555_leds = {
	.name	= "leds-gpio",
	.id	= 0,
	.dev	= {
		.platform_data	= &pca9555_gpio_led_info,
	}
};
#endif //#if defined(CONFIG_LEDS_GPIO) || defined(CONFIG_LEDS_GPIO_MODULE)
#endif //#ifdef CONFIG_GPIO_PCA953X

#ifdef CONFIG_I2C_LS1X
static struct i2c_board_info __initdata ls1x_i2c0_devs[] = {
#ifdef CONFIG_TOUCHSCREEN_TSC2007
	{
		I2C_BOARD_INFO("tsc2007", 0x48),
		.irq = LS1X_GPIO_FIRST_IRQ + TSC2007_GPIO_IRQ,
		.platform_data	= &tsc2007_info,
	},
#endif
#ifdef CONFIG_TOUCHSCREEN_FT5X0X
	{
		I2C_BOARD_INFO(FT5X0X_NAME, 0x38),
		.irq = LS1X_GPIO_FIRST_IRQ + FT5X0X_GPIO_IRQ,
		.platform_data	= &ft5x0x_info,
	},
#endif
#ifdef CONFIG_SOC_CAMERA_LS1B
	{
		I2C_BOARD_INFO("gc0307", 0x21),
		.platform_data = &gc0307_plat,
	},
#endif
#ifdef CONFIG_GPIO_PCA953X
	{
		I2C_BOARD_INFO("pca9555", 0x27),
		.irq = LS1X_GPIO_FIRST_IRQ + PCA9555_GPIO_IRQ,
		.platform_data = &ls1x_i2c_pca9555_platdata,
	},
#endif
};

static struct resource ls1x_i2c0_resource[] = {
	[0]={
		.start	= LS1X_I2C0_BASE,
		.end	= LS1X_I2C0_BASE + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device ls1x_i2c0_device = {
	.name		= "ls1x-i2c",
	.id			= 0,
	.num_resources	= ARRAY_SIZE(ls1x_i2c0_resource),
	.resource	= ls1x_i2c0_resource,
};

static struct resource ls1x_i2c1_resource[] = {
	[0]={
		.start	= LS1X_I2C1_BASE,
		.end	= LS1X_I2C1_BASE + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device ls1x_i2c1_device = {
	.name		= "ls1x-i2c",
	.id			= 1,
	.num_resources	= ARRAY_SIZE(ls1x_i2c1_resource),
	.resource	= ls1x_i2c1_resource,
};

static struct resource ls1x_i2c2_resource[] = {
	[0]={
		.start	= LS1X_I2C2_BASE,
		.end	= LS1X_I2C2_BASE + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device ls1x_i2c2_device = {
	.name		= "ls1x-i2c",
	.id			= 2,
	.num_resources	= ARRAY_SIZE(ls1x_i2c2_resource),
	.resource	= ls1x_i2c2_resource,
};
#endif //#ifdef CONFIG_I2C_LS1X

#ifdef CONFIG_SENSORS_SHT15
#include <linux/sht15.h>
static struct sht15_platform_data platform_data_sht15 = {
	.gpio_data =  2,	/* 注意:可能需要修改sht15.c驱动中的gpio延时参数 */
	.gpio_sck  =  3,
	.supply_mv = 5000, /* 5000mV */
	.checksum = 1,
};

static struct platform_device sht15 = {
	.name = "sht10",
	.id = -1,
	.dev = {
		.platform_data = &platform_data_sht15,
	},
};
#endif

/*
 * lcd
 */
#if defined(CONFIG_FB_LOONGSON1)
#include "../video_modes.c"
#ifdef CONFIG_LS1X_FB0
static struct resource ls1x_fb0_resource[] = {
	[0] = {
		.start = LS1X_DC0_BASE,
		.end   = LS1X_DC0_BASE + 0x0010 - 1,	/* 1M? */
		.flags = IORESOURCE_MEM,
	},
};

struct ls1xfb_mach_info ls1x_lcd0_info = {
	.id			= "Graphic lcd",
	.modes			= video_modes,
	.num_modes		= ARRAY_SIZE(video_modes),
	.pix_fmt		= PIX_FMT_RGB565,
	.de_mode		= 0,	/* 注意：lcd是否使用DE模式 */
	/* 根据lcd屏修改invert_pixclock和invert_pixde参数(0或1)，部分lcd可能显示不正常 */
	.invert_pixclock	= 0,
	.invert_pixde	= 0,
};

struct platform_device ls1x_fb0_device = {
	.name			= "ls1x-fb",
	.id				= 0,
	.num_resources	= ARRAY_SIZE(ls1x_fb0_resource),
	.resource		= ls1x_fb0_resource,
	.dev			= {
		.platform_data = &ls1x_lcd0_info,
	}
};
#endif	//#ifdef CONFIG_LS1X_FB0
#endif	//#if defined(CONFIG_FB_LOONGSON1)

#define GPIO_BACKLIGHT_CTRL	53
#ifdef CONFIG_BACKLIGHT_GENERIC
#include <linux/backlight.h>
static void ls1x_bl_set_intensity(int intensity)
{
	if (intensity)
		gpio_direction_output(GPIO_BACKLIGHT_CTRL, 1);
	else
		gpio_direction_output(GPIO_BACKLIGHT_CTRL, 0);
}

static struct generic_bl_info ls1x_bl_info = {
	.name			= "ls1x-bl",
	.max_intensity		= 0xff,
	.default_intensity	= 0xff,
	.set_bl_intensity	= ls1x_bl_set_intensity,
};

static struct platform_device ls1x_bl_dev = {
	.name			= "generic-bl",
	.id			= 1,
	.dev = {
		.platform_data	= &ls1x_bl_info,
	},
};
#endif //#ifdef CONFIG_BACKLIGHT_GENERIC

#ifdef CONFIG_LCD_PLATFORM
#include <video/platform_lcd.h>
static void ls1x_lcd_power_set(struct plat_lcd_data *pd,
				   unsigned int power)
{
	if (power)
		gpio_direction_output(GPIO_BACKLIGHT_CTRL, 1);
	else
		gpio_direction_output(GPIO_BACKLIGHT_CTRL, 0);
}

static struct plat_lcd_data ls1x_lcd_power_data = {
	.set_power		= ls1x_lcd_power_set,
};

static struct platform_device ls1x_lcd_powerdev = {
	.name			= "platform-lcd",
	.dev.parent		= &ls1x_fb0_device.dev,
	.dev.platform_data	= &ls1x_lcd_power_data,
};
#endif //#ifdef CONFIG_LCD_PLATFORM

#ifdef CONFIG_BACKLIGHT_PWM
#include <linux/pwm_backlight.h>
static struct platform_pwm_backlight_data ls1x_backlight_data = {
	.pwm_id		= 2,
	.max_brightness	= 255,
	.dft_brightness	= 100,
	.pwm_period_ns	= 7812500,
};

static struct platform_device ls1x_pwm_backlight = {
	.name = "pwm-backlight",
	.dev = {
		.platform_data = &ls1x_backlight_data,
	},
};
#endif //#ifdef CONFIG_BACKLIGHT_PWM

//gmac
#ifdef CONFIG_STMMAC_ETH
void __init ls1x_gmac_setup(void)
{
#ifdef CONFIG_LS1X_GMAC0
{
	u32 x;
	x = __raw_readl(LS1X_MUX_CTRL1);
	#if defined(CONFIG_LS1X_GMAC0_100M)
	x = x | GMAC0_USE_TXCLK | GMAC0_USE_PWM01;
	#elif defined(CONFIG_LS1X_GMAC0_1000M)
	x = x & (~GMAC0_USE_TXCLK) & (~GMAC0_USE_PWM01);
	#endif
	__raw_writel(x & (~GMAC0_SHUT), LS1X_MUX_CTRL1);
}
#endif
#ifdef CONFIG_LS1X_GMAC1
{
	u32 x;
	x = __raw_readl(LS1X_MUX_CTRL0);
	x = x | GMAC1_USE_UART1 | GMAC1_USE_UART0;	/* 复用UART0&1 */
	__raw_writel(x, LS1X_MUX_CTRL0);
	x = __raw_readl(LS1X_MUX_CTRL1);
	#if defined(CONFIG_LS1X_GMAC1_100M)
	x = x | GMAC1_USE_TXCLK | GMAC1_USE_PWM23;
	#elif defined(CONFIG_LS1X_GMAC1_1000M)
	x = x & (~GMAC1_USE_TXCLK) & (~GMAC1_USE_PWM23);
	#endif
	__raw_writel(x & (~GMAC1_SHUT), LS1X_MUX_CTRL1);
}
#endif
}

#ifdef CONFIG_LS1X_GMAC0
static struct resource ls1x_mac0_resources[] = {
	[0] = {
		.start  = LS1X_GMAC0_BASE,
		.end    = LS1X_GMAC0_BASE + SZ_8K - 1,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.name   = "macirq",
		.start  = LS1X_GMAC0_IRQ,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct plat_stmmacenet_data ls1x_mac0_data = {
	.bus_id = 0,
	.pbl = 0,
	.has_gmac = 1,
	.enh_desc = 0,
};

struct platform_device ls1x_gmac0_mac = {
	.name           = "stmmaceth",
	.id             = 0,
	.num_resources  = ARRAY_SIZE(ls1x_mac0_resources),
	.resource       = ls1x_mac0_resources,
	.dev            = {
		.platform_data = &ls1x_mac0_data,
	},
};

static struct plat_stmmacphy_data  phy0_private_data = {
	.bus_id = 0,
#ifdef CONFIG_RTL8305SC
	.phy_addr = 4,
#else
	.phy_addr = 0,	/* -1自动检测 */
#endif
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_MII,
	
};

struct platform_device ls1x_gmac0_phy = {
	.name = "stmmacphy",
	.id = 0,
	.num_resources = 1,
	.resource = (struct resource[]){
		{
			.name = "phyirq",
			.start = PHY_POLL,
			.end = PHY_POLL,
			.flags = IORESOURCE_IRQ,
		},
	},
	.dev = {
		.platform_data = &phy0_private_data,
	},
};
#endif //#ifdef CONFIG_LS1X_GMAC0

#ifdef CONFIG_LS1X_GMAC1
static struct resource ls1x_mac1_resources[] = {
	[0] = {
		.start  = LS1X_GMAC1_BASE,
		.end    = LS1X_GMAC1_BASE + SZ_8K - 1,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.name   = "macirq",
		.start  = LS1X_GMAC1_IRQ,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct plat_stmmacenet_data ls1x_mac1_data = {
	.bus_id = 1,
	.pbl = 0,
	.has_gmac = 1,
	.enh_desc = 0,
};

struct platform_device ls1x_gmac1_mac = {
	.name           = "stmmaceth",
	.id             = 1,
	.num_resources  = ARRAY_SIZE(ls1x_mac1_resources),
	.resource       = ls1x_mac1_resources,
	.dev            = {
		.platform_data = &ls1x_mac1_data,
	},
};

static struct plat_stmmacphy_data  phy1_private_data = {
	.bus_id = 1,
	.phy_addr = 0,	/* -1自动检测 */
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_MII,
	
};

struct platform_device ls1x_gmac1_phy = {
	.name = "stmmacphy",
	.id = 1,
	.num_resources = 1,
	.resource = (struct resource[]){
		{
			.name = "phyirq",
			.start = PHY_POLL,
			.end = PHY_POLL,
			.flags = IORESOURCE_IRQ,
		},
	},
	.dev = {
		.platform_data = &phy1_private_data,
	},
};
#endif //#ifdef CONFIG_LS1X_GMAC1
#endif //#ifdef CONFIG_STMMAC_ETH

#ifdef CONFIG_SOUND_LS1X_AC97
static struct resource ls1x_ac97_resource[] = {
	[0]={
		.start	= LS1X_AC97_BASE,
		.end	= LS1X_AC97_BASE + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device ls1x_audio_device = {
	.name           = "ls1x-audio",
	.id             = -1,
	.num_resources	= ARRAY_SIZE(ls1x_ac97_resource),
	.resource		= ls1x_ac97_resource,
};
#endif

#ifdef CONFIG_SND_LS1X_SOC_AC97
static struct resource ls1x_ac97_resource[] = {
	[0]={
		.start	= LS1X_AC97_BASE,
		.end	= LS1X_AC97_BASE + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1]={
		.start	= LS1X_AC97_IRQ,
		.end	= LS1X_AC97_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device ls1x_ac97_device = {
	.name           = "ls1x-ac97",
	.id             = -1,
	.num_resources	= ARRAY_SIZE(ls1x_ac97_resource),
	.resource		= ls1x_ac97_resource,
};

static struct platform_device ls1x_stac_dev = {
	.name		= "ac97-codec",
	.id		= -1,
};
#endif

#ifdef CONFIG_SND_LS1X_SOC
static struct platform_device ls1x_pcm_device = {
	.name = "loongson1-pcm-audio",
	.id = -1,
};
#endif

#ifdef CONFIG_MTD_M25P80
static struct mtd_partition partitions[] = {
	[0] = {
		.name		= "pmon",
		.offset		= 0,
		.size		= 512 * 1024,	//512KB
	//	.mask_flags	= MTD_WRITEABLE,
	},
};

static struct flash_platform_data flash __maybe_unused = {
	.name		= "spi-flash",
	.parts		= partitions,
	.nr_parts	= ARRAY_SIZE(partitions),
	.type		= "w25x40",
};
#endif /* CONFIG_MTD_M25P80 */

#if defined(CONFIG_MMC_SPI) || defined(CONFIG_MMC_SPI_MODULE)
/* 开发板使用GPIO40(CAN1_RX)引脚作为MMC/SD卡的插拔探测引脚 */
#define DETECT_GPIO  41
#if 1
/* 轮询方式探测card的插拔 */
static int mmc_spi_get_cd(struct device *dev)
{
	return !gpio_get_value(DETECT_GPIO);
}
#else
#define MMC_SPI_CARD_DETECT_INT  (LS1X_GPIO_FIRST_IRQ + DETECT_GPIO)
/* 中断方式方式探测card的插拔，由于loongson1 CPU不支持双边沿触发 所以不建议使用中断方式 */
static int ls1b_mmc_spi_init(struct device *dev,
	irqreturn_t (*detect_int)(int, void *), void *data)
{
	return request_irq(MMC_SPI_CARD_DETECT_INT, detect_int,
		IRQF_TRIGGER_FALLING, "mmc-spi-detect", data);
}
/* 释放中断 */
static void ls1b_mmc_spi_exit(struct device *dev, void *data)
{
	free_irq(MMC_SPI_CARD_DETECT_INT, data);
}
#endif

static struct mmc_spi_platform_data mmc_spi __maybe_unused = {
	/* 中断方式方式探测card的插拔 */
//	.init = ls1b_mmc_spi_init,
//	.exit = ls1b_mmc_spi_exit,
//	.detect_delay = 1200,	/* msecs */
	/* 轮询方式方式探测card的插拔 */
	.get_cd = mmc_spi_get_cd,
	.caps = MMC_CAP_NEEDS_POLL,
};	
#endif  /* defined(CONFIG_MMC_SPI) || defined(CONFIG_MMC_SPI_MODULE) */

#ifdef CONFIG_TOUCHSCREEN_ADS7846
#include <linux/spi/ads7846.h>
#define ADS7846_GPIO_IRQ 60 /* 开发板触摸屏使用的外部中断 */
static struct ads7846_platform_data ads_info __maybe_unused = {
	.model				= 7846,
	.vref_delay_usecs	= 1,
	.keep_vref_on		= 0,
	.settle_delay_usecs	= 20,
//	.x_plate_ohms		= 800,
	.pressure_min		= 0,
	.pressure_max		= 2048,
	.debounce_rep		= 3,
	.debounce_max		= 10,
	.debounce_tol		= 50,
//	.get_pendown_state	= ads7846_pendown_state,
	.get_pendown_state	= NULL,
	.gpio_pendown		= ADS7846_GPIO_IRQ,
	.filter_init		= NULL,
	.filter 			= NULL,
	.filter_cleanup 	= NULL,
};
#endif /* TOUCHSCREEN_ADS7846 */

#ifdef CONFIG_LCD_JBT6K74
/* JBT6k74 display controller */
static void gta02_jbt6k74_probe_completed(struct device *dev)
{
//	pcf50633_bl_set_brightness_limit(gta02_pcf, 0x3f);
}

const struct jbt6k74_platform_data jbt6k74_pdata = {
//	.gpio_reset = 41,
};
#endif

#ifdef CONFIG_SPI_LS1X_SPI0
#include <linux/spi/spi_ls1x.h>
static struct spi_board_info ls1x_spi0_devices[] = {
#ifdef CONFIG_MTD_M25P80
	{
		.modalias	= "m25p80",
		.bus_num 		= 0,
		.chip_select	= SPI0_CS0,
		.max_speed_hz	= 60000000,
		.platform_data	= &flash,
	},
#endif
#ifdef CONFIG_SPI_MCP3201
	{
		.modalias	= "mcp3201",
		.bus_num 	= 0,
		.chip_select	= SPI0_CS3,
		.max_speed_hz	= 1000000,
	},
#endif
#ifdef CONFIG_TOUCHSCREEN_ADS7846
	{
		.modalias = "ads7846",
		.platform_data = &ads_info,
		.bus_num 		= 0,
		.chip_select 	= SPI0_CS1,
		.max_speed_hz 	= 2500000,
		.mode 			= SPI_MODE_1,
		.irq			= LS1X_GPIO_FIRST_IRQ + ADS7846_GPIO_IRQ,
	},
#endif
#if defined(CONFIG_MMC_SPI) || defined(CONFIG_MMC_SPI_MODULE)
	{
		.modalias		= "mmc_spi",
		.bus_num 		= 0,
		.chip_select	= SPI0_CS2,
		.max_speed_hz	= 25000000,
		.platform_data	= &mmc_spi,
		.mode = SPI_MODE_3,
	},
#endif
};
	
static struct resource ls1x_spi0_resource[] = {
	[0]={
		.start	= LS1X_SPI0_BASE,
		.end	= LS1X_SPI0_BASE + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	},
#if defined(CONFIG_SPI_IRQ_MODE)
	[1]={
		.start	= LS1X_SPI0_IRQ,
		.end	= LS1X_SPI0_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
#endif
};

#ifdef CONFIG_SPI_CS_USED_GPIO
static int spi0_gpios_cs[] =
	{ 27, 28, 29, 30 };
#endif

static struct ls1x_spi_platform_data ls1x_spi0_platdata = {
#ifdef CONFIG_SPI_CS_USED_GPIO
	.gpio_cs_count = ARRAY_SIZE(spi0_gpios_cs),
	.gpio_cs = spi0_gpios_cs,
#elif CONFIG_SPI_CS
	.cs_count = SPI0_CS3 + 1,
#endif
};

static struct platform_device ls1x_spi0_device = {
	.name		= "spi_ls1x",
	.id 		= 0,
	.num_resources	= ARRAY_SIZE(ls1x_spi0_resource),
	.resource	= ls1x_spi0_resource,
	.dev		= {
		.platform_data	= &ls1x_spi0_platdata,//&ls1x_spi_devices,
	},
};

#elif defined(CONFIG_SPI_GPIO)	/* 使用GPIO模拟SPI代替 */
struct spi_gpio_platform_data spi0_gpio_platform_data = {
	.sck = 24,	/*gpio24*/
	.mosi = 26,	/*gpio26*/
	.miso = 25,	/*gpio25*/
	.num_chipselect = 4,
};

static struct platform_device spi0_gpio_device = {
	.name = "spi_gpio",
	.id   = 2,	/* 用于区分spi0和spi1 */
	.dev = {
		.platform_data = &spi0_gpio_platform_data,
	},
};

static struct spi_board_info spi0_gpio_devices[] = {
#ifdef CONFIG_MTD_M25P80
	{
		.modalias	= "m25p80",
		.bus_num 		= 2,	/* 对应spigpio_device的.id=2 */
		.controller_data = (void *)27,	/*gpio27*/
		.chip_select	= 0,
		.max_speed_hz	= 60000000,
		.platform_data	= &flash,
	},
#endif
#ifdef CONFIG_SPI_MCP3201
	{
		.modalias	= "mcp3201",
		.bus_num 	= 2,
		.controller_data = (void *)30,	/*gpio30*/
		.chip_select	= 3, /*SPI0_CS2*/
		.max_speed_hz	= 1000000,
	},
#endif
#ifdef CONFIG_TOUCHSCREEN_ADS7846
	{
		.modalias = "ads7846",
		.platform_data = &ads_info,
		.bus_num 		= 2,	/* 对应spigpio_device的.id=2 */
		.controller_data = (void *)28,	/*gpio28*/
		.chip_select 	= 1,
		.max_speed_hz 	= 2500000,
		.mode 			= SPI_MODE_1,
		.irq			= LS1X_GPIO_FIRST_IRQ + ADS7846_GPIO_IRQ,
	},
#endif
#ifdef CONFIG_LCD_JBT6K74
	{
		.modalias	= "jbt6k74",
		.platform_data	= &jbt6k74_pdata,
		.bus_num	= 2,		/* 对应spigpio_device的.id=2 */
		.controller_data = (void *)43,	/*gpio43*/
		.chip_select = 2,
		/* irq */
		.max_speed_hz	= 100 * 1000,
	},
#endif
#if defined(CONFIG_MMC_SPI) || defined(CONFIG_MMC_SPI_MODULE)
	{
		.modalias		= "mmc_spi",
		.bus_num 		= 2,
		.controller_data = (void *)29,	/*gpio29*/
		.chip_select	= 2,
		.max_speed_hz	= 25000000,
		.platform_data	= &mmc_spi,
		.mode = SPI_MODE_3,
	},
#endif
};
#endif //#ifdef CONFIG_SPI_LS1X_SPI0

#ifdef CONFIG_SPI_LS1X_SPI1
/* SPI1 控制器 */
#include <linux/spi/spi_ls1x.h>
static struct spi_board_info ls1x_spi1_devices[] = {
	{
		.modalias		= "spidev",
		.bus_num 		= 1,
		.chip_select	= SPI1_CS0,
		.max_speed_hz	= 25000000,
		.mode = SPI_MODE_0,
	},
	{
		.modalias		= "spidev",
		.bus_num 		= 1,
		.chip_select	= SPI1_CS1,
		.max_speed_hz	= 25000000,
		.mode = SPI_MODE_0,
	},
	{
		.modalias		= "spidev",
		.bus_num 		= 1,
		.chip_select	= SPI1_CS2,
		.max_speed_hz	= 25000000,
		.mode = SPI_MODE_0,
	},
};

static struct resource ls1x_spi1_resource[] = {
	[0]={
		.start	= LS1X_SPI1_BASE,
		.end	= LS1X_SPI1_BASE + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	},
#if defined(CONFIG_SPI_IRQ_MODE)
	[1]={
		.start	= LS1X_SPI1_IRQ,
		.end	= LS1X_SPI1_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
#endif
};

#ifdef CONFIG_SPI_CS_USED_GPIO
static int spi1_gpios_cs[] =
	{ 38, 0, 1 };
#endif

static struct ls1x_spi_platform_data ls1x_spi1_platdata = {
#ifdef CONFIG_SPI_CS_USED_GPIO
	.gpio_cs_count = ARRAY_SIZE(spi1_gpios_cs),
	.gpio_cs = spi1_gpios_cs,
#elif CONFIG_SPI_CS
	.cs_count = SPI1_CS2 + 1,
#endif
};

static struct platform_device ls1x_spi1_device = {
	.name		= "spi_ls1x",
	.id 		= 1,
	.num_resources	= ARRAY_SIZE(ls1x_spi1_resource),
	.resource	= ls1x_spi1_resource,
	.dev		= {
		.platform_data	= &ls1x_spi1_platdata,//&ls1x_spi_devices,
	},
};
//#elif defined(CONFIG_SPI_GPIO)	/* 使用GPIO模拟SPI代替 */
#elif 0
struct spi_gpio_platform_data spi1_gpio_platform_data = {
	.sck = 39,	/*gpio39*/
	.mosi = 40,	/*gpio40*/
	.miso = 41,	/*gpio41*/
	.num_chipselect = 3,
};

static struct platform_device spi1_gpio_device = {
	.name = "spi_gpio",
//	.name = "spi_gpio_delay",
	.id   = 3,
	.dev = {
		.platform_data = &spi1_gpio_platform_data,
	},
};

static struct spi_board_info spi1_gpio_devices[] = {
	{
		.modalias		= "spidev",
		.bus_num 		= 3,
		.controller_data = (void *)38,	/*gpio38*/
		.chip_select	= 0,	/* SPI1_CS0 */
		.max_speed_hz	= 25000000,
		.mode = SPI_MODE_0,
	}, {
		.modalias		= "spidev",
		.bus_num 		= 3,
		.controller_data = (void *)0,	/*gpio0*/
		.chip_select	= 1,	/* SPI1_CS1 */
		.max_speed_hz	= 25000000,
		.mode = SPI_MODE_0,
	}, {
		.modalias		= "spidev",
		.bus_num 		= 3,
		.controller_data = (void *)1,	/*gpio1*/
		.chip_select	= 2,	/* SPI1_CS2 */
		.max_speed_hz	= 25000000,
		.mode = SPI_MODE_0,
	},
};
#endif	//#ifdef CONFIG_SPI_LS1X_SPI1

#if defined(CONFIG_SPI_GPIO) && defined(CONFIG_GPIO_74X165)
struct spi_gpio_platform_data spigpio_74x165_data = {
	.sck = 39,	/*gpio39 cp*/
	.mosi = SPI_GPIO_NO_MOSI,
	.miso = 41,	/*gpio41 q7*/
	.num_chipselect = 1,
};

static struct platform_device spigpio_74x165_device = {
	.name = "spi_gpio",
	.id   = 4,
	.dev = {
		.platform_data = &spigpio_74x165_data,
	},
};

#include <linux/spi/74x165.h>
static struct nxp_74x165_chip_platform_data nxp_74x165_info = {
	.base = 188,	/* 自定义的74x165 gpio起始号 */
	.latch = 38,	/*gpio38 pl*/
	.daisy_chained = 2,
};

static struct spi_board_info spi3_gpio_devices[] __maybe_unused = {
	{
		.modalias	= "74x165",
		.bus_num 		= 4,	/* 对应spigpio_74x165_device的 .id=2 */
		.controller_data = (void *)SPI_GPIO_NO_CHIPSELECT,
		.chip_select	= 0,
		.max_speed_hz	= 1000000,
		.platform_data	= &nxp_74x165_info,
		.mode = SPI_MODE_2,
	},
};
#endif	//#if defined(CONFIG_SPI_GPIO) && defined(CONFIG_GPIO_74X165)

/************************************************/	//GPIO && buzzer && button
#ifdef CONFIG_KEYBOARD_GPIO_POLLED
static struct gpio_keys_button ls1b_gpio_button[] = {
	[0] = {
		.code		= 'A',
		.gpio	 	= 37,
		.desc		= "SW1",
	},
	[1] = {
		.code		= 'B',
		.gpio	 	= 38,
		.desc		= "SW2",
	},
	[2] = {
		.code		= 'C',
		.gpio	 	= 39,
		.desc		= "SW3",
	},
	[3] = {
		.code		= 'D',
		.gpio	 	= 40,
		.desc		= "SW4",
	},
	[4] = {
		.code		= 'E',
		.gpio	 	= 41,
		.desc		= "SW5",
	},
};

static struct gpio_keys_platform_data ls1b_gpio_key_dat = {
	.buttons 		= ls1b_gpio_button,
	.nbuttons 		= 5, 
	.poll_interval	= 200,
};

static struct platform_device ls1b_gpio_key_device = {
	.name 	= "gpio-keys-polled",
	.id	= -1,
	.dev	= {
		.platform_data = &ls1b_gpio_key_dat,
	},
};
#endif //#ifdef CONFIG_KEYBOARD_GPIO_POLLED

/**
 * Rotary encoder input device
 */
#ifdef CONFIG_INPUT_GPIO_ROTARY_ENCODER
#define GPIO_ROTARY_A 59
#define GPIO_ROTARY_B 51
#define GPIO_KEY_C 53

static struct rotary_encoder_platform_data raumfeld_rotary_encoder_info = {
	.steps		= 30,
	.axis		= REL_X,
	.relative_axis	= 1,
	.rollover	= false,
	.gpio_a		= GPIO_ROTARY_A,
	.gpio_b		= GPIO_ROTARY_B,
	.gpio_c		= GPIO_KEY_C,
	.debounce_ms	= 10,
	.active_low		= 1,
	.key		= KEY_ENTER,
	.inverted_a	= 0,
	.inverted_b	= 0,
	.half_period	= 1,
};

static struct platform_device rotary_encoder_device = {
	.name		= "rotary-encoder",
	.id		= 0,
	.dev		= {
		.platform_data = &raumfeld_rotary_encoder_info,
	}
};
#endif //#ifdef CONFIG_INPUT_GPIO_ROTARY_ENCODER

/* matrix keypad */
#if defined(CONFIG_KEYBOARD_MATRIX) || defined(CONFIG_KEYBOARD_MATRIX_MODULE)
/*
static const uint32_t ls1bkbd_keymap[] = {
	KEY(0, 0, KEY_A),
	KEY(0, 1, KEY_B),
	KEY(0, 2, KEY_C),
	KEY(0, 3, KEY_D),

	KEY(1, 0, KEY_E),
	KEY(1, 1, KEY_F),
	KEY(1, 2, KEY_G),
	KEY(1, 3, KEY_H),
};
*/
static const uint32_t ls1bkbd_keymap[] = {
	KEY(0, 0, KEY_1),
	KEY(0, 1, KEY_2),
	KEY(0, 2, KEY_3),
	KEY(0, 3, KEY_4),

	KEY(1, 0, KEY_5),
	KEY(1, 1, KEY_6),
	KEY(1, 2, KEY_7),
	KEY(1, 3, KEY_8),
	
	KEY(2, 0, KEY_9),
	KEY(2, 1, KEY_0),
	KEY(2, 2, KEY_A),
	KEY(2, 3, KEY_B),
	
	KEY(3, 0, KEY_C),
	KEY(3, 1, KEY_D),
	KEY(3, 2, KEY_E),
	KEY(3, 3, KEY_F),
};

static struct matrix_keymap_data ls1bkbd_keymap_data = {
	.keymap			= ls1bkbd_keymap,
	.keymap_size	= ARRAY_SIZE(ls1bkbd_keymap),
};
/*
static const int ls1bkbd_row_gpios[] =
	{ 30, 28 };
static const int ls1bkbd_col_gpios[] =
	{ 29, 58, 50, 52 };
*/
static const int ls1bkbd_row_gpios[] =
	{ 16, 17, 18, 19 };	//gpio 16 17 18 19
static const int ls1bkbd_col_gpios[] =
	{ 20, 21, 22, 23 };	//gpio 20 21 22 23

static struct matrix_keypad_platform_data ls1bkbd_pdata = {
	.keymap_data		= &ls1bkbd_keymap_data,
	.row_gpios			= ls1bkbd_row_gpios,
	.col_gpios			= ls1bkbd_col_gpios,
	.num_row_gpios		= ARRAY_SIZE(ls1bkbd_row_gpios),
	.num_col_gpios		= ARRAY_SIZE(ls1bkbd_col_gpios),
	.col_scan_delay_us	= 2,
	.debounce_ms		= 24,
	.active_low			= 1,
	.wakeup				= 1,
	.no_autorepeat		= 0,
};

static struct platform_device ls1bkbd_device = {
	.name	= "matrix-keypad",
	.id		= -1,
	.dev	= {
		.platform_data = &ls1bkbd_pdata,
	},
};
#endif	//#if defined(CONFIG_KEYBOARD_MATRIX) || defined(CONFIG_KEYBOARD_MATRIX_MODULE)

#ifdef CONFIG_FB_SSD1305
static struct ssd1305_platform_data ssd1305_pdata = {
	.gpio_outpu = (unsigned int)LS1X_GPIO_OUT0,
	.gpios_res = 17,
	.gpios_cs = 16,
	.gpios_dc = 18,
	.gpios_rd = 20,
	.gpios_wr = 19,
	
	.gpios_d0 = 8,
	.gpios_d1 = 9,
	.gpios_d2 = 10,
	.gpios_d3 = 11,
	.gpios_d4 = 12,
	.gpios_d5 = 13,
	.gpios_d6 = 14,
	.gpios_d7 = 15,
	.datas_offset = 8,
};

struct platform_device ssd1305fb_device = {
	.name	= "ssd1305fb",
	.id		= -1,
	.dev	= {
		.platform_data = &ssd1305_pdata,
	},
};
#endif //#ifdef CONFIG_FB_SSD1305

#ifdef CONFIG_FB_ST7565
#include <linux/st7565.h>
static struct st7565_platform_data st7565_pdata = {
	.gpio_outpu = (unsigned int)LS1X_GPIO_OUT0,
	.gpios_res = 17,
	.gpios_cs = 16,
	.gpios_dc = 18,
	.gpios_rd = 20,
	.gpios_wr = 19,
	
	.gpios_d0 = 8,
	.gpios_d1 = 9,
	.gpios_d2 = 10,
	.gpios_d3 = 11,
	.gpios_d4 = 12,
	.gpios_d5 = 13,
	.gpios_d6 = 14,
	.gpios_d7 = 15,
	.datas_offset = 8,
};

struct platform_device st7565fb_device = {
	.name	= "st7565fb",
	.id		= -1,
	.dev	= {
		.platform_data = &st7565_pdata,
	},
};
#endif //#ifdef CONFIG_FB_ST7565

#ifdef CONFIG_FB_ST7920
static struct st7920_platform_data st7920_pdata = {
	.gpio_outpu = (unsigned int)LS1X_GPIO_OUT0,
	.gpios_res = 8,
	.gpios_cs = 11,
	.gpios_sid = 9,
	.gpios_sck = 10,

	.datas_offset = 8,
};

struct platform_device st7920fb_device = {
	.name	= "st7920fb",
	.id		= -1,
	.dev	= {
		.platform_data = &st7920_pdata,
	},
};
#endif //#ifdef CONFIG_FB_ST7920

#ifdef CONFIG_KEYBOARD_74X165_GPIO_POLLED
static struct gpio_keys_button gen74x165_gpio_keys_table[] = {
	{
		.code		= KEY_0,
		.active_low	= 1,
	}, {
		.code		= KEY_1,
		.active_low	= 1,
	}, {
		.code		= KEY_2,
		.active_low	= 1,
	}, {
		.code		= KEY_3,
		.active_low	= 1,
	}, {
		.code		= KEY_4,
		.active_low	= 1,
	}, {
		.code		= KEY_5,
		.active_low	= 1,
	}, {
		.code		= KEY_6,
		.active_low	= 1,
	}, {
		.code		= KEY_7,
		.active_low	= 1,
	}, {
		.code		= KEY_8,
		.active_low	= 1,
	}, {
		.code		= KEY_9,
		.active_low	= 1,
	}, {
		.code		= KEY_A,
		.active_low	= 1,
	}, {
		.code		= KEY_B,
		.active_low	= 1,
	}, {
		.code		= KEY_C,
		.active_low	= 1,
	}, {
		.code		= KEY_D,
		.active_low	= 1,
	}, {
		.code		= KEY_E,
		.active_low	= 1,
	}, {
		.code		= KEY_F,
		.active_low	= 1,
	},
};

static struct gen_74x165_platform_data gen74x165_gpio_keys_info = {
	.q7 = 41,
	.cp = 39,
	.pl = 38,
	.debounce_interval = 1,
	.buttons	= gen74x165_gpio_keys_table,
	.nbuttons	= ARRAY_SIZE(gen74x165_gpio_keys_table),
	.poll_interval	= 50, /* default to 50ms */
};

static struct platform_device gen74x165_gpio_keys_device = {
	.name		= "gen74x165_gpio-keys-polled",
	.dev		= {
		.platform_data	= &gen74x165_gpio_keys_info,
	},
};
#endif //#ifdef CONFIG_KEYBOARD_74X165_GPIO_POLLED

#ifdef CONFIG_LEDS_PWM
#include <linux/leds_pwm.h>
static struct led_pwm ls1x_pwm_leds[] = {
	{
		.name		= "ls1x_pwm_led1",
		.pwm_id		= 2,
		.max_brightness	= 255,
		.pwm_period_ns	= 7812500,
	},
	{
		.name		= "ls1x_pwm_led2",
		.pwm_id		= 3,
		.max_brightness	= 255,
		.pwm_period_ns	= 7812500,
	},
};

static struct led_pwm_platform_data ls1x_pwm_data = {
	.num_leds	= ARRAY_SIZE(ls1x_pwm_leds),
	.leds		= ls1x_pwm_leds,
};

static struct platform_device ls1x_leds_pwm = {
	.name	= "leds_pwm",
	.id		= -1,
	.dev	= {
		.platform_data = &ls1x_pwm_data,
	},
};
#endif //#ifdef CONFIG_LEDS_PWM

#ifdef CONFIG_CAN_SJA1000_PLATFORM
#include <linux/can/platform/sja1000.h>
#ifdef CONFIG_LS1X_CAN0
static struct resource ls1x_sja1000_resources_0[] = {
	{
		.start   = LS1X_CAN0_BASE,
		.end     = LS1X_CAN0_BASE + SZ_16K - 1,
		.flags   = IORESOURCE_MEM | IORESOURCE_MEM_8BIT,
	}, {
		.start   = LS1X_CAN0_IRQ,
		.end     = LS1X_CAN0_IRQ,
		.flags   = IORESOURCE_IRQ,
	},
};

static struct sja1000_platform_data ls1x_sja1000_platform_data_0 = {
	.ocr		= OCR_TX1_PULLDOWN | OCR_TX0_PUSHPULL,
	.cdr		= CDR_CBP,
};

static struct platform_device ls1x_sja1000_0 = {
	.name = "sja1000_platform",
	.id = 0,
	.dev = {
		.platform_data = &ls1x_sja1000_platform_data_0,
	},
	.resource = ls1x_sja1000_resources_0,
	.num_resources = ARRAY_SIZE(ls1x_sja1000_resources_0),
};
#endif	//#ifdef CONFIG_LS1X_CAN0
#ifdef CONFIG_LS1X_CAN1
static struct resource ls1x_sja1000_resources_1[] = {
	{
		.start   = LS1X_CAN1_BASE,
		.end     = LS1X_CAN1_BASE + SZ_16K - 1,
		.flags   = IORESOURCE_MEM | IORESOURCE_MEM_8BIT,
	}, {
		.start   = LS1X_CAN1_IRQ,
		.end     = LS1X_CAN1_IRQ,
		.flags   = IORESOURCE_IRQ,
	},
};

static struct sja1000_platform_data ls1x_sja1000_platform_data_1 = {
	.ocr		= OCR_TX1_PULLDOWN | OCR_TX0_PUSHPULL,
	.cdr		= CDR_CBP,
};

static struct platform_device ls1x_sja1000_1 = {
	.name = "sja1000_platform",
	.id = 1,
	.dev = {
		.platform_data = &ls1x_sja1000_platform_data_1,
	},
	.resource = ls1x_sja1000_resources_1,
	.num_resources = ARRAY_SIZE(ls1x_sja1000_resources_1),
};
#endif //#ifdef CONFIG_LS1X_CAN1

static void ls1x_can_setup(void)
{
	struct sja1000_platform_data *sja1000_pdata;
	struct clk *clk;
	u32 x;

	clk = clk_get(NULL, "apb");
	if (IS_ERR(clk))
		panic("unable to get apb clock, err=%ld", PTR_ERR(clk));

	#ifdef CONFIG_LS1X_CAN0
	sja1000_pdata = &ls1x_sja1000_platform_data_0;
	sja1000_pdata->osc_freq = clk_get_rate(clk);
	#endif
	#ifdef CONFIG_LS1X_CAN1
	sja1000_pdata = &ls1x_sja1000_platform_data_1;
	sja1000_pdata->osc_freq = clk_get_rate(clk);
	#endif

#ifdef CONFIG_LS1B_MACH
	#ifdef CONFIG_LS1X_CAN0
	/* CAN0复用设置 */
/*	gpio_request(38, NULL);
	gpio_request(39, NULL);
	gpio_free(38);
	gpio_free(39);*/
	/* 清除与 SPI1 UART1_2 的复用  */
	x = __raw_readl(LS1X_MUX_CTRL1);
	x = x & (~SPI1_USE_CAN) & (~UART1_2_USE_CAN0);
	__raw_writel(x, LS1X_MUX_CTRL1);
	/* 清除与 I2C1 的复用  */
	x = __raw_readl(LS1X_MUX_CTRL0);
	x = x & (~I2C1_USE_CAN0);
	__raw_writel(x, LS1X_MUX_CTRL0);
	#endif
	#ifdef CONFIG_LS1X_CAN1
	/* CAN1复用设置 */
/*	gpio_request(40, NULL);
	gpio_request(41, NULL);
	gpio_free(40);
	gpio_free(41);*/
	/* 清除与 SPI1 UART1_3 的复用  */
	x = __raw_readl(LS1X_MUX_CTRL1);
	x = x & (~SPI1_USE_CAN) & (~UART1_3_USE_CAN1);
	__raw_writel(x, LS1X_MUX_CTRL1);
	/* 清除与 I2C2 的复用  */
	x = __raw_readl(LS1X_MUX_CTRL0);
	x = x & (~I2C2_USE_CAN1);
	__raw_writel(x, LS1X_MUX_CTRL0);
	#endif
#endif
}
#endif //#ifdef CONFIG_CAN_SJA1000_PLATFORM

#if defined(CONFIG_W1_MASTER_GPIO) || defined(CONFIG_W1_MASTER_GPIO_MODULE)
#include <linux/w1-gpio.h>
static struct w1_gpio_platform_data w1_gpio_pdata = {
	/* If you choose to use a pin other than PB16 it needs to be 3.3V */
	.pin		= 33,
//	.is_open_drain  = 1,
};

static struct platform_device w1_device = {
	.name		= "w1-gpio",
	.id			= -1,
	.dev.platform_data	= &w1_gpio_pdata,
};
#endif

#if defined(CONFIG_SENSORS_AM2301) || defined(CONFIG_SENSORS_AM2301_MODULE)
#include <linux/am2301.h>
static struct am2301_platform_data am2301_pdata = {
	.pin		= 32,
//	.is_open_drain  = 1,
};

static struct platform_device am2301_device = {
	.name		= "am2301",
	.id			= 0,
	.dev.platform_data	= &am2301_pdata,
};
#endif

#if defined(CONFIG_KEYBOARD_GPIO) || defined(CONFIG_KEYBOARD_GPIO_MODULE)
#include <linux/gpio_keys.h>
static struct gpio_keys_button gpio_buttons[] = {
	{
		.code			= KEY_A,
		.gpio			= 38,
		.active_low		= 1,
		.desc			= "test0",
//		.wakeup			= 1,
		.debounce_interval	= 10, /* debounce ticks interval in msecs */
	},
	{
		.code			= KEY_B,
		.gpio			= 39,
		.active_low		= 1,
		.desc			= "test1",
//		.wakeup			= 1,
		.debounce_interval	= 10, /* debounce ticks interval in msecs */
	},
};

static struct gpio_keys_platform_data gpio_key_info = {
	.buttons	= gpio_buttons,
	.nbuttons	= ARRAY_SIZE(gpio_buttons),
	.rep	= 1,	/* enable input subsystem auto repeat */
};

static struct platform_device keys_gpio = {
	.name	= "gpio-keys",
	.id	= -1,
	.dev	= {
		.platform_data	= &gpio_key_info,
	},
};
#endif

#if defined(CONFIG_LEDS_GPIO) || defined(CONFIG_LEDS_GPIO_MODULE)
#include <linux/leds.h>
struct gpio_led gpio_leds[] = {
	{
		.name			= "led_green0",
		.gpio			= 38,
		.active_low		= 1,
		.default_trigger	= "timer",
		.default_state	= LEDS_GPIO_DEFSTATE_ON,
	},
	{
		.name			= "led_green1",
		.gpio			= 39,
		.active_low		= 1,
		.default_trigger	= "heartbeat",
		.default_state	= LEDS_GPIO_DEFSTATE_ON,
	},
	{
		.name			= "buzzer",	/* 这里使用控制led的方式控制蜂鸣器 */
		.gpio			= 40,
		.active_low		= 0,
//		.default_trigger	= "heartbeat",	/* 触发方式 */
		.default_trigger	= "timer",	/* 触发方式 */
//		.default_trigger	= NULL,
		.default_state	= LEDS_GPIO_DEFSTATE_ON,
	},
};

static struct gpio_led_platform_data gpio_led_info = {
	.leds		= gpio_leds,
	.num_leds	= ARRAY_SIZE(gpio_leds),
};

static struct platform_device leds = {
	.name	= "leds-gpio",
	.id	= -1,
	.dev	= {
		.platform_data	= &gpio_led_info,
	}
};
#endif //#if defined(CONFIG_LEDS_GPIO) || defined(CONFIG_LEDS_GPIO_MODULE)


/***********************************************/
static struct platform_device *ls1b_platform_devices[] __initdata = {
	&ls1x_uart_device,

#ifdef CONFIG_LS1X_FB0
	&ls1x_fb0_device,
#endif

#ifdef CONFIG_MTD_NAND_LS1X
	&ls1x_nand_device,
#endif

#ifdef CONFIG_USB_OHCI_HCD_LS1X
	&ls1x_ohci_device,
#endif
#ifdef CONFIG_USB_OHCI_HCD_PLATFORM
	&ls1x_ohci_device,
#endif
#ifdef CONFIG_USB_EHCI_HCD_LS1X
	&ls1x_ehci_device,
#endif

#ifdef CONFIG_STMMAC_ETH
#ifdef CONFIG_LS1X_GMAC0
	&ls1x_gmac0_mac,
	&ls1x_gmac0_phy,
#endif
#ifdef CONFIG_LS1X_GMAC1
	&ls1x_gmac1_mac,
	&ls1x_gmac1_phy,
#endif
#endif

#ifdef CONFIG_SOUND_LS1X_AC97
	&ls1x_audio_device,
#endif

#ifdef CONFIG_SND_LS1X_SOC
	&ls1x_pcm_device,
#endif
#ifdef CONFIG_SND_LS1X_SOC_AC97
	&ls1x_ac97_device,
	&ls1x_stac_dev,
#endif

#if defined(CONFIG_SPI_LS1X_SPI0)
	&ls1x_spi0_device,
#elif defined(CONFIG_SPI_GPIO)
	&spi0_gpio_device,
#endif

#if defined(CONFIG_SPI_LS1X_SPI1)
	&ls1x_spi1_device,
//elif defined(CONFIG_SPI_GPIO)
#elif 0
	&spi1_gpio_device,
#endif

#if defined(CONFIG_SPI_GPIO) && defined(CONFIG_GPIO_74X165)
	&spigpio_74x165_device,
#endif

#ifdef CONFIG_LS1X_WDT
	&ls1x_wdt_device,
#endif

#ifdef CONFIG_RTC_DRV_RTC_LOONGSON1
	&ls1x_rtc_device,
#endif
#ifdef CONFIG_RTC_DRV_TOY_LOONGSON1
	&ls1x_toy_device,
#endif

#ifdef CONFIG_I2C_LS1X
	&ls1x_i2c0_device,
	&ls1x_i2c1_device,
	&ls1x_i2c2_device,
#endif

#ifdef CONFIG_SENSORS_SHT15
	&sht15,
#endif

#ifdef CONFIG_GPIO_PCA953X
#if defined(CONFIG_LEDS_GPIO) || defined(CONFIG_LEDS_GPIO_MODULE)
	&pca9555_leds,
#endif
#endif

#ifdef CONFIG_KEYBOARD_GPIO_POLLED
	&ls1b_gpio_key_device,
#endif

#ifdef CONFIG_INPUT_GPIO_ROTARY_ENCODER
	&rotary_encoder_device,
#endif

#if defined(CONFIG_KEYBOARD_MATRIX) || defined(CONFIG_KEYBOARD_MATRIX_MODULE)
	&ls1bkbd_device,
#endif

#ifdef CONFIG_FB_SSD1305
	&ssd1305fb_device,
#endif
#ifdef CONFIG_FB_ST7565
	&st7565fb_device,
#endif
#ifdef CONFIG_FB_ST7920
	&st7920fb_device,
#endif

#ifdef CONFIG_KEYBOARD_74X165_GPIO_POLLED
	&gen74x165_gpio_keys_device,
#endif

#ifdef CONFIG_LEDS_PWM
	&ls1x_leds_pwm,
#endif

#ifdef CONFIG_CAN_SJA1000_PLATFORM
#ifdef CONFIG_LS1X_CAN0
	&ls1x_sja1000_0,
#endif
#ifdef CONFIG_LS1X_CAN1
	&ls1x_sja1000_1,
#endif
#endif

#ifdef CONFIG_BACKLIGHT_GENERIC
	&ls1x_bl_dev,
#endif
#ifdef CONFIG_LCD_PLATFORM
	&ls1x_lcd_powerdev,
#endif
#ifdef CONFIG_BACKLIGHT_PWM
	&ls1x_pwm_backlight,
#endif
#if defined(CONFIG_W1_MASTER_GPIO) || defined(CONFIG_W1_MASTER_GPIO_MODULE)
	&w1_device,
#endif
#if defined(CONFIG_SENSORS_AM2301) || defined(CONFIG_SENSORS_AM2301_MODULE)
	&am2301_device,
#endif
#if defined(CONFIG_KEYBOARD_GPIO) || defined(CONFIG_KEYBOARD_GPIO_MODULE)
	&keys_gpio,
#endif
#if defined(CONFIG_LEDS_GPIO) || defined(CONFIG_LEDS_GPIO_MODULE)
	&leds,
#endif
};

int __init ls1b_platform_init(void)
{
	ls1x_serial_setup();

#ifdef CONFIG_STMMAC_ETH
	ls1x_gmac_setup();
#endif

#ifdef CONFIG_CAN_SJA1000_PLATFORM
	ls1x_can_setup();
#endif	//#ifdef CONFIG_CAN_SJA1000_PLATFORM

#ifdef CONFIG_I2C_LS1X
	i2c_register_board_info(0, ls1x_i2c0_devs, ARRAY_SIZE(ls1x_i2c0_devs));
#endif

#if defined(CONFIG_MMC_SPI) || defined(CONFIG_MMC_SPI_MODULE)
	/* 轮询方式或中断方式探测card的插拔 */
	gpio_request(DETECT_GPIO, "MMC_SPI GPIO detect");
	gpio_direction_input(DETECT_GPIO);		/* 输入使能 */
#endif

#if defined(CONFIG_SPI_LS1X_SPI0)
	/* disable gpio24-27 */
	*(volatile unsigned int *)0xbfd010c0 &= ~(0xf << 24);
	spi_register_board_info(ls1x_spi0_devices, ARRAY_SIZE(ls1x_spi0_devices));
#elif defined(CONFIG_SPI_GPIO)
	spi_register_board_info(spi0_gpio_devices, ARRAY_SIZE(spi0_gpio_devices));
#endif

#if defined(CONFIG_SPI_LS1X_SPI1)
	/* 使能SPI1控制器，与CAN0 CAN1 GPIO38-GPIO41复用,同时占用PWM0 PWM1用于片选. */
	/* 编程需要注意 */
	*(volatile unsigned int *)0xbfd00424 |= (0x3 << 23);
	/* disable gpio38-41 */
	*(volatile unsigned int *)0xbfd010c4 &= ~(0xf << 6);
	spi_register_board_info(ls1x_spi1_devices, ARRAY_SIZE(ls1x_spi1_devices));
//#elif defined(CONFIG_SPI_GPIO)
#elif 0
	spi_register_board_info(spi1_gpio_devices, ARRAY_SIZE(spi1_gpio_devices));
#endif

#if defined(CONFIG_SPI_GPIO) && defined(CONFIG_GPIO_74X165)
	spi_register_board_info(spi3_gpio_devices, ARRAY_SIZE(spi3_gpio_devices));
#endif

#ifdef CONFIG_BACKLIGHT_GENERIC
	gpio_request(GPIO_BACKLIGHT_CTRL, "backlight");
#endif
#ifdef CONFIG_LCD_PLATFORM
	gpio_request(GPIO_BACKLIGHT_CTRL, "lcd_enable");
#endif
	return platform_add_devices(ls1b_platform_devices, ARRAY_SIZE(ls1b_platform_devices));
}

arch_initcall(ls1b_platform_init);

