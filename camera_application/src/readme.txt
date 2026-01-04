NOTES from 4/1/26. sLessons learned from yesterday's (3/1/26) debug developments

1. Always check the level / edge nature of interrupts that are connected to the Zynq PS. 
    - AXI IIC IP was generating a level-high interrupt
    - Use the XScuGic_SetPriorityTriggerType to use the appropriate trigger
    - 0x3 for Rising Edge, 0x1 for Active High

2. XIic Driver expects a 7-bit slave address, not an 8 bit.
    - OV7670 Address should be set to ( 42 >> 1 ) = 21, XIic_SetAddress
    - Read address, write address are calculated by the XIic driver
    - Even (21 << 1) = 42 -> Write, Odd (21 << 1 ) + 1 = 43 -> Read Address

3. The IIC signals emitted by the FPGA ( sioc, siod ) should be pulled up
    - There is a way that we can PULLUP true a signal coming from the FPGA
    - Using basic multimeter, found that this increased the voltage on SDA-GND from 2.7V -> 3.0V

4. IIC Status Register helps check the condition of the IP
    - Bus Busy even though TX, RX FIFOs are showing up empty etc.

5. Manual buffering is not recommended for AXI IIC Output signals
    - Instead, generate the HDL Wrapper and then rename the pins in the XDC accordingly

6. Everytime the platform is updated, it is not necessary that the application will use the updated platform
    - Currently, i was creating components again and again
    - Need to find a formal way

7. Used the PS FCLK0 to generate the 24MHz signal for the XCLK instead of using sys_clock (50MHz)
    - Also captured the clocking wizard locked signal via GPIO to make sure the XCLK is up
    - Apparently, OV7670 needs a stable XCLK to start IIC Comms

8. Manual Reset sequence using GPIO RESET and PWDN pins, with sufficient delays in between.