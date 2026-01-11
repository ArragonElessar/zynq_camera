#ifndef __OV7670_H__
#define __OV7670_H__

#include "iic_helper.h"
#include "xil_printf.h"
#include "xgpio.h"

// Register Addresses
#define REG_PID     0x0A // Product ID - High ( expected 0x76 ) - R
#define REG_VER     0x0B // Product ID - Low  ( expected 0x73 ) - R
#define REG_MIDH    0x1C // Mfr ID - High ( expected 0x7f) - R
#define REG_MIDL    0x1D // Mfr ID - Low ( expected 0xA2)  - R
#define REG_BLUE    0x01 // Blue Channel Gain Setting    - R/W
#define REG_CLKRC   0x11 // Internal Clock Register      - R/W
#define REG_COM7    0x12 // Common Control 7             - R/W
#define REG_COM15   0x40 // Common Control 15 - Output Format - R/W

// Register Controls
#define REG_COM7_RESET              (u8)0x80
#define REG_COM7_RGB_MODE           (u8)0x04
#define REG_COM15_RGB555_FULL_RANGE (u8)0xD0
#define REG_CLKRC_CLK_DIV_2         (u8)0x01

// IIC Address for the OV7670 - 7 bit
#define OV7670_IIC_ADDR (u8)0x21 

// Define the device structure
typedef struct {
    IicCtrl* iic_ctrl; // Pointer to IIC Helper Instance
    XGpio*   gpio;     // Pointer to Dual Channel GPIO - RESET, PWDN, XCLK Locked Signals
} OV7670;

// API Prototypes
int OV7670_Init(OV7670* cam, IicCtrl* iic_ctrl, XGpio* gpio);
int OV7670_ReadReg(OV7670* cam, u8 reg, u8 *buf);
int OV7670_WriteReg(OV7670* cam, u8 reg, u8 data);

// Self-Test 
int OV7670_Reg_ReadWrite_Test(OV7670* cam);

// Software Reset
int OV7670_Reset(OV7670* cam);

// Basic Setup
int OV7670_Basic_Setup(OV7670* cam);

#endif