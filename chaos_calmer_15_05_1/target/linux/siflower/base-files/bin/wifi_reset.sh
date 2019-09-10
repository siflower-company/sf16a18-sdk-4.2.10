#!/bin/sh
timer=1000000
while [ $timer -gt 0 ]; do
	timer=$(($timer - 1))
	sfwifi reset
done
