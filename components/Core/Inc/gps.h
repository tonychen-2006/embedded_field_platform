#ifndef GPS_H
#define GPS_H

#include <stdint.h>

#include "main.h"
#include "nmea_parse.h"

#define GPS_LINE_BUFFER_SIZE    128U

/*
 * GPS receive flow:
 * 1. CubeMX configures USART RX and the USART global interrupt.
 * 2. GPS_StartReceive() starts one-byte UART receive interrupts.
 * 3. HAL_UART_RxCpltCallback() feeds bytes into one pending NMEA sentence.
 * 4. GPS_Process() parses the pending sentence from the main loop.
 */

/* Clear internal state before starting GPS receive. */
void GPS_Init(void);

/* Start UART receive on the UART connected to the GPS module. */
HAL_StatusTypeDef GPS_StartReceive(UART_HandleTypeDef *huart);

/* Call often from the main loop so the pending NMEA sentence gets parsed. */
void GPS_Process(void);

/* Exposed for tests or other receive paths. */
void GPS_ProcessByte(uint8_t byte);
void GPS_ProcessBuffer(uint8_t *buffer, uint16_t length);

/* Prefer GPS_GetData() when reading from application code. */
bool GPS_HasNewData(void);
void GPS_ClearNewData(void);
GPS GPS_GetData(void);
const GPS *GPS_GetDataPtr(void);

#endif /* GPS_H */
