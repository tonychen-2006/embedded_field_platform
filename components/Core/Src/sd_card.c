#include "sd_card.h"

#include <stddef.h>
#include <string.h>

#include "fatfs.h"
#include "gps_logger.h"

static bool sd_ready;
static SDCard_Status sd_last_status = SD_CARD_NOT_READY;
static uint32_t sd_write_errors;

static SDCard_Status SDCard_FromFatFs(FRESULT result);
static bool SDCard_BuildPath(char *buffer, uint16_t size, const char *filename);
static bool SDCard_IsCsvHeader(const char *text);

SDCard_Status SDCard_Init(SPI_HandleTypeDef *hspi)
{
    FRESULT result;

    (void)hspi;

    result = f_mount(&USERFatFS, (TCHAR const *)USERPath, 1U);
    sd_last_status = SDCard_FromFatFs(result);
    sd_ready = sd_last_status == SD_CARD_OK;

    return sd_last_status;
}

SDCard_Status SDCard_AppendText(const char *filename, const char *text)
{
    FIL file;
    char path[24];
    FRESULT result;
    UINT bytes_written = 0U;
    UINT length;
    FSIZE_t existing_size;

    if (!sd_ready || filename == NULL || text == NULL)
    {
        sd_last_status = SD_CARD_NOT_READY;
        sd_write_errors++;
        return sd_last_status;
    }

    length = (UINT)strlen(text);
    if (length == 0U)
    {
        return SD_CARD_OK;
    }

    if (!SDCard_BuildPath(path, sizeof(path), filename))
    {
        sd_last_status = SD_CARD_ERROR;
        sd_write_errors++;
        return sd_last_status;
    }

    result = f_open(&file, path, FA_OPEN_APPEND | FA_WRITE);
    if (result != FR_OK)
    {
        sd_last_status = SDCard_FromFatFs(result);
        sd_write_errors++;
        return sd_last_status;
    }

    existing_size = f_size(&file);
    if (existing_size > 0U && SDCard_IsCsvHeader(text))
    {
        (void)f_close(&file);
        sd_last_status = SD_CARD_OK;
        return sd_last_status;
    }

    result = f_write(&file, text, length, &bytes_written);
    if (result == FR_OK && bytes_written == length)
    {
        result = f_sync(&file);
    }

    (void)f_close(&file);

    sd_last_status = SDCard_FromFatFs(result);
    if (sd_last_status != SD_CARD_OK || bytes_written != length)
    {
        sd_last_status = SD_CARD_ERROR;
        sd_write_errors++;
    }

    return sd_last_status;
}

bool SDCard_IsReady(void)
{
    return sd_ready;
}

SDCard_Status SDCard_GetLastStatus(void)
{
    return sd_last_status;
}

uint32_t SDCard_GetWriteErrors(void)
{
    return sd_write_errors;
}

GPSLogger_Status GPSLogger_Write(const char *filename, const char *text)
{
    return SDCard_AppendText(filename, text) == SD_CARD_OK ? GPS_LOGGER_OK
                                                           : GPS_LOGGER_ERROR;
}

static SDCard_Status SDCard_FromFatFs(FRESULT result)
{
    switch (result)
    {
    case FR_OK:
        return SD_CARD_OK;
    case FR_NOT_READY:
    case FR_DISK_ERR:
    case FR_INT_ERR:
        return SD_CARD_NOT_READY;
    case FR_NO_FILESYSTEM:
        return SD_CARD_NO_FILESYSTEM;
    case FR_DENIED:
    case FR_NOT_ENOUGH_CORE:
    case FR_TOO_MANY_OPEN_FILES:
        return SD_CARD_NO_SPACE;
    default:
        return SD_CARD_ERROR;
    }
}

static bool SDCard_BuildPath(char *buffer, uint16_t size, const char *filename)
{
    uint16_t path_length;
    uint16_t filename_length;

    if (buffer == NULL || size == 0U || filename == NULL)
    {
        return false;
    }

    path_length = (uint16_t)strlen(USERPath);
    filename_length = (uint16_t)strlen(filename);

    if ((path_length + filename_length + 1U) > size)
    {
        return false;
    }

    (void)strcpy(buffer, USERPath);
    (void)strcat(buffer, filename);

    return true;
}

static bool SDCard_IsCsvHeader(const char *text)
{
    const char header_prefix[] = "date,utc_time,";

    if (text == NULL)
    {
        return false;
    }

    return strncmp(text, header_prefix, sizeof(header_prefix) - 1U) == 0;
}
