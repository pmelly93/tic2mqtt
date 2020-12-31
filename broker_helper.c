#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <syslog.h>

#include <mosquitto.h>

#include "broker_helper.h"

/**
 * @brief Log callback for MQTT.
 * @param mosq Mosquitto instance making the callback.
 * @param userdata User data provided in mosquitto_new.
 * @param level Log message level.
 * @param str Message string.
 */

static void mosq_log_callback(struct mosquitto *mosq, void *userdata, int level, const char *str)
{
    switch (level) {
#if 0
    case MOSQ_LOG_DEBUG:
        syslog(LOG_DEBUG, "%s", str);
        break;

    case MOSQ_LOG_INFO:
        syslog(LOG_INFO, "%s", str);
        break;

    case MOSQ_LOG_NOTICE:
        syslog(LOG_NOTICE, "%s", str);
        break;
#endif

    case MOSQ_LOG_WARNING:
        syslog(LOG_WARNING, "%s", str);
        break;

    case MOSQ_LOG_ERR:
        syslog(LOG_ERR, "%s", str);
        break;

    default:
        break;
    }
}

/**
 * @brief Open connection to MQTT broker.
 * @brief host The hostname or ip address of the broker to connect to.
 * @brief port The network port to connect to.
 * @brief keepalive The number of seconds after which the broker should send a PING message to the client if no other messages have been exchanged in that time.
 * @return Pointer to a struct mosquitto on success. NULL on failure.
 */

struct mosquitto *broker_open(const char *host, int port, int keepalive)
{
    struct mosquitto *mosq;
    int res;

    mosquitto_lib_init();

    /* Create MQTT client. */
    mosq = mosquitto_new(NULL, 1, NULL);
    if (mosq == NULL) {
        syslog(LOG_ERR, "Cannot create mosquitto client: %s", strerror(errno));
        return NULL;
    }

    mosquitto_log_callback_set(mosq, mosq_log_callback);

    /* Connect to broker. */
    res = mosquitto_connect(mosq, host, port, keepalive);
    if (res != MOSQ_ERR_SUCCESS) {
        syslog(LOG_ERR, "Unable to connect to MQTT broker %s:%d: %s", host, port, mosquitto_strerror(res));
        return NULL;
    }

    /* Start network loop. */
    res = mosquitto_loop_start(mosq);
    if (res != MOSQ_ERR_SUCCESS) {
        syslog(LOG_ERR, "Unable to start loop: %s", mosquitto_strerror(res));
        return NULL;
    }

    return mosq;
}

/**
 * @brief Close connection to MQTT broker.
 * @param mosq Mosquitto instance.
 */

void broker_close(struct mosquitto *mosq)
{
    /* Stop network loop. */
    mosquitto_loop_stop(mosq, 1);

    /* Disconnect from broker. */
    mosquitto_disconnect(mosq);

    /* Create MQTT client. */
    mosquitto_destroy(mosq);

    mosquitto_lib_cleanup();
}

/**
 * @brief Publish a message to MQTT broker.
 * @param mosq Mosquitto instance.
 * @param topic_prefix Topic prefix. May be NULL.
 * @param topic_suffix Topic suffix. May be NULL.
 * @param payload Payload.
 * @param qos QOS.
 * @return 0 on success, -1 on failure.
 * @note topic_prefix and topic_suffix cannot be both NULL.
 */

int broker_publish(struct mosquitto *mosq, const char *topic_prefix, const char *topic_suffix, const void *payload, int qos)
{
    char topic[TOPIC_MAXLEN + 1];
    int res;

    if (topic_prefix != NULL) {
        if (topic_suffix != NULL) {
            sprintf(topic, "%s%s", topic_prefix, topic_suffix);
        } else {
            sprintf(topic, "%s", topic_prefix);
        }
    } else {
        if (topic_suffix != NULL) {
            sprintf(topic, "%s", topic_suffix);
        } else {
            return -1;
        }
    }

    res = mosquitto_publish(mosq, NULL, topic, strlen(payload), payload, qos, 1);
    if (res != 0)
        syslog(LOG_ERR, "Cannot publish topic %s: %s\n", topic, mosquitto_strerror(res));

    return res;
}
