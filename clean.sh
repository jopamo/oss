#!/bin/bash

terminate_processes() {
	local process_name="$1"
	echo "Terminating $process_name processes..."
	local pids=$(pgrep -f "$process_name" | grep -v $$)
	if [ -z "$pids" ]; then
		echo "No $process_name processes found."
		return
	fi

	echo "Sending SIGTERM to $process_name processes: $pids"
	kill -TERM $pids
	sleep 5

	for pid in $pids; do
		if kill -0 $pid 2>/dev/null; then
			echo "Process $pid did not terminate, sending SIGKILL."
			kill -9 $pid
		fi
	done
}

terminate_processes "psmgmt"
terminate_processes "worker"
terminate_processes "timekeeper"
terminate_processes "tableprinter"

echo "Attempting to remove shared memory segments associated with the application..."
segments=$(ipcs -m | grep "0x" | awk '{print $2}')
for seg in $segments; do
	if ipcrm -m "$seg"; then
		echo "Removed shared memory segment: $seg"
	else
		echo "Failed to remove shared memory segment: $seg"
	fi
done

echo "Cleanup completed."
