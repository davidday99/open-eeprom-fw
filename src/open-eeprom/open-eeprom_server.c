/** 
 * @file
 * 
 * This file contains a server implementation
 * for handling OpenEEPROM commands.
 *
 * It also includes the remaining implementations 
 * for commands that are related to transport
 * such as buffer size and syncing. 
 * These functions follow the same conventions
 * as those in `open_eeprom_core.c`.
 */

#include "open-eeprom.h"
#include "open-eeprom_server.h"
#include "programmer.h"
#include "transport.h"
#include "string.h"

static char *RxBuf;
static char *TxBuf;
static size_t RxBufSize;
static size_t TxBufSize;

static int (*Commands[])(const char *in, char *out) = {
    OpenEEPROM_nop,
    OpenEEPROM_sync,
    OpenEEPROM_getInterfaceVersion,
    OpenEEPROM_getMaxRxSize,
    OpenEEPROM_getMaxTxSize,
    OpenEEPROM_toggleIO,
    OpenEEPROM_getSupportedBusTypes,
    OpenEEPROM_setAddressBusWidth,
    OpenEEPROM_setAddressHoldTime,
    OpenEEPROM_setAddressPulseWidthTime,
    OpenEEPROM_parallelRead,
    OpenEEPROM_parallelWrite,
    OpenEEPROM_setSpiFrequency,
    OpenEEPROM_setSpiMode,
    OpenEEPROM_getSupportedSpiModes,
    OpenEEPROM_spiTransmit,
};

static int parseCommand(void);

/**
 * @brief Initialize the internal state of the OpenEEPROM server.
 *
 * Initialize the receive and transmit buffers and their sizes.
 * These buffers should be defined in the calling code.
 *
 * @param rxbuf buffer from which commands are read
 * 
 * @param maxRxSize size of rxbuf
 *
 * @param txbuf buffer to which responses are written
 *
 * @param maxTxSize size of txbuf
 *
 * @return 1 if successful, else 0
 */
int OpenEEPROM_serverInit(char *rxbuf, size_t maxRxSize, char *txbuf, size_t maxTxSize) {
    // TODO: error checking
    RxBuf = rxbuf;
    TxBuf = txbuf;
    RxBufSize = maxRxSize;
    TxBufSize = maxTxSize;
    Programmer_init();
    Transport_init();
    return 1;
}

/**
 * @brief Check for and run any pending commands.
 *
 * To make use of the server, the caller should 
 * call this function periodically. For example, 
 * it could be placed inside of a loop along with 
 * a function to enter a low-power sleep mode until 
 * an external event such as new data received.
 *
 * @return 1 if a valid command was received and run,
 *      or 0 if the command was invalid or no command
 *      was pending
 */
int OpenEEPROM_serverTick(void) {
    int validCmd = 0;
    int response_len = 1;

    if (!Transport_dataWaiting()) {
        return 0;
    }

    validCmd = parseCommand();
    

    if (validCmd) {
        response_len = OpenEEPROM_runCommand(RxBuf, TxBuf);
    } else {
        TxBuf[0] = OpenEEPROM_NAK;
    } 

    Transport_putData(TxBuf, response_len);

    return validCmd;
}

/**
 * @brief Run an OpenEEPROM command.
 * 
 * Interpret the contents of the input 
 * buffer as a valid command, run it, and 
 * write the response to the output buffer.
 * 
 * This function does no input validation.
 *
 * @param in a well-formed OpenEEPROM command
 *
 * @param out response for the command
 *
 * @return length in bytes of the response
 */
size_t OpenEEPROM_runCommand(const char *in, char *out) {
    enum OpenEEPROM_Command cmd;
    size_t response_len = 0;
    memcpy(&cmd, in, sizeof(cmd));

    int (*func)(const char *in, char *out) = Commands[(uint8_t) cmd];

    response_len = func(in, out);

    return response_len;
}

/**
 * @brief Flush any data in the transport.
 *
 * @return 1
 */
int OpenEEPROM_sync(const char *in, char *out) {
    Transport_flush();
    out[0] = OpenEEPROM_ACK;
    return sizeof(OpenEEPROM_ACK);
}

/**
 * @brief Return the max size of the receive buffer.
 *
 * This determines the max number of bytes
 * that can be sent in a single command.
 *
 * @param out 32-bit receive buffer size
 *
 * @return 5
 */
int OpenEEPROM_getMaxRxSize(const char *in, char *out) {
    out[0] = OpenEEPROM_ACK;
    memcpy(&out[sizeof(OpenEEPROM_ACK)], &RxBufSize, sizeof(RxBufSize));
    return sizeof(OpenEEPROM_ACK) + sizeof(RxBufSize);
}

/**
 * @brief Return the max size of the transmit buffer.
 *
 * This determines the max number of bytes
 * that can be returned from a single response.
 *
 * @param out 32-bit transmit buffer size
 *
 * @return 5
 */
int OpenEEPROM_getMaxTxSize(const char *in, char *out) {
    out[0] = OpenEEPROM_ACK;
    memcpy(&out[sizeof(OpenEEPROM_ACK)], &TxBufSize, sizeof(TxBufSize));
    return sizeof(OpenEEPROM_ACK) + sizeof(TxBufSize);
}

static int parseCommand(void) {
    unsigned int idx = 0;
    uint32_t nLen;
    int validCmd = 1;
    Transport_getData(RxBuf, 1); 
    idx++;

    enum OpenEEPROM_Command cmd;
    memcpy(&cmd, RxBuf, sizeof(cmd));

    switch (cmd) {
        case OPEN_EEPROM_CMD_NOP:
        case OPEN_EEPROM_CMD_SYNC:
        case OPEN_EEPROM_CMD_GET_INTERFACE_VERSION:
        case OPEN_EEPROM_CMD_GET_MAX_RX_SIZE:
        case OPEN_EEPROM_CMD_GET_MAX_TX_SIZE:
        case OPEN_EEPROM_CMD_GET_SUPPORTED_BUS_TYPES:
        case OPEN_EEPROM_CMD_GET_SUPPORTED_SPI_MODES:
            break;

        case OPEN_EEPROM_CMD_TOGGLE_IO:
        case OPEN_EEPROM_CMD_SET_ADDRESS_BUS_WIDTH:
        case OPEN_EEPROM_CMD_SET_SPI_MODE:
            Transport_getData(&RxBuf[idx], 1);
            idx++;
            break;
        
        case OPEN_EEPROM_CMD_SET_ADDRESS_HOLD_TIME:
        case OPEN_EEPROM_CMD_SET_PULSE_WIDTH_TIME:
        case OPEN_EEPROM_CMD_SET_SPI_CLOCK_FREQ:
            Transport_getData(&RxBuf[idx], 4);
            idx += 4;  
            break;

        case OPEN_EEPROM_CMD_PARALLEL_WRITE:   
            Transport_getData(&RxBuf[idx], 4);
            idx += 4;
            Transport_getData(&RxBuf[idx], 4);
            memcpy(&nLen, &RxBuf[idx], sizeof(nLen));
            idx += 4;
            
            // Account for the 9 bytes already inside the buffer.
            if (nLen + 9 > RxBufSize) {
                validCmd = 0;
            } else {
                Transport_getData(&RxBuf[idx], nLen);
                idx += nLen;
            }

            break;

        case OPEN_EEPROM_CMD_PARALLEL_READ:   
            Transport_getData(&RxBuf[idx], 4);
            idx += 4;
            Transport_getData(&RxBuf[idx], 4);
            memcpy(&nLen, &RxBuf[idx], sizeof(nLen));
            idx += 4;

            // Account for the status byte inside the buffer.
            if (nLen + 1 > TxBufSize) {
                validCmd = 0;
            }
            
            break;

        case OPEN_EEPROM_CMD_SPI_TRANSMIT:
            Transport_getData(&RxBuf[idx], 4);
            memcpy(&nLen, &RxBuf[idx], sizeof(nLen));
            idx += 4;
            
            /* In addition to the n bytes represented by nLen, the RxBuf will already contain
               the 1-byte command and 4-byte nLen, and the TxBuf will contain the command status ACK/NAK. */
            if (nLen + 5  > RxBufSize || nLen + 1 > TxBufSize) {
                validCmd = 0;
            } else {
                Transport_getData(&RxBuf[idx], nLen);
            }

            break;

        default:
            validCmd = 0;
            break;
    }
    
    return validCmd;
}

