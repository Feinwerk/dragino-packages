mips-openwrt-linux-gcc stdoutsub.c  MQTTClient.c MQTTLinux.c MQTTFormat.c  MQTTPacket.c MQTTDeserializePublish.c MQTTConnectClient.c MQTTSubscribeClient.c MQTTSerializePublish.c -o stdoutsub MQTTConnectServer.c MQTTSubscribeServer.c MQTTUnsubscribeServer.c MQTTUnsubscribeClient.c -DMQTTCLIENT_PLATFORM_HEADER=MQTTLinux.h -lpthread
