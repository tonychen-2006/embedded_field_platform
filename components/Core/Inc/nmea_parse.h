#ifndef NMEA_PARSE_H
#define NMEA_PARSE_H

#include "gps.h"

int nmea_parse_sentence(GPS *gps, char *sentence);

#endif /* NMEA_PARSE_H */