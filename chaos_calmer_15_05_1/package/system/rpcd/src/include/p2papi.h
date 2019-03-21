#include <syslog.h>
#define LOG(X,...) syslog(LOG_CRIT,X,##__VA_ARGS__)
#define XCLOUD_REMOTE_P2P_ADDR "cloud.siflower.cn/v3/cloud/getLastP2PServerInfo"
extern char oray_id[];
extern char oray_key[];

void do_cp2p(char *data, char **callback);

void do_dp2p(char *data, char **callback);

int32_t get_oray_info(char *id, char *key);

int32_t parseStringFromJson(const char *key,void *data,char *value);

void prepareCallbackData(char **callback,const char *ret,char *reason);

void p2p_init2(void);

void update_key(void);
void getserverVersion(char *version);
int32_t getServerAddress(char *retBuffer);
