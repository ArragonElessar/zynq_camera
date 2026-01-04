#include <sleep.h>
#include <stdio.h>
#include <xiic_l.h>
#include <xil_exception.h>
#include <xstatus.h>
#include <stdlib.h>

#include "platform.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xgpio.h"
#include "xiic.h"
#include "xscugic.h"

#define LED_CONTROL_BA          XPAR_LED_CONTROL_BASEADDR           // Base Address for the AXI GPIO that controls the LEDs
#define CAMERA_CONTROL_BA       XPAR_CAMERA_CONTROL_BASEADDR        // Base Address for the AXI GPIO that controls reset and power down for OV7670 
#define LOCKED_BA               XPAR_CLK_LOCKED_GPIO_BASEADDR
#define IIC_CAMERA_BA           XPAR_CAMERA_IIC_BASEADDR            // Base Address for the AXI IIC Controller used to configure the OV7670 
#define XSCUGIC_BA              XPAR_XSCUGIC_0_BASEADDR             // Base Address for the ARM General Interrupt Controller Device
#define IIC_INTERRUPT_ID        61U                                 // Interrupt ID that used by the IIC controller
#define BUFFER_SIZE             128                                 // Buffer size for IIC communications


static XGpio led_gpio, camera_gpio, locked_gpio;     // XGpio Structures
static XIic iic_camera;                 // IIC Controller to talk to the camera
static XIic_Config* iic_camera_config;  // Configuration to work the IIC Controller
static XScuGic intr_ctl;                // Interrupt Controller Struct
static XScuGic_Config* intr_cfg;        // Configuration for the Interrupt controller struct

// We need to confirm this for the OV7670
static const int iic_write_address = 0x21; // 7-bit address !!

volatile u8 iic_transmit_complete;      // Flag to check IIC Transmission complete
volatile u8 iic_recieve_complete;       // Flag to check IIC Recieve Data complete

void blink_leds();  // basic function to test GPIO functionality

// These functions will be called by the IIC Interrupt Response
static void SendHandler(XIic* iic);
static void RecvHandler(XIic* iic);
static void StatHandler(XIic* iic, int event);

// These are the main IIC Read / Write functions
int Iic_WriteData(int byte_cnt);
int Iic_ReadData(u8 address, int byte_cnt);

// Helper Methods
void print_iic_register(u8 reg_offset);

// Buffers for IIC Read, Write
u8 *iic_write_buf;
u8 *iic_read_buf;

int main()
{
    init_platform();
    
    print("\n\n\n[INFO]  Welcome to Camera Application!\n");
    int status;

    // Init the GPIOs and present a basic pattern to the LEDs
    status = XGpio_Initialize(&led_gpio, LED_CONTROL_BA);
    if(status != XST_SUCCESS) return XST_FAILURE;

    status = XGpio_Initialize(&camera_gpio, CAMERA_CONTROL_BA);
    if(status != XST_SUCCESS) return XST_FAILURE;

    // Set the Data Directions
    XGpio_SetDataDirection(&led_gpio, 1, 0x00000000); // All outputs
    XGpio_SetDataDirection(&camera_gpio, 1, 0x00000000); // All outputs

    // PWDN Should be pulled down to enable Power
    /**
        RESET = 1 -> Normal Mode
        PWDN  = 0 -> Normal Mode
    */
    XGpio_DiscreteWrite(&camera_gpio, 1, 0U); // Force everything low first
    usleep(100000); // 100ms
    XGpio_DiscreteWrite(&camera_gpio, 1, 2U); // Power On and Release Reset
    usleep(100000); // Wait for Camera to boot
    
    // Verify written values ( not necessary )
    u32 camera_control_state = XGpio_DiscreteRead(&camera_gpio, 1);
    xil_printf("[DEBUG] Current Camera State, RESET: %d, PWDN: %d\n", (camera_control_state >> 1), ( camera_control_state & 1 ));

    // Set up the Camera Controls
    // We need the XCLK to be up at 24MHz before starting IIC Communications
    
    status = XGpio_Initialize(&locked_gpio, LOCKED_BA);
    if(status != XST_SUCCESS) return XST_FAILURE;
    XGpio_SetDataDirection(&camera_gpio, 1, 0xFFFFFFFF); // All inputs

    u32 clock_locked_signal = XGpio_DiscreteRead(&locked_gpio, 1);
    int timeout = 100;
    while ( ( clock_locked_signal != 1 ) && ( timeout > 0 ) )
    {
        clock_locked_signal = XGpio_DiscreteRead(&locked_gpio, 1);
        timeout--;
        usleep(100);
    }

    if( timeout == 0 )
    {
        xil_printf("[ERROR] Timed out waiting for 24MHz XCLK Signal to be locked.\n");
        return XST_FAILURE;
    }
    xil_printf("[DEBUG] XCLK (24MHz) clock locked signal: %d\n", clock_locked_signal);

    // ------------------------------- Now Initialise the IIC Driver -----------------------------------------
    
    // Look the config up from our base address
    iic_camera_config = XIic_LookupConfig(IIC_CAMERA_BA);

    // Configure the IIC Controller structure with this configuration
    status = XIic_CfgInitialize(&iic_camera, iic_camera_config, iic_camera_config->BaseAddress);
    if(status != XST_SUCCESS) return XST_FAILURE;

    // Also, allocate space for the buffers
    iic_read_buf = (u8*) malloc(BUFFER_SIZE);
    iic_write_buf = (u8*) malloc(BUFFER_SIZE);

    xil_printf("[DEBUG] Initialied IIC controller, buffers of size: %d bytes\n", BUFFER_SIZE);

    // ------------------------------ Now Setup the Interrupt System ------------------------------------------

    /*
    So the effective interrupt handling mechanism looks like this
    ARM Core Exception -> XScuGic -> XIic
    */

    // Set up the interrupt handler control structure
    intr_cfg = XScuGic_LookupConfig(XSCUGIC_BA);
    if(intr_cfg == NULL) return XST_FAILURE;
    status = XScuGic_CfgInitialize(&intr_ctl, intr_cfg, intr_cfg->CpuBaseAddress);
    if(status != XST_SUCCESS) return XST_FAILURE;

    // Define the priority and triggers for the interrupt supplied
    XScuGic_SetPriorityTriggerType(&intr_ctl, IIC_INTERRUPT_ID, 0xA0, 0x1); // IIC IP provides a level high interrupt

    // Point the interrupt handler to the IIC Controller
    status = XScuGic_Connect(&intr_ctl, IIC_INTERRUPT_ID, (Xil_InterruptHandler)XIic_InterruptHandler, &iic_camera );
    if(status != XST_SUCCESS) return XST_FAILURE;

    // Enable the Interrupt
    XScuGic_Enable(&intr_ctl, IIC_INTERRUPT_ID);

    Xil_ExceptionInit();
    // Enable Exceptions & set the default handler to XSCUGIC
    Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT, (Xil_ExceptionHandler)XScuGic_InterruptHandler, &intr_ctl);
    Xil_ExceptionEnable();

    // Now we define the functions that get triggered by the IIC Controller - Send, Recieve, Status
    XIic_SetSendHandler(&iic_camera, &iic_camera, (XIic_Handler) SendHandler);
    XIic_SetRecvHandler(&iic_camera, &iic_camera, (XIic_Handler) RecvHandler);
    XIic_SetStatusHandler(&iic_camera, &iic_camera, (XIic_StatusHandler) StatHandler);

    xil_printf("[DEBUG] Interrupt System for IIC setup.\n");

    // --------------------------------- Basic Read Write Test --------------------------------------------------------
    status = XIic_SetAddress(&iic_camera, XII_ADDR_TO_SEND_TYPE, iic_write_address);
    if(status != XST_SUCCESS)
    {
        xil_printf("[ERROR] Failed to set IIC Device address to: %d, status: %d\n", iic_write_address, status);
        return XST_FAILURE;
    }

    u8 reg_addr[4] = { 0x0A, 0x0B, 0x1C, 0x1D};
    for(int i = 0; i < 4; i++){
        status = Iic_ReadData(reg_addr[i], 1);
        if(status != XST_SUCCESS)
        {
            xil_printf("[ERROR] Failed to read register at %d, status: %d\n", reg_addr[i], status);
            return XST_FAILURE;
        }

        xil_printf("[INFO]  OV7670 Reg: 0x%02X: 0x%02X\n", reg_addr[i], iic_read_buf[0]);
    }


    iic_write_buf[0] = 0x01;
    iic_write_buf[1] = 0x80;
    status = Iic_WriteData(2);
    if(status!=XST_SUCCESS)
    {
        xil_printf("[ERROR] Failed to write register at %d, status: %d\n", 0x01, status);
        return XST_FAILURE;
    }
    xil_printf("[DEBUG] OV7670 Reg: 0x%02X: 0x%02X\n", iic_write_buf[0], iic_write_buf[1]);

    // Trying a read, write
    status = Iic_ReadData(0x01, 1); // Register for blue Gain
    if(status!=XST_SUCCESS)
    {
        xil_printf("[ERROR] Failed to read register at %d, status: %d\n", 0x01, status);
        return XST_FAILURE;
    }
    xil_printf("[INFO]  OV7670 Reg: 0x%02X: 0x%02X\n", 0x01, iic_read_buf[0]);

    // blink_leds();
    cleanup_platform();
    return 0;
}

void blink_leds()
{
    // Simple Blink LED Pattern
    int count = 0;
    while(1)
    {
        xil_printf("[DEBUG] Blink LED: %d\n", count++);
        XGpio_DiscreteWrite(&led_gpio, 1, count);
        usleep(1000000);
    }
}

static void SendHandler(XIic* iic)
{
    iic_transmit_complete = 0;
}

static void RecvHandler(XIic* iic)
{
    iic_recieve_complete = 0;
}

static void StatHandler(XIic* iic, int event)
{
    // If anything goes wrong (NACK, Lost Arbitration, etc.)
    // we must clear the flags so the main thread stops waiting.
    iic_transmit_complete = 0;
    iic_recieve_complete = 0;
    xil_printf("[DEBUG] IIC Event/Error: %d\n", event);
}

int Iic_WriteData(int byte_cnt)
{
    // Protect memory access
    if(byte_cnt >= BUFFER_SIZE) return XST_FAILURE;

    int status;

    // Reset at the start of every data write
    iic_transmit_complete = 1;

    // Start the IIC instance
    status = XIic_Start(&iic_camera);
    if(status != XST_SUCCESS)
    {
        xil_printf("[ERROR] Failed to start IIC instance, Iic_WriteData, status: %d\n", status);
        return status;
    }

    // Expecting the data to be already written to the write buffer

    // Write the given byte to the IIC camera address
    status = XIic_MasterSend(&iic_camera, iic_write_buf, byte_cnt);
    if(status != XST_SUCCESS)
    {
        xil_printf("[ERROR] Failed to start IIC tx, XIic_MasterSend, status: %d\n", status);
        return status;
    }

    // Once the transmission is done, the XIIC Controller will raise an interrupt, which will be handled by our Send Handler method.
    // So we need to check the iic_transmit_complete flag;
    while( iic_transmit_complete || (XIic_IsIicBusy(&iic_camera) == TRUE) );

    // Stop the IIC Device
    status = XIic_Stop(&iic_camera);
    if(status != XST_SUCCESS)
    {
        xil_printf("[ERROR] Failed to stop the IIC instance, at XIic_Stop, status: %d\n", status);
        return status;
    }

    return XST_SUCCESS;
}

int Iic_ReadData(u8 address, int byte_cnt)
{
    int status;

    iic_recieve_complete = 1; // Reset at the beginning of each read transaction

    // For now, we will fill up the address in the first byte of the write buffer
    iic_write_buf[0] = address;
    status = Iic_WriteData(1);
    if(status != XST_SUCCESS)
    {
        xil_printf("[ERROR] Failed to write the address, Iic_ReadData, status: %d\n", status);
        return status;
    }

    // Start the IIC instance
    status = XIic_Start(&iic_camera);
    if(status != XST_SUCCESS)
    {
        xil_printf("[ERROR] Failed to start IIC instance, Iic_ReadData, status: %d\n", status);
        return status;
    }

    status = XIic_MasterRecv(&iic_camera, iic_read_buf, byte_cnt);
    if(status != XST_SUCCESS)
    {
        xil_printf("[ERROR] Failed to start IIC Rx, XIic_MasterRecv, status: %d\n", status);
        return status;
    }

    // Now we wait for the read complete interrupt to come in
    while( iic_recieve_complete || (XIic_IsIicBusy(&iic_camera) == TRUE) ); // May need to add debug signals here if tx/rx fail.
 
    // Stop the IIC Device
    status = XIic_Stop(&iic_camera);
    if(status != XST_SUCCESS)
    {
        xil_printf("[ERROR] Failed to stop the IIC instance, at XIic_Stop, status: %d\n", status);
        return status;
    }

    return XST_SUCCESS;
}


void print_iic_register(u8 reg_offset)
{
    u32 reg_value = XIic_ReadReg(IIC_CAMERA_BA, reg_offset);
    xil_printf("[DEBUG] Recieve operation complete, IIC Register: 0x%02X: 0x%08X\n", reg_offset, reg_value);
}
