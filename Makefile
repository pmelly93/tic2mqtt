CFLAGS += -Wall -Werror

TIC2MQTT_OBJS = tic2mqtt.o broker_helper.o homie_helper.o
TIC2MQTT_LIBS = -lmosquitto

.PHONY: all
all: tic2mqtt

tic2mqtt: $(TIC2MQTT_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(TIC2MQTT_OBJS) $(TIC2MQTT_LIBS)

.PHONY: test
test: tic2mqtt
	-./tic2mqtt -t /dev/ttyS0 -h 10.0.0.5 -p 1883 -k 60

.PHONY: clean
clean:
	-rm tic2mqtt $(TIC2MQTT_OBJS)
