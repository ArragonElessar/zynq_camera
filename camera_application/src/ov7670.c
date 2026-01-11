#include <sleep.h>
#include <xgpio.h>
#include <xstatus.h>

#include "iic_helper.h"
#include "ov7670.h"

int OV7670_Init(OV7670* cam, IicCtrl* iic_ctrl, XGpio* gpio)
{
    // Store pointers to IIC Control Structure & GPIO 
    cam->iic_ctrl = iic_ctrl;
    cam->gpio = gpio;

    // Set the data direction for both channels of the GPIO 
    XGpio_SetDataDirection(cam->gpio, 1, 0x00000000); // All outputs - this controls reset and pwdn for OV7670
    XGpio_SetDataDirection(cam->gpio, 2, 0xFFFFFFFF); // All inputs - check if the External Clock is locked for XCLK 0V7670

    // PWDN Should be pulled down to enable Power
    /**
        RESET = 1 -> Normal Mode
        PWDN  = 0 -> Normal Mode
    */
    XGpio_DiscreteWrite(cam->gpio, 1, 0U); // Force everything low first
    usleep(100000); // 100ms
    XGpio_DiscreteWrite(cam->gpio, 1, 2U); // Power On and Release Reset
    usleep(100000); // Wait for Camera to boot
    
    // Verify written values ( not necessary )
    u32 camera_control_state = XGpio_DiscreteRead(cam->gpio, 1);
    xil_printf("[DEBUG] Current Camera State, RESET: %d, PWDN: %d\n", (camera_control_state >> 1), ( camera_control_state & 1 ));

    // Set up the Camera Controls
    // We need the XCLK to be up at 24MHz before starting IIC Communications
    // u32 clock_locked_signal = XGpio_DiscreteRead(cam->gpio, 2);
    // int timeout = 100;
    // while ( ( clock_locked_signal != 1 ) && ( timeout > 0 ) )
    // {
    //     clock_locked_signal = XGpio_DiscreteRead(cam->gpio, 2);
    //     timeout--;
    //     usleep(100);
    // }

    // if( timeout == 0 )
    // {
    //     xil_printf("[ERROR] Timed out waiting for 24MHz XCLK Signal to be locked.\n");
    //     return XST_FAILURE;
    // }
    // xil_printf("[DEBUG] XCLK (24MHz) clock locked signal: %d\n", clock_locked_signal);
    usleep(100000);

    return XST_SUCCESS;
}

int OV7670_ReadReg(OV7670 *cam, u8 reg, u8 *buf)
{
    return Iic_Read(cam->iic_ctrl, reg, buf, 1); // Assuming all registers are 1 byte
}

int OV7670_WriteReg(OV7670 *cam, u8 reg, u8 data)
{
    u8 tx_buf[] = {reg, data};
    return Iic_Write(cam->iic_ctrl, tx_buf, 2); // Reg Addr, Data
}

int OV7670_Reg_ReadWrite_Test(OV7670 *cam)
{
    int status = XST_SUCCESS;
    u8 rx_buf;

    // Simple Read test, here are 4 read only registers, find the values and print
    u8 read_regs[4] = {REG_PID, REG_VER, REG_MIDH, REG_MIDL};
    for(int i = 0; i < 4; i++)
    {
        status = OV7670_ReadReg(cam, read_regs[i], &rx_buf);
        if(status != XST_SUCCESS)
        {
            xil_printf("[ERROR] Failed Read Self Test for Reg: 0x%02X\n", read_regs[i]);
            return status;
        }
        xil_printf("[DEBUG] Read Self Test, Reg 0x%02X: 0x%02X\n", read_regs[i], rx_buf);
    }
    if( status == XST_SUCCESS )
    {
        xil_printf("[INFO] Passed Read Registers test!\n");
    }

    // At this point, we know that the read functionality is working
    // Simple Write Test, read the blue reg, update its value, read it again, revert back to original
    u8 new_value = 0x79, original_value;
    OV7670_ReadReg(cam, REG_BLUE, &rx_buf);
    xil_printf("[DEBUG] Write Self Test, Reg 0x%02X: 0x%02X\n", REG_BLUE, rx_buf);
    original_value = rx_buf;

    // Write the new value
    status = OV7670_WriteReg(cam, REG_BLUE, new_value);
    if(status != XST_SUCCESS)
    {
        xil_printf("[ERROR] Failed Write Self Test for Reg: 0x%02X\n", REG_BLUE);
        return status;
    }

    // Verify that update is successfull
    OV7670_ReadReg(cam, REG_BLUE, &rx_buf);
    xil_printf("[DEBUG] Write Self Test, Reg 0x%02X: 0x%02X, Expected: 0x%02X\n", REG_BLUE, rx_buf, new_value);
    if( rx_buf != new_value )
    {
        xil_printf("[INFO] Failed Write Registers test!\n");
        return XST_FAILURE;
    }
    else{
        xil_printf("[INFO] Passed Write Registers test!\n");
    }

    // Revert back to original value
    status = OV7670_WriteReg(cam, REG_BLUE, original_value);
    if(status != XST_SUCCESS)
    {
        xil_printf("[ERROR] Failed Write Self Test for Reg: 0x%02X\n", REG_BLUE);
        return status;
    }

    return status;
}


