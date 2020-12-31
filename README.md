# tic2mqtt
MQTT client for TIC output of Linky electricity meter

**tic2mqtt** is a simple MQTT client for french electricity meters using the TIC (Télé-Information Client) protocol.

Currently supported meters are:
- Linky single phase meter 60 A.
- Linky single phase meter 90 A.

**tic2mqtt** only supports legacy mode. Standard mode will be added later.

**tic2mqtt** follows the Homie convention (https://homieiot.github.io).

The TIC protocol reference can be found in the Enedis specification **Enedis-NOI-CPT_54E v3** (https://www.enedis.fr/sites/default/files/Enedis-NOI-CPT_54E.pdf).

**tic2mqtt** runs fine on a Raspberry Pi Zero W with a PiTInfo v1.2 shield (https://hallard.me/pitinfov12).
