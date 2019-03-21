/*
 * =====================================================================================
 *
 *       Filename:  sf_factory_read_pl_ref.h
 *
 *    Description:  all statement reference with the platform are defined here
 *        Version:  1.0
 *        Created:  12/30/2016 02:34:36 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  franklin (), franklin.wang@siflower.com.cn
 *        Company:  Siflower Communication Tenology Co.,Ltd
 *
 * =====================================================================================
 */

#ifndef _SF_FFACTORY_READ_PL_REF_H_
#define _SF_FFACTORY_READ_PL_REF_H_

#include <linux/device.h>
#include <linux/sfax8_factory_read.h>

//SF factory_read PRIVATE PLATFORM DRIVER DATA
struct sfax8_factory_read_context {
	//main lan eth macaddr
	unsigned char macaddr[MACADDR_SIZE];
	unsigned char sn[SN_SIZE];
	unsigned char sn_flag;
	unsigned char pcba_boot[PCBA_BOOT_SIZE];
	unsigned char hw_ver_flag[HARDWARE_VER_FLAG_SIZE];
	unsigned char hw_ver[HARDWARE_VER_SIZE];
	unsigned char model_ver_flag[MODEL_VER_FLAG_SIZE];
	unsigned char model_ver[MODEL_VER_SIZE];
	unsigned char countryID[COUNTRYID_SIZE];
	uint16_t xo_config;
	unsigned char *lb_tx_cali_table_p;
	unsigned char *hb_tx_cali_table_p;
	unsigned int exist_flag;
	bool cali_exist;
	unsigned char wifi_lb_macaddr[MACADDR_SIZE];
	unsigned char wifi_hb_macaddr[MACADDR_SIZE];
	unsigned char wan_macaddr[MACADDR_SIZE];
	unsigned char macaddr0[MACADDR_SIZE];

	//the debug fs root node
    struct dentry *debugfs;

	//the fs root node
	uint32_t start_offset;
	uint32_t len;

	struct device_node *np;
};
#endif
