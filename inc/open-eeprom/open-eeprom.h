#ifndef __OPEN_EEPROM_H__
#define __OPEN_EEPROM_H__

#include <stdint.h>
#include <stddef.h>

enum IoState {
    IO_STATE_DISABLED,
    IO_STATE_ENABLED,
};

enum BusMode {
    BUS_MODE_NOT_SET = 0,
    BUS_MODE_PARALLEL = 1,
    BUS_MODE_SPI = 2,
    BUS_MODE_I2C = 4,
};

enum SpiMode {
    SPI_MODE_0 = 1 ,
    SPI_MODE_1 = 2,
    SPI_MODE_2 = 4,
    SPI_MODE_3 = 8,
};

enum OpenEEPROM_Command {
    EPROG_CMD_NOP,
    EPROG_CMD_GET_INTERFACE_VERSION,
    EPROG_CMD_GET_MAX_RX_SIZE,
    EPROG_CMD_GET_MAX_TX_SIZE,
    EPROG_CMD_TOGGLE_IO,
    EPROG_CMD_GET_SUPPORTED_BUS_TYPES,
    EPROG_CMD_SET_ADDRESS_BUS_WIDTH,
    EPROG_CMD_SET_ADDRESS_HOLD_TIME,
    EPROG_CMD_SET_PULSE_WIDTH_TIME,
    EPROG_CMD_PARALLEL_READ,
    EPROG_CMD_PARALLEL_WRITE,
    EPROG_CMD_SET_SPI_CLOCK_FREQ,
    EPROG_CMD_SET_SPI_MODE,
    EPROG_CMD_GET_SUPPORTED_SPI_MODES,
    EPROG_CMD_SPI_TRANSMIT,
};

extern const uint8_t OpenEEPROM_ACK;
extern const uint8_t OpenEEPROM_NAK;

/* General Commands */
int OpenEEPROM_Init(char *rxbuf, size_t maxRxSize, char *txbuf, size_t maxTxSize);
size_t OpenEEPROM_RunCommand(void);
int OpenEEPROM_nop(const char *in, char *out);
int OpenEEPROM_getInterfaceVersion(const char *in, char *out);
int OpenEEPROM_getSupportedBusTypes(const char *in, char *out);
int OpenEEPROM_getMaxRxSize(const char *in, char *out);
int OpenEEPROM_getMaxTxSize(const char *in, char *out);
int OpenEEPROM_ToggleIo(const char *in, char *out);

/* Parallel Commands */
int OpenEEPROM_setAddressBusWidth(const char *in, char *out);
int OpenEEPROM_setAddressHoldTime(const char *in, char *out);
int OpenEEPROM_setAddressPulseWidthTime(const char *in, char *out);
int OpenEEPROM_parallelRead(const char *in, char *out);
int OpenEEPROM_parallelWrite(const char *in, char *out);

/* SPI Commands */
int OpenEEPROM_setSpifrequency(const char *in, char *out);
int OpenEEPROM_setSpiMode(const char *in, char *out);
int OpenEEPROM_getSupportedSpiModes(const char *in, char *out);

int OpenEEPROM_SpiTransmit(const char *in, char *out);

#endif /* __OPEN_EEPROM_H__ */

