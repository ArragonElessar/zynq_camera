#ifndef __IIC_HELPER_H__
#define __IIC_HELPER_H__

#include "xiic.h"
#include "xscugic.h"
#include "xstatus.h"
#include "xil_printf.h"
#include <xil_types.h>

// Structure to hold all IIC Peripheral related information
typedef struct {

    XIic iic_instance;                   // Actual IIC Driver Instance
    XScuGic *intc_ptr;                   // Pointer to the system interrupt controller
    UINTPTR iic_base_addr;               // Base Address to initialise the IIC instance
    int interrupt_id;                    // 61U for our case
    volatile u8 iic_transmit_complete;  // Transmit complete flag for IIC ( updated by interrupt )
    volatile u8 iic_recieve_complete;   // Recieve complete flag for IIC ( updated by interrupt )
    u8 iic_device_addr;
} IicCtrl;

// Initialise the IIC Helper driver
int Iic_Helper_Init(IicCtrl *instance_ptr, UINTPTR iic_base_addr, XScuGic *intc_ptr, int interrupt_id, u8 iic_device_addr);

// IIC Write Function
int Iic_Write(IicCtrl *instance_ptr, u8 *data, int byte_count);

// IIC Read function, specify the device addr, register address and the number of bytes
int Iic_Read(IicCtrl *instance_ptr, u8 reg_addr, u8 *buf, int byte_count);

// Read an IIC IP Register over the AXI4-Lite interface
u32 Iic_Read_Internal_Reg(IicCtrl *instance_ptr, u8 reg_offset);


#endif