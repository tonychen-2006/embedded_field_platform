#ifndef SD_CARD_H
#define SD_CARD_H

#include <stdbool.h>
#include <stdint.h>

#include "main.h"

typedef enum
{
    SD_CARD_OK = 0,
    SD_CARD_ERROR = 1,
    SD_CARD_NOT_READY = 2,
    SD_CARD_NO_FILESYSTEM = 3,
    SD_CARD_NO_SPACE = 4
} SDCard_Status;

SDCard_Status SDCard_Init(SPI_HandleTypeDef *hspi);
SDCard_Status SDCard_AppendText(const char *filename, const char *text);

bool SDCard_IsReady(void);
SDCard_Status SDCard_GetLastStatus(void);
uint32_t SDCard_GetWriteErrors(void);

#endif /* SD_CARD_H */
