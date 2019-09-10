#!/bin/sh
timer=10000000
while [ $timer -gt 0 ]; do
	timer=$(($timer - 1))
	iw dev raii0 disconnect
	iw dev raii1 disconnect
	iw dev raii2 disconnect
	iw dev raii3 disconnect
	iw dev rai1 disconnect
	iw dev rai2 disconnect
	iw dev rai3 disconnect
	iw dev rai4 disconnect
	sleep 3
done
