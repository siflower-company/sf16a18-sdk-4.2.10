#!/bin/sh

PATH="/mnt/sdb"
PATH_DEV="/dev/mmcblk0"

if [ -b "$PATH_DEV" ] ;then
	/bin/mkdir -p $PATH
	/bin/mount /dev/mmcblk0 /mnt/sdb
fi

