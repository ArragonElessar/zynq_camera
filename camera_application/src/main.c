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
#include "ov7670.h"
#include "iic_helper.h"

#define LED_CONTROL_BA          XPAR_LED_CONTROL_BASEADDR           // Base Address for the AXI GPIO that controls the LEDs
#define CAMERA_CONTROL_BA       XPAR_CAMERA_CONTROL_BASEADDR        // Base Address for the AXI GPIO that controls reset and power down for OV7670 
#define LOCKED_BA               XPAR_CLK_LOCKED_GPIO_BASEADDR
#define IIC_CAMERA_BA           XPAR_CAMERA_iic_base_addr            // Base Address for the AXI IIC Controller used to configure the OV7670 
#define XSCUGIC_BA              XPAR_XSCUGIC_0_BASEADDR             // Base Address for the ARM General Interrupt Controller Device
#define IIC_INTERRUPT_ID        61U                                 // Interrupt ID that used by the IIC controller
#define BUFFER_SIZE             128                                 // Buffer size for IIC communications


static XGpio led_gpio, camera_gpio, locked_gpio;     // XGpio Structures
static XScuGic intr_ctl;                // Interrupt Controller Struct
static XScuGic_Config* intr_cfg;        // Configuration for the Interrupt controller struct

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


    // --------------------------------- Basic Read Write Test --------------------------------------------------------
    u8 reg_addr[] = {0x0A, 0x0B, 0x1C, 0x1D, 0x01};

    for(int i = 0; i < 5; i++)
    {
        status = Iic_Read(&iic_ctrl, reg_addr[i], iic_read_buf, 1);
        if(status != XST_SUCCESS) return XST_FAILURE;
        xil_printf("[DEBUG] OV7670 Reg 0x%02X: 0x%02X\n", reg_addr[i], iic_read_buf[0]);    
    }

    // Write test
    iic_write_buf[0] = 0x01; // BLUE Reg
    iic_write_buf[1] = 0x79; // Reduce the blue

    status = Iic_Write(&iic_ctrl, iic_write_buf, 2);
    if(status != XST_SUCCESS) return XST_FAILURE;

    // Read back
    status = Iic_Read(&iic_ctrl, iic_write_buf[0], iic_read_buf, 1);
    if(status != XST_SUCCESS) return XST_FAILURE;
    xil_printf("[DEBUG] OV7670 Reg 0x%02X: 0x%02X\n", iic_write_buf[0], iic_read_buf[0]);    
    
    iic_write_buf[1] = 0x80; // Reset

    status = Iic_Write(&iic_ctrl, iic_write_buf, 2);
    if(status != XST_SUCCESS) return XST_FAILURE;

    status = Iic_Read(&iic_ctrl, iic_write_buf[0], iic_read_buf, 1);
    if(status != XST_SUCCESS) return XST_FAILURE;
    xil_printf("[DEBUG] OV7670 Reg 0x%02X: 0x%02X\n", iic_write_buf[0], iic_read_buf[0]);    
    
    
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
