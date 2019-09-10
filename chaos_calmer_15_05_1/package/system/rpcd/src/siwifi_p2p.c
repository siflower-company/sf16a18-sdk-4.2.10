#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/statvfs.h>
#include <dirent.h>
#include <arpa/inet.h>
#include <signal.h>
#include <glob.h>
#include <libubox/blobmsg_json.h>
#include <libubox/avl-cmp.h>
#include <libubus.h>
#include <uci.h>

#include <rpcd/plugin.h>
#include <p2papi.h>
#include <p2p.h>

enum {
	RPC_DP2P_NUM = 0,
};
static const struct rpc_daemon_ops *ops;
static struct blob_buf buf;

static const struct blobmsg_policy set_dp2p_data[1] = {
    [RPC_DP2P_NUM] = { .name = "p2p_name",  .type = BLOBMSG_TYPE_STRING  },
};


static int
set_cp2p(struct ubus_context *ctx, struct ubus_object *obj, struct ubus_request_data *req, const char *method, struct blob_attr *msg)
{
    char *data = NULL;
    char *callback_tmp = NULL;
    char **callback = &callback_tmp;
    do_cp2p(data, callback);
    if(callback_tmp)
    {
		blob_buf_init(&buf, 0);
		blobmsg_add_string(&buf, "p2p_data", callback_tmp);
		ubus_send_reply(ctx, req, buf.head);

		free(callback_tmp);
	}
	return 0;
}

static int
set_dp2p(struct ubus_context *ctx, struct ubus_object *obj, struct ubus_request_data *req, const char *method, struct blob_attr *msg)
{
  //  char *data = NULL;
	char *p2p_name = NULL;
    char *callback_tmp = NULL;
    char **callback = &callback_tmp;
	struct blob_attr *tb_ping[1];
	blobmsg_parse(set_dp2p_data, 1, tb_ping, blob_data(msg), blob_len(msg));
	if (!tb_ping[RPC_DP2P_NUM])
		return UBUS_STATUS_INVALID_ARGUMENT;

	p2p_name= blobmsg_get_string(tb_ping[RPC_DP2P_NUM]);
	char tmp[256] = "";
	int32_t ret = p2p_destroy(p2p_name, tmp);
	if(callback){
		prepareCallbackData(callback,ret < 0 ? "fail" : "success",tmp);
		LOG("[server] write system event ret=%d callback:%s!\n",ret,*callback);
	}
    if(callback_tmp)
    {
		blob_buf_init(&buf, 0);
		blobmsg_add_string(&buf, "p2p_data", callback_tmp);
		ubus_send_reply(ctx, req, buf.head);

		free(callback_tmp);
	}
	return 0;
}
static int
rpc_luci2_api_init(const struct rpc_daemon_ops *o, struct ubus_context *ctx)
{
	int rv = 0;

    p2p_init2();
	static const struct ubus_method luci2_network_methods[] = {
		UBUS_METHOD_NOARG("set_cp2p", set_cp2p), //no params operation
		UBUS_METHOD("set_dp2p", set_dp2p, set_dp2p_data) //params operation
	};

	static struct ubus_object_type luci2_network_type =
    UBUS_OBJECT_TYPE("luci-rpc-luci2-siwifi_p2p_api", luci2_network_methods);

	static struct ubus_object network_obj = {
        .name = "siwifi_p2p_api.network", //interface name
        .type = &luci2_network_type,
        .methods = luci2_network_methods,
        .n_methods = ARRAY_SIZE(luci2_network_methods),

	};

	ops = o;

	rv |= ubus_add_object(ctx, &network_obj);

	return rv;

}

struct rpc_plugin rpc_plugin = {
    .init = rpc_luci2_api_init,
};

