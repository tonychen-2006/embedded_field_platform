#include "gps.h"

#include <string.h>

static GPS gps_data;
static UART_HandleTypeDef *gps_uart;

static uint8_t gps_rx_byte;
static char gps_line[GPS_LINE_BUFFER_SIZE];
static char gps_pending_line[GPS_LINE_BUFFER_SIZE];
static uint16_t gps_line_index;
static volatile bool gps_line_ready;

static HAL_StatusTypeDef GPS_RestartReceive(void);
static void GPS_SaveCompletedLine(void);
static void GPS_ProcessLine(char *line);
static float GPS_ApplyDirection(float value, char direction);

void GPS_Init(void)
{
    memset(&gps_data, 0, sizeof(gps_data));
    memset(gps_line, 0, sizeof(gps_line));
    memset(gps_pending_line, 0, sizeof(gps_pending_line));

    gps_uart = NULL;
    gps_rx_byte = 0U;
    gps_line_index = 0U;
    gps_line_ready = false;
}

HAL_StatusTypeDef GPS_StartReceive(UART_HandleTypeDef *huart)
{
    if (huart == NULL)
    {
        return HAL_ERROR;
    }

    gps_uart = huart;
    gps_rx_byte = 0U;
    gps_line_index = 0U;
    gps_line_ready = false;

    return GPS_RestartReceive();
}

void GPS_Process(void)
{
    char line[GPS_LINE_BUFFER_SIZE];

    if (!gps_line_ready)
    {
        return;
    }

    strncpy(line, gps_pending_line, sizeof(line) - 1U);
    line[sizeof(line) - 1U] = '\0';
    gps_line_ready = false;

    GPS_ProcessLine(line);
}

void GPS_ProcessByte(uint8_t byte)
{
    gps_data.bytesReceived++;

    if (byte == '$')
    {
        gps_line_index = 0U;
        gps_line[gps_line_index++] = (char)byte;
        return;
    }

    if (gps_line_index == 0U)
    {
        return;
    }

    if (byte == '\r')
    {
        return;
    }

    if (byte == '\n')
    {
        gps_line[gps_line_index] = '\0';
        GPS_SaveCompletedLine();
        gps_line_index = 0U;
        return;
    }

    if (gps_line_index < (GPS_LINE_BUFFER_SIZE - 1U))
    {
        gps_line[gps_line_index++] = (char)byte;
    }
    else
    {
        gps_line_index = 0U;
        gps_data.overflowErrors++;
    }
}

void GPS_ProcessBuffer(uint8_t *buffer, uint16_t length)
{
    if (buffer == NULL)
    {
        return;
    }

    for (uint16_t i = 0U; i < length; i++)
    {
        GPS_ProcessByte(buffer[i]);
    }
}

bool GPS_HasNewData(void)
{
    return gps_data.newData;
}

void GPS_ClearNewData(void)
{
    gps_data.newData = false;
}

GPS GPS_GetData(void)
{
    GPS copy = gps_data;

    gps_data.newData = false;

    return copy;
}

const GPS *GPS_GetDataPtr(void)
{
    return &gps_data;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart != gps_uart)
    {
        return;
    }

    GPS_ProcessByte(gps_rx_byte);
    (void)GPS_RestartReceive();
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    uint32_t error_code;

    if (huart != gps_uart)
    {
        return;
    }

    error_code = huart->ErrorCode;
    gps_data.receiveErrors++;
    gps_data.lastUartErrorCode = error_code;

    if ((error_code & HAL_UART_ERROR_ORE) != 0U)
    {
        gps_data.uartOverrunErrors++;
    }
    if ((error_code & HAL_UART_ERROR_FE) != 0U)
    {
        gps_data.uartFramingErrors++;
    }
    if ((error_code & HAL_UART_ERROR_NE) != 0U)
    {
        gps_data.uartNoiseErrors++;
    }
    if ((error_code & HAL_UART_ERROR_PE) != 0U)
    {
        gps_data.uartParityErrors++;
    }

    (void)GPS_RestartReceive();
}

static HAL_StatusTypeDef GPS_RestartReceive(void)
{
    if (gps_uart == NULL)
    {
        return HAL_ERROR;
    }

    return HAL_UART_Receive_IT(gps_uart, &gps_rx_byte, 1U);
}

static void GPS_SaveCompletedLine(void)
{
    if (gps_line_ready)
    {
        gps_data.sentencesDropped++;
        return;
    }

    strncpy(gps_pending_line, gps_line, sizeof(gps_pending_line) - 1U);
    gps_pending_line[sizeof(gps_pending_line) - 1U] = '\0';
    gps_line_ready = true;
    gps_data.sentencesReceived++;
}

static void GPS_ProcessLine(char *line)
{
    int parsed_sentences;

    if (line == NULL)
    {
        return;
    }

    parsed_sentences = nmea_parse(&gps_data, (uint8_t *)line);

    if (parsed_sentences > 0)
    {
        gps_data.latitude = GPS_ApplyDirection(gps_data.latitude, gps_data.latSide);
        gps_data.longitude = GPS_ApplyDirection(gps_data.longitude, gps_data.lonSide);
        gps_data.sentencesParsed += (uint32_t)parsed_sentences;
        gps_data.newData = true;
    }
}

static float GPS_ApplyDirection(float value, char direction)
{
    if ((direction == 'S' || direction == 'W') && value > 0.0f)
    {
        return -value;
    }

    return value;
}
