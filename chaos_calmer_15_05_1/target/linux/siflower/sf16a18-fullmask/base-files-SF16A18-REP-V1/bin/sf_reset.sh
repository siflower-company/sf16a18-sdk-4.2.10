#!/bin/sh
/bin/led-button -l 21
/sbin/jffs2reset -y && /sbin/reboot
