#include "gps_logger.h"

#include <stdbool.h>
#include <stdio.h>

#if defined(__GNUC__)
#define GPSLOGGER_WEAK __attribute__((weak))
#else
#define GPSLOGGER_WEAK
#endif

static bool gps_logger_started;
static bool gps_logger_has_logged;
static uint32_t gps_logger_last_log_ms;
static uint32_t gps_logger_write_errors;

static bool GPSLogger_HasFix(const GPS *gps);
static void GPSLogger_CountError(void);
static int GPSLogger_CheckLength(int length, uint16_t size);
static bool GPSLogger_FormatDecimal(char *buffer,
                                    uint16_t size,
                                    float value,
                                    uint8_t decimals);
static int32_t GPSLogger_Pow10(uint8_t decimals);
static bool GPSLogger_FormatIsoTime(const GPS *gps, char *buffer, uint16_t size);
static bool GPSLogger_HasDigits(const char *text, uint8_t count);
static uint8_t GPSLogger_TwoDigits(const char *text);

void GPSLogger_Init(void)
{
    char text[GPS_LOGGER_TEXT_BUFFER_SIZE];

    gps_logger_started = true;
    gps_logger_has_logged = false;
    gps_logger_last_log_ms = 0U;
    gps_logger_write_errors = 0U;

    if (GPSLogger_FormatCsvHeader(text, sizeof(text)) < 0 ||
        GPSLogger_Write(GPS_LOGGER_CSV_FILENAME, text) != GPS_LOGGER_OK)
    {
        GPSLogger_CountError();
    }
}

void GPSLogger_Process(const GPS *gps, uint32_t now_ms)
{
    if (!gps_logger_started)
    {
        GPSLogger_Init();
    }

    if (!GPSLogger_HasFix(gps))
    {
        return;
    }

    if (gps_logger_has_logged &&
        (now_ms - gps_logger_last_log_ms) < GPS_LOGGER_INTERVAL_MS)
    {
        return;
    }

    (void)GPSLogger_LogNow(gps);

    gps_logger_last_log_ms = now_ms;
    gps_logger_has_logged = true;
}

GPSLogger_Status GPSLogger_LogNow(const GPS *gps)
{
    char text[GPS_LOGGER_TEXT_BUFFER_SIZE];
    GPSLogger_Status status = GPS_LOGGER_OK;

    if (!GPSLogger_HasFix(gps))
    {
        return GPS_LOGGER_ERROR;
    }

    if (GPSLogger_FormatCsvLine(text, sizeof(text), gps) < 0 ||
        GPSLogger_Write(GPS_LOGGER_CSV_FILENAME, text) != GPS_LOGGER_OK)
    {
        GPSLogger_CountError();
        status = GPS_LOGGER_ERROR;
    }

    return status;
}

GPSLogger_Status GPSLogger_Close(void)
{
    return GPS_LOGGER_OK;
}

uint32_t GPSLogger_GetWriteErrors(void)
{
    return gps_logger_write_errors;
}

int GPSLogger_FormatCsvHeader(char *buffer, uint16_t size)
{
    int length;

    if (buffer == NULL || size == 0U)
    {
        return -1;
    }

    length = snprintf(buffer,
                      size,
                      "date,utc_time,fix,latitude,longitude,altitude_m,"
                      "speed_kmh,heading_deg,satellites,hdop\r\n");

    return GPSLogger_CheckLength(length, size);
}

int GPSLogger_FormatCsvLine(char *buffer, uint16_t size, const GPS *gps)
{
    char latitude[20];
    char longitude[20];
    char altitude[16];
    char speed[16];
    char heading[16];
    char hdop[16];
    int length;

    if (buffer == NULL || size == 0U || gps == NULL)
    {
        return -1;
    }

    if (!GPSLogger_FormatDecimal(latitude, sizeof(latitude), gps->latitude, 6U) ||
        !GPSLogger_FormatDecimal(longitude, sizeof(longitude), gps->longitude, 6U) ||
        !GPSLogger_FormatDecimal(altitude, sizeof(altitude), gps->altitude, 2U) ||
        !GPSLogger_FormatDecimal(speed, sizeof(speed), gps->speedKmh, 2U) ||
        !GPSLogger_FormatDecimal(heading, sizeof(heading), gps->trueHeading, 2U) ||
        !GPSLogger_FormatDecimal(hdop, sizeof(hdop), gps->hdop, 2U))
    {
        return -1;
    }

    length = snprintf(buffer,
                      size,
                      "%s,%s,%u,%s,%s,%s,%s,%s,%u,%s\r\n",
                      gps->date,
                      gps->utcTime,
                      (unsigned int)gps->fix,
                      latitude,
                      longitude,
                      altitude,
                      speed,
                      heading,
                      (unsigned int)gps->satelliteCount,
                      hdop);

    return GPSLogger_CheckLength(length, size);
}

int GPSLogger_FormatGpxHeader(char *buffer, uint16_t size)
{
    int length;

    if (buffer == NULL || size == 0U)
    {
        return -1;
    }

    length = snprintf(buffer,
                      size,
                      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"
                      "<gpx version=\"1.1\" creator=\"embedded_field_platform\" "
                      "xmlns=\"http://www.topografix.com/GPX/1/1\">\r\n"
                      "  <trk>\r\n"
                      "    <name>GPS Track</name>\r\n"
                      "    <trkseg>\r\n");

    return GPSLogger_CheckLength(length, size);
}

int GPSLogger_FormatGpxPoint(char *buffer, uint16_t size, const GPS *gps)
{
    char iso_time[24];
    char latitude[20];
    char longitude[20];
    char altitude[16];
    int length;

    if (buffer == NULL || size == 0U || gps == NULL)
    {
        return -1;
    }

    if (!GPSLogger_FormatDecimal(latitude, sizeof(latitude), gps->latitude, 6U) ||
        !GPSLogger_FormatDecimal(longitude, sizeof(longitude), gps->longitude, 6U) ||
        !GPSLogger_FormatDecimal(altitude, sizeof(altitude), gps->altitude, 2U))
    {
        return -1;
    }

    if (GPSLogger_FormatIsoTime(gps, iso_time, sizeof(iso_time)))
    {
        length = snprintf(buffer,
                          size,
                          "      <trkpt lat=\"%s\" lon=\"%s\">"
                          "<ele>%s</ele><time>%s</time></trkpt>\r\n",
                          latitude,
                          longitude,
                          altitude,
                          iso_time);
    }
    else
    {
        length = snprintf(buffer,
                          size,
                          "      <trkpt lat=\"%s\" lon=\"%s\">"
                          "<ele>%s</ele></trkpt>\r\n",
                          latitude,
                          longitude,
                          altitude);
    }

    return GPSLogger_CheckLength(length, size);
}

int GPSLogger_FormatGpxFooter(char *buffer, uint16_t size)
{
    int length;

    if (buffer == NULL || size == 0U)
    {
        return -1;
    }

    length = snprintf(buffer, size, "    </trkseg>\r\n  </trk>\r\n</gpx>\r\n");

    return GPSLogger_CheckLength(length, size);
}

GPSLOGGER_WEAK GPSLogger_Status GPSLogger_Write(const char *filename,
                                                const char *text)
{
    (void)filename;
    (void)text;

    return GPS_LOGGER_ERROR;
}

static bool GPSLogger_HasFix(const GPS *gps)
{
    return gps != NULL &&
           gps->fix != 0U &&
           (gps->latitude != 0.0f || gps->longitude != 0.0f);
}

static void GPSLogger_CountError(void)
{
    gps_logger_write_errors++;
}

static int GPSLogger_CheckLength(int length, uint16_t size)
{
    if (length < 0 || length >= (int)size)
    {
        return -1;
    }

    return length;
}

static bool GPSLogger_FormatDecimal(char *buffer,
                                    uint16_t size,
                                    float value,
                                    uint8_t decimals)
{
    bool negative;
    int32_t scale;
    int32_t scaled;
    int32_t whole;
    int32_t fraction;
    int length;

    if (buffer == NULL || size == 0U || decimals > 6U)
    {
        return false;
    }

    negative = value < 0.0f;

    if (negative)
    {
        value = -value;
    }

    scale = GPSLogger_Pow10(decimals);
    scaled = (int32_t)((value * (float)scale) + 0.5f);
    if (scaled == 0)
    {
        negative = false;
    }

    whole = scaled / scale;
    fraction = scaled % scale;

    if (decimals == 0U)
    {
        length = snprintf(buffer,
                          size,
                          "%s%ld",
                          negative ? "-" : "",
                          (long)whole);
    }
    else
    {
        length = snprintf(buffer,
                          size,
                          "%s%ld.%0*ld",
                          negative ? "-" : "",
                          (long)whole,
                          (int)decimals,
                          (long)fraction);
    }

    return GPSLogger_CheckLength(length, size) >= 0;
}

static int32_t GPSLogger_Pow10(uint8_t decimals)
{
    int32_t value = 1;

    for (uint8_t i = 0U; i < decimals; i++)
    {
        value *= 10;
    }

    return value;
}

static bool GPSLogger_FormatIsoTime(const GPS *gps, char *buffer, uint16_t size)
{
    uint8_t day;
    uint8_t month;
    uint8_t year_short;
    uint16_t year;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    int length;

    if (gps == NULL || buffer == NULL || size < 21U)
    {
        return false;
    }

    if (!GPSLogger_HasDigits(gps->date, 6U) ||
        !GPSLogger_HasDigits(gps->utcTime, 6U))
    {
        return false;
    }

    day = GPSLogger_TwoDigits(&gps->date[0]);
    month = GPSLogger_TwoDigits(&gps->date[2]);
    year_short = GPSLogger_TwoDigits(&gps->date[4]);
    hour = GPSLogger_TwoDigits(&gps->utcTime[0]);
    minute = GPSLogger_TwoDigits(&gps->utcTime[2]);
    second = GPSLogger_TwoDigits(&gps->utcTime[4]);

    if (day == 0U || day > 31U ||
        month == 0U || month > 12U ||
        hour > 23U || minute > 59U || second > 59U)
    {
        return false;
    }

    year = (year_short >= 80U) ? (uint16_t)(1900U + year_short)
                               : (uint16_t)(2000U + year_short);

    length = snprintf(buffer,
                      size,
                      "%04u-%02u-%02uT%02u:%02u:%02uZ",
                      (unsigned int)year,
                      (unsigned int)month,
                      (unsigned int)day,
                      (unsigned int)hour,
                      (unsigned int)minute,
                      (unsigned int)second);

    return GPSLogger_CheckLength(length, size) >= 0;
}

static bool GPSLogger_HasDigits(const char *text, uint8_t count)
{
    if (text == NULL)
    {
        return false;
    }

    for (uint8_t i = 0U; i < count; i++)
    {
        if (text[i] < '0' || text[i] > '9')
        {
            return false;
        }
    }

    return true;
}

static uint8_t GPSLogger_TwoDigits(const char *text)
{
    return (uint8_t)(((text[0] - '0') * 10) + (text[1] - '0'));
}
