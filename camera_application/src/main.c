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
#include "xscugic.h"
#include "ov7670.h"
#include "iic_helper.h"

#define LED_CONTROL_BA          XPAR_LED_CONTROL_BASEADDR           // Base Address for the AXI GPIO that controls the LEDs
#define CAMERA_CONTROL_BA       XPAR_CAMERA_CONTROL_BASEADDR        // Base Address for the AXI GPIO that controls reset and power down for OV7670 
#define IIC_CAMERA_BA           XPAR_CAMERA_IIC_BASEADDR            // Base Address for the AXI IIC Controller used to configure the OV7670 
#define XSCUGIC_BA              XPAR_XSCUGIC_0_BASEADDR             // Base Address for the ARM General Interrupt Controller Device
#define IIC_INTERRUPT_ID        61U                                 // Interrupt ID that used by the IIC controller
#define BUFFER_SIZE             32                                  // Buffer size for IIC communications

static XGpio led_gpio, camera_gpio;     // XGpio Structures
static XScuGic intr_ctl;                // Interrupt Controller Struct
static XScuGic_Config* intr_cfg;        // Configuration for the Interrupt controller struct
static OV7670 camera;

u8 *iic_read_buf;
u8 *iic_write_buf;

void blink_leds();  // basic function to test GPIO functionality

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
    
    // ------------------------------- Now Initialise the IIC Driver -----------------------------------------
    
    // Also, allocate space for the buffers
    iic_read_buf = (u8*) malloc(BUFFER_SIZE);
    iic_write_buf = (u8*) malloc(BUFFER_SIZE);

    xil_printf("[DEBUG] Initialied IIC controller, buffers of size: %d bytes\n", BUFFER_SIZE);

    // ------------------------------ Now Setup the Interrupt System ------------------------------------------

    // Set up the interrupt handler control structure
    intr_cfg = XScuGic_LookupConfig(XSCUGIC_BA);
    if(intr_cfg == NULL) return XST_FAILURE;
    status = XScuGic_CfgInitialize(&intr_ctl, intr_cfg, intr_cfg->CpuBaseAddress);
    if(status != XST_SUCCESS) return XST_FAILURE;

    // -------------------------------- Setup the IIC Helper ---------------------------------------------
    IicCtrl iic_ctrl;
    u8 ov7670_iic_address = 0x21;
    status = Iic_Helper_Init(&iic_ctrl, IIC_CAMERA_BA, &intr_ctl, IIC_INTERRUPT_ID, ov7670_iic_address);
    if( status != XST_SUCCESS ) return XST_FAILURE;

    // -------------------------------- Setup the OV7670 Driver -------------------------------------------
    status = OV7670_Init(&camera, &iic_ctrl, &camera_gpio);
    if( status != XST_SUCCESS ) return XST_FAILURE; 
    xil_printf("[DEBUG] OV7670 Camera IIC Control System Ready!\n");

    // Perform a software reset
    status = OV7670_Reset(&camera);

    // -------------------------------- IIC Test for OV7670 Driver -----------------------------------------
    status = OV7670_Reg_ReadWrite_Test(&camera);
    if(status != XST_SUCCESS) return XST_FAILURE;

    // basic setup for camera
    status = OV7670_Basic_Setup(&camera);
    if(status != XST_SUCCESS)
    {
        xil_printf("[DEBUG] Failed to setup OV7670 with status: %d\n", status);
        return XST_FAILURE;
    }

    // Now we need some time to catch the data ( check if it is coming as well )
    blink_leds();

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
