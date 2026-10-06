/* MCP4921.c
 * Jonathan Valvano
 * June 29, 2026
 */
#include <ti/devices/msp/msp.h>
#include "../inc/Clock.h"
#include "../inc/LaunchPad.h"

// calls Clock_Freq to get bus clock
// initialize SPI for 8 MHz baud clock
// 16 bit data size
// busy-wait synchronization

// MCP4921
// pin signal MSPM0   mode
//  1   Vdd   +3.3V
//  2   /CS   PA8    SPI0_CS0
//  3   SCLK  PB18   SPI0_SCK
//  4   SDI   PB17   SPI0_PICO
//  5   /LDAC ground
//  6   REF          1.255 V refernce
//  7   AGND         ground
//  8   OUT          DAC output

// SPI0,SPI1 in power domain PD1 SysClk equals bus CPU clock



//---------MCP4921_Out------------
// Output 16-bit data to SPI0 port
// Input: data is  16-bit data to be transferred
// Output: none
// bit 14 is 0 to run unbuffered (LDAC grounded)
// bit 13 is 0 for gain=2 (1.255V reference gives 0 to 2.51V output)
// bit 12 is 1 to run in active mode
// bits 11-0 are data
void MCP4921_Out(uint32_t data){
  // ****ECE445L write this ****
  
  // Make sure the transmit FIFO has room
  while((SPI0->STAT & 0x02U) == 0U){
  }

  // Send the configuration bits and 12-bit sample.
  SPI0->TXDATA = (data & 0x0FFFU) | 0x1000U;
 
}

// initialize MCP4921 for 8 MHz baud clock
// 16 bit data size
// busy-wait synchronization
// PA8 SPI0_CS0
// PB18 SPII_SCK
// PB17 SPI_PICO
void MCP4921_Init(uint32_t data){
  // ECE445L Lab 5, enter code here
  
  // assumes GPIOA and GPIOB are reset and powered previously
  SPI0->GPRCM.RSTCTL = 0xB1000003;
  SPI0->GPRCM.PWREN = 0x26000001;
  
  // configure PB18 PB17 PA8 as alternate SPI0 function
  IOMUX->SECCFG.PINCM[PB18INDEX] = 0x00000083;  // SPI0 SCLK
  IOMUX->SECCFG.PINCM[PA8INDEX]  = 0x00000083;  // SPI0 CS0
  IOMUX->SECCFG.PINCM[PB17INDEX] = 0x00000083;  // SPI0 PICO
  Clock_Delay(24); // time for gpio to power up
  
  SPI0->CLKSEL = 8; // SYSCLK
  SPI0->CLKDIV = 0; // divide by 1
// SCR is in bits 2-0 (0 to 7), divide by SCR+1
  SPI0->CLKCTL = 4; // 8 MHz = 80MHz/((4 + 1) * 2)
  SPI0->CTL0 = 0x002F;
// bit 14 CSCLR=0 not cleared
// bits 13-12 CSSEL=0 CS0
// bit 9 SPH = 0
// bit 8 SPO = 0
// bits 6-5 FRF = 01 (4 wire)
// bits 4-0 n=15, data size is n+1 (16-bit data)
  SPI0->CTL1 = 0x0015;
// bits 29-24 RXTIMEOUT=0
// bits 23-16 REPEATX=0 disabled
// bits 15-12 CDMODE=0 manual
// bit 11 CDENABLE=0 CS3
// bit 7-5 =0 no parity
// bit 4=1 MSB first
// bit 3=0 POD (not used, not peripheral)
// bit 2=1 CP controller mode
// bit 1=0 LBM disable loop back
// bit 0=1 enable SPI

}
