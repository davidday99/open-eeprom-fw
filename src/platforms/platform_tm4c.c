/**
 * @file 
 *
 * This file contains implementations for the 
 * `programmer` and `trasnport` interfaces 
 *  required to implement the OpenEEPROM server
 *  and commands.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "platforms/tm4c/driverlib/hw_memmap.h"
#include "platforms/tm4c/driverlib/sysctl.h"
#include "platforms/tm4c/driverlib/gpio.h"
#include "platforms/tm4c/driverlib/ssi.h"
#include "platforms/tm4c/driverlib/uart.h"
#include "programmer.h"
#include "transport.h"

#define PART_TM4C123GH6PM
#include "platforms/tm4c/driverlib/pin_map.h"

#define MAX_DATA_WIDTH 8
#define MAX_ADDRESS_WIDTH 15

/**
 * @struct
 * Representation of a GPIO pin on the TM4C MCU.
 */
typedef struct {
    uint32_t port;
    uint8_t pin;
} DriverLibGpioPin;


/**
 * @struct 
 * Representation of a SPI peripheral on the TM4C MCU.
 */
typedef struct {
    DriverLibGpioPin CLK;
    DriverLibGpioPin CS;
    DriverLibGpioPin RX;
    DriverLibGpioPin TX;
} DriverLibSpiModule;

/**
 * @struct 
 * Representation of a TM4C programmer.
 *      
 * Maintains the state of the address and data lines,
 * control lines, and the SPI peripheral.  
 */
typedef struct {
    uint32_t ports[10];
    DriverLibGpioPin A[MAX_ADDRESS_WIDTH];
    DriverLibGpioPin IO[MAX_DATA_WIDTH];
    DriverLibGpioPin WEn;
    DriverLibGpioPin OEn;
    DriverLibGpioPin CEn;
    DriverLibSpiModule spi;
} DriverLibProgrammer;

static DriverLibProgrammer Progr = {
    .ports = {
        SYSCTL_PERIPH_GPIOA,
        SYSCTL_PERIPH_GPIOB,
        SYSCTL_PERIPH_GPIOC,
        SYSCTL_PERIPH_GPIOD,
        SYSCTL_PERIPH_GPIOE,
        SYSCTL_PERIPH_GPIOF
    },
    .A = {
        {GPIO_PORTB_BASE, GPIO_PIN_5},
        {GPIO_PORTB_BASE, GPIO_PIN_0},
        {GPIO_PORTB_BASE, GPIO_PIN_1},
        {GPIO_PORTE_BASE, GPIO_PIN_4},
        {GPIO_PORTE_BASE, GPIO_PIN_5},
        {GPIO_PORTB_BASE, GPIO_PIN_4},
        {GPIO_PORTA_BASE, GPIO_PIN_5},
        {GPIO_PORTA_BASE, GPIO_PIN_6},
        {GPIO_PORTA_BASE, GPIO_PIN_7},
        {GPIO_PORTF_BASE, GPIO_PIN_1},
        {GPIO_PORTE_BASE, GPIO_PIN_3},
        {GPIO_PORTE_BASE, GPIO_PIN_2},
        {GPIO_PORTE_BASE, GPIO_PIN_1},
        {GPIO_PORTD_BASE, GPIO_PIN_3},
        {GPIO_PORTD_BASE, GPIO_PIN_2},
    },
    .IO = {
        {GPIO_PORTA_BASE, GPIO_PIN_3},
        {GPIO_PORTA_BASE, GPIO_PIN_4},
        {GPIO_PORTB_BASE, GPIO_PIN_6},
        {GPIO_PORTB_BASE, GPIO_PIN_7},
        {GPIO_PORTC_BASE, GPIO_PIN_5},
        {GPIO_PORTC_BASE, GPIO_PIN_4},
        {GPIO_PORTE_BASE, GPIO_PIN_0},
        {GPIO_PORTB_BASE, GPIO_PIN_2},

    },
    .CEn = {GPIO_PORTA_BASE, GPIO_PIN_2},
    .OEn = {GPIO_PORTD_BASE, GPIO_PIN_6},
    .WEn = {GPIO_PORTC_BASE, GPIO_PIN_7},
    .spi = {
        .CLK = {GPIO_PORTA_BASE, GPIO_PIN_2},
        .CS = {GPIO_PORTA_BASE, GPIO_PIN_3},
        .RX = {GPIO_PORTA_BASE, GPIO_PIN_4},
        .TX = {GPIO_PORTA_BASE, GPIO_PIN_5}
    }
};

static DriverLibProgrammer *ProgrPtr = &Progr;
static uint32_t CurrentSpiMode;
static uint32_t CurrentSpiFreq;

/* 
 * The TM4C has a max clock speed of 80 MHz,
 * or 12.5 ns per instruction.
 */
const uint32_t Programmer_MinimumDelay = 13;

int Programmer_init(void) {
    SysCtlClockSet(SYSCTL_SYSDIV_2_5 | SYSCTL_USE_PLL | SYSCTL_XTAL_16MHZ | SYSCTL_OSC_MAIN);

    for (uint32_t *port = ProgrPtr->ports; *port != 0; port++) {
        SysCtlPeripheralEnable(*port);
        while (!SysCtlPeripheralReady(*port))
            ;
    }
    return 1;
}

int Programmer_initParallel(void) {
    GPIOPinTypeGPIOOutput(ProgrPtr->WEn.port, ProgrPtr->WEn.pin);
    GPIOPinTypeGPIOOutput(ProgrPtr->CEn.port, ProgrPtr->CEn.pin);
    GPIOPinTypeGPIOOutput(ProgrPtr->OEn.port, ProgrPtr->OEn.pin);

    GPIOPinWrite(ProgrPtr->WEn.port, ProgrPtr->WEn.pin, ProgrPtr->WEn.pin);
    GPIOPinWrite(ProgrPtr->CEn.port, ProgrPtr->CEn.pin, ProgrPtr->CEn.pin);
    GPIOPinWrite(ProgrPtr->OEn.port, ProgrPtr->OEn.pin, ProgrPtr->OEn.pin);

    for (int i = 0; i < MAX_ADDRESS_WIDTH; i++ ) {
        GPIOPinTypeGPIOOutput(ProgrPtr->A[i].port, ProgrPtr->A[i].pin);
    }    

    return 1;
}

int Programmer_initSpi(void) {
    SysCtlPeripheralEnable(SYSCTL_PERIPH_SSI0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    SysCtlPeripheralEnable(ProgrPtr->spi.CLK.port);
    SysCtlPeripheralEnable(ProgrPtr->spi.CS.port);
    SysCtlPeripheralEnable(ProgrPtr->spi.RX.port);
    SysCtlPeripheralEnable(ProgrPtr->spi.TX.port);

    GPIOPinConfigure(GPIO_PA2_SSI0CLK);
    GPIOPinConfigure(GPIO_PA4_SSI0RX);
    GPIOPinConfigure(GPIO_PA5_SSI0TX);

    GPIOPinTypeSSI(GPIO_PORTA_BASE, 
                     GPIO_PIN_5 | GPIO_PIN_4 | GPIO_PIN_2);

    GPIOPinTypeGPIOOutput(ProgrPtr->spi.CS.port, ProgrPtr->spi.CS.pin);

    /* Default to 1MHz, don't need to set CurrentSpiMode
       because its 0 by default. */
    if (CurrentSpiFreq == 0) {
        CurrentSpiFreq = 1000000;
    }

    SSIConfigSetExpClk(SSI0_BASE, SysCtlClockGet(), CurrentSpiMode, 
            SSI_MODE_MASTER, CurrentSpiFreq, 8);

    GPIOPinWrite(ProgrPtr->spi.CS.port, ProgrPtr->spi.CS.pin, ProgrPtr->spi.CS.pin);

    SSIEnable(SSI0_BASE);

    return 1;
}

// TODO: confirm that this disables peripheral
int Programmer_disableIOPins(void) {
    for (uint32_t *port = ProgrPtr->ports; *port != 0; port++) {
        SysCtlPeripheralDisable(*port);
    }
    return 1;
}

int Programmer_toggleDataIOMode(uint8_t mode) {
    if (mode == 0) {
        for (int i = 0; i < MAX_DATA_WIDTH; i++) {
            GPIOPinTypeGPIOInput(ProgrPtr->IO[i].port, ProgrPtr->IO[i].pin);
        }
    } else {
        for (int i = 0; i < MAX_DATA_WIDTH; i++) {
            GPIOPinTypeGPIOOutput(ProgrPtr->IO[i].port, ProgrPtr->IO[i].pin);
        }
    }
    return 1;
}

int Programmer_getAddressPinCount(void) {
    return sizeof(ProgrPtr->A) / sizeof(ProgrPtr->A[0]);
}

int Programmer_setAddress(uint8_t busWidth, uint32_t address) {
    for (int i = 0; i < busWidth; i++) {
        GPIOPinWrite(ProgrPtr->A[i].port, ProgrPtr->A[i].pin, address & 1 ? ProgrPtr->A[i].pin : 0);
        address >>= 1;
    }
    return 1;
}

int Programmer_setData(uint8_t value) {
    for (int i = 0; i < MAX_DATA_WIDTH; i++) {
        GPIOPinWrite(ProgrPtr->IO[i].port, ProgrPtr->IO[i].pin, (value & 1) ? ProgrPtr->IO[i].pin : 0);
        value >>= 1;
    }
    return 1;
}

int Programmer_toggleCE(uint8_t state) {
    GPIOPinWrite(ProgrPtr->CEn.port, ProgrPtr->CEn.pin, state == 0 ? 0 : ProgrPtr->CEn.pin); 
    return 1;
}

int Programmer_toggleOE(uint8_t state) {
    GPIOPinWrite(ProgrPtr->OEn.port, ProgrPtr->OEn.pin, state == 0 ? 0 : ProgrPtr->OEn.pin); 
    return 1;
}

int Programmer_toggleWE(uint8_t state) {
    GPIOPinWrite(ProgrPtr->WEn.port, ProgrPtr->WEn.pin, state == 0 ? 0 : ProgrPtr->WEn.pin); 
    return 1;
}

uint8_t Programmer_getData(void) {
    uint8_t data = 0;
    for (int i = 0; i < MAX_DATA_WIDTH; i++) {
        data |= (GPIOPinRead(ProgrPtr->IO[i].port, ProgrPtr->IO[i].pin) ? 1 : 0) << i;
    }
    return data;
}

int Programmer_delay1ns(uint32_t delay) {
    /* Assume system clock running at 80MHz -> 12.5ns per cycle
       This means the delay can only be in 12.5ns increments.
       However, as delay increases the percent error decreases.
       Generally it should be okay if the delay is a bit longer
       than requested; shorter could be a problem. 
       So we round up just to be safe. */
    if (delay < Programmer_MinimumDelay || delay > (UINT32_MAX / 10)) {
        return 0;
    } else {
        delay = ((delay * 10) + (124)) / 125;  // fixed-point to make 12.5 a whole number
        SysCtlDelay(delay);
        return 1;
    }
}

int Programmer_enableChip(void) {
    return 1;
}

int Programmer_setSpiClockFreq(uint32_t freq) {
    SSIDisable(SSI0_BASE);
    CurrentSpiFreq = freq;
    SSIConfigSetExpClk(SSI0_BASE, SysCtlClockGet(), CurrentSpiMode, 
            SSI_MODE_MASTER, CurrentSpiFreq, 8);
    SSIEnable(SSI0_BASE);
    return 1;
}

int Programmer_setSpiMode(uint8_t mode) {
    SSIDisable(SSI0_BASE);
    CurrentSpiMode = mode;
    SSIConfigSetExpClk(SSI0_BASE, SysCtlClockGet(), CurrentSpiMode, 
            SSI_MODE_MASTER, CurrentSpiFreq, 8);
    SSIEnable(SSI0_BASE);
    return 1;
}

uint8_t Programmer_getSupportedSpiModes(void) {
    return 0;
}

int Programmer_spiTransmit(const char *txbuf, char *rxbuf, size_t count) {
    uint32_t readVal;
    GPIOPinWrite(ProgrPtr->spi.CS.port, ProgrPtr->spi.CS.pin, 0);
    for (size_t i = 0; i < count; i++) {
        SSIDataPut(SSI0_BASE, txbuf[i]);
        SSIDataGet(SSI0_BASE, &readVal);
        rxbuf[i] = (char) readVal;
    }
    GPIOPinWrite(ProgrPtr->spi.CS.port, ProgrPtr->spi.CS.pin, ProgrPtr->spi.CS.pin);
    return 1;
}

int Transport_init(void) {
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);

    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    UARTConfigSetExpClk(UART0_BASE, SysCtlClockGet(), 115200, 
            (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));
    return 1;
}

int Transport_getData(char *in, size_t count) {
    while (count--) {
        *in++ = UARTCharGet(UART0_BASE);
    }
    return 1;
}

int Transport_putData(const char *out, size_t count) {
    while (count--) {
        UARTCharPut(UART0_BASE, *out++);
    }
    return 1;
}

int Transport_dataWaiting(void) {
    return UARTCharsAvail(UART0_BASE);
}

int Transport_flush(void) {
    while (UARTCharsAvail(UART0_BASE)) {
        UARTCharGet(UART0_BASE);
    }
    return 1;
}
