#ifndef __HOMIE_HELPER_H__
#define __HOMIE_HELPER_H__ 1

#include <mosquitto.h>

#define HOMIE_BASE_TOPIC "homie/"

#define HOMIE_DEVICE_ID                 "linky"
#define HOMIE_DEVICE_CONVENTION_VERSION "3.0.0"
#define HOMIE_DEVICE_NAME               "Linky"
#define HOMIE_DEVICE_EXTENSIONS         ""
#define HOMIE_DEVICE_IMPLEMENTATION     "RPi"
#define HOMIE_DEVICE_NODES              "tic"

#define HOMIE_NODE_ID   "tic"
#define HOMIE_NODE_NAME "TIC output"
#define HOMIE_NODE_TYPE "Electricity meter"

enum {
    HOMIE_INTEGER,
    HOMIE_FLOAT,
    HOMIE_BOOLEAN,
    HOMIE_STRING,
    HOMIE_ENUM,
    HOMIE_COLOR
};

/* Attributes for Homie property. */

struct homie_prop_attrs {
    const char *prop_id;        // Property id.
    const char *name;           // Friendly name of the property.
    const int datatype;         // Data type.
    const char *unit;           // Optional unit of this property.
    const char * const *values; // Enumeration of all valid payloads (datatype == HOMIE_ENUM).
};

extern void homie_init(struct mosquitto *mosq, const struct homie_prop_attrs *attrs);
extern void homie_close(struct mosquitto *mosq);

#endif /* __HOMIE_HELPER_H__ */
