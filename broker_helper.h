#ifndef __BROKER_HELPER_H__
#define __BROKER_HELPER_H__ 1

#define TOPIC_MAXLEN 255

extern struct mosquitto *broker_open(const char *host, int port, int keepalive);
extern void broker_close(struct mosquitto *mosq);
extern int broker_publish(struct mosquitto *mosq, const char *topic_prefix, const char *topic_suffix, const void *payload, int qos);

#endif /* __BROKER_HELPER_H__ */
