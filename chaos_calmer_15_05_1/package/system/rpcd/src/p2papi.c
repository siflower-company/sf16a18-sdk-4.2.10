#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <uci.h>
#include <cJSON.h>
#include <p2papi.h>
#include <ssst_request.h>
#include <token.h>

extern int p2p_create(char *id, char *key, char *address, char *p2pname, char *tmp);
extern int p2p_destroy(char *p2pname, char *tmp);

int32_t get_oray_info(char *id, char *key)
{
	struct uci_context *ctx = uci_alloc_context();
	struct uci_package *p = NULL;
	int ret = 0;
	uci_set_confdir(ctx, "/etc/config");
	if(uci_load(ctx, "siserver", &p) == UCI_OK)
	{
		struct uci_section *addr= uci_lookup_section(ctx, p, "p2p");
		//lookup values
		if(addr!= NULL){
			const char *value = uci_lookup_option_string(ctx, addr, "id");
			if(value != NULL){
				sprintf(id,"%s",value);
			}else {
				ret = -1;
			}

			value = uci_lookup_option_string(ctx, addr, "key");
			if(value != NULL){
				sprintf(key,"%s",value);
			}else{
				ret = -1;
			}
		}else{
			ret = -1;
		}

		uci_unload(ctx,p);
	}else{
		ret = -1;
	}
	uci_free_context(ctx);
	return ret;
}

int32_t getServerAddress(char *retBuffer)
{
	struct uci_context *ctx = uci_alloc_context();
	struct uci_package *p = NULL;
	int ret = 0;
	uci_set_confdir(ctx, "/etc/config");
	if(uci_load(ctx, "sicloud", &p) == UCI_OK)
	{
		struct uci_section *addr= uci_lookup_section(ctx, p, "addr");
		//lookup values
		if(addr!= NULL){
			const char *value = uci_lookup_option_string(ctx, addr, "ip");
			int len = strlen(value);
			if(value != NULL){
				sprintf(retBuffer,"%s",value);
			}else {
				ret = -1;
			}

			value = uci_lookup_option_string(ctx, addr, "port");
			if(value != NULL){
				sprintf(retBuffer+len,":%s",value);
			}else{
				ret = -1;
			}


		}else{
			ret = -1;
		}

		uci_unload(ctx,p);
	}else{
		ret = -1;
	}
	uci_free_context(ctx);
	return ret;
}

void getserverVersion(char *version){
	struct uci_context *ctx = uci_alloc_context();
	struct uci_package *p = NULL;
	uci_set_confdir(ctx, "/etc/config");
	if(uci_load(ctx, "sicloud", &p) == UCI_OK){
		struct uci_section *cloudcode = uci_lookup_section(ctx, p, "addr");
		strcpy(version,uci_lookup_option_string(ctx, cloudcode, "version"));
	}
	uci_unload(ctx,p);
	uci_free_context(ctx);
}

void update_key(void)
{
	char url[256] = "",key_data[128] = "";
	char XCLOUD_REMOTE_ADDR[64] = {0}, ssversion[16], addr[41] = {'\0'};
	char *tmp_key = NULL;
	int i;
	struct uci_context *ctx = uci_alloc_context();
	struct uci_package *p = NULL;
	struct HttpResonseData response;
	response.size = 0;
	cJSON* json = NULL, *data = NULL, *key_res = NULL;
	//get server addr
	if(getServerAddress(addr) < 0){
		LOG("get server address fail");
		exit(0);
	}
	getserverVersion(ssversion);
	sprintf(XCLOUD_REMOTE_ADDR,"https://%s/%s", addr,ssversion);
	remote_addr_init(XCLOUD_REMOTE_ADDR);

	sprintf(url,"https://%s",XCLOUD_REMOTE_P2P_ADDR);
	int ret = postHttps(url, NULL, &response);

	if(response.size > 0){
		//get p2p key from response.data
		json = cJSON_Parse(response.data);
		data = cJSON_GetObjectItem(json,"data");
		key_res = cJSON_GetObjectItem(data,"secretKey");
		tmp_key = cJSON_Print(key_res);
		sprintf(key_data,"%s",tmp_key);
		if(key_data[0] == '\"')
		{
			i = 1;
			while(key_data[i] != '\0')
			{
				key_data[i - 1] = key_data[i];
				i++;
			}
			key_data[i - 2] = key_data[i];
		}
		//set p2p key
		uci_set_confdir(ctx, "/etc/config");
		if(uci_load(ctx, "siserver", &p) == UCI_OK)
		{
			struct uci_section *addr= uci_lookup_section(ctx, p, "p2p");
			if(addr != NULL){
				struct uci_ptr ptr = {.p = p, .s = addr};
				ptr.o      = NULL;
				ptr.option  = "key";
				ptr.value  = key_data;
				uci_set(ctx, &ptr);
				uci_save(ctx, p);
				uci_commit(ctx, &p, false);
			}
			uci_unload(ctx, p);
		}
		uci_free_context(ctx);
		free(response.data);
        if(json) cJSON_Delete(json);
		if(tmp_key) free(tmp_key);
	}
}

void cp2pprepareCallbackData(char **callback,char *ret,char *reason, char *p2p_url, char *p2p_name)
{
	if(callback){
		cJSON *json = cJSON_CreateObject();
		cJSON_AddStringToObject(json, "ret", ret);
		cJSON_AddStringToObject(json, "reason",reason);
		cJSON_AddStringToObject(json, "url", p2p_url);
		cJSON_AddStringToObject(json, "session", p2p_name);
		*callback = (char *)malloc(strlen(reason) + strlen(p2p_url) + strlen(p2p_name) + 256);
		char *json_tmp = cJSON_Print(json);
		sprintf(*callback,"%s",json_tmp);
		cJSON_Delete(json);
		free(json_tmp);
	}
}

void prepareCallbackData(char **callback,const char *ret,char *reason)
{
	if(callback){
		cJSON *json = cJSON_CreateObject();
		cJSON_AddStringToObject(json,"ret",ret);
		cJSON_AddStringToObject(json,"reason",reason);
		*callback = (char *)malloc(strlen(reason) + 256);
		char *json_tmp = cJSON_Print(json);
		sprintf(*callback,"%s", json_tmp);
		if(json) cJSON_Delete(json);
		if(json_tmp) free(json_tmp);
	}
}

int32_t parseStringFromJson(const char *key,void *data,char *value)
{
	int32_t res = -1;
	cJSON* json = cJSON_Parse(data);
	if(!json) goto clean;
	cJSON *object = cJSON_GetObjectItem(json,key);
	if(!object){
		LOG("objectid not exist\n");
		goto clean;
	}
	char *json_tmp = cJSON_Print(object);
	sprintf(value,"%s", json_tmp);
	res = 0;
	free(json_tmp);
clean:
	if(json) cJSON_Delete(json);
	return res;
}

int32_t getUpdateKeyMode(void)
{
    struct uci_context *ctx = uci_alloc_context();
    struct uci_package *p = NULL;
    int ret = -1;
    uci_set_confdir(ctx, "/etc/config");
    if(uci_load(ctx, "basic_setting", &p) == UCI_OK)
    {
        struct uci_section *updateKeyMode = uci_lookup_section(ctx, p, "updateKeyMode");
        //lookup values
        if(updateKeyMode != NULL){
            ret = atoi(uci_lookup_option_string(ctx, updateKeyMode, "enable"));
        }
        uci_unload(ctx,p);
    }
    uci_free_context(ctx);
    return ret;
}

void do_cp2p(char *data, char **callback)
{
	LOG("[server]%s, args : %s\n",__func__, data ? data : "NULL");
	/*    if(!data){
	LOG("[server] args is null!\n");
	return;}*/
	char oray_id[128], oray_key[128];
	get_oray_info(oray_id, oray_key);
	char p2p_url[256] = "";
	char p2p_name[256] = "";
	char tmp[256] = "";
	int32_t ret = p2p_create(oray_id, oray_key, p2p_url, p2p_name, tmp);
	LOG("ret is %d",ret);
    int32_t update_key_mode = getUpdateKeyMode();
	while(ret == -2){
        if(update_key_mode == 1)
        {
            update_key();
        }
		memset(oray_id, 0, sizeof(oray_id));
		memset(oray_key, 0, sizeof(oray_key));
		get_oray_info(oray_id, oray_key);
		ret = p2p_create(oray_id, oray_key, p2p_url, p2p_name, tmp);
	}
	if(callback){
		cp2pprepareCallbackData(callback,ret < 0 ? "fail" : "success",tmp, p2p_url, p2p_name);
		LOG("[server] write system event ret=%d callback:%s!\n",ret,*callback);
	}
}

void do_dp2p(char *data, char **callback)
{
	char p2p_name[256];
	LOG("[server]%s, args : %s\n",__func__, data ? data : "NULL");
	if(!data){
		LOG("[server] args is null!\n");
		return;
	}
	if(parseStringFromJson("session",data, p2p_name) != 0){
		LOG("Oray destroy function get param error!\n");
		return -1;
	}

	char tmp[256] = "";
	int32_t ret = p2p_destroy(p2p_name, tmp);
	if(callback){
		prepareCallbackData(callback,ret < 0 ? "fail" : "success",tmp);
		LOG("[server] write system event ret=%d callback:%s!\n",ret,*callback);
	}
}

void p2p_init2(void)
{
	p2p_init();
}
