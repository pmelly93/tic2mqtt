#include <stdio.h>
#include <string.h>

#include "broker_helper.h" 
#include "homie_helper.h"
#include "tic2mqtt.h"

static const char * const homie_datatypes[] = {
    "integer",
    "float",
    "boolean",
    "string",
    "enum",
    "color"
};

/**
 * @brief Get string representation for datatype.
 * @param datatype Datatype.
 * @return The string representation.
 */

static const char *homie_get_datatype(int datatype)
{
    return homie_datatypes[datatype];
}

/**
 * @brief Set Last Will and Testament.
 * @param mosq Mosquitto instance.
 */

static void homie_set_will(struct mosquitto *mosq)
{
    char topic[TOPIC_MAXLEN + 1];

    sprintf(topic, "%s%s/$state", HOMIE_BASE_TOPIC, HOMIE_DEVICE_ID);
    mosquitto_will_set(mosq, topic, strlen("lost"), "lost", 2, 1);
}

/**
 * @brief Initialize connection to broker using Homie convention.
 * @param mosq Mosquitto instance.
 * @param attrs Property attributes for 'tic' node.
 */

void homie_init(struct mosquitto *mosq, const struct homie_prop_attrs *attrs)
{
    char topic_prefix[TOPIC_MAXLEN + 1];
    const struct homie_prop_attrs *pattrs;
    char payload[1024 + 1];

    homie_set_will(mosq);

    // -- Device part.

    sprintf(topic_prefix, "%s%s/", HOMIE_BASE_TOPIC, HOMIE_DEVICE_ID);

    // Mandatory device attributes.
    broker_publish(mosq, topic_prefix, "$homie", HOMIE_DEVICE_CONVENTION_VERSION, TIC_QOS);
    broker_publish(mosq, topic_prefix, "$name", HOMIE_DEVICE_NAME, TIC_QOS);
    broker_publish(mosq, topic_prefix, "$state", "ready", TIC_QOS);
    broker_publish(mosq, topic_prefix, "$nodes", HOMIE_DEVICE_ID, TIC_QOS);
    broker_publish(mosq, topic_prefix, "$extensions", HOMIE_DEVICE_EXTENSIONS, TIC_QOS);

    // Optional device attributes.
    broker_publish(mosq, topic_prefix, "$implementation", HOMIE_DEVICE_IMPLEMENTATION, TIC_QOS);

    // -- Node part.

    sprintf(topic_prefix, "%s%s/%s/", HOMIE_BASE_TOPIC, HOMIE_DEVICE_ID, HOMIE_NODE_ID);

    // Mandatory node attributes.
    broker_publish(mosq, topic_prefix, "$name", HOMIE_NODE_NAME, TIC_QOS);
    broker_publish(mosq, topic_prefix, "$type", HOMIE_NODE_TYPE, TIC_QOS);

    for (pattrs = attrs, payload[0] = '\0'; pattrs->prop_id != NULL; pattrs++) {
        if (pattrs > attrs)
            strcat(payload, ",");
        strcat(payload, pattrs->prop_id);
    }

    broker_publish(mosq, topic_prefix, "$properties", payload, TIC_QOS);

    // -- Properties part.

    for (pattrs = attrs; pattrs->prop_id != NULL; pattrs++) {
        sprintf(topic_prefix, "%s%s/%s/%s/", HOMIE_BASE_TOPIC, HOMIE_DEVICE_ID, HOMIE_NODE_ID, pattrs->prop_id);
    
        // Mandatory property attributes.
        broker_publish(mosq, topic_prefix, "$name", pattrs->name, TIC_QOS);
        broker_publish(mosq, topic_prefix, "$datatype", homie_get_datatype(pattrs->datatype), TIC_QOS);
    
        // Optional property attributes.
        broker_publish(mosq, topic_prefix, "$unit", pattrs->unit, TIC_QOS);

        if (pattrs->datatype == HOMIE_ENUM) {
            const char * const *value;

            for (value = pattrs->values, payload[0] = '\0'; *value != NULL; value++) {
                if (value > pattrs->values)
                    strcat(payload, ",");
                strcat(payload, *value);
            }

            broker_publish(mosq, topic_prefix, "$format", payload, TIC_QOS);
        }
    }
}

/**
 * @brief Close connection to broker using Homie convention.
 * @param mosq Mosquitto instance.
 */

void homie_close(struct mosquitto *mosq)
{
    char topic_prefix[TOPIC_MAXLEN + 1];

    sprintf(topic_prefix, "%s%s/", HOMIE_BASE_TOPIC, HOMIE_DEVICE_ID);

    broker_publish(mosq, topic_prefix, "$state", "disconnected", TIC_QOS);
}
