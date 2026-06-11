#ifndef GPS_LOGGER_H
#define GPS_LOGGER_H

#include <stdbool.h>
#include <stdint.h>

#include "nmea_parse.h"

#define GPS_LOGGER_INTERVAL_MS       1000U
#define GPS_LOGGER_GPX_CLOSE_MS      300000U
#define GPS_LOGGER_CSV_FILENAME      "gps.csv"
#define GPS_LOGGER_GPX_FILENAME      "gps.gpx"
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

/* Enable starts a fresh GPX file. Disable closes the current GPX file. */
GPSLogger_Status GPSLogger_SetGpxEnabled(bool enabled, uint32_t now_ms);

/* Call from the main loop so timed close still runs without new GPS data. */
void GPSLogger_Tick(uint32_t now_ms);

/* Write one CSV row and one GPX track point immediately. */
GPSLogger_Status GPSLogger_LogNow(const GPS *gps);

/* Write the GPX footer so the test track can be opened by map tools. */
GPSLogger_Status GPSLogger_Close(void);

uint32_t GPSLogger_GetWriteErrors(void);
bool GPSLogger_IsGpxEnabled(void);
bool GPSLogger_IsGpxClosed(void);

int GPSLogger_FormatCsvHeader(char *buffer, uint16_t size);
int GPSLogger_FormatCsvLine(char *buffer, uint16_t size, const GPS *gps);
int GPSLogger_FormatGpxHeader(char *buffer, uint16_t size);
int GPSLogger_FormatGpxPoint(char *buffer, uint16_t size, const GPS *gps);
int GPSLogger_FormatGpxFooter(char *buffer, uint16_t size);

/*
 * Storage hooks. sd_card.c provides the SD-card implementation. If the SD module
 * is not linked, the weak fallbacks in gps_logger.c return an error.
 */
GPSLogger_Status GPSLogger_Write(const char *filename, const char *text);
GPSLogger_Status GPSLogger_Rewrite(const char *filename, const char *text);

#endif /* GPS_LOGGER_H */
