#!/bin/sh
/bin/led-button -l 9 &
/sbin/jffs2reset -y && /sbin/reboot
