#!/bin/bash

ps -ef | grep 'oss' | awk '{print $2}' | xargs kill -9
ps -ef | grep 'worker' | awk '{print $2}' | xargs kill -9
ps -ef | grep 'timekeeper' | awk '{print $2}' | xargs kill -9

segments=$(ipcs -m | awk '/^0x/ {print $2}')

for seg in $segments; do
	ipcrm -m "$seg"
	echo "Removed shared memory segment: $seg"
done

echo "All shared memory segments removed."

