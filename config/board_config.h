#pragma once

// ============================================================
// ESP32-S3-Touch-AMOLED-1.8 板级硬件配置
// 来源：官方 BSP + 原理图交叉核对
// ============================================================

// --- I2C 总线（GPIO14=SCL, GPIO15=SDA）---
#define BOARD_I2C_SCL_IO            GPIO_NUM_14
#define BOARD_I2C_SDA_IO            GPIO_NUM_15
#define BOARD_I2C_FREQ_HZ           400000

// I2C 器件地址
#define I2C_ADDR_FT3168             0x38    // 触摸
#define I2C_ADDR_PCF85063           0x51    // RTC
#define I2C_ADDR_QMI8658            0x6B    // IMU
#define I2C_ADDR_IO_EXPANDER        0x20    // IO 扩展器
#define I2C_ADDR_AXP2101            0x34    // PMU（待确认）
#define I2C_ADDR_ES8311             0x18    // 音频 Codec

// --- LCD 显示（QSPI）---
#define BOARD_LCD_CS_IO             GPIO_NUM_12
#define BOARD_LCD_PCLK_IO           GPIO_NUM_11
#define BOARD_LCD_DATA0_IO          GPIO_NUM_4
#define BOARD_LCD_DATA1_IO          GPIO_NUM_5
#define BOARD_LCD_DATA2_IO          GPIO_NUM_6
#define BOARD_LCD_DATA3_IO          GPIO_NUM_7
#define BOARD_LCD_TE_IO             GPIO_NUM_13
#define BOARD_LCD_H_RES             368
#define BOARD_LCD_V_RES             448

// --- 触摸 ---
#define BOARD_TOUCH_INT_IO          GPIO_NUM_21

// --- IO 扩展器 bit 定义 ---
#define IOEXP_BIT_LCD_RST           0
#define IOEXP_BIT_DSI_PWR_EN        1
#define IOEXP_BIT_TOUCH_RST         2
#define IOEXP_BIT_SD_CS             7

// --- I2S 音频 ---
#define BOARD_I2S_MCLK_IO           GPIO_NUM_16
#define BOARD_I2S_BCLK_IO           GPIO_NUM_9
#define BOARD_I2S_LRCK_IO           GPIO_NUM_45
#define BOARD_I2S_DOUT_IO           GPIO_NUM_8
#define BOARD_I2S_DIN_IO            GPIO_NUM_10
#define BOARD_I2S_AMP_EN_IO         GPIO_NUM_46

// --- SDMMC ---
#define BOARD_SD_CMD_IO             GPIO_NUM_1
#define BOARD_SD_CLK_IO             GPIO_NUM_2
#define BOARD_SD_D0_IO              GPIO_NUM_3

// --- USB ---
#define BOARD_USB_DP_IO             GPIO_NUM_20
#define BOARD_USB_DN_IO             GPIO_NUM_19
