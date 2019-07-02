#!/bin/sh
/bin/led-button -l 26
/sbin/jffs2reset -y && /sbin/reboot
