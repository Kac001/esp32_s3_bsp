/*
 * bsp_lcd_config_internal.h — LCD 旋转/镜像/反色的 Kconfig 归一化（BSP 内部使用）
 *
 * 显示面板、LVGL rotation、触摸坐标变换三处必须使用同一组值。
 */
#pragma once

#if defined(CONFIG_BSP_LCD_SWAP_XY)
#define BSP_LCD_CFG_SWAP_XY     (1)
#else
#define BSP_LCD_CFG_SWAP_XY     (0)
#endif

#if defined(CONFIG_BSP_LCD_MIRROR_X)
#define BSP_LCD_CFG_MIRROR_X    (1)
#else
#define BSP_LCD_CFG_MIRROR_X    (0)
#endif

#if defined(CONFIG_BSP_LCD_MIRROR_Y)
#define BSP_LCD_CFG_MIRROR_Y    (1)
#else
#define BSP_LCD_CFG_MIRROR_Y    (0)
#endif

#if defined(CONFIG_BSP_LCD_INVERT_COLOR)
#define BSP_LCD_CFG_INVERT      (1)
#else
#define BSP_LCD_CFG_INVERT      (0)
#endif

/* SPI 时钟由 Kconfig 提供（int 选项，恒有定义） */
#define BSP_LCD_CFG_PIXEL_CLOCK_HZ  (CONFIG_BSP_LCD_SPI_FREQ_MHZ * 1000 * 1000)

/* 背光极性由 Kconfig 决定（BSP_LCD_BACKLIGHT_INVERTED） */
#if defined(CONFIG_BSP_LCD_BACKLIGHT_INVERTED)
#define BSP_LCD_CFG_BACKLIGHT_INVERT (1)
#else
#define BSP_LCD_CFG_BACKLIGHT_INVERT (0)
#endif
