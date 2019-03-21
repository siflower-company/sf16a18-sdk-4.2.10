#include <syslog.h>

#define LOG(X,...) syslog(LOG_CRIT,X,##__VA_ARGS__)

int p2p_create(char *id, char *key, char *address, char *p2pname, char *tmp);
int p2p_destroy(char *p2pname, char *tmp);
void p2p_init(void);
void p2p_exit(void);
