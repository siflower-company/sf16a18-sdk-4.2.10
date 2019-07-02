#!/bin/sh
/bin/led-button -l 53
/sbin/jffs2reset -y && /sbin/reboot
