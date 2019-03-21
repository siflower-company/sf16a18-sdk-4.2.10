/*
 * =====================================================================================
 *
 *       Filename:  sfax8_factory_read.h
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  2018年11月05日 20时22分03秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  star (), star.jiang@siflower.com.cn
 *        Company:  Siflower
 *
 * =====================================================================================
 */
#ifndef _SFAX8_FACTORY_READ_H_
#define _SFAX8_FACTORY_READ_H_

#define LB_ONE_CHAN_GAIN_NUM 28
/* 8(11a) + 8(HT20 MCS 0-7) + 8(HT40 MCS 0-7) + 9(VHT20 MCS 0-8) + 10(VHT40 MCS 0-9) + 10(VHT80 MCS 0-9) */
#define HB_ONE_CHAN_GAIN_NUM 53
#define LB_CHANNEL_COUNT 13 /* channel 1-13 */
#define HB_CHANNEL_COUNT 25 /* channel 36-64, 100-144, 149-165 */

#define MACADDR_SIZE            6
#define SN_SIZE                 16
#define SN_FLAG_SIZE            1
#define PCBA_BOOT_SIZE          4
#define HARDWARE_VER_FLAG_SIZE  2
#define HARDWARE_VER_SIZE       32
#define MODEL_VER_FLAG_SIZE     2
#define MODEL_VER_SIZE          32
#define COUNTRYID_SIZE          2
#define XO_CONFIG_SIZE          2
#define LB_TX_CALI_TABLE_SIZE   LB_ONE_CHAN_GAIN_NUM*LB_CHANNEL_COUNT
#define HB_TX_CALI_TABLE_SIZE   HB_ONE_CHAN_GAIN_NUM*HB_CHANNEL_COUNT

enum sfax8_factory_read_action {
	//for eth basic address
	READ_MAC_ADDRESS,
	//for wifi lb basic address
	READ_WIFI_LB_MAC_ADDRESS,
	//for wifi hb bsic address
	READ_WIFI_HB_MAC_ADDRESS,
	//for wan basic address
	READ_WAN_MAC_ADDRESS,
	READ_SN,
	READ_SN_FLAG,
	READ_PCBA_BOOT,
	READ_HARDWARE_VER_FLAG,
	READ_HARDWARE_VER,
	READ_MODEL_VER_FLAG,
	READ_MODEL_VER,
	READ_COUNTRY_ID,
	READ_RF_XO_CONFIG,
	READ_LB_TXPOWER_CALI_TABLE,
	READ_HB_TXPOWER_CALI_TABLE,
};

int sf_get_value_from_factory(enum sfax8_factory_read_action action, void *buffer, int len);
#endif
