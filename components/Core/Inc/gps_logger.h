#ifndef GPS_LOGGER_H
#define GPS_LOGGER_H

#include <stdbool.h>
#include <stdint.h>

#include "nmea_parse.h"

#define GPS_LOGGER_INTERVAL_MS       (15UL * 60UL * 1000UL)
#define GPS_LOGGER_CSV_FILENAME      "gps.csv"
#define GPS_LOGGER_GPX_FILENAME      "track.gpx"
#define GPS_LOGGER_TEXT_BUFFER_SIZE  256U

typedef enum
{
    GPS_LOGGER_OK = 0,
    GPS_LOGGER_ERROR = 1
} GPSLogger_Status;

/*
 * Call once after SPI/SD setup. This writes the CSV and GPX headers through
 * GPSLogger_Write().
 */
void GPSLogger_Init(void);

/* Call from the main loop after GPS_Process() finds new parsed GPS data. */
void GPSLogger_Process(const GPS *gps, uint32_t now_ms);

/* Write one CSV row and one GPX track point immediately. */
GPSLogger_Status GPSLogger_LogNow(const GPS *gps);

/* Write the GPX footer. Call this before a planned shutdown, if possible. */
GPSLogger_Status GPSLogger_Close(void);

uint32_t GPSLogger_GetWriteErrors(void);

int GPSLogger_FormatCsvHeader(char *buffer, uint16_t size);
int GPSLogger_FormatCsvLine(char *buffer, uint16_t size, const GPS *gps);
int GPSLogger_FormatGpxHeader(char *buffer, uint16_t size);
int GPSLogger_FormatGpxPoint(char *buffer, uint16_t size, const GPS *gps);
int GPSLogger_FormatGpxFooter(char *buffer, uint16_t size);

/*
 * Storage hook.
 *
 * The default implementation in gps_logger.c is weak and returns
 * GPS_LOGGER_ERROR. When SD/FatFs is added, create one normal implementation
 * that appends text to filename.
 */
GPSLogger_Status GPSLogger_Write(const char *filename, const char *text);

#endif /* GPS_LOGGER_H */
