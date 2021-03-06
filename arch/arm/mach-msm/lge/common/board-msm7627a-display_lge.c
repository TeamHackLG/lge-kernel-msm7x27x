/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/bootmem.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <asm/mach-types.h>
#include <mach/msm_bus_board.h>
#include <mach/msm_memtypes.h>
#include <mach/board.h>
#include <mach/gpiomux.h>
#include <mach/socinfo.h>
#include <mach/rpc_pmapp.h>
#include <linux/fb.h>

#include "devices.h"
#include "board-msm7627a.h"
#include CONFIG_LGE_BOARD_HEADER_FILE
#include <mach/lge/lge_proc_comm.h>

#ifdef CONFIG_FB_MSM_TRIPLE_BUFFER
#define MSM_FB_SIZE		0x1C2000
#else
#define MSM_FB_SIZE		0x32A000
#endif

#define GPIO_LCD_RESET_N 125

#define MSM_V4L2_VIDEO_OVERLAY_BUF_SIZE 460800

static struct resource msm_fb_resources[] = {
	{
		.flags  = IORESOURCE_DMA,
	}
};

#ifdef CONFIG_MSM_V4L2_VIDEO_OVERLAY_DEVICE
static struct resource msm_v4l2_video_overlay_resources[] = {
	{
		.flags = IORESOURCE_DMA,
	}
};
#endif

static int msm_fb_detect_panel(const char *name)
{
	int ret = -ENODEV;

	if (!strncmp(name, "lcdc_toshiba_fwvga_pt", 21))
		ret = 0;

	return ret;
}

static struct msm_fb_platform_data msm_fb_pdata = {
	.detect_client = msm_fb_detect_panel,
};

static struct platform_device msm_fb_device = {
	.name   = "msm_fb",
	.id     = 0,
	.num_resources  = ARRAY_SIZE(msm_fb_resources),
	.resource       = msm_fb_resources,
	.dev    = {
		.platform_data = &msm_fb_pdata,
	}
};

#ifdef CONFIG_MSM_V4L2_VIDEO_OVERLAY_DEVICE
static struct platform_device msm_v4l2_video_overlay_device = {
		.name	= "msm_v4l2_overlay_pd",
		.id	= 0,
		.num_resources	= ARRAY_SIZE(msm_v4l2_video_overlay_resources),
		.resource	= msm_v4l2_video_overlay_resources,
	};
#endif

static struct platform_device *msm_fb_devices[] __initdata = {
	&msm_fb_device,
#ifdef CONFIG_MSM_V4L2_VIDEO_OVERLAY_DEVICE
	&msm_v4l2_video_overlay_device,
#endif
};

void __init msm_msm7627a_allocate_memory_regions(void)
{
	void *addr;
	unsigned long fb_size;

	fb_size = MSM_FB_SIZE;
	addr = alloc_bootmem_align(fb_size, 0x1000);
	msm_fb_resources[0].start = __pa(addr);
	msm_fb_resources[0].end = msm_fb_resources[0].start + fb_size - 1;
	pr_info("allocating %lu bytes at %p (%lx physical) for fb\n", fb_size,
						addr, __pa(addr));

#ifdef CONFIG_MSM_V4L2_VIDEO_OVERLAY_DEVICE
	fb_size = MSM_V4L2_VIDEO_OVERLAY_BUF_SIZE;
	addr = alloc_bootmem_align(fb_size, 0x1000);
	msm_v4l2_video_overlay_resources[0].start = __pa(addr);
	msm_v4l2_video_overlay_resources[0].end =
		msm_v4l2_video_overlay_resources[0].start + fb_size - 1;
	pr_debug("allocating %lu bytes at %p (%lx physical) for v4l2\n",
		fb_size, addr, __pa(addr));
#endif

}

static struct msm_panel_common_pdata mdp_pdata = {
	.gpio = 97,
	.mdp_rev = MDP_REV_303,
	.cont_splash_enabled = 0x1,
};

#ifdef CONFIG_BACKLIGHT_RT9396
static struct gpio_i2c_pin bl_i2c_pin = {
	.sda_pin = 112,
	.scl_pin = 111,
	.reset_pin = 124,
};

static struct i2c_gpio_platform_data bl_i2c_pdata = {
	.sda_is_open_drain = 0,
	.scl_is_open_drain = 0,
	.udelay = 2,
};

static struct platform_device bl_i2c_device = {
	.name = "i2c-gpio",
	.dev.platform_data = &bl_i2c_pdata,
};
#endif

#ifdef CONFIG_BACKLIGHT_LGE_RT8966
static struct lge_backlight_platform_data rt8966bl_data = {
	.version = 8966,
};

static struct platform_device bl_device = {
	.name = "rt8966-bl",
	.dev.platform_data = &rt8966bl_data,
};

void __init msm7x27a_common_init_backlight(void)
{
	platform_device_register(&bl_device);
}
#endif

#ifdef CONFIG_BACKLIGHT_RT9396
static struct lge_backlight_platform_data rt9396bl_data = {
	.gpio = 124,
	.version = 9396,
};

static struct i2c_board_info bl_i2c_bdinfo[] = {
	[0] = {
		I2C_BOARD_INFO("rt9396bl", 0x54),
		.type = "rt9396bl",
	},
};

void __init msm7x27a_common_init_i2c_backlight(int bus_num)
{
	bl_i2c_device.id = bus_num;
	bl_i2c_bdinfo[0].platform_data = &rt9396bl_data;

	/* workaround for HDK rev_a no pullup */
	lge_init_gpio_i2c_pin_pullup(&bl_i2c_pdata, bl_i2c_pin, &bl_i2c_bdinfo[0]);
	i2c_register_board_info(bus_num, &bl_i2c_bdinfo[0], 1);
	platform_device_register(&bl_i2c_device);
}
#endif /*CONFIG_BACKLIGHT_RT9396*/

static int ebi2_tovis_power_save(int on);
static struct msm_panel_ilitek_pdata ebi2_tovis_panel_data = {
	.gpio = GPIO_LCD_RESET_N,
	.lcd_power_save = ebi2_tovis_power_save,
	.maker_id = PANEL_ID_TOVIS,
	.initialized = 1,
};

static struct platform_device ebi2_tovis_panel_device = {
	.name	= "ebi2_tovis_qvga",
	.id	= 0,
	.dev	= {
		.platform_data = &ebi2_tovis_panel_data,
	}
};

/* input platform device */
static struct platform_device *common_panel_devices[] __initdata = {
	&ebi2_tovis_panel_device,
};

#define REGULATOR_OP(name, op, level) \
	do { \
		vreg = regulator_get(0, name); \
		regulator_set_voltage(vreg, level, level); \
		if (regulator_##op(vreg)) \
			pr_err("%s: %s vreg operation failed\n", \
				(regulator_##op == regulator_enable) ? "regulator_enable" \
				: "regulator_disable", name); \
	} while (0)


static char *msm_fb_vreg[] = {
	"wlan_tcx0",
	"emmc",
};

static int mddi_power_save_on;

#ifdef CONFIG_MACH_MSM7X25A_V1
extern int lge_rt8966a_backlight_control(int onoff);
extern int StatusBacklightOnOff;
#endif

static int ebi2_tovis_power_save(int on)
{
	struct regulator *vreg;
	int flag_on = !!on;

	pr_debug("%s: on = %d\n", __func__, flag_on);

	if (mddi_power_save_on == flag_on)
		return 0;

	mddi_power_save_on = flag_on;

	if (on)
		REGULATOR_OP(msm_fb_vreg[1], enable, 2800000);
	else {
#ifdef CONFIG_MACH_MSM7X25A_V1
		lge_rt8966a_backlight_control(0);
		StatusBacklightOnOff = 0;
#endif
		REGULATOR_OP(msm_fb_vreg[1], disable, 2800000);
	}
	return 0;
}

static int common_fb_event_notify(struct notifier_block *self,
	unsigned long action, void *data)
{
	struct fb_event *event = data;
	struct fb_info *info = event->info;
	struct fb_var_screeninfo *var = &info->var;
	if (action == FB_EVENT_FB_REGISTERED) {
		var->width = 49;
		var->height = 65;
	}
	return 0;
}

static struct notifier_block common_fb_event_notifier = {
	.notifier_call	= common_fb_event_notify,
};

void __init msm_fb_add_devices(void)
{
	if (ebi2_tovis_panel_data.initialized)
		ebi2_tovis_power_save(1);

	fb_register_client(&common_fb_event_notifier);
	platform_add_devices(msm_fb_devices, ARRAY_SIZE(msm_fb_devices));
	platform_add_devices(common_panel_devices, ARRAY_SIZE(common_panel_devices));
	msm_fb_register_device("mdp", &mdp_pdata);
	msm_fb_register_device("lcdc", 0);
#ifdef CONFIG_FB_MSM_EBI2
	msm_fb_register_device("ebi2", 0);
#endif

#ifdef CONFIG_BACKLIGHT_RT9396
	lge_add_gpio_i2c_device(msm7x27a_common_init_i2c_backlight);
#endif
#ifdef CONFIG_BACKLIGHT_LGE_RT8966
	msm7x27a_common_init_backlight();
#endif
}
