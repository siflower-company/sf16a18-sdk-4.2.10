#!/bin/sh
. /lib/functions.sh


check_device_config()
{


}

#parser input

#load config
config_load wifilist
config_foreach check_device_config device
