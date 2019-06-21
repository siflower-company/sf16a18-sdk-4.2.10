#include "action.h"
#include <linux/input.h>

int plat_btn_handler(int action, struct board_name *bn, int btn)
{
	int ret = -1;

	switch (action << 16 | bn->board) {
	case (mACTION(BTN, SPRESS, 0)):
		/*
		 * p10m short press
		 * */
		if (btn == KEY_WPS_BUTTON) {
			debug(LOG_ERR, "linking through wps-btn...\n");
			system("sh /usr/share/led-button/wps.sh");
			ret = 0;
		} else{
			debug(LOG_ERR, "detect key is %d, but input key is %d\n", KEY_WPS_BUTTON, btn);
			ret = -1;
		}
		break;
	case (mACTION(BTN, LPRESS, 0)):
	case (mACTION(BTN, ELPRESS, 0)):
		/*
		 * p10m long or extra long press
		 * */
		if (btn == KEY_RESTART) {
			debug(LOG_ERR, "factory reset by reset-btn...\n");
			system("/bin/led-button -l 9 &");
			system("/sbin/jffs2reset -y && reboot");
			ret = 0;
		} else{
			debug(LOG_ERR, "detect key is %d, but input key is %d\n", KEY_RESTART,  btn);
			ret = -1;
		}
		break;

	case (mACTION(BTN, SPRESS, 1)):
	case (mACTION(BTN, SPRESS, 7)):
		/*
		 * 86v short press
		 * */
		if(btn == BTN_1){
			system("sh /usr/share/led-button/wifi_ctrl.sh");
			ret = 0;
		} else if (btn == KEY_RESTART) {
			debug(LOG_ERR, "reboot by reset-btn...\n");
			system("/sbin/reboot");
			ret = 0;
		} else {
			debug(LOG_ERR, "!!!detect key is %d or %d, but input key is %d\n", KEY_RESTART, BTN_1, btn);
			ret = -1;
		}
		break;

	case (mACTION(BTN, LPRESS, 1)):
	case (mACTION(BTN, LPRESS, 7)):
	case (mACTION(BTN, ELPRESS, 1)):
	case (mACTION(BTN, ELPRESS, 7)):
		/*
		 * 86v long or extra long press
		 * */
		if (btn == KEY_RESTART) {
			system("/bin/led-button -l 26 &");
			debug(LOG_ERR, "factory reset by reset-btn...\n");
			system("/sbin/jffs2reset -y && reboot");
			ret = 0;
		} else{
			debug(LOG_ERR, "detect key is %d, but input key is %d\n", KEY_RESTART,  btn);
			ret = -1;
		}
		break;

	case (mACTION(BTN, HIGH, 1)):
	case (mACTION(BTN, HIGH, 7)):
	case (mACTION(BTN, LOW, 1)):
	case (mACTION(BTN, LOW, 7)):
		/*
		 * 86v high, low or edge btn
		 * */
		if (btn == BTN_2) {
			debug(LOG_ERR, "FAT/FIT mode changed! now need to reboot...\n");
			system("/sbin/reboot");
			ret = 0;
		} else {
			debug(LOG_ERR, "!!!detect key is %d, but input key is %d\n", BTN_2, btn);
			ret = -1;
		}
		break;

	case (mACTION(BTN, SPRESS, 3)):
		/*
		 * repeater short press
		 * */
		if (btn == KEY_WPS_BUTTON) {
			debug(LOG_ERR, "linking through wps-btn...\n");
			system("sh /usr/share/led-button/wps.sh");
			ret = 0;
		} else {
			debug(LOG_ERR, "!!!detect key is %d, but input key is %d\n",KEY_WPS_BUTTON, btn);
			ret = -1;
		}
		break;

	case (mACTION(BTN, LPRESS, 3)):
	case (mACTION(BTN, ELPRESS, 3)):
		/*
		 * repeater long or extra long press
		 * */
		if (btn == KEY_RESTART) {
			debug(LOG_ERR, "factory reset by reset-btn...\n");
			system("/bin/led-button -l 21 &");
			system("/sbin/jffs2reset -y && reboot");
			ret = 0;
		} else {
			debug(LOG_ERR, "!!!detect key is %d, but input key is %d\n", KEY_RESTART, btn);
			ret = -1;
		}
		break;
	case (mACTION(BTN, SPRESS, 4)):
		/*
		 * evb short press
		 * */
		if (btn == KEY_WPS_BUTTON) {
			debug(LOG_ERR, "linking through wps-btn...\n");
			system("sh /usr/share/led-button/wps.sh");
			ret = 0;
		} else{
			debug(LOG_ERR, "detect key is %d, but input key is %d\n", KEY_WPS_BUTTON, btn);
			ret = -1;
		}
		break;
	case (mACTION(BTN, LPRESS, 4)):
	case (mACTION(BTN, ELPRESS, 4)):
		/*
		 * evb long or extra long press
		 * */
		if (btn == KEY_RESTART) {
			debug(LOG_ERR, "factory reset by reset-btn...\n");
			system("/bin/led-button -l 41 &");
			system("/sbin/jffs2reset -y && reboot");
			ret = 0;
		} else {
			debug(LOG_ERR, "!!!detect key is %d, but input key is %d\n", KEY_RESTART, btn);
			ret = -1;
		}
		break;
	case (mACTION(BTN, SPRESS, 5)):
	case (mACTION(BTN, SPRESS, 6)):
		/*
		 * p10h short press-wps
		 * */
		if (btn == KEY_RESTART) {
			debug(LOG_ERR, "linking through wps-btn...\n");
			system("sh /usr/share/led-button/wps.sh");
			ret = 0;
		} else {
			debug(LOG_ERR, "!!!detect key is %d, but input key is %d\n",KEY_RESTART, btn);
			ret = -1;
		}
		break;
	case (mACTION(BTN, LPRESS, 5)):
	case (mACTION(BTN, LPRESS, 6)):
	case (mACTION(BTN, ELPRESS, 5)):
	case (mACTION(BTN, ELPRESS, 6)):
		/*
		 * p10h long or extra long press
		 * */
		if (btn == KEY_RESTART) {
			debug(LOG_ERR, "factory reset by reset-btn...\n");
			system("/bin/led-button -l 33 &");
			system("/sbin/jffs2reset -y && reboot");
			ret = 0;
		} else {
			debug(LOG_ERR, "!!!detect key is %d, but input key is %d\n", KEY_RESTART, btn);
			ret = -1;
		}
		break;


	case (mACTION(BTN, SPRESS, 8)):
		/*
		 * repeater nopa short press
		 * */
		if (btn == KEY_WPS_BUTTON) {
			debug(LOG_ERR, "linking through wps-btn...\n");
			system("sh /usr/share/led-button/wps.sh");
			ret = 0;
		} else {
			debug(LOG_ERR, "!!!detect key is %d, but input key is %d\n",KEY_WPS_BUTTON, btn);
			ret = -1;
		}
		break;

	case (mACTION(BTN, LPRESS, 8)):
	case (mACTION(BTN, ELPRESS, 8)):
		/*
		 * repeater nopa long or extra long press
		 * */
		if (btn == KEY_RESTART) {
			debug(LOG_ERR, "factory reset by reset-btn...\n");
			system("/bin/led-button -l 53 &");
			system("/sbin/jffs2reset -y && reboot");
			ret = 0;
		} else {
			debug(LOG_ERR, "!!!detect key is %d, but input key is %d\n", KEY_RESTART, btn);
			ret = -1;
		}
		break;

	default:
		debug(LOG_DEBUG, "%s: nothing to be done with %d, %s, %d\n",
					__func__, action, bn->name, btn);
		break;
	}
	return ret;
}
