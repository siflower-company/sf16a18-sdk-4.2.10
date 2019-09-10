#!/bin/sh
# Copyright (C) 2006-2011 OpenWrt.org

# austin: login.sh is used only in telnet and now we will set up telnet whether we have login password.

#if ( ! grep -qsE '^root:[!x]?:' /etc/shadow || \
#     ! grep -qsE '^root:[!x]?:' /etc/passwd ) && \
#   [ -z "$FAILSAFE" ]
#then
#	echo "Login failed."
#	exit 0
#else
#cat << EOF
# === IMPORTANT ============================
#  Use 'passwd' to set your login password
#  this will disable telnet and enable SSH
# ------------------------------------------
#EOF
#fi

exec /bin/ash --login
