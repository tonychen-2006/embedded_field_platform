#ifndef NMEA_PARSE_H
#define NMEA_PARSE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    /* Time/date fields are kept as NMEA text so other modules can format them. */
    char utcTime[16];
    char lastMeasure[16];
    char date[8];

    /* Position and movement fields are converted to regular numeric units. */
    float latitude;
    float longitude;
    float altitude;
    float speedKmh;
    float trueHeading;
    float magneticHeading;
    float hdop;
    float pdop;
    float vdop;

    char latSide;
    char lonSide;

    /* Fix quality and satellite health fields. */
    uint8_t fix;
    uint8_t RMC_Flag;
    uint8_t satelliteCount;
    uint8_t satInView;
    uint8_t snr;

    /* Runtime status counters for debugging receive/parse behavior. */
    bool newData;
    uint32_t bytesReceived;
    uint32_t sentencesReceived;
    uint32_t sentencesParsed;
    uint32_t sentencesDropped;
    uint32_t overflowErrors;
    uint32_t receiveErrors;
    uint32_t checksumErrors;
} GPS;

/* Parse a buffer containing one or more NMEA sentences into a GPS struct. */
int nmea_parse(GPS *gps, uint8_t *buffer);

/* Parse one sentence such as "$GNGGA,...*xx" into a GPS struct. */
int nmea_parse_sentence(GPS *gps, char *sentence);

#endif /* NMEA_PARSE_H */
