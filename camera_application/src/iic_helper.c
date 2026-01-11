#include "iic_helper.h"
#include <xiic.h>
#include <xiic_l.h>
#include <xil_exception.h>
#include <xil_printf.h>
#include <xscugic.h>
#include <xstatus.h>

#define UNUSED(x) (void)(x)

// Define the interrupt handlers
static void SendHandler( void *callback_ref, int byte_count)
{
    UNUSED(byte_count);

    IicCtrl *inst = (IicCtrl *)callback_ref;
    inst->iic_transmit_complete = 0; // Update the transmit flag
}

static void RecvHandler( void *callback_ref, int byte_count)
{
    UNUSED(byte_count);

    IicCtrl *inst = (IicCtrl *)callback_ref;
    inst->iic_recieve_complete = 0; // Update the recv flag
}

static void StatHandler( void *callback_ref, int event)
{
    IicCtrl *inst = (IicCtrl *)callback_ref;
    inst->iic_recieve_complete = 0;
    inst->iic_transmit_complete = 0;
    xil_printf("[DEBUG] IIC Event/Error: %d\n", event);    
}

int Iic_Helper_Init(IicCtrl *instance_ptr, UINTPTR iic_base_addr, XScuGic *intc_ptr, int interrupt_id, u8 iic_device_addr)
{
    int status;

    // Initialise the IIC Structure
    XIic_Config *iic_cfg_ptr;

    // Setup the IicCtrl structure variables for interrupts
    instance_ptr->intc_ptr = intc_ptr;
    instance_ptr->interrupt_id = interrupt_id;

    // Save the IIC device address if needed
    instance_ptr->iic_device_addr = iic_device_addr;

    // Configure and initialize the XIic instance
    iic_cfg_ptr = XIic_LookupConfig(iic_base_addr);
    if(iic_cfg_ptr == NULL)
    {
        xil_printf("[ERROR] Could not lookup config, validate baseaddr: 0x%08X\n", iic_base_addr);
        return XST_FAILURE;
    }
    status = XIic_CfgInitialize(&instance_ptr->iic_instance, iic_cfg_ptr, iic_cfg_ptr->BaseAddress);
    if(status != XST_SUCCESS)
    {
        xil_printf("[ERROR] Failed to initialize iic_instance with status: %d\n", status);
        return status;
    }

    /*
        So the effective interrupt handling mechanism looks like this
        ARM Core Exception -> XScuGic -> XIic
    */


    // Declare the type and priority of the interrupt
    // TODO - Currently Hard Coding to a Level High Interrupt
    XScuGic_SetPriorityTriggerType(instance_ptr->intc_ptr, instance_ptr->interrupt_id, 0xA0, 0x1);

    // Connect the interrupts
    status = XScuGic_Connect(instance_ptr->intc_ptr, instance_ptr->interrupt_id, (Xil_InterruptHandler)XIic_InterruptHandler, &instance_ptr->iic_instance);
    if(status != XST_SUCCESS)
    {
        xil_printf("[ERROR] Failed to connect to the main interrupt controller with status: %d.\n", status);
        return status;
    }

    // Enable the interrupt
    XScuGic_Enable(instance_ptr->intc_ptr, instance_ptr->interrupt_id);

    // Enable and configure Xil_Exceptions
    Xil_ExceptionInit();
    Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT, (Xil_ExceptionHandler)XScuGic_InterruptHandler, instance_ptr->intc_ptr);
    Xil_ExceptionEnable();

    // Set up the IIC Interrupt Callback Handlers
    XIic_SetSendHandler(&instance_ptr->iic_instance, instance_ptr, (XIic_Handler)SendHandler);
    XIic_SetRecvHandler(&instance_ptr->iic_instance, instance_ptr, (XIic_Handler)RecvHandler);
    XIic_SetStatusHandler(&instance_ptr->iic_instance, instance_ptr, (XIic_Handler)StatHandler);

    // Set the IIC Address for the device
    status = XIic_SetAddress(&instance_ptr->iic_instance, XII_ADDR_TO_SEND_TYPE, instance_ptr->iic_device_addr);
    if(status != XST_SUCCESS)
    {
        xil_printf("[ERROR] Failed to set IIC Device address to: %d, status: %d\n", instance_ptr->iic_device_addr, status);
        return XST_FAILURE;
    }

    xil_printf("[INFO] Successfully initialised IIC Helper & Interrupt system!\n");
    
    return XST_SUCCESS;
}

// IIC Write Function
int Iic_Write(IicCtrl *instance_ptr, u8 *data, int byte_count)
{
    int status;

    // Reset the stats and the transmit flag
    instance_ptr->iic_transmit_complete = 1;

    // Start the IIC instance
    status = XIic_Start(&instance_ptr->iic_instance);
    if(status != XST_SUCCESS)
    {
        xil_printf("[ERROR] Failed to start IIC instance, Iic_WriteData, status: %d\n", status);
        return status;
    }

    // IIC Address already setup at initialisation

    // Write the # of bytes to the IIC Device
    status = XIic_MasterSend(&instance_ptr->iic_instance, data, byte_count);
    if(status != XST_SUCCESS)
    {
        xil_printf("[ERROR] Failed to start IIC tx, XIic_MasterSend, status: %d\n", status);
        return status;
    }

    // Wait for the transmit to complete
    while( instance_ptr->iic_transmit_complete || (XIic_IsIicBusy(&instance_ptr->iic_instance) == TRUE) );

    // Stop the IIC Device
    status = XIic_Stop(&instance_ptr->iic_instance);
    if(status != XST_SUCCESS)
    {
        xil_printf("[ERROR] Failed to stop the IIC instance, at XIic_Stop, status: %d\n", status);
        return status;
    }

    return XST_SUCCESS;

}

// IIC Read Function
int Iic_Read(IicCtrl *instance_ptr, u8 reg_addr, u8 *buf, int byte_count)
{
    int status;

    // Reset the stats and recieve flag
    instance_ptr->iic_recieve_complete = 1;

    // Write the Register Address
    u8 reg = reg_addr;
    status = Iic_Write(instance_ptr, &reg, 1);
    if(status != XST_SUCCESS)
    {
        xil_printf("[ERROR] Failed to write the address, Iic_ReadData, status: %d\n", status);
        return status;
    }

    // Start the IIC instance
    status = XIic_Start(&instance_ptr->iic_instance);
    if(status != XST_SUCCESS)
    {
        xil_printf("[ERROR] Failed to start IIC instance, Iic_ReadData, status: %d\n", status);
        return status;
    }

    // Start Recieving
    status = XIic_MasterRecv(&instance_ptr->iic_instance, buf, byte_count);
    if(status != XST_SUCCESS)
    {
        xil_printf("[ERROR] Failed to start IIC Rx, XIic_MasterRecv, status: %d\n", status);
        return status;
    }

    // Wait for the interrupt to come in.
    while( instance_ptr->iic_recieve_complete || (XIic_IsIicBusy(&instance_ptr->iic_instance) == TRUE));

    // Stop the IIC Device
    status = XIic_Stop(&instance_ptr->iic_instance);
    if(status != XST_SUCCESS)
    {
        xil_printf("[ERROR] Failed to stop the IIC instance, at XIic_Stop, status: %d\n", status);
        return status;
    }

    return XST_SUCCESS;

}

u32 Iic_Read_Internal_Reg(IicCtrl *instance_ptr, u8 reg_offset)
{
    return XIic_ReadReg(instance_ptr->iic_base_addr, reg_offset);
}