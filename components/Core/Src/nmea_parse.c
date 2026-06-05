#include "nmea_parse.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#define NMEA_MAX_FIELDS 24
#define NMEA_LINE_SIZE  128

static void split_commas(char *sentence, char *fields[], int max_fields);
static float nmea_to_decimal_degrees(char *value, int degree_digits);

static int parse_gga(GPS *gps, char *fields[]);
static int parse_rmc(GPS *gps, char *fields[]);
static int parse_gsa(GPS *gps, char *fields[]);
static int parse_gll(GPS *gps, char *fields[]);
static int parse_vtg(GPS *gps, char *fields[]);
static int parse_gsv(GPS *gps, char *fields[]);

int nmea_parse_sentence(GPS *gps, char *sentence)
{
    char line[NMEA_LINE_SIZE];
    char *fields[NMEA_MAX_FIELDS] = {0};

    if (gps == NULL || sentence == NULL)
    {
        return 0;
    }

    if (sentence[0] != '$')
    {
        return 0;
    }

    /*
     * Copy sentence because split_commas() modifies the string.
     */
    strncpy(line, sentence, sizeof(line) - 1);
    line[sizeof(line) - 1] = '\0';

    /*
     * Remove checksum part.
     *
     * Before:
     * $GNGGA,123519,...*47
     *
     * After:
     * $GNGGA,123519,...
     */
    char *star = strchr(line, '*');
    if (star != NULL)
    {
        *star = '\0';
    }

    split_commas(line, fields, NMEA_MAX_FIELDS);

    if (fields[0] == NULL)
    {
        return 0;
    }

    /*
     * Decide which parser to use based on sentence type.
     *
     * GP = GPS only
     * GN = multi-GNSS
     */
    if (strcmp(fields[0], "$GPGGA") == 0 || strcmp(fields[0], "$GNGGA") == 0)
    {
        return parse_gga(gps, fields);
    }
    else if (strcmp(fields[0], "$GPRMC") == 0 || strcmp(fields[0], "$GNRMC") == 0)
    {
        return parse_rmc(gps, fields);
    }
    else if (strcmp(fields[0], "$GPGSA") == 0 || strcmp(fields[0], "$GNGSA") == 0)
    {
        return parse_gsa(gps, fields);
    }
    else if (strcmp(fields[0], "$GPGLL") == 0 || strcmp(fields[0], "$GNGLL") == 0)
    {
        return parse_gll(gps, fields);
    }
    else if (strcmp(fields[0], "$GPVTG") == 0 || strcmp(fields[0], "$GNVTG") == 0)
    {
        return parse_vtg(gps, fields);
    }
    else if (strcmp(fields[0], "$GPGSV") == 0 || strcmp(fields[0], "$GNGSV") == 0)
    {
        return parse_gsv(gps, fields);
    }

    return 0;
}

static void split_commas(char *sentence, char *fields[], int max_fields)
{
    int count = 0;
    char *start = sentence;

    while (start != NULL && count < max_fields)
    {
        fields[count] = start;
        count++;

        char *comma = strchr(start, ',');

        if (comma == NULL)
        {
            break;
        }

        *comma = '\0';
        start = comma + 1;
    }
}

static float nmea_to_decimal_degrees(char *value, int degree_digits)
{
    char degrees_text[4] = {0};

    if (value == NULL || value[0] == '\0')
    {
        return 0.0f;
    }

    strncpy(degrees_text, value, degree_digits);

    float degrees = strtof(degrees_text, NULL);
    float minutes = strtof(value + degree_digits, NULL);

    return degrees + minutes / 60.0f;
}

static int parse_gga(GPS *gps, char *fields[])
{
    if (fields[1] != NULL && fields[1][0] != '\0')
    {
        strncpy(gps->utcTime, fields[1], sizeof(gps->utcTime) - 1);
        gps->utcTime[sizeof(gps->utcTime) - 1] = '\0';

        strncpy(gps->lastMeasure, fields[1], sizeof(gps->lastMeasure) - 1);
        gps->lastMeasure[sizeof(gps->lastMeasure) - 1] = '\0';
    }

    if (fields[6] != NULL && fields[6][0] != '\0')
    {
        int fix_quality = atoi(fields[6]);
        gps->fix = fix_quality > 0 ? 1 : 0;
    }

    if (fields[7] != NULL && fields[7][0] != '\0')
    {
        gps->satelliteCount = atoi(fields[7]);
    }

    if (fields[8] != NULL && fields[8][0] != '\0')
    {
        gps->hdop = strtof(fields[8], NULL);
    }

    if (fields[9] != NULL && fields[9][0] != '\0')
    {
        gps->altitude = strtof(fields[9], NULL);
    }

    // Only update latitude and longitude if all coordinate fields exist.

    if (fields[2] != NULL && fields[2][0] != '\0' &&
        fields[3] != NULL && fields[3][0] != '\0' &&
        fields[4] != NULL && fields[4][0] != '\0' &&
        fields[5] != NULL && fields[5][0] != '\0')
    {
        gps->latitude = nmea_to_decimal_degrees(fields[2], 2);
        gps->longitude = nmea_to_decimal_degrees(fields[4], 3);

        gps->latSide = fields[3][0];
        gps->lonSide = fields[5][0];
    }

    return 1;
}

static int parse_rmc(GPS *gps, char *fields[])
{
    if (fields[2] != NULL && fields[2][0] == 'V')
    {
        gps->fix = 0;
        return 0;
    }

    if (fields[1] != NULL && fields[1][0] != '\0')
    {
        strncpy(gps->utcTime, fields[1], sizeof(gps->utcTime) - 1);
        gps->utcTime[sizeof(gps->utcTime) - 1] = '\0';
    }

    if (fields[9] != NULL && strlen(fields[9]) >= 6)
    {
        strncpy(gps->date, fields[9], 6);
        gps->date[6] = '\0';
        gps->RMC_Flag = 1;
    }

    if (fields[3] != NULL && fields[3][0] != '\0' &&
        fields[4] != NULL && fields[4][0] != '\0' &&
        fields[5] != NULL && fields[5][0] != '\0' &&
        fields[6] != NULL && fields[6][0] != '\0')
    {
        gps->latitude = nmea_to_decimal_degrees(fields[3], 2);
        gps->longitude = nmea_to_decimal_degrees(fields[5], 3);

        gps->latSide = fields[4][0];
        gps->lonSide = fields[6][0];

        gps->fix = 1;
    }

    if (fields[7] != NULL && fields[7][0] != '\0')
    {
        float speed_knots = strtof(fields[7], NULL);
        gps->speedKmh = speed_knots * 1.852f;
    }

    return 1;
}

static int parse_gsa(GPS *gps, char *fields[])
{
    if (fields[2] != NULL && fields[2][0] != '\0')
    {
        int fix_type = atoi(fields[2]);
        gps->fix = fix_type > 1 ? 1 : 0;
    }

    int satellite_count = 0;

    for (int i = 3; i <= 14; i++)
    {
        if (fields[i] != NULL && fields[i][0] != '\0')
        {
            satellite_count++;
        }
    }

    gps->satelliteCount = satellite_count;

    if (fields[15] != NULL && fields[15][0] != '\0')
    {
        gps->pdop = strtof(fields[15], NULL);
    }

    if (fields[16] != NULL && fields[16][0] != '\0')
    {
        gps->hdop = strtof(fields[16], NULL);
    }

    if (fields[17] != NULL && fields[17][0] != '\0')
    {
        gps->vdop = strtof(fields[17], NULL);
    }

    return 1;
}

static int parse_gll(GPS *gps, char *fields[])
{
    if (fields[6] != NULL && fields[6][0] == 'V')
    {
        gps->fix = 0;
        return 0;
    }

    if (fields[5] != NULL && fields[5][0] != '\0')
    {
        strncpy(gps->utcTime, fields[5], sizeof(gps->utcTime) - 1);
        gps->utcTime[sizeof(gps->utcTime) - 1] = '\0';
    }

    if (fields[1] != NULL && fields[1][0] != '\0' &&
        fields[2] != NULL && fields[2][0] != '\0' &&
        fields[3] != NULL && fields[3][0] != '\0' &&
        fields[4] != NULL && fields[4][0] != '\0')
    {
        gps->latitude = nmea_to_decimal_degrees(fields[1], 2);
        gps->longitude = nmea_to_decimal_degrees(fields[3], 3);

        gps->latSide = fields[2][0];
        gps->lonSide = fields[4][0];

        gps->fix = 1;
    }

    return 1;
}

static int parse_vtg(GPS *gps, char *fields[])
{
    if (fields[1] != NULL && fields[1][0] != '\0')
    {
        gps->trueHeading = strtof(fields[1], NULL);
    }

    if (fields[3] != NULL && fields[3][0] != '\0')
    {
        gps->magneticHeading = strtof(fields[3], NULL);
    }

    if (fields[7] != NULL && fields[7][0] != '\0')
    {
        gps->speedKmh = strtof(fields[7], NULL);
    }

    return 1;
}

static int parse_gsv(GPS *gps, char *fields[])
{
    if (fields[3] != NULL && fields[3][0] != '\0')
    {
        gps->satInView = atoi(fields[3]);
    }

    for (int i = 7; i < NMEA_MAX_FIELDS; i += 4)
    {
        if (fields[i] != NULL && fields[i][0] != '\0')
        {
            gps->snr = atoi(fields[i]);
            break;
        }
    }

    return 1;
}