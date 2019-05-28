define Package/base-files/install-target
	if  [ "$(shell echo $(PROFILE) | grep "REP")" != "" ]; then\
		echo deletefilehere \
		rm $(1)/etc/opkg/keys/53bad1233d4c98c5; \
		rm $(1)/etc/opkg/keys/de98a2dd1d0f8a07; \
		rm $(1)/bin/aclscript; \
		rm $(1)/bin/set_cpufreq; \
		rm $(1)/bin/netclash; \
		rm $(1)/bin/gwifi; \
		rm $(1)/etc/config/devlist; \
		rm $(1)/etc/config/wldevlist; \
		rm $(1)/etc/config/timelist; \
		rm $(1)/etc/config/notify; \
		rm $(1)/etc/config/qos_cfg; \
		rm $(1)/etc/config/samba; \
		rm $(1)/etc/config/wifi_info; \
		rm $(1)/etc/config/ap_groups; \
		rm $(1)/etc/config/sicloud; \
		rm $(1)/etc/init.d/acl; \
		rm $(1)/etc/sysupgrade_except.conf; \
		rm $(1)/etc/se_pub.key; \
		rm $(1)/etc/pubkey.pem; \
		rm $(1)/etc/ssst_replace.sh; \
		rm $(1)/etc/sysupgrade.conf; \
		rm $(1)/sbin/check_net; \
		rm $(1)/usr/bin/init_devlist; \
		rm $(1)/usr/bin/speed; \
        else \
        	rm -f $(1)/etc/config/network; \
	fi
endef
