#!/bin/sh

logger "TCP_Process"

#Check If we have set debug
DEBUG=`uci get gateway.general.DEB` 
DEBUG=`echo $DEBUG| awk '{print int($0)}'`
KEY_FILE="/etc/lora/devskey"

server=`uci -q get tcp_client.general.server_address`
port=`uci -q get tcp_client.general.server_port`

# Run Forever - process publish requests.
while [ 1 ]
do
	inotifywait -q -e 'create,modify' /var/iot/channels/
		CID=`ls /var/iot/channels/`
		[ $DEBUG -ge 2 ] && logger "[IoT.TCP]: Check for sensor update"
		if [ -n "$CID" ];then
			[ $DEBUG -ge 2 ] && [ -n "$CID" ] && logger "[IoT.TCP]: Found Data at Local Channels:" $CID
			for channel in $CID; do
					DECODER=`sqlite3 $KEY_FILE "SELECT decoder from abpdevs where devaddr = '$channel';"`					
					logger "[IoT.TCP]: DECODER $DECODER $channel"
					# Send the File
					if [ ! -z $DECODER ]; then
						if [ "$DECODER" == "ASCII" ]; then
							#Send As ASCII String
							rssi=`hexdump -v -e '11/1 "%c"'  -n 16 /var/iot/channels/$channel | tr A-Z a-z`
							payload=`xxd -p /var/iot/channels/$channel`
							payload=`echo ${payload:32}`
							tcp_data=$rssi$payload
						else
							#Decode the sensor value use pre-set format and send
							tcp_data=`/etc/lora/decoder/$DECODER $channel`
						fi
					fi
					
					tcp_data="$channel:$tcp_data;"
					
					echo $tcp_data | telnet $server $port

					# Debug output to log
					if [ $DEBUG -ge 1 ]; then
						logger "[IoT.TCP]:  "
						logger "[IoT.TCP]:-----"
						logger "[IoT.TCP]:server: "$server
						logger "[IoT.TCP]:port: "$port
						logger "[IoT.TCP]:decoder: "$DECODER
						logger "[IoT.TCP]:tcp_data: $tcp_data"
						logger "[IoT.TCP]:------"
					fi 
					

					# Delete the Channel info
					rm /var/iot/channels/$channel*
			done
		fi
done
