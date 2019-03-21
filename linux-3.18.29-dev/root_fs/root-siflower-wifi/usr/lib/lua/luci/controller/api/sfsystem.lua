--[[
LuCI - Lua Configuration Interface

Description:
Offers an interface for handle app request
]]--

module("luci.controller.api.sfsystem", package.seeall)

local sysutil = require "luci.siwifi.sf_sysutil"
local sysconfig = require "luci.siwifi.sf_sysconfig"
local disk = require "luci.siwifi.sf_disk"
local sferr = require "luci.siwifi.sf_error"
local sfgw = require "luci.siwifi.sf_gateway"
local nixio = require "nixio"
local fs = require "nixio.fs"
local json = require("luci.json")
local http = require "luci.http"
local uci = require "luci.model.uci"
local _uci_real  = cursor or _uci_real or uci.cursor()

local SAVE_MODE = 0
local NORMAL_MODE = 1
local PERFORMANCE_MODE = 2

local SAVE_MODE_TXPOWER = 0
local NORMAL_MODE_TXPOWER = 0
local PERFORMANCE_MODE_TXPOWER = 0

local reset_interval = 30

local CLOUD_TYPE = sysutil.getCloudType()

function index()
    local page   = node("api","sfsystem")
    page.target  = firstchild()
    page.title   = ("")
    page.order   = 100
    page.sysauth = "root"
    page.sysauth_authenticator = "jsonauth"
    page.index = true
    entry({"api", "sfsystem"}, firstchild(), (""), 100)
    entry({"api", "sfsystem", "welcome"}, call("welcome"), nil)
    entry({"api", "sfsystem", "init_info"}, call("getInitInfo"), nil)

    page = entry({"api", "sfsystem", "command"}, call("parse_command"), nil)
    page.leaf = true

    entry({"api", "sfsystem", "get_stok_local"}, call("get_stok_local"), nil)
    entry({"api", "sfsystem", "get_stok_remote"}, call("get_stok_remote"), nil)
    entry({"api", "sfsystem", "setpasswd"}, call("set_Password"), nil)
    entry({"api", "sfsystem", "wifi_detail"}, call("get_wifi_detail"),nil)
    entry({"api", "sfsystem", "setwifi"}, call("set_wifi_info"),nil)
    entry({"api", "sfsystem", "main_status"}, call("get_router_status"),nil)
    entry({"api", "sfsystem", "bind"}, call("bind"),nil)
    entry({"api", "sfsystem", "unbind"}, call("unbind"),nil)
    entry({"api", "sfsystem", "manager"}, call("manager_op"),nil)
    entry({"api", "sfsystem", "device_list_backstage"}, call("get_device_list"),nil)           --just internal call
    entry({"api", "sfsystem", "device_list"}, call("get_device_info"),nil)
    entry({"api", "sfsystem", "setdevice"}, call("set_fw_rule"),nil)
    entry({"api", "sfsystem", "ota_check"}, call("ota_check"),nil)
    entry({"api", "sfsystem", "ota_upgrade"}, call("ota_upgrade"),nil)
    entry({"api", "sfsystem", "check_wan_type"}, call("check_wan_type"),nil)
    entry({"api", "sfsystem", "get_wan_type"}, call("get_wan_type"),nil)
    entry({"api", "sfsystem", "set_wan_type"}, call("set_wan_type"),nil)
    entry({"api", "sfsystem", "qos_set"}, call("set_qos"),nil)
    entry({"api", "sfsystem", "qos_info"}, call("get_qos_info"),nil)
    entry({"api", "sfsystem", "netdetect"}, call("net_detect"),nil)
    entry({"api", "sfsystem", "set_wifi_filter"}, call("set_wifi_filter"),nil)
    entry({"api", "sfsystem", "get_wifi_filter"}, call("get_wifi_filter"),nil)
	entry({"api", "sfsystem", "upload_log"}, call("upload_log"),nil)
    entry({"api", "sfsystem", "sync"}, call("sync"),nil)
	entry({"api", "sfsystem", "download"}, call("download"),nil)
	entry({"api", "sfsystem", "update_qos_local"}, call("update_qos_local"),nil)                            --just internal call
    entry({"api", "sfsystem", "get_zigbee_dev"}, call("get_zigbee_dev"),nil)
    entry({"api", "sfsystem", "set_zigbee_dev"}, call("set_zigbee_dev"),nil)
    entry({"api", "sfsystem", "create_zigbee_rule"}, call("create_zigbee_rule"),nil)
    entry({"api", "sfsystem", "del_zigbee_rule"}, call("del_zigbee_rule"),nil)
    entry({"api", "sfsystem", "get_zigbee_rule"}, call("get_zigbee_rule"),nil)
    entry({"api", "sfsystem", "set_zigbee_rule"}, call("set_zigbee_rule"),nil)
    entry({"api", "sfsystem", "get_zigbee_event_record"}, call("get_zigbee_event_record"),nil)
    entry({"api", "sfsystem", "post_zigbee_changes"}, call("post_zigbee_changes"),nil)
    entry({"api", "sfsystem", "get_zigbee_info"}, call("get_zigbee_info"), nil)

end

function sync()
    --string.format("Downloading %s from %s to %s", file, host, outfile)
--    local cmd = "SYNC -data "..luci.http.formvalue("enable")
    local action = luci.http.formvalue("action");
    local userid = luci.http.formvalue("userid");
    local type = luci.http.formvalue("type");
    local data = luci.http.formvalue("data");

    local cmd = string.format("SUBE need-callback -data {\"action\": %d,\"userid\": \"%s\",\"type\": %s,\"data\":\"%s\"}",action,userid,type,data);
    local cmd_ret = {}
    local ret1 = sysutil.sendCommandToLocalServer(cmd, cmd_ret)

    local code = 0
    local result = {}

    if(ret1 ~= 0) then
        result["code"] = -1
        result["msg"] = "send command fail"
    else
        --parse json result
        local decoder = {}
        if cmd_ret["json"] then
            decoder = json.decode(cmd_ret["json"]);
            if(decoder["ret"] == "success") then
                result["code"] = 0
                result["msg"] = "success"
            else
                result["code"] = -1
                result["msg"] = "internal-server-error"..(decoder["reason"] or "")
            end
        else
            result["code"] = -1
            result["msg"] = "internal-server-error"
        end
    end

    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
end


function SendToLocalServer(config_name)
    local changes = _uci_real:changes(config_name)
    if((changes ~= nil) and (type(changes) == "table") and (next(changes) ~= nil)) then
        local json_encoder = json.Encoder(changes, 4096)
        local json_source = json_encoder:source()
        local cmd = "UCIM -data "..json_source()
        local cmd_ret = {}
        sysutil.sendCommandToLocalServer(cmd, cmd_ret)
    end
    _uci_real:save(config_name)
    _uci_real:commit(config_name)
end

function SendDataToLocalServer(data_str)
    local cmd = "UCIM -data "..data_str
    local cmd_ret = {}
    sysutil.sendCommandToLocalServer(cmd, cmd_ret)
end

function checkversion()
    return luci.http.formvalue("version")
end

function check_wan_type()
    local result = {}
    local code = 0
    local protocol = checkversion()
    if(not protocol) then
        result = sferr.errProtocolNotFound()
    elseif(protocol == sysconfig.SF_PROTOCOL_VERSION_01) then
        local time1 = os.time()
        local wantype = luci.util.exec("ubus call network.internet wantype")
        nixio.syslog("crit","check wan type cost time "..tostring(os.time() - time1).."s--result:"..tostring(wantype))
        if(wantype) then
            local decoder = json.decode(wantype);
            if(decoder and decoder['result']) then
                result['type'] = decoder['result']
                if(decoder['result'] < 0) then
                    result["code"] = sferr.ERROR_NO_WAN_TYPE_PARSER_FAIL
                else
                    result["code"] = 0
                end
            else
                result["code"] = sferr.ERROR_NO_WAN_TYPE_PARSER_FAIL
            end
        else
            result["code"] = sferr.ERROR_NO_WAN_TYPE_EXCEPTION
        end
        result["msg"]  = sferr.getErrorMessage(code)
    else
        result = sferr.errProtocolNotSupport()
    end

    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
end



function bind()
    local result = {}
    local code = 0
    local extraInfo = ""
    local protocol = checkversion()
    if(not protocol) then
        result = sferr.errProtocolNotFound()
    elseif(protocol == sysconfig.SF_PROTOCOL_VERSION_01) then
        local userid = luci.http.formvalue("userobjectid")
        if(not userid) then
            code = sferr.ERROR_NO_USERID_EMPTY
        else
            --check if router is bind
            local bindvalue = sysutil.getbind()
            local binder = sysutil.getbinderId()
            if(bindvalue == sysconfig.SF_BIND_YET) then
                code = sferr.ERROR_NO_ROUTER_HAS_BIND
                if(binder == userid) then
                    result["routerobjectid"] = sysutil.getrouterId()
                end
            else
                --do bind
                local bindret = {}
                --init sn to uci config in getSN,so the socket server can get config from uic
                local sn = sysutil.getSN()
                local ret1 = sysutil.sendCommandToLocalServer("BIND need-callback -data {\"binder\":\""..tostring(userid).."\"}",bindret)
                if(ret1 ~= 0) then
                    code = sferr.ERROR_NO_INTERNAL_SOCKET_FAIL
                else
                    --parse json result
                    local decoder = {}
                    if bindret["json"] then
                        decoder = json.decode(bindret["json"]);
                        if(decoder["ret"] == "success") then
                            result["routerobjectid"] = decoder["router"]
                            code = 0
                        else
                            result["routerobjectid"] = ""
                            code = sferr.ERROR_NO_BIND_FAIL
                            extraInfo = "-"..(decoder["reason"] or "")
                        end
                    else
                        result["routerobjectid"] = ""
                        code = sferr.ERROR_NO_BIND_FAIL
                        extraInfo = "-"..(decoder["reason"] or "")
                    end
                end
            end
        end
        result["code"] = code
        result["msg"]  = sferr.getErrorMessage(code)..extraInfo
    else
        result = sferr.errProtocolNotSupport()
    end

    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
end

function manager_op()
    local result = {}
    local code = 0
    local extraInfo = ""
    local protocol = checkversion()
    if(not protocol) then
        result = sferr.errProtocolNotFound()
    elseif(protocol == sysconfig.SF_PROTOCOL_VERSION_01) then
        --check params
        local action = luci.http.formvalue("action")
        local userid = luci.http.formvalue("userid")
        local phonenumber = luci.http.formvalue("phonenumber")
        local username = luci.http.formvalue("username") or ""
        local tag = luci.http.formvalue("tag") or ""
        local hostuserid = getfenv(1).userid
        if((not action) or (not (userid or phonenumber))) then
            code = sferr.ERROR_NO_CHECK_MANAGER_PARAMS_FAIL
        else
            --check if router is bind
            local bindvalue = sysutil.getbind()
            if(bindvalue == sysconfig.SF_BIND_NO) then
                code = sferr.ERROR_NO_ROUTER_HAS_NOT_BIND
            else
                --do manager operation
                local bindret = {}
                local ret1 = sysutil.send_manager_op_command(action,userid or "",phonenumber or "",username or "",tag or "",hostuserid or "",bindret)
                if(ret1 ~= 0) then
                    code = sferr.ERROR_NO_INTERNAL_SOCKET_FAIL
                else
                    --parse json result
                    local decoder = {}
                    decoder = json.decode(bindret["json"]);
                    if(decoder["ret"] == "success") then
                        code = 0
                    else
                        code = sferr.ERROR_NO_MANAGER_OP_FAIL
                        extraInfo = "-"..decoder["reason"]
                    end
                end
            end
        end
        result["code"] = code
        result["msg"]  = sferr.getErrorMessage(code)..extraInfo
    else
        result = sferr.errProtocolNotSupport()
    end

    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
end

function unbind()
    local result = {}
    local code = 0
    local extraInfo = ""
    local protocol = checkversion()
    if(not protocol) then
        result = sferr.errProtocolNotFound()
    elseif(protocol == sysconfig.SF_PROTOCOL_VERSION_01) then
        local userid = luci.http.formvalue("userobjectid")
        if(not userid) then
            code = sferr.ERROR_NO_USERID_EMPTY
        else
            --check if router is bind
            local bindvalue = sysutil.getbind()
            local binder = sysutil.getbinderId()
            if(bindvalue == sysconfig.SF_BIND_NO) then
                code = sferr.ERROR_NO_ROUTER_HAS_NOT_BIND
            elseif(binder ~= userid) then
                code = sferr.ERROR_NO_CALLER_NOT_BINDER
            else
                --do unbind
                local bindret = {}
                local ret1 = sysutil.send_unbind_command(bindret)
                if(ret1 ~= 0) then
                    code = sferr.ERROR_NO_INTERNAL_SOCKET_FAIL
                else
                    --parse json result
                    local decoder = {}
                    if bindret["json"] then
                        decoder = json.decode(bindret["json"]);
                        if(decoder["ret"] == "success") then
                            code = 0
                        else
                            code = sferr.ERROR_NO_UNBIND_FAIL
                            extraInfo = "-"..(decoder["reason"] or "")
                        end
                    else
                        code = sferr.ERROR_NO_UNBIND_FAIL
                        extraInfo = "-"..(decoder["reason"] or "")
                    end

                end
            end
        end
        result["code"] = code
        result["msg"]  = sferr.getErrorMessage(code)..extraInfo
    else
        result = sferr.errProtocolNotSupport()
    end

    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
end

function getInitInfo()
    local uci =require "luci.model.uci".cursor()
    local result = {}
    local protocol = checkversion()
    if(not protocol) then
        result = sferr.errProtocolNotFound()
    elseif(protocol == sysconfig.SF_PROTOCOL_VERSION_01) then
        result["romtype"] = sysutil.getRomtype()
        result["name"] = sysutil.getRouterName()
        result["romversion"] = sysutil.getRomVersion()
        result["romversionnumber"] = 0       ---------------------------------TODO----------------------
        result["sn"] = sysutil.getSN()
        result["hardware"] = sysutil.getHardware()
        result["account"] =  sysutil.getRouterAccount()
        result["mac"] = sysutil.getMac("eth0")
        result["disk"] = disk.getDiskAvaiable()
        result["routerid"] = uci:get("siserver","bmobrouter","routerid") or ''
		result["zigbee"] = sysutil.getZigbeeAttr()
        result["code"] = 0
        result["msg"]  = sferr.getErrorMessage(0)
    else
        result = sferr.errProtocolNotSupport()
    end

    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
end

function parse_command()

    local result = {}
    local code = 0
    local protocol = checkversion()
    if(not protocol) then
        result = sferr.errProtocolNotFound()
    elseif(protocol == sysconfig.SF_PROTOCOL_VERSION_01) then
        local cmd = luci.http.formvalue("cmd")
        if cmd == "0" then

            code = reboot()

        elseif cmd == "2" then

            code = reset()

        else
            code = sferr.ERROR_NO_UNKNOWN_CMD
        end

        result["code"] = code
        result["msg"]  = sferr.getErrorMessage(code)
    else
        result = sferr.errProtocolNotSupport()
    end

    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
end

function reboot()
    local reset_shortest_time = 0
    if sysutil.sane("/tmp/reset_shortest_time") then
        reset_shortest_time  = tonumber(fs.readfile("/tmp/reset_shortest_time"))
    else
        reset_shortest_time = 0
    end
    if os.time() > reset_shortest_time then
        sysutil.sendSystemEvent(sysutil.SYSTEM_EVENT_REBOOT)
        sysutil.fork_exec("sleep 5;reboot");
        sysutil.resetAllDevice()
        reset_shortest_time = reset_interval + os.time()
        local f = nixio.open("/tmp/reset_shortest_time", "w", 600)
        f:writeall(reset_shortest_time)
        f:close()

        return 0
    else
        return sferr.ERROR_NO_WAITTING_RESET
    end
end

function network_shutdown(iface)
    local netmd = require "luci.model.network".init()
    local net = netmd:get_network(iface)
    if net then
        luci.sys.call("env -i /sbin/ifdown %q >/dev/null 2>/dev/null" % iface)
        luci.http.status(200, "Shutdown")
        return
    end
end

function reset()
    local reset_shortest_time = 0
    if sysutil.sane("/tmp/reset_shortest_time") then
        reset_shortest_time  = tonumber(fs.readfile("/tmp/reset_shortest_time"))
    else
        reset_shortest_time = 0
    end
    if os.time() > reset_shortest_time then
        --before reset we notify the server unbind the current user,maybe unsuccess if the network is not reachable
        sysutil.sendSystemEvent(sysutil.SYSTEM_EVENT_RESET)
        sysutil.unbind();
        --do reset
        sysutil.fork_exec("sleep 1; killall dropbear uhttpd; sleep 1; mtd -r erase rootfs_data")
        reset_shortest_time = reset_interval + os.time()
        local f = nixio.open("/tmp/reset_shortest_time", "w", 600)
        f:writeall(reset_shortest_time)
        f:close()

        return 0
    else
        return sferr.ERROR_NO_WAITTING_RESET
    end
end


function get_stok_local()
    result = {}
    code = 0
    local stok = luci.dispatcher.build_url()
    local stok1,stok2 = string.match(stok,"(%w+)=([a-fA-F0-9]*)")

    local protocol = checkversion()
    if(not protocol) then
            result = sferr.errProtocolNotFound()
    elseif(protocol == sysconfig.SF_PROTOCOL_VERSION_01) then
        result["code"] = code
        result["msg"] = sferr.getErrorMessage(code)
        result["stok"] = stok2
    else
        result = sferr.errProtocolNotSupport()
    end

    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
end

function get_stok_remote()
    --return the same value as local request
    get_stok_local()
end

function set_Password()
    local result = {}
    local code = 0
    local username = "root"
    local oldpasswd = luci.http.formvalue("oldpwd")
    local pwd = luci.http.formvalue("newpwd")

    local protocol = checkversion()
    if(not protocol) then
        result = sferr.errProtocolNotFound()
    elseif(protocol == sysconfig.SF_PROTOCOL_VERSION_01) then
        local authen_vaild = luci.sys.user.checkpasswd(username,oldpasswd)

        if authen_vaild == false then
            code = sferr.ERROR_NO_OLDPASSWORD_INCORRECT
        else
            code = luci.sys.user.setpasswd("root", pwd)
        end
        result["code"] = code
        result["msg"] = sferr.getErrorMessage(code)
    else
        result = sferr.errProtocolNotSupport()
    end

    luci.http.prepare_content("application/json")
    luci.http.write_json(result)

end


function get_wifi_detail()

    local code = 0
    local rv = {}
    local result = { }
    local wifis = sysutil.sf_wifinetworks()

    local protocol = checkversion()
    if(not protocol) then
        result = sferr.errProtocolNotFound()
    elseif(protocol == sysconfig.SF_PROTOCOL_VERSION_01) then

        for i, dev in pairs(wifis) do
            for n=1,#dev.networks do
                    rv[#rv+1] = {}

                    _uci_real:foreach("wireless","wifi-device",
                        function(s)
                                if s.type==dev.device then
                                        rv[#rv]["band"] = s.band
                                end
                        end)

                    rv[#rv]["ifname"]     = dev.networks[n].ifname
                    rv[#rv]["mac"]     = sysutil.getMac(dev.networks[n].ifname)
                    rv[#rv]["ssid"]       = dev.networks[n].ssid
                    rv[#rv]["enable"]     = dev.networks[n].disable ==nil and 1 or 0
                    rv[#rv]["encryption"] = dev.networks[n].encryption_src
                    rv[#rv]["signal"]     = dev.networks[n].signal
                    rv[#rv]["password"]   = dev.networks[n].password
                    rv[#rv]["channel"]    = dev.networks[n].channel
            end
        end
        code = 0
        result["info"] = rv
        result["code"] = code
        result["msg"]  = sferr.getErrorMessage(code)
    else
        result = sferr.errProtocolNotSupport()
    end

    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
    return

end

function get_ssids()

    local wifis = sysutil.sf_wifinetworks()
    local ssids = {}
    for i, dev in pairs(wifis) do
        for n=1,#dev.networks do

            ssids[#ssids+1] = dev.networks[n].ssid

        end
    end
    return ssids
end


function getdev_by_ssid(ssid)

    local wifis = sysutil.sf_wifinetworks()
    for i, dev in pairs(wifis) do
        for n=1,#dev.networks do

            if dev.networks[n].ssid == ssid then
                return dev
            end
        end
    end
    return
end

function get_wan_type()
    local uci = require "luci.model.uci".cursor()
    local result = {}
    local code = 0
    local protocol = checkversion()
    if(not protocol) then
        result = sferr.errProtocolNotFound()
    elseif(protocol == sysconfig.SF_PROTOCOL_VERSION_01) then
        local proto   = uci:get("network","wan","proto")
        if(proto == "dhcp") then
            result["type"] = 0
        elseif(proto == "pppoe") then
            result["type"]    = 1
            result["pppname"] = uci:get("network","wan","username")
            result["pppwd"]   = uci:get("network","wan","password")
        elseif(proto == "static") then
            result["type"]    = 2
            result["ip"]      = uci:get("network","wan","ipaddr")
            result["mask"]    = uci:get("network","wan","netmask")
            result["gateway"] = uci:get("network","wan","gateway")
        else
            code = sferr.ERROR_NO_WAN_PROTO_EXCEPTION
        end
        if(code == 0) then
            if( uci:get("network","wan","dns") ) then
                result["autodns"] = 0
                local dns = uci:get("network","wan","dns")
                local dns1, dns2 = dns:match('([^%s]+)%s+([^%s]+)')
                if(not dns1) then
                    result["dns1"] = dns
                else
                    result["dns1"]    = dns1
                    result["dns2"]    = dns2
                end
            else
                result["autodns"] = 1
            end
        end
        result["code"] = code
        result["msg"]  = sferr.getErrorMessage(code)
    else
        result = sferr.errProtocolNotSupport()
    end
    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
end

function set_wan_type()
    local uci = require "luci.model.uci".cursor()
    local result = {}
    local code   = 0
    local pure_config = "basic_setting"
    local protocol = checkversion()
    if(not protocol) then
        result = sferr.errProtocolNotFound()
    elseif(protocol == sysconfig.SF_PROTOCOL_VERSION_01) then
        local _type = luci.http.formvalue("type")
        if(not _type) then
            code = sferr.ERROR_NO_WANSET_TYPE_NOT_FOUND
        elseif( _type~='0' and _type~='1' and _type~='2' ) then
            code = sferr.ERROR_NO_WANSET_TYPE_EXCEPTION
        else

            local basic_setting = _uci_real:get_all(pure_config)
            if basic_setting.wan_type == nil then
                _uci_real:set(pure_config, "wan_type", "setting")
            end
            _uci_real:set(pure_config, "wan_type", "type", _type)

            local proto = uci:get("network","wan","proto")
            if(proto == "pppoe") then
                uci:delete("network","wan","username")
                uci:delete("network","wan","ppppwd")
            end
            if(proto == "static") then
                uci:delete("network","wan","ipaddr")
                uci:delete("network","wan","netmask")
                uci:delete("network","wan","gateway")
            end
            if(_type == '0') then
                uci:set("network","wan","proto","dhcp")
            elseif(_type == '1') then
                uci:set("network","wan","proto","pppoe")
                local pppname = luci.http.formvalue("pppname")
                if pppname and string.len(pppname)>31 then
                    local pppname_tmp = pppname
                    pppname = string.sub(pppname,1,31)
                end
                local ppppwd  = luci.http.formvalue("ppppwd")
                if ppppwd and string.len(ppppwd)>31 then
                    local ppppwd_tmp = ppppwd
                    ppppwd = string.sub(ppppwd,1,31)
                end

                if(pppname) then
                    _uci_real:set(pure_config, "wan_type", "pppname", pppname)
                    uci:set("network","wan","username",pppname)
                end
                if(ppppwd) then
                    _uci_real:set(pure_config, "wan_type", "ppppwd", ppppwd)
                    uci:set("network","wan","password",ppppwd)
                end
            else
                uci:set("network","wan","proto","static")
                local address = luci.http.formvalue("address")
                if address and string.len(address)>31 then
                    local address_tmp = address
                    address = string.sub(address,1,31)
                end
                local mask    = luci.http.formvalue("mask")
                if mask and string.len(mask)>31 then
                    local mask_tmp = mask
                    mask = string.sub(mask,1,31)
                end
                local gateway = luci.http.formvalue("gateway")
                if gateway and string.len(gateway)>31 then
                    local gateway_tmp = gateway
                    gateway = string.sub(gateway,1,31)
                end
                if(address) then
                    uci:set("network","wan","ipaddr",address)
                    _uci_real:set(pure_config, "wan_type", "ip", address)
                end
                if(mask) then
                    uci:set("network","wan","netmask",mask)
                    _uci_real:set(pure_config, "wan_type", "mask", mask)
                end
                if(gateway) then
                    uci:set("network","wan","gateway",gateway)
                    _uci_real:set(pure_config, "wan_type", "gateway", gateway)
                end
            end
            local autodns = luci.http.formvalue("autodns")

            if autodns then _uci_real:set(pure_config, "wan_type", "autodns", autodns)  end

            if(autodns == '1') then
                uci:delete("network","wan","peerdns")
                uci:delete("network","wan","dns")
            end
            if(autodns == '0' or  _type == '2') then
                local dns =""
                local dns1 = luci.http.formvalue("dns1")
                if dns1 and string.len(dns1)>31 then
                    local dns1_tmp = dns1
                    dns1 = string.sub(dns1,1,31)
                end
                local dns2 = luci.http.formvalue("dns2")
                if dns2 and string.len(dns2)>31 then
                    local dns2_tmp = dns2
                    dns2 = string.sub(dns2,1,31)
                end
                if dns1 then _uci_real:set(pure_config, "wan_type", "dns1", dns1) end
                if dns2 then _uci_real:set(pure_config, "wan_type", "dns2", dns2) end
                if(dns1 and dns2) then
                    dns  = dns1..' '..dns2
                elseif(dns1) then
                    dns=dns1
                else
                    code = sferr.ERROR_NO_WANSET_DNS_NOT_FOUND
                end
                if(autodns == '0') then
                    uci:set("network","wan","peerdns","0")
                end
                uci:set("network","wan","dns",dns)
            end
        end
        if(code == 0) then
            local changes = uci:changes("network")
            if((changes ~= nil) and (type(changes) == "table") and (next(changes) ~= nil)) then
                uci:save("network")
                uci:commit("network")
                uci:load("network")
                luci.sys.call("env -i /bin/ubus call network reload >/dev/null 2>/dev/null; sleep 2")
                luci.util.exec("ping -w4 www.baidu.com")
            end
        end
        result["code"] = code
        result["msg"]  = sferr.getErrorMessage(code)
        SendToLocalServer(pure_config)
    else
        result = sferr.errProtocolNotSupport()
    end
    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
end



function set_wifi_info()

    local network = require("luci.model.network").init()
    local wifinet = {}
    local wifidev = {}
    local code = 0
    local rv = {}
    local result = { }
    local dev = nil
    local protocol = checkversion()

    local setting_param = luci.http.formvalue("setting")
    local param_disableall = luci.http.formvalue("disableall")
    local setting_param_json = nil

    if param_disableall and param_disableall == "0" or param_disableall == "1" then
        ssids = get_ssids()
        setting_param_json = {}
        for i=1,#ssids do
            setting_param_json[i] = {}
            setting_param_json[i].oldssid = ssids[i]
            setting_param_json[i].enable = param_disableall=="1" and 0 or 1
        end
    else
        if setting_param then
            setting_param_json = json.decode(setting_param)
        else
            code = sferr.ERROR_NO_WIFI_SETTING_EMPTY
        end
    end

    local matchssid = false

    local pure_config = "wifi_info"

    local device_5G_name = nil
    local device_2dot4G_name = nil
    local sn = sysutil.getSN()
    local wifi_info = _uci_real:get_all(pure_config)
    if wifi_info.wifi2_4G == nil then
       _uci_real:set(pure_config, "wifi2_4G", "band")
    end
    if wifi_info.wifi5G == nil then
       _uci_real:set(pure_config, "wifi5G", "band")
    end
--    _uci_real:set(pure_config, "wifi2_4G", "sn", tostring(sn))
--    _uci_real:set(pure_config, "wifi5G", "sn", tostring(sn))


    _uci_real:foreach("wireless", "wifi-device",
            function(s)
                if s.band and s.band == "5G" then
                    device_5G_name = s[".name"]
                    _uci_real:set(pure_config, "wifi5G", "device", device_5G_name)
                elseif s.band and s.band == "2.4G" then
                    device_2dot4G_name = s[".name"]
                    _uci_real:set(pure_config, "wifi2_4G", "device", device_2dot4G_name)
                end
            end
            )

    if(not protocol) then
        result = sferr.errProtocolNotFound()
    elseif(protocol == sysconfig.SF_PROTOCOL_VERSION_01) then
        if setting_param_json then
            for i=1,#setting_param_json do
                if setting_param_json[i].oldssid then
                    dev = getdev_by_ssid(setting_param_json[i].oldssid)
                end
                if dev then
                    for n=1,#dev.networks do
                        local param_ssid = nil
                        local param_enable = nil
                        local param_key = nil
                        local param_encryption = nil
                        local param_signal_mode = nil
                        local param_channel = nil
                        if setting_param_json[i].newssid then
                            param_ssid = setting_param_json[i].newssid
                        end
                        if setting_param_json[i].enable then
                            param_enable = setting_param_json[i].enable
                        end
                        if setting_param_json[i].password then
                            param_key = setting_param_json[i].password
                        end
                        if setting_param_json[i].encryption then
                            param_encryption = setting_param_json[i].encryption
                        end
                        if setting_param_json[i].signalmode then
                            param_signal_mode = setting_param_json[i].signalmode
                        end
                        if setting_param_json[i].channel then
                            param_channel = setting_param_json[i].channel
                        end

                        if param_ssid and string.len(param_ssid)>31 then
                            local param_ssid_tmp = param_ssid
                            param_ssid = string.sub(param_ssid_tmp,1,31)
                        end
                        if param_key and string.len(param_key)>31 then
                            local param_key_tmp = param_key
                            param_key = string.sub(param_key_tmp,1,31)
                        end



                        local cur_device = nil
                        _uci_real:foreach("wireless", "wifi-iface",
                        function(s)
                            if s.ssid == dev.networks[n].ssid then
                                cur_device = s.device
                            end
                        end
                        )

                        if cur_device == device_5G_name then
                            if param_ssid then _uci_real:set(pure_config, "wifi5G", "ssid", param_ssid) end
                            if param_enable then _uci_real:set(pure_config, "wifi5G", "enable", param_enable) end
                            if param_key then _uci_real:set(pure_config, "wifi5G", "password", param_key) end
                            if param_encryption then _uci_real:set(pure_config, "wifi5G", "encryption", param_encryption)  end
                            if param_signal_mode then _uci_real:set(pure_config, "wifi5G", "signal_mode", param_signal_mode) end
                            if param_channel then _uci_real:set(pure_config, "wifi5G", "channel", param_channel) end
                        elseif cur_device == device_2dot4G_name then
                            if param_ssid then _uci_real:set(pure_config, "wifi2_4G", "ssid", param_ssid) end
                            if param_enable then _uci_real:set(pure_config, "wifi2_4G", "enable", param_enable) end
                            if param_key then _uci_real:set(pure_config, "wifi2_4G", "password", param_key) end
                            if param_encryption then _uci_real:set(pure_config, "wifi2_4G", "encryption", param_encryption)  end
                            if param_signal_mode then _uci_real:set(pure_config, "wifi2_4G", "signal_mode", param_signal_mode) end
                            if param_channel then _uci_real:set(pure_config, "wifi2_4G", "channel", param_channel) end
                        end

                        if(param_ssid or param_enable or param_key or param_encryption or param_signal_mode or param_channel or param_disableall) then
                            matchssid = true
                            wifidev = network:get_wifidev(dev.device)
                            if(wifidev and param_channel) then
                                wifidev:set("channel", param_channel)
                            end
                            wifinet = network:get_wifinet(dev.device..".network1")
                            if(wifinet) then
                                if(param_ssid) then wifinet:set("ssid", param_ssid) end
--                                if(param_disableall) then
--                                    wifinet:set("disabled", param_disableall == "1" and 1 or nil )
--                                    wifidev:set("radio", 0)
--                                else
                                if(param_enable) then
                                    wifinet:set("disabled", param_enable~=1 and 1 or nil )
                                    wifidev:set("radio", param_enable)
                                end
--                                end
                                if(param_key) then wifinet:set("key", param_key) end
                                if(param_encryption) then wifinet:set("encryption",param_encryption) end
                                if(param_signal_mode) then
                                    if param_signal_mode == SAVE_MODE then
                                        wifinet:set("txpower", SAVE_MODE_TXPOWER)
                                    elseif param_signal_mode == NORMAL_MODE then
                                        wifinet:set("txpower", NORMAL_MODE_TXPOWER)
                                    elseif param_signal_mode == PREFORMANCE_MODE then
                                        wifinet:set("txpower", PREFORMANCE_MODE_TXPOWER)
                                    else
                                        code = sferr.ERROR_NO_UNKNOWN_SIGNAL_MODE
                                    end
                                end
                            end
                        end
                    end

                else
                    code = sferr.ERROR_NO_SSID_NONEXIST
                end
            end
        end

        if(not matchssid) then
            code = sferr.ERROR_NO_SSID_UNMATCH
        end

        if code==0 then
            local changes = network:changes()
            if((changes ~= nil) and (type(changes) == "table") and (next(changes) ~= nil)) then
                nixio.syslog("crit","apply changes")
                network:save("wireless")
                network:commit("wireless")
                SendToLocalServer(pure_config)
                sysutil.resetWifiDevice()
                sysutil.fork_exec("sleep 1; env -i; ubus call network reload ; wifi reload-legacy")
            end
        end

        result["code"] = code
        result["msg"]  = sferr.getErrorMessage(code)
    else
        result = sferr.errProtocolNotSupport()
    end

    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
    return

end

function get_assic_count()

    local assic_count = 0
    local wifis = sysutil.sf_wifinetworks()
    for i, dev in pairs(wifis) do
        for n=1,#dev.networks do
            for assic_addr,assic_info in pairs(dev.networks[n].assoclist) do
                assic_count = assic_count + 1
            end
        end
    end

    return assic_count
end

function get_wifi_speed()

    local wifis = sysutil.sf_wifinetworks()
    local ln = {}
    for i,dev in pairs(wifis) do
        ln[i] = {}
        for n=1,#dev.networks do
            ln[i][n] = {}
            local bwc = io.popen("luci-bwc -i %q 2>/dev/null" % dev.networks[n].ifname)
            if bwc then

                while true do
                    local tmp = bwc:read("*l")
                    if not tmp then break end
                    ln[i][n][#ln[i][n]+1] = {}
                        nixio.syslog("crit",tmp)
                    local stamp,rxb,rxp,txb,txp = string.match(tmp,"(%w+),%s(%w+),%s(%w+),%s(%w+),%s(%w+)")
                    ln[i][n][#ln[i][n]]["stamp"] = stamp
                    ln[i][n][#ln[i][n]]["rxb"] = rxb
                    ln[i][n][#ln[i][n]]["rxp"] = rxp
                    ln[i][n][#ln[i][n]]["txb"] = txb
                    ln[i][n][#ln[i][n]]["txp"] = txp

                end

                bwc:close()
            end
        end
    end

    return ln
end

function get_wan_speed()
    local ntm = require "luci.model.network".init()
    local wandev = ntm:get_wandev()
    local wan_ifname = wandev.ifname
    local data = {}
    local speed = {}
    local bwc = io.popen("luci-bwc -i %q 2>/dev/null" % wan_ifname)
    if bwc then

           while true do
               local tmp = bwc:read("*l")
               if not tmp then break end
               data[#data+1] = {}
               local stamp,rxb,rxp,txb,txp = string.match(tmp,"(%w+),%s(%w+),%s(%w+),%s(%w+),%s(%w+)")
               data[#data]["stamp"] = stamp
               data[#data]["rxb"] = rxb
               data[#data]["rxp"] = rxp
               data[#data]["txb"] = txb
               data[#data]["txp"] = txp

          end
          bwc:close()
    else
        return
    end

    local time_delta = 0
    local rx_speed_avg = 0
    local tx_speed_avg = 0
    if #data>1 then
        time_delta = data[#data]["stamp"] - data[1]["stamp"]
        rx_speed_avg = (data[#data]["rxb"]-data[1]["rxb"])/time_delta
        tx_speed_avg = (data[#data]["txb"]-data[1]["txb"])/time_delta
    end

    speed["rx_speed_avg"] = math.floor(rx_speed_avg)
    speed["tx_speed_avg"] = math.floor(tx_speed_avg)
    return speed
end

function memory_load()

    local _, _, memtotal, memcached, membuffers, memfree, _, swaptotal, swapcached, swapfree = luci.sys.sysinfo()

    local memrunning = memtotal - memfree
    local memload_rate = memrunning/memtotal
    memload_rate = (memload_rate - memload_rate%0.01)*100
    return memload_rate

end

function cpu_load()

    local data = {}
    local cpuload_rate = -1
    while cpuload_rate == -1 do
        local info =  io.popen("top -n 1 2>/dev/null")
        if info then
              while true do
                   local tmp = info:read("*l")
                   if not tmp then break end
                   data[#data+1] = tmp
                   --get idle
                   local idle = string.match(tmp,"nic%s+(%d+)%%%sidle")
                   if idle then
                      cpuload_rate = 100 - idle
                      break
                   end
              end
              info:close()
              break
        end
    end

    return cpuload_rate

end

function get_router_status()

    local code = 0
    local result = { }
    local wifis = sysutil.sf_wifinetworks()
    local querycpu = luci.http.formvalue("querycpu")
    local querymem = luci.http.formvalue("querymem")

    local protocol = checkversion()
    if(not protocol) then
        result = sferr.errProtocolNotFound()
    elseif(protocol == sysconfig.SF_PROTOCOL_VERSION_01) then
        result["status"] = sysutil.readRouterState()
        result["devicecount"] = get_assic_count()
        if get_wan_speed() then
            result["upspeed"] = get_wan_speed().tx_speed_avg
            result["downspeed"] = get_wan_speed().rx_speed_avg
        else
            code = sferr.ERROR_NO_CANNOT_GET_LANSPEED
        end

        result["cpuload"] = (querycpu == "1") and cpu_load() or 0
        result["memoryload"] = (querymem == "1") and memory_load() or 0

        result["downloadingcount"] = 0   --------------------------TODO----------------------
        result["useablespace"] = 0       --------------------------TODO---------------------

        result["code"] = code
        result["msg"]  = sferr.getErrorMessage(code)
    else
        result = sferr.errProtocolNotSupport()
    end

    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
    return

end

function get_useableifname()

    local useable_ifname = {}
    local allifname = luci.sys.net.devices()
    for i,ifname in pairs(allifname) do
        if ifname:match("eth") or ifname:match("ra") then
                useable_ifname[#useable_ifname+1] = ifname
        end
    end
    return useable_ifname
end

function getdevname(ipaddr)
    local hostname = nil
    dhcp_fd = io.open("/tmp/dhcp.leases", "r")
    if dhcp_fd then
        while true do
            local tmp = dhcp_fd:read("*l")
            if not tmp then break end
            if tmp:find(ipaddr) then
                hostname = tmp:match("^%d+ %S+ %S+ (%S+)")
                break
            end
        end
        dhcp_fd:close()
    end
    if hostname == nil then
        hostname = ""
    end
    return hostname
end

function getofflinemac(ipaddr)
    local mac = nil
    arp_fd = io.open("/proc/net/arp", "r")
    if arp_fd then
        while true do
            local tmp = arp_fd:read("*l")
            if not tmp then break end
            if tmp:find(ipaddr) then
                mac = tmp:match("([a-fA-F0-9]*:[a-fA-F0-9]*:[a-fA-F0-9]*:[a-fA-F0-9]*:[a-fA-F0-9]*:[a-fA-F0-9]*)")
                break
            end
        end
        arp_fd:close()
    end

    return mac
end

function get_system_runtime()
    local runtime = nil
    local time_fd = io.open("/proc/uptime","r")
    if time_fd then
        local tmp = time_fd:read("*l")
        if tmp then
            runtime = tmp:match("(%d+).*")
        end
    end
    if runtime then
        return tonumber(runtime)
    else
        return 0
    end
end

function ndscan(ipaddr, interface)

    local data = nil
    local scan_fd = nil
    scan_fd = io.popen("ndscan -i %s -d %s -t 10 -c 2 2>/dev/null" %{interface, ipaddr})
    if scan_fd then
        local tmp = scan_fd:read("*l")
        if tmp then
            local mac1,mac2,mac3,mac4,mac5,mac6 = tmp:match("([a-fA-F0-9]*):([a-fA-F0-9]*):([a-fA-F0-9]*):([a-fA-F0-9]*):([a-fA-F0-9]*):([a-fA-F0-9]*)")
            if mac1 and mac2 and mac3 and mac4 and mac5 and mac6 then
                data = {}
                data["dev"] = interface
                data["mac"] = string.upper(mac1.."_"..mac2.."_"..mac3.."_"..mac4.."_"..mac5.."_"..mac6)
                data["ip"] = ipaddr
                data["online"] = "1"
                data["port"] = "0"
            end
        end
        scan_fd:close()
    end

    return data
end

function get_ip_mac()
    local mac = nil
    local ip_mac = nil
    arp_fd = io.open("/proc/net/arp", "r")
    local wan_ifname = _uci_real:get("network", "wan", "ifname");
    if arp_fd then
        ip_mac = {}
        while true do
            local tmp = arp_fd:read("*l")
            if not tmp then break end
            if(not string.match(tmp,wan_ifname)) then
            	ip = tmp:match("([0-9]+.[0-9]+.[0-9]+.[0-9]+)")
            	mac = tmp:match("([a-fA-F0-9]*:[a-fA-F0-9]*:[a-fA-F0-9]*:[a-fA-F0-9]*:[a-fA-F0-9]*:[a-fA-F0-9]*)")
            	if ip and mac then
                	local mac_first_part = mac:match("^([0-9]+):")
                	ip_mac[#ip_mac+1] = {}
                	ip_mac[#ip_mac]["ip"] = ip
                	if mac_first_part == "0" then
                    	ip_mac[#ip_mac]["mac"] = "0"..(mac:upper()):gsub(":", "_")
                	else
                    	ip_mac[#ip_mac]["mac"] = (mac:upper()):gsub(":", "_")
                	end
            	end
            end
        end
        arp_fd:close()
    end

    return ip_mac

end
function get_wire_assocdev()
    local data = {}
    local info = nil
    local arp_fd = nil
    local device = {}
    local wire_ifname = _uci_real:get("network", "lan", "ifname");

    local wire_flag = nil
    local dev_exist = nil
    local online = nil
    local ip2mac = {}
    local wire_info = {}

    ip2mac = get_ip_mac()

    _uci_real:foreach("devlist", "device",
        function(s)
            local flag = 0
            for j=1,#ip2mac do
                if (ip2mac[j]["mac"] and ip2mac[j]["mac"] == s.mac) then
                    flag = 1
                    break
                end
            end
            if(flag == 0) then
                if(s.online == "1") then
                    data[#data+1] = {}
                    data[#data]["mac"] = s.mac
                    data[#data]["online"] = "0"
                    data[#data]["ip"] = "0.0.0.0"
                    data[#data]["associate_time"] = -1
                end
            end
        end
        )

    for i=1,#ip2mac do
        if ip2mac[i]["mac"] ~= "00_00_00_00_00_00" and string.match(ip2mac[i]["ip"],"192.168.12.*") then
            wire_flag = 1
            dev_exist = 0
            local ip = ""
            _uci_real:foreach("devlist", "device",
                        function(s)
                            if ip2mac[i]["mac"] == s.mac then
                                dev_exist = 1
                                if s.port == nil or s.port == "1" then
                                    wire_flag = 0
                                else
                                    online = s.online
                                    ip = s.ip
                                end
                            end
                        end
                    )
            if(wire_flag == 1) then
                local scan_value = os.execute("ping -4 -w3 -c1 %s" %{ip2mac[i]["ip"]})
                if(dev_exist == 1) then
                    data[#data+1] = {}
                    if scan_value ~= 0 then
                        if not(wire_info[ip2mac[i]["mac"]]) then
                            if(os.execute("ping -4 -w1 -c1 %s" %{ip}) ~= 0) then
                                data[#data]["online"] = "0"
                                data[#data]["ip"] = "0.0.0.0"
                                data[#data]["associate_time"] = -1
                            end
                        end
                    else
                        wire_info[ip2mac[i]["mac"]] = "1"
                        if online == "0" or ip2mac[i]["ip"] ~= ip then
                            data[#data]["online"] = "1"
                            data[#data]["ip"] = ip2mac[i]["ip"]
                            data[#data]["hostname"] = getdevname(ip2mac[i]["ip"])
                            data[#data]["associate_time"] = get_system_runtime()
                        end
                    end
                    data[#data]["mac"] = ip2mac[i]["mac"]
                    local section_json = _uci_real:get_all("devlist", ip2mac[i]["mac"]:upper())
                    if section_json then
                        data[#data]["internet"] = section_json.internet and section_json.internet or "1"
                        data[#data]["lan"] = section_json.lan and section_json.lan or "1"
                    end
                else
                    if scan_value == 0 then
                        data[#data+1] = {}
                        data[#data]["dev"] = wire_ifname
                        data[#data]["ip"] = ip2mac[i]["ip"]
                        data[#data]["port"] = "0"
                        data[#data]["online"] = "1"
                        data[#data]["mac"] = ip2mac[i]["mac"]
                        data[#data]["hostname"] = getdevname(ip2mac[i]["ip"])
                        data[#data]["associate_time"] = get_system_runtime()
                        local section_json = _uci_real:get_all("devlist", ip2mac[i]["mac"]:upper())
                        if section_json then
                            data[#data]["internet"] = section_json.internet and section_json.internet or "1"
                            data[#data]["lan"] = section_json.lan and section_json.lan or "1"
                        end
                    end
                end
            end
        end
    end

    return data
end

function get_all_assocdev()

    local data = {}
    local count = 0
    _uci_real:foreach("devlist", "device",
                function(s)
                    if s[".name"] and s[".name"] ~= "00_00_00_00_00_00" then
                        count = count + 1
                        data[count] = {}
                        data[count]["mac"] = s.mac or ""
                        data[count]["hostname"] = s.hostname or ""
                        data[count]["dev"] = s.dev or ""
                        data[count]["online"] = s.online or "0"
                        if s.dev and s.dev:match("eth") then
                            data[count]["port"] = "0"
                        else
                            data[count]["port"] = "1"
                        end
                        if s.ip == nil then
                            data[count]["ip"] = "0.0.0.0"
                        else
                            data[count]["ip"] = s.ip
                        end
                        if s.internet == nil then
                            data[count]["internet"] = 1
                        end
                        if s.lan == nil then
                            data[count]["lan"] = 1
                        end
                        if s.limitdown == nil then
                            data[count]["limitdown"] = -1
                        end
                        if s.limitup == nil then
                            data[count]["limitup"] = -1
                        end
                        if s.speedlvl == nil then
                            data[count]["speedlvl"] = 2
                        end
                    end
                end
            )

    return data

end

function get_fw_assocdev()

    local data = {}
    _uci_real:foreach("firewall", "rule",
                      function(s)
                        if s.name == "custom" then
                            data[#data+1] = {}
                            data[#data]["mac"] = s.src_mac:gsub(":","_")
                            if s.target ~= "ACCEPT" then
                                data[#data]["internet"] = 0
                                if s.dest ~= "wan" then
                                    data[#data]["lan"] = 0
                                else
                                    data[#data]["lan"] = 1
                                end
                            else
                                data[#data]["internet"] = 1
                            end

                        end
                      end
                      )
    return data

end

function set_tab(dev)

    local alldev = dev
    local ex_list = _uci_real:get_all("devlist")
    local exist_flag = 0

    for i=1,#alldev do
        _uci_real:set("devlist", alldev[i].mac , "device")
        _uci_real:tset("devlist", alldev[i].mac , alldev[i])
    end
    _uci_real:save("devlist")
    _uci_real:commit("devlist")

end

function get_dev_trafficinfo(dev_ip, time_stamp)
    local data = {}
    local traffic = {}
    local bwc = io.popen("luci-bwc -d %s 2>/dev/null" %{dev_ip})
    if bwc then

           while true do
               local tmp = bwc:read("*l")
               if not tmp then break end
               data[#data+1] = {}
               local stamp,rxb,rxp,txb,txp = string.match(tmp,"(%w+),%s(%w+),%s(%w+),%s(%w+),%s(%w+)")
               data[#data]["stamp"] = stamp
               data[#data]["rxb"] = rxb
               data[#data]["rxp"] = rxp
               data[#data]["txb"] = txb
               data[#data]["txp"] = txp

          end
          bwc:close()
    else
        return
    end

    local time_delta = 0
    local rx_speed_cur = 0
    local tx_speed_cur = 0
    local down_traffic_total = 0
    local up_traffic_total = 0
    if #data>1 and #data<4 then
        time_delta = data[#data]["stamp"] - data[1]["stamp"]
        rx_speed_cur = (data[#data]["rxb"] - data[1]["rxb"])/time_delta
        tx_speed_cur = (data[#data]["txb"] - data[1]["txb"])/time_delta
    elseif #data >= 4 and tonumber(time_stamp or "0") < tonumber(data[#data-3]["stamp"]) then
        time_delta = data[#data]["stamp"] - data[#data-3]["stamp"]
        rx_speed_cur = (data[#data]["rxb"] - data[#data-3]["rxb"])/time_delta
        tx_speed_cur = (data[#data]["txb"] - data[#data-3]["txb"])/time_delta
    end

    if #data>1 then
        down_traffic_total = data[#data]["rxb"]
        up_traffic_total = data[#data]["txb"]
    end

    traffic["down_speed_cur"] = math.floor(rx_speed_cur)
    traffic["up_speed_cur"] = math.floor(tx_speed_cur)
    traffic["down_traffic_total"] = down_traffic_total
    traffic["up_traffic_total"] = up_traffic_total
    if(traffic["down_speed_cur"] < 0) then
        traffic["down_speed_cur"] = 0
    end
    if(traffic["up_speed_cur"] < 0) then
        traffic["up_speed_cur"] = 0
    end
    return traffic
end

function delete_rule()
    local rule_should_delete = 0
    for line in io.lines("/tmp/mac-ip") do
        rule_should_delete = 1
        _uci_real:foreach("devlist","device",
            function(s)
                if s.online == "1" and s[".name"] ~= "00_00_00_00_00_00" then
                    if string.find(line, s.ip) then
                        rule_should_delete = 0
                    end
                end
            end
        )
        if rule_should_delete == 1 then
            luci.sys.call("iptables -D FORWARD -s %s -j UPLOAD >/dev/null 2>/dev/null" %{line})
            luci.sys.call("iptables -D FORWARD -d %s -j DOWNLOAD >/dev/null 2>/dev/null" %{line})
        end
    end
end

function insert_rule()
    local rule_should_insert = 0
    _uci_real:foreach("devlist","device",
        function(s)
            if s.online == "1" and s[".name"] ~= "00_00_00_00_00_00" then
                rule_should_insert = 1
                for line in io.lines("/tmp/mac-ip") do
                    if string.find(s.ip, line) then
                        rule_should_insert = 0
                    end
                end
            else
                rule_should_insert = 0
            end
            if rule_should_insert == 1 then
                luci.sys.call("iptables -I FORWARD 1 -s %s -j UPLOAD >/dev/null 2>/dev/null" %{s.ip})
                luci.sys.call("iptables -I FORWARD 1 -d %s -j DOWNLOAD >/dev/null 2>/dev/null" %{s.ip})
            end
        end
    )
end

function update_rule()
    delete_rule()
    insert_rule()

-----------empty mac-ip------------------------------
    local file = io.open("/tmp/mac-ip","w+")
    file:close()

----------refresh mac-ip-----------------------------
    file = io.open("/tmp/mac-ip","a+")
    _uci_real:foreach("devlist", "device",
                function(s)
                    if s.online == "1" and s[".name"] ~= "00_00_00_00_00_00" then
                        file:write(s.ip.."\n")
                    end
                end
            )
    file:close()
end

function create_chain()
    luci.sys.call("speed start >/dev/null 2>/dev/null")
end

function set_trafficinfo_to_devlist()

    local chain_exist = 0
    local chain =  io.popen("iptables -nvx -L FORWARD 2>/dev/null")
    if chain then
        while true do
            local tmp = chain:read("*l")
            if not tmp then
                chain_exist = 0
                break
            end
            if string.find(tmp,"DOWNLOAD") or string.find(tmp,"UPLOAD") then
                chain_exist = 1
                break
            end
        end
        chain:close()
    end


    if chain_exist == 1 then
        update_rule()
    else
        create_chain()
    end

    _uci_real:foreach("devlist","device",
        function(s)
            if s.online == "1" and s[".name"] ~= "00_00_00_00_00_00" then
                local trafficinfo = get_dev_trafficinfo(s.ip, s.reset_stamp)
                if trafficinfo then
                    if trafficinfo.up_speed_cur then _uci_real:set("devlist", s[".name"], "upspeed" , trafficinfo.up_speed_cur) end
                    if trafficinfo.down_speed_cur then _uci_real:set("devlist", s[".name"], "downspeed", trafficinfo.down_speed_cur) end
                    if trafficinfo.up_traffic_total then _uci_real:set("devlist", s[".name"], "uploadtotal" , trafficinfo.up_traffic_total + (s.uploadtotal_offset or 0)) end
                    if trafficinfo.down_traffic_total then _uci_real:set("devlist", s[".name"], "downloadtotal", trafficinfo.down_traffic_total + (s.downloadtotal_offset or 0)) end
                    if trafficinfo.up_speed_cur then _uci_real:set("devlist", s[".name"], "maxuploadspeed", trafficinfo.up_speed_cur>=tonumber(s.maxuploadspeed or 0) and trafficinfo.up_speed_cur or s.maxuploadspeed) end
                    if trafficinfo.down_speed_cur then _uci_real:set("devlist", s[".name"], "maxdownloadspeed", trafficinfo.down_speed_cur>=tonumber(s.maxdownloadspeed or 0) and trafficinfo.down_speed_cur or s.maxdownloadspeed) end
                end

            else
                _uci_real:set("devlist", s[".name"], "upspeed" , 0 )
                _uci_real:set("devlist", s[".name"], "downspeed", 0 )
                _uci_real:set("devlist", s[".name"], "uploadtotal" , 0 )
                _uci_real:set("devlist", s[".name"], "downloadtotal", 0 )
                _uci_real:set("devlist", s[".name"], "maxuploadspeed", 0 )
                _uci_real:set("devlist", s[".name"], "maxdownloadspeed", 0 )
            end

        end
    )
    _uci_real:save("devlist")
    _uci_real:commit("devlist")

end

function set_basicinfo_to_devlist()

    local wiredev = get_wire_assocdev()
    set_tab(wiredev)
    local all = get_all_assocdev()
    set_tab(all)

end

function get_devinfo_from_devlist()
        local list_all = {}
        local list_online = {}
        _uci_real:foreach("devlist", "device",
                function(s)
                    if (s[".name"] ~= "00_00_00_00_00_00") then
                        list_all[#list_all+1] = {}
                        list_all[#list_all]["authority"] = {}
                        list_all[#list_all]["speed"]     = {}
                        list_all[#list_all]["hostname"]                   = s.hostname or ""
                        list_all[#list_all]["nickname"]                   = s.nickname or s.hostname or ""
                        list_all[#list_all]["mac"]                        = s.mac or ""
                        list_all[#list_all]["dev"]                        = s.dev or ""
                        list_all[#list_all]["port"]                       = tonumber(s.port or -1)
                        list_all[#list_all]["online"]                     = tonumber(s.online or -1)
                        list_all[#list_all]["ip"]                         = s.ip or "0.0.0.0"
                        list_all[#list_all]["authority"]["internet"]      = tonumber(s.internet or -1)
                        list_all[#list_all]["authority"]["lan"]           = tonumber(s.lan or -1)
                        list_all[#list_all]["authority"]["speedlvl"]      = tonumber(s.speedlvl or -1)
                        list_all[#list_all]["authority"]["limitup"]       = tonumber(s.limitup or -1)
                        list_all[#list_all]["authority"]["limitdown"]     = tonumber(s.limitdown or -1)
                        list_all[#list_all]["authority"]["notify"]        = tonumber(s.notify or -1)
                        list_all[#list_all]["speed"]["upspeed"]           = tonumber(s.upspeed or 0)
                        list_all[#list_all]["speed"]["downspeed"]         = tonumber(s.downspeed or 0)

                        if(list_all[#list_all]["authority"]["limitup"] and list_all[#list_all]["authority"]["limitup"] > 0 and list_all[#list_all]["speed"]["upspeed"] and list_all[#list_all]["authority"]["limitup"]*1000 < list_all[#list_all]["speed"]["upspeed"]) then
                            list_all[#list_all]["speed"]["upspeed"] = list_all[#list_all]["authority"]["limitup"]*1000
                        end
                        if(list_all[#list_all]["authority"]["limitdown"] and list_all[#list_all]["authority"]["limitdown"] > 0 and list_all[#list_all]["speed"]["downspeed"] and list_all[#list_all]["authority"]["limitdown"]*1000 < list_all[#list_all]["speed"]["downspeed"]) then
                            list_all[#list_all]["speed"]["downspeed"] = list_all[#list_all]["authority"]["limitdown"]*1000
                        end
                        list_all[#list_all]["speed"]["uploadtotal"]       = tonumber(s.uploadtotal or 0)
                        list_all[#list_all]["speed"]["downloadtotal"]     = tonumber(s.downloadtotal or 0)
                        list_all[#list_all]["speed"]["maxuploadspeed"]    = tonumber(s.maxuploadspeed or 0)
                        list_all[#list_all]["speed"]["maxdownloadspeed"]  = tonumber(s.maxdownloadspeed or 0)
                        if list_all[#list_all]["online"] == 0 then
                            list_all[#list_all]["speed"]["online"] = 0
                        else
                            list_all[#list_all]["speed"]["online"]        = tonumber(s.associate_time or -1)~=-1 and (get_system_runtime()-tonumber(s.associate_time)) or 0
                        end
                        if s.online == "1" and s[".name"] ~= "00_00_00_00_00_00" then
                            list_online[#list_online+1] = {}
                            list_online[#list_online]["authority"] = {}
                            list_online[#list_online]["speed"]     = {}
                            list_online[#list_online]["hostname"]                  = s.hostname or ""
                            list_online[#list_online]["nickname"]                  = s.nickname or s.hostname or ""
                            list_online[#list_online]["mac"]                       = s.mac or ""
                            list_online[#list_online]["dev"]                       = s.dev or ""
                            list_online[#list_online]["port"]                      = tonumber(s.port or -1)
                            list_online[#list_online]["online"]                    = tonumber(s.online or -1)
                            list_online[#list_online]["ip"]                        = s.ip or "0.0.0.0"
                            list_online[#list_online]["authority"]["internet"]     = tonumber(s.internet or -1)
                            list_online[#list_online]["authority"]["lan"]          = tonumber(s.lan or -1)
                            list_online[#list_online]["authority"]["speedlvl"]     = tonumber(s.speedlvl or -1)
                            list_online[#list_online]["authority"]["limitup"]      = tonumber(s.limitup or -1)
                            list_online[#list_online]["authority"]["limitdown"]    = tonumber(s.limitdown or -1)
                            list_online[#list_online]["authority"]["notify"]       = tonumber(s.notify or -1)
                            list_online[#list_online]["speed"]["upspeed"]          = tonumber(s.upspeed or 0)
                            list_online[#list_online]["speed"]["downspeed"]        = tonumber(s.downspeed or 0)
                            if(list_online[#list_online]["authority"]["limitup"] and list_online[#list_online]["authority"]["limitup"] > 0 and list_online[#list_online]["speed"]["upspeed"] and list_online[#list_online]["authority"]["limitup"]*1000 < list_online[#list_online]["speed"]["upspeed"]) then
                                list_online[#list_online]["speed"]["upspeed"] = list_online[#list_online]["authority"]["limitup"]*1000
                            end
                            if(list_online[#list_online]["authority"]["limitdown"] and list_online[#list_online]["authority"]["limitdown"] > 0 and list_online[#list_online]["speed"]["downspeed"] and list_online[#list_online]["authority"]["limitdown"]*1000 < list_online[#list_online]["speed"]["downspeed"]) then
                                list_online[#list_online]["speed"]["downspeed"] = list_online[#list_online]["authority"]["limitdown"]*1000
                            end
                            list_online[#list_online]["speed"]["uploadtotal"]      = tonumber(s.uploadtotal or 0)
                            list_online[#list_online]["speed"]["downloadtotal"]    = tonumber(s.downloadtotal or 0)
                            list_online[#list_online]["speed"]["maxuploadspeed"]   = tonumber(s.maxuploadspeed or 0)
                            list_online[#list_online]["speed"]["maxdownloadspeed"] = tonumber(s.maxdownloadspeed or 0)
                            list_online[#list_online]["speed"]["online"]           = tonumber(s.associate_time or -1)~=-1 and (get_system_runtime()-tonumber(s.associate_time)) or 0
                        end
                    end
                end
                )
        return list_all,list_online
end

function get_wireless_device_mac()
    local ifname = {}
    local number = 0
    local data = {}
    local i = 0
    _uci_real:foreach("wireless", "wifi-iface",
        function(s)
            if s.ifname then

                ifname[number] = s.ifname
                number = number + 1
            end
        end
        )
    for n=0,number do
        if(ifname[n]) then
            local name = ifname[n]
            local iwinfo = io.popen("iwinfo \"%s\" assoclist" %{name})
            local mac = nil
            if iwinfo then
                while true do
                    mac = nil
                    local tmp = iwinfo:read("*l")
                    if not tmp then
                        break
                    end
                    mac = tmp:match("([a-fA-F0-9]*:[a-fA-F0-9]*:[a-fA-F0-9]*:[a-fA-F0-9]*:[a-fA-F0-9]*:[a-fA-F0-9]*)")
                    if mac then
                        data[i] = (mac:upper()):gsub(":", "_")
                        i = i + 1
                    end
                end
            end
        end
    end
    return data,i
end

function update_wireless_device()
    local data,j = get_wireless_device_mac()
    local change = {}
    for i=0,j do
        _uci_real:foreach("devlist", "device",
            function(s)
                if(data[i] and data[i] == s.mac and s.port == '1') then
                    if(s.online == '0') then
                        change[#change+1] = {}
                        change[#change]["mac"] = s.mac
                        change[#change]["online"] = '1'
                        change[#change]["associate_time"] = get_system_runtime()
--                        change[j]["ip"] =
                    end
                end
            end
            )

    end
    _uci_real:foreach("devlist", "device",
        function(s)
            if(s.online == '1' and s.port == '1') then
                local dev_online = '0'
                for i=0,j do
                    if(data[i] and data[i] == s.mac) then
                        dev_online = '1'
                        break
                    end
                end
                if(dev_online == '0') then
                    change[#change+1] = {}
                    change[#change]["mac"] = s.mac
                    change[#change]["online"] = '0'
                    change[#change]["associate_time"] = -1
                    change[#change]["ip"] = "0.0.0.0"
                end
            end
        end
        )
    set_tab(change)
end

function get_device_list()

    local code = 0
    local result = { }
    local protocol = checkversion()
    if(not protocol) then
        result = sferr.errProtocolNotFound()
    elseif(protocol == sysconfig.SF_PROTOCOL_VERSION_01) then

        set_basicinfo_to_devlist()
        set_trafficinfo_to_devlist()
        result["code"] = code
        result["msg"] = sferr.getErrorMessage(code)
    else
        result = sferr.errProtocolNotSupport()
    end

    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
    return

end

function get_device_info()
    local result = {}
    local code = 0
    local info_type = "1"
    local mac = nil
    if luci.http.formvalue("type") then
        info_type = luci.http.formvalue("type")
    end
    if luci.http.formvalue("mac") then
        mac = luci.http.formvalue("mac"):gsub(":","_")
    end

    local protocol = checkversion()
    if(not protocol) then
        result = sferr.errProtocolNotFound()
    elseif(protocol == sysconfig.SF_PROTOCOL_VERSION_01) then
        update_wireless_device()
        local list_all,list_online = get_devinfo_from_devlist()
        if info_type == "1" then
            result["list"] = list_all
        elseif info_type == "2" then
            result["list"] = list_online
        elseif info_type == "3" then
            result["online"] = #list_online
        elseif mac and info_type == "4" then
            for i=1,#list_all do
                if list_all[i]["mac"] == mac then
                    list_each = list_all[i]
                    break
                end
            end
            result["list"] = list_each
        else
        --------add ERRCODE------------
        end
        result["code"] = code
        result["msg"] = sferr.getErrorMessage(code)

    else
        result = sferr.errProtocolNotSupport()
    end
    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
    return
end

function set_qos_config(alldev)

    local ex_list = _uci_real:get_all("qos")
    local exist_flag = 0

    for i=1,#alldev do
        _uci_real:set("qos", alldev[i].mac , "device")
        _uci_real:tset("qos", alldev[i].mac , alldev[i])
    end
    _uci_real:save("qos")
    _uci_real:commit("qos")

end

function update_qos()

    local result = {}
    local wifi_ifname_array = {}
    local wire_ifname_array = {}
    _uci_real:foreach("wireless", "wifi-iface",
    function(s)
        if s.ifname then wifi_ifname_array[#wifi_ifname_array+1] = s.ifname end
    end
    )
    wire_ifname_array[#wire_ifname_array+1] = _uci_real:get("network", "lan", "ifname")
    ---------------------qos--------------------------------------
    local net = {}
    _uci_real:foreach("devlist","device",
    function(s)
        if s.dev and s.online=="1" and s[".name"] ~= "00_00_00_00_00_00" and s.limitdown and s.limitup and s.speedlvl then
            if net[s.dev] == nil then
                net[s.dev] = {}
                net[s.dev][#net[s.dev]+1] = {}
                net[s.dev][#net[s.dev]]["ip"] = s.ip
                net[s.dev][#net[s.dev]]["limitdown"] = (tonumber(s.limitdown)>0) and s.limitdown or nil
                net[s.dev][#net[s.dev]]["limitup"] = (tonumber(s.limitup)>0) and s.limitup or nil
                net[s.dev][#net[s.dev]]["speedlvl"] = (tonumber(s.speedlvl)>0) and s.speedlvl or 2
            else
                net[s.dev][#net[s.dev]+1] = {}
                net[s.dev][#net[s.dev]]["ip"] = s.ip
                net[s.dev][#net[s.dev]]["limitdown"] = (tonumber(s.limitdown)>0) and s.limitdown or nil
                net[s.dev][#net[s.dev]]["limitup"] = (tonumber(s.limitup)>0) and s.limitup or nil
                net[s.dev][#net[s.dev]]["speedlvl"] = (tonumber(s.speedlvl)>0) and s.speedlvl or 2
            end
        end
    end
    )

    local qos = _uci_real:get_all("qos_cfg", "qos")
    local file = nil
    file = io.open("/etc/firewall.user", "w+")
    if (not qos) or qos["enable"] ~= "1" then
        file:close()
        file = io.open("/etc/firewall.user.bk", "w+")
    end

    file:write("#!/bin/sh\n")
    local iface_num = 0

    local lan_ifname_array = {}
    _uci_real:foreach("wireless", "wifi-iface",
    function(s)
        if s.ifname then
            lan_ifname_array[#lan_ifname_array+1] = s.ifname
        end
    end
    )

    lan_ifname_array[#lan_ifname_array+1] = _uci_real:get("network", "lan", "ifname")

    ----------------qos--------download-speed-limit-----------------------
    if qos and qos["mode"] == "2" then
        for lan_ifname_index, lan_ifname in pairs(lan_ifname_array) do
            file:write("tc qdisc del dev %s root\n" %{lan_ifname})
        end
        for ifname,dev_tbl in pairs(net) do
            iface_num = iface_num+1
            file:write("tc qdisc add dev %s root handle %s: htb default 256\n" %{ifname, tostring(iface_num)})
            for i,dev_info in pairs(dev_tbl) do
                if dev_info.limitdown then
                    file:write("tc class add dev %s parent %s: classid %s:%s htb rate %skbps ceil %skbps\n" %{ifname, tostring(iface_num), tostring(iface_num), tostring(i), dev_info.limitdown, dev_info.limitdown })
                    file:write("tc qdisc add dev %s parent %s:%s handle %s: sfq perturb 10\n" %{ifname, tostring(iface_num), tostring(i), tostring(iface_num)..tostring(i) })
                    file:write("tc filter add dev %s parent %s: protocol ip handle %s fw classid %s:%s\n" %{ifname, tostring(iface_num), tostring(iface_num)..tostring(i), tostring(iface_num), tostring(i)  })
                    file:write("iptables -t mangle -A POSTROUTING -d %s -j MARK --set-mark %s\n" %{dev_info.ip, tostring(iface_num)..tostring(i)})
                end
            end
        end

        ----------------qos--------upload-speed-limit-----------------------
        local wan_ifname = _uci_real:get("network","wan","ifname")
        iface_num = 0
        file:write("tc qdisc del dev %s root\n" %{wan_ifname})
        for ifname,dev_tbl in pairs(net) do
            iface_num = iface_num+1
            file:write("tc qdisc add dev %s root handle %s: htb default 256\n" %{wan_ifname, tostring(iface_num)})
            for i,dev_info in pairs(dev_tbl) do
                if dev_info.limitup then
                    file:write("tc class add dev %s parent %s: classid %s:%s htb rate %skbps ceil %skbps\n" %{wan_ifname, tostring(iface_num), tostring(iface_num), tostring(i), dev_info.limitup, dev_info.limitup })
                    file:write("tc qdisc add dev %s parent %s:%s handle %s: sfq perturb 10\n" %{wan_ifname, tostring(iface_num), tostring(i), tostring(iface_num)..tostring(i) })
                    file:write("tc filter add dev %s parent %s: protocol ip handle %s fw classid %s:%s\n" %{wan_ifname, tostring(iface_num), tostring(iface_num)..tostring(i), tostring(iface_num), tostring(i)  })
                    file:write("iptables -t mangle -A PREROUTING -s %s -j MARK --set-mark %s\n" %{dev_info.ip, tostring(iface_num)..tostring(i)})
                end
            end
        end
        ----------------qos--------download-priority-----------------------
    elseif qos and qos["mode"] == "1" then
        for ifname,dev_tbl in pairs(net) do
            iface_num = iface_num+1
            file:write("tc qdisc del dev %s root\n" %{ifname})
            file:write("tc qdisc add dev %s root handle %s: htb default 256\n" %{ifname, tostring(iface_num)})
            for i,dev_info in pairs(dev_tbl) do
                if dev_info.speedlvl then
                    file:write("tc class add dev %s parent %s: classid %s:%s htb default 256\n" %{ifname, tostring(iface_num), tostring(iface_num), tostring(i)})
                    file:write("tc qdisc add dev %s parent %s:%s handle %s: sfq perturb 10\n" %{ifname, tostring(iface_num), tostring(i), tostring(iface_num)..tostring(i) })
                    file:write("tc filter add dev %s parent %s: protocol ip prio %s handle %s fw classid %s:%s\n" %{ifname, tostring(iface_num), dev_info.speedlvl, tostring(iface_num)..tostring(i), tostring(iface_num), tostring(i)  })
                    file:write("iptables -t mangle -A POSTROUTING -d %s -j MARK --set-mark %s\n" %{dev_info.ip, tostring(iface_num)..tostring(i)})
                end
            end
        end

        ----------------qos--------upload-priority-----------------------
        local wan_ifname = _uci_real:get("network","wan","ifname")
        iface_num = 0
        file:write("tc qdisc del dev %s root\n" %{wan_ifname})
        for ifname,dev_tbl in pairs(net) do
            iface_num = iface_num+1
            file:write("tc qdisc add dev %s root handle %s: htb default 256\n" %{wan_ifname, tostring(iface_num)})
            for i,dev_info in pairs(dev_tbl) do
                if dev_info.speedlvl then
                    file:write("tc class add dev %s parent %s: classid %s:%s htb default 256\n" %{wan_ifname, tostring(iface_num), tostring(iface_num), tostring(i)})
                    file:write("tc qdisc add dev %s parent %s:%s handle %s: sfq perturb 10\n" %{wan_ifname, tostring(iface_num), tostring(i), tostring(iface_num)..tostring(i) })
                    file:write("tc filter add dev %s parent %s: protocol ip prio %s handle %s fw classid %s:%s\n" %{wan_ifname, tostring(iface_num), dev_info.speedlvl, tostring(iface_num)..tostring(i), tostring(iface_num), tostring(i)  })
                    file:write("iptables -t mangle -A PREROUTING -s %s -j MARK --set-mark %s\n" %{dev_info.ip, tostring(iface_num)..tostring(i)})
                end
            end
        end
    elseif qos and qos["mode"] == "0" then

    end

    file:close()

    _uci_real:foreach("devlist","device",
    function(s)
        if s.online == "1" and s[".name"] ~= "00_00_00_00_00_00" then
            local trafficinfo = get_dev_trafficinfo(s.ip, s.reset_stamp)
            _uci_real:set("devlist", s[".name"], "upspeed" , trafficinfo.up_speed_cur)
            _uci_real:set("devlist", s[".name"], "downspeed", trafficinfo.down_speed_cur)
            _uci_real:set("devlist", s[".name"], "uploadtotal_offset", s.uploadtotal or "0")
            _uci_real:set("devlist", s[".name"], "downloadtotal_offset", s.downloadtotal or "0")
            _uci_real:set("devlist", s[".name"], "reset_stamp", get_system_runtime())
        end
    end
    )
    _uci_real:save("devlist")
    _uci_real:commit("devlist")

    luci.sys.call("/etc/init.d/firewall restart")

    create_chain()
--[[
    code = 0
    result["code"] = code
    result["msg"] = sferr.getErrorMessage(code)

    if (http.getenv("HTTP_AUTHORIZATION") ~= "") then
        -------the caller is syncservice
        luci.http.prepare_content("application/json")
        luci.http.write_json(result)
    else

    end
    ]]
    return
end

function update_qos_local()
    update_qos()
    code = 0
    result = {}
    result["code"] = code
    result["msg"] = sferr.getErrorMessage(code)
    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
    return
end

function set_fw_rule()

    local code = 0
    local result = {}
    local s_name = nil
    local authority_obj = {}
    local mac = luci.http.formvalue("mac")
    if not mac then
        code = sferr.ERROR_NO_MAC_EMPTY
        result["code"] = code
        result["msg"] = sferr.getErrorMessage(code)
        luci.http.prepare_content("application/json")
        luci.http.write_json(result)
        return
    end
    mac = mac:gsub("_",":")
    local internet = luci.http.formvalue("internet") or (_uci_real:get("devlist", mac:gsub(":","_"), "internet") or "1")
    local lan = luci.http.formvalue("lan") or (_uci_real:get("devlist", mac:gsub(":","_"), "lan") or "1")
    local speedlvl = luci.http.formvalue("speedlvl") or (_uci_real:get("devlist", mac:gsub(":","_"), "speedlvl") or "2")
    local limitup = luci.http.formvalue("limitup") or (_uci_real:get("devlist", mac:gsub(":","_"), "limitup") or tostring(-1))
    local limitdown = luci.http.formvalue("limitdown") or (_uci_real:get("devlist", mac:gsub(":","_"), "limitdown") or tostring(-1))
    local nickname = luci.http.formvalue("nickname")
    if nickname and string.len(nickname) > 31 then
        local nickname_tmp = nickname
        nickname = string.sub(nickname_tmp,1,31)
    end
    local notify = luci.http.formvalue("notify")
    local disk = luci.http.formvalue("disk")
    local pure_config = "device_bomb"

    local protocol = checkversion()
    if(not protocol) then
        result = sferr.errProtocolNotFound()
    elseif(protocol == sysconfig.SF_PROTOCOL_VERSION_01) then

        local sn = sysutil.getSN()
        local config_obj = _uci_real:get_all(pure_config)

        local section_tmp = "MAC"..(mac:gsub(":","_"))
        if config_obj[section_tmp] == nil then
            _uci_real:set(pure_config, section_tmp, "device")
        end

        local mac_tmp = mac:gsub(":","_")
         _uci_real:set(pure_config, section_tmp, "mac", mac_tmp)

        if internet then _uci_real:set(pure_config, section_tmp, "internet", tostring(internet))   end
        if lan then _uci_real:set(pure_config, section_tmp, "lan", tostring(lan))    end
        if speedlvl then _uci_real:set(pure_config, section_tmp, "speedlvl", tostring(speedlvl))    end
        if limitup then _uci_real:set(pure_config, section_tmp, "limitup", tostring(limitup))    end
        if limitdown then _uci_real:set(pure_config, section_tmp, "limitdown", tostring(limitdown))   end
        if nickname then _uci_real:set(pure_config, section_tmp, "nickname", tostring(nickname))    end
        if notify then _uci_real:set(pure_config, section_tmp, "notify", tostring(notify))    end
        if disk then _uci_real:set(pure_config, section_tmp, "disk", tostring(disk))    end


        local qos_dev = {}
        qos_dev[1] = {}
        qos_dev[1]["mac"] = mac:gsub(":","_")
        qos_dev[1]["speedlvl"] = speedlvl
        qos_dev[1]["limitup"] = limitup
        qos_dev[1]["limitdown"] = limitdown

        qos_dev[1]["internet"] = internet
        qos_dev[1]["lan"] = lan
        qos_dev[1]["nickname"] = nickname
        qos_dev[1]["notify"] = notify
        set_tab(qos_dev)


        local exist_flag = 0
        _uci_real:foreach("firewall", "rule",
            function(s)
                if s.src_mac == mac then
                    exist_flag = 1
                    s_name = s[".name"]
                end
            end
        )

    ------------------firewall---------------------------
        if exist_flag==0 then
            s_name = _uci_real:add("firewall","rule")
            _uci_real:set("firewall", s_name, "proto", "tcp udp")
            _uci_real:set("firewall", s_name, "name", "custom")
            _uci_real:set("firewall", s_name, "icmp_type", "destination-unreachable")
            _uci_real:set("firewall", s_name, "src", "*")
            local mac_tmp = mac:gsub("_",":")
            _uci_real:set("firewall", s_name, "src_mac", mac_tmp)
            if lan == "0" then
                _uci_real:delete("firewall", s_name, "dest")
                _uci_real:set("firewall", s_name, "target", "REJECT")
                local ifname = _uci_real:get("devlist", mac:gsub(":","_"), "dev")
                if ifname then luci.util.exec("iwpriv "..ifname.." set DisConnectSta="..mac:gsub("_",":")) end
            elseif lan == "1" and internet == "1" then
                _uci_real:set("firewall", s_name, "target", "ACCEPT")
            elseif lan == "1" and internet == "0" then
                _uci_real:set("firewall", s_name, "dest", "wan")
                _uci_real:set("firewall", s_name, "target", "REJECT")
            end
        else
            _uci_real:set("firewall", s_name, "proto", "tcp udp")
            _uci_real:set("firewall", s_name, "name", "custom")
            _uci_real:set("firewall", s_name, "icmp_type", "destination-unreachable")
            _uci_real:set("firewall", s_name, "src", "*")
            _uci_real:set("firewall", s_name, "dest", "wan")
            local mac_tmp = mac:gsub("_",":")
            _uci_real:set("firewall", s_name, "src_mac", mac_tmp)
            if lan == "0" then
                _uci_real:delete("firewall", s_name, "dest")
                _uci_real:set("firewall", s_name, "target", "REJECT")
                local ifname = _uci_real:get("devlist", mac:gsub(":","_"), "dev")
                if ifname then luci.util.exec("iwpriv "..ifname.." set DisConnectSta="..mac:gsub("_",":")) end
            elseif lan == "1" and internet == "1" then
                _uci_real:set("firewall", s_name, "target", "ACCEPT")
            elseif lan == "1" and internet == "0" then
                _uci_real:set("firewall", s_name, "dest", "wan")
                _uci_real:set("firewall", s_name, "target", "REJECT")
            end
        end
        _uci_real:save("firewall")
        _uci_real:commit("firewall")

 --[===[
    ---------------------qos--------------------------------------
        local net = {}
        _uci_real:foreach("devlist","device",
                function(s)
                    if s.dev and s.online=="1" and s[".name"] ~= "00_00_00_00_00_00" and s.limitdown and s.limitup and s.speedlvl then
                        if net[s.dev] == nil then
                            net[s.dev] = {}
                            net[s.dev][#net[s.dev]+1] = {}
                            net[s.dev][#net[s.dev]]["ip"] = s.ip
                            net[s.dev][#net[s.dev]]["limitdown"] = (tonumber(s.limitdown)>0) and s.limitdown or nil
                            net[s.dev][#net[s.dev]]["limitup"] = (tonumber(s.limitup)>0) and s.limitup or nil
                            net[s.dev][#net[s.dev]]["speedlvl"] = (tonumber(s.speedlvl)>0) and s.speedlvl or 2
                        else
                            net[s.dev][#net[s.dev]+1] = {}
                            net[s.dev][#net[s.dev]]["ip"] = s.ip
                            net[s.dev][#net[s.dev]]["limitdown"] = (tonumber(s.limitdown)>0) and s.limitdown or nil
                            net[s.dev][#net[s.dev]]["limitup"] = (tonumber(s.limitup)>0) and s.limitup or nil
                            net[s.dev][#net[s.dev]]["speedlvl"] = (tonumber(s.speedlvl)>0) and s.speedlvl or 2
                        end
                    end
                end
        )

        local qos = _uci_real:get_all("qos_cfg", "qos")
        local file = nil
        file = io.open("/etc/firewall.user", "w+")
        if (not qos) or qos["enable"] ~= "1" then
            file:close()
            file = io.open("/etc/firewall.user.bk", "w+")
        end

        file:write("#!/bin/sh\n")
        local iface_num = 0


    ----------------qos--------download-speed-limit-----------------------
        if qos and qos["mode"] == "2" then
            for ifname,dev_tbl in pairs(net) do
                iface_num = iface_num+1
                file:write("tc qdisc del dev %s root\n" %{ifname})
                file:write("tc qdisc add dev %s root handle %s: htb default 256\n" %{ifname, tostring(iface_num)})
                for i,dev_info in pairs(dev_tbl) do
                    if dev_info.limitdown then
                        file:write("tc class add dev %s parent %s: classid %s:%s htb rate %skbps ceil %skbps\n" %{ifname, tostring(iface_num), tostring(iface_num), tostring(i), dev_info.limitdown, dev_info.limitdown })
                        file:write("tc qdisc add dev %s parent %s:%s handle %s: sfq perturb 10\n" %{ifname, tostring(iface_num), tostring(i), tostring(iface_num)..tostring(i) })
                        file:write("tc filter add dev %s parent %s: protocol ip handle %s fw classid %s:%s\n" %{ifname, tostring(iface_num), tostring(iface_num)..tostring(i), tostring(iface_num), tostring(i)  })
                        file:write("iptables -t mangle -A POSTROUTING -d %s -j MARK --set-mark %s\n" %{dev_info.ip, tostring(iface_num)..tostring(i)})
                    end
                end
            end

        ----------------qos--------upload-speed-limit-----------------------
            local wan_ifname = _uci_real:get("network","wan","ifname")
            iface_num = 0
            file:write("tc qdisc del dev %s root\n" %{wan_ifname})
            for ifname,dev_tbl in pairs(net) do
                iface_num = iface_num+1
                file:write("tc qdisc add dev %s root handle %s: htb default 256\n" %{wan_ifname, tostring(iface_num)})
                for i,dev_info in pairs(dev_tbl) do
                    if dev_info.limitup then
                        file:write("tc class add dev %s parent %s: classid %s:%s htb rate %skbps ceil %skbps\n" %{wan_ifname, tostring(iface_num), tostring(iface_num), tostring(i), dev_info.limitup, dev_info.limitup })
                        file:write("tc qdisc add dev %s parent %s:%s handle %s: sfq perturb 10\n" %{wan_ifname, tostring(iface_num), tostring(i), tostring(iface_num)..tostring(i) })
                        file:write("tc filter add dev %s parent %s: protocol ip handle %s fw classid %s:%s\n" %{wan_ifname, tostring(iface_num), tostring(iface_num)..tostring(i), tostring(iface_num), tostring(i)  })
                        file:write("iptables -t mangle -A PREROUTING -s %s -j MARK --set-mark %s\n" %{dev_info.ip, tostring(iface_num)..tostring(i)})
                    end
                end
            end
    ----------------qos--------download-priority-----------------------
        elseif qos and qos["mode"] == "1" then
            for ifname,dev_tbl in pairs(net) do
                iface_num = iface_num+1
                file:write("tc qdisc del dev %s root\n" %{ifname})
                file:write("tc qdisc add dev %s root handle %s: htb default 256\n" %{ifname, tostring(iface_num)})
                for i,dev_info in pairs(dev_tbl) do
                    if dev_info.speedlvl then
                        file:write("tc class add dev %s parent %s: classid %s:%s htb default 256\n" %{ifname, tostring(iface_num), tostring(iface_num), tostring(i)})
                        file:write("tc qdisc add dev %s parent %s:%s handle %s: sfq perturb 10\n" %{ifname, tostring(iface_num), tostring(i), tostring(iface_num)..tostring(i) })
                        file:write("tc filter add dev %s parent %s: protocol ip prio %s handle %s fw classid %s:%s\n" %{ifname, tostring(iface_num), dev_info.speedlvl, tostring(iface_num)..tostring(i), tostring(iface_num), tostring(i)  })
                        file:write("iptables -t mangle -A POSTROUTING -d %s -j MARK --set-mark %s\n" %{dev_info.ip, tostring(iface_num)..tostring(i)})
                    end
                end
            end

        ----------------qos--------upload-priority-----------------------
            local wan_ifname = _uci_real:get("network","wan","ifname")
            iface_num = 0
            file:write("tc qdisc del dev %s root\n" %{wan_ifname})
            for ifname,dev_tbl in pairs(net) do
                iface_num = iface_num+1
                file:write("tc qdisc add dev %s root handle %s: htb default 256\n" %{wan_ifname, tostring(iface_num)})
                for i,dev_info in pairs(dev_tbl) do
                    if dev_info.speedlvl then
                        file:write("tc class add dev %s parent %s: classid %s:%s htb default 256\n" %{wan_ifname, tostring(iface_num), tostring(iface_num), tostring(i)})
                        file:write("tc qdisc add dev %s parent %s:%s handle %s: sfq perturb 10\n" %{wan_ifname, tostring(iface_num), tostring(i), tostring(iface_num)..tostring(i) })
                        file:write("tc filter add dev %s parent %s: protocol ip prio %s handle %s fw classid %s:%s\n" %{wan_ifname, tostring(iface_num), dev_info.speedlvl, tostring(iface_num)..tostring(i), tostring(iface_num), tostring(i)  })
                        file:write("iptables -t mangle -A PREROUTING -s %s -j MARK --set-mark %s\n" %{dev_info.ip, tostring(iface_num)..tostring(i)})
                    end
                end
            end
        elseif qos and qos["mode"] == "0" then

        end

        file:close()


        _uci_real:foreach("devlist","device",
            function(s)
                if s.online == "1" and s[".name"] ~= "00_00_00_00_00_00" then
                    _uci_real:set("devlist", s[".name"], "upspeed" , get_dev_trafficinfo(s.ip, s.reset_stamp).up_speed_cur)
                    _uci_real:set("devlist", s[".name"], "downspeed", get_dev_trafficinfo(s.ip, s.reset_stamp).down_speed_cur)
                    _uci_real:set("devlist", s[".name"], "uploadtotal_offset", s.uploadtotal or "0")
                    _uci_real:set("devlist", s[".name"], "downloadtotal_offset", s.downloadtotal or "0")
                    _uci_real:set("devlist", s[".name"], "reset_stamp", get_system_runtime())
                end
            end
        )
        _uci_real:save("devlist")
        _uci_real:commit("devlist")

        luci.sys.call("/etc/init.d/firewall restart")

        create_chain()
]===]

        update_qos()
        result["code"] = code
        result["msg"] = sferr.getErrorMessage(code)

    else
        result = sferr.errProtocolNotSupport()
    end

    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
    SendToLocalServer(pure_config)
end

function set_qos()

    local result = {}
    local code = 0
    local pure_config = nil
    local qos_enable = luci.http.formvalue("enable")
    local qos_mode   = luci.http.formvalue("mode")

    local protocol = checkversion()
    if(not protocol) then
        result = sferr.errProtocolNotFound()
    elseif(protocol == sysconfig.SF_PROTOCOL_VERSION_01) then

        pure_config = "basic_setting"
        basic_setting = _uci_real:get_all(pure_config)
        if basic_setting.qos == nil then
            _uci_real:set(pure_config, "qos", "setting")
        end

        if qos_enable then _uci_real:set(pure_config, "qos", "enable", qos_enable) end
        if qos_mode then _uci_real:set(pure_config, "qos", "mode", qos_mode) end

        if _uci_real:get("qos_cfg", "qos") == nil then
            _uci_real:set("qos_cfg", "qos", "function")
        end

        _uci_real:set("qos_cfg", "qos", "enable", qos_enable)
        _uci_real:set("qos_cfg", "qos", "mode", qos_mode)

        result["code"] = code
        result["msg"] = sferr.getErrorMessage(code)

    else
        result = sferr.errProtocolNotSupport()
    end

    _uci_real:save("qos_cfg")
    _uci_real:commit("qos_cfg")

    luci.http.prepare_content("application/json")
    luci.http.write_json(result)

    if pure_config then
        SendToLocalServer(pure_config)
    end
end

function set_qos_default()

    _uci_real:set("qos_cfg", "qos", "function")
    _uci_real:set("qos_cfg", "qos", "enable", "0")
    _uci_real:set("qos_cfg", "qos", "mode", "0")

    _uci_real:save("qos_cfg")
    _uci_real:commit("qos_cfg")

end

function get_qos_info()

    local result = {}
    local code = 0
    local list = {}
    local protocol = checkversion()
    if(not protocol) then
        result = sferr.errProtocolNotFound()
    elseif(protocol == sysconfig.SF_PROTOCOL_VERSION_01) then

        qos_setting = _uci_real:get_all("qos_cfg","qos")

        if qos_setting == nil then
            set_qos_default()
        end

        qos_setting = _uci_real:get_all("qos_cfg","qos")

        result["enable"] = qos_setting.enable
        result["mode"] = qos_setting.mode
--[[        result["devices"] = {}

        set_basicinfo_to_devlist()
        set_trafficinfo_to_devlist()

        _uci_real:foreach("devlist", "device",
                function(s)
                    list[#list+1] = {}
                    list[#list]["speed"]     = {}
                    list[#list]["hostname"]                   = s.hostname
                    list[#list]["nickname"]                   = s.nickname or s.hostname
                    list[#list]["mac"]                        = s.mac
                    list[#list]["ip"]                         = s.ip
                    list[#list]["speed"]["speedlvl"]          = s.speedlvl
                    list[#list]["speed"]["limitup"]           = s.limitup
                    list[#list]["speed"]["limitdown"]         = s.limitdown
                    list[#list]["speed"]["upspeed"]           = s.upspeed
                    list[#list]["speed"]["downspeed"]         = s.downspeed
                end
                )

        result["devices"] = list
]]
        result["code"] = code
        result["msg"] = sferr.getErrorMessage(code)

    else
        result = sferr.errProtocolNotSupport()
    end

    luci.http.prepare_content("application/json")
    luci.http.write_json(result)

end

function welcome()
    local userid = getfenv(1).userid
    local rv = { }
    rv["msg"] = "welcome to sf-system"
    rv["code"] = 0
    rv["userid"] = userid or ""
    luci.http.prepare_content("application/json")
    luci.http.write_json(rv)
end

function ota_check()
    local uci =require "luci.model.uci".cursor()
    local result = {}
    local code = 0
    local protocol = checkversion()
    if(not protocol) then
        result = sferr.errProtocolNotFound()
    elseif(protocol == sysconfig.SF_PROTOCOL_VERSION_01) then
        local remote_info = sysutil.getBmobInfo()
        if (remote_info) then
            result["romversion"] = sysutil.getRomVersion()
            result["romtime"]    = uci:get("siwifi","hardware","romtime")
            result["otaversion"] = remote_info["otaversion"]
            result["otatime"]    = remote_info["otatime"]
            result["size"]       = remote_info["size"]
            result["type"]       = remote_info["type"]
            result["log"]        = remote_info["log"]
            code = 0
            result["code"] = code
            result["msg"]  = sferr.getErrorMessage(code)
        else
            code = sferr.EEROR_NO_OTAVESION_NOT_DOWNLOADED
            result["code"] = code
            result["msg"]  = sferr.getErrorMessage(code)
        end
    else
        result = sferr.errProtocolNotSupport()
    end
    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
end

function ota_upgrade()
    local result = {}
    local code   = 0
	local flag = 0

    local check = luci.http.formvalue("check")
    local userid = luci.http.formvalue("userid")
    local msgid = luci.http.formvalue("msgid")
    local usersubid = luci.http.formvalue("usersubid")
    --nixio.syslog("crit", "=========check="..check.."===userid="..userid.."===msgid="..msgid.."==zhanggong=="..usersubid)
    local protocol = checkversion()
    result["status"] = 0
    result["downloaded"] = 0
    if(not protocol) then
        result = sferr.errProtocolNotFound()
    elseif(protocol == sysconfig.SF_PROTOCOL_VERSION_01) then
        if(check ~= "1") then
            local upgrade_shortest_time = 0
            local upgrade_interval = 120
            if sysutil.sane("/tmp/upgrade_shortest_time") then
                upgrade_shortest_time  = tonumber(fs.readfile("/tmp/upgrade_shortest_time"))
            else
                upgrade_shortest_time = 0
            end
            if os.time() > upgrade_shortest_time then
                luci.util.exec("rm /tmp/upgrade_status")
                upgrade_shortest_time = upgrade_interval + os.time()
                local f = nixio.open("/tmp/upgrade_shortest_time", "w", 600)
                f:writeall(upgrade_shortest_time)
                f:close()
                local remote_info = sysutil.getBmobInfo()
                if ( not remote_info ) then
                    luci.util.exec("rm /tmp/upgrade_shortest_time")
                    code = sferr.EEROR_NO_OTAVESION_NOT_DOWNLOADED
                else
					nixio.syslog("crit","+++++++++++>>>> OTA Version = "..tostring(remote_info))
                    local otaversion = remote_info["otaversion"]
                    local romversion = sysutil.getRomVersion()
                    if (otaversion ~= romversion) then
                        local info = {}
                        info["size"] = remote_info["size"]
                        info["url"] = remote_info["url"]
                        info["checksum"] = remote_info["checksum"]
                        local json_info = json.encode(info)
                        local f = nixio.open("/tmp/ota_info", "w", 600)
                        f:writeall(json_info)
                        f:close()

                        local cmd_obj = {}
                        userid = getfenv(1).userid
                        if (http.getenv("HTTP_AUTHORIZATION") ~= "") then
                            cmd_obj["userid"] = userid
                            cmd_obj["usersubid"] = usersubid
                            cmd_obj["msgid"] = msgid
                            cmd_obj["action"] = "1"
                            local cmd_str = json.encode(cmd_obj)
                            local cmd = "RSCH -data "..cmd_str
                            local cmd_ret = {}
                            sysutil.sendCommandToLocalServer(cmd, cmd_ret)
    --                        luci.util.exec("sleep 10")
                        end

                        sysutil.fork_exec("/usr/bin/otaupgrade")
                    else
                        luci.util.exec("rm /tmp/upgrade_shortest_time")
                        code = sferr.EEROR_NO_LOCALVERSION_EAQULE_OTAVERSION
                    end
                end
            else
                code = sferr.ERROR_NO_WAITTING_OTA_UPGRADE
            end
            result["code"] = code
            result["msg"]  = sferr.getErrorMessage(code)
        else
            result["code"] = 0
            if sysutil.sane("/tmp/upgrade_status") then
                local info  = json.decode( fs.readfile("/tmp/upgrade_status") )
                result["status"] = info["status"]
                result["msg"] = info["msg"]
            else
                result["status"] = 4
                result["msg"] = "ota upgrade is not running"
            end
            if sysutil.sane("/tmp/upgrade_shortest_time") then
                local ret = luci.util.exec("ls -l /tmp/firmware.img")
                local ota_image_size = 0
                if sysutil.sane("/tmp/ota_info") then
                    local info = json.decode(fs.readfile("/tmp/ota_info"))
                    ota_image_size = info["size"]
                end
                if(ota_image_size == 0) then
                    result["downloaded"] = 0
                else
                    local downloaded_size = tonumber(string.match(ret, "root%s+(%d+)")) or 0
                    local downloaded = downloaded_size/ota_image_size
                    result["downloaded"] = (downloaded - downloaded%0.01)*100
                end
            else
                result["downloaded"] = 0
            end
        end
    else
        result = sferr.errProtocolNotSupport()
    end
    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
end

function net_detect()
    local wifis = sysutil.sf_wifinetworks()
    local result = {}
    local code   = 0
    local bandwidth = {}
	bandwidth["upbandwidth"] = -1
	bandwidth["downbandwidth"] = -1

    ------function wifi_pwd_strong(pwd) to check the strong of wifi password
    local function wifi_pwd_strong(pwd)
        local matching_only_number         = pwd:match('(%d*).*')
        local matching_only_small_letter   = pwd:match('(%l*).*')
        local matching_only_capital_letter = pwd:match('(%u*).*')
        local matching_only_othertype      = pwd:match('(^[%d%l%u]*).*')
        if (matching_only_number == pwd or matching_only_small_letter == pwd or matching_only_capital_letter == pwd or matching_only_othertype == pwd) then
            return 0
        else
            return 1
        end
    end

    local function if_not_calc_bandwidth()
        return luci.http.formvalue("nobandwidth")
    end

	local function getpingstatus()
        local  pingbaidu      =  luci.util.exec("ubus call network.internet pingbaidu")
        local _pingbaidu      =   json.decode(pingbaidu)
        local  ping_status    =  _pingbaidu["result"]
		return ping_status
	end

    local protocol = checkversion()
    if(not protocol) then
        result = sferr.errProtocolNotFound()
    elseif(protocol == sysconfig.SF_PROTOCOL_VERSION_01) then
    ----- to examine whether wan interface is connected, wanlink = 1 means connected, 0 means not connected.
        local    wanstatus   =  luci.util.exec("ubus call network.internet wanstatus")
        local   _wanstatus   =  json.decode(wanstatus)
        local    wanlink     =  _wanstatus["result"]
        result["wanlink"]    =  wanlink

    ------to check the information of wifi password
        local wifi_pwd = {}
        for i, dev in pairs(wifis) do
            for n=1,#dev.networks do
                wifi_pwd[#wifi_pwd+1] = {}
                _uci_real:foreach("wireless","wifi-device",
                                  function(s)
                                    if s.type==dev.device then
                                        wifi_pwd[#wifi_pwd]["band"] = s.band
                                    end
                end)
                if(dev.networks[n].password) then
                    wifi_pwd[#wifi_pwd]["strong"]   = wifi_pwd_strong(dev.networks[n].password)
                    if(luci.sys.user.checkpasswd("root",dev.networks[n].password)) then
                        wifi_pwd[#wifi_pwd]["same"] = 1
                    else
                        wifi_pwd[#wifi_pwd]["same"] = 0
                    end
                else
                    wifi_pwd[#wifi_pwd]["strong"] = -1
                    wifi_pwd[#wifi_pwd]["same"] = -1
                end
            end
        end
        result["wifi"] = wifi_pwd

        result["memoryuse"]  =  memory_load() or 0
        result["cpuuse"]     =  cpu_load() or 0

        if( wanlink == 0 ) then
            local wanspeed       = {}
            wanspeed["upspeed"]  = 0
            wanspeed["downspeed"]= 0
            result["wanspeed"]   = wanspeed

            local ping     = {}
            ping["status"] = 0
            ping["lost"]   = 100
            result["ping"] = ping

            ------dns = 2 means dns timeout; delay = 10000 means not connect web successfully
            result["dns"]  = 2
            result["delay"]  = 10000

            if(if_not_calc_bandwidth() ~= '1') then
                bandwidth["downbandwidth"] = 0
                bandwidth["upbandwidth"]   = 0
            end
        else
            local wanspeed = {}
            wanspeed["upspeed"]        =  get_wan_speed().tx_speed_avg
            wanspeed["downspeed"]      =  get_wan_speed().rx_speed_avg
            result["wanspeed"]         =  wanspeed

            local  ping           =  {}
			ping["status"] = getpingstatus()
			if(ping["status"] == 0) then
				ping["status"] = getpingstatus()
			end
            if(ping["status"] ==1 ) then

                local ping_info       =   luci.util.exec("netdetect -p")
                local ping_total, ping_success, ping_delay = ping_info : match ('[^%d]+(%d*)[^%d]+(%d*)[^%d]+(%d*).*')
                if(ping_total and ping_success and ping_total ~= 0) then
                    local lost            =  (ping_total-ping_success)/ping_total
                    ping["lost"]          =  (lost - lost%0.01 )*100
                    result["delay"]       =  ping_delay
                else
                    ----- when value of  variable is -1 , means something went wrong in program
                    ping["lost"]          =  -1
                    result["delay"]       =  -1
                end

                if(if_not_calc_bandwidth() ~= '1') then
                    local downbandwidth_info   = luci.util.exec("netdetect -d")
                    bandwidth["downbandwidth"] = downbandwidth_info : match('[^%d]+(%d+).*')
                    if(bandwidth["downbandwidth"] == nil) then
                        bandwidth["downbandwidth"] = -1

                    end
                    local upbandwidth_info     = luci.util.exec("netdetect -u")
                    bandwidth["upbandwidth"]   = upbandwidth_info : match('[^%d]+(%d+).*')
                    if(bandwidth["upbandwidth"] == nil) then
                        bandwidth["upbandwidth"] = -1
                    end
                end

                result["dns"]     = 1
            else
                ping["lost"]     = 100
                result["delay"]  = 10000

                -----dns = 1, means dns analysis successfully, dns = 0 means failed
                local nslookup_info=luci.util.exec("nslookup www.baidu.com")
                if( string.match(nslookup_info, "www.baidu.com")  ) then
                    result["dns"]     = 1
                else
                    result["dns"]     = 0
                end
                if(if_not_calc_bandwidth() ~= '1') then
                    bandwidth["downbandwidth"] = 0
                    bandwidth["upbandwidth"]   = 0
                end
            end
            result["ping"]        =  ping
        end
        result["bandwidth"]  = bandwidth

        result["code"]       =  code
        result["msg"]        = sferr.getErrorMessage(code)
    else
        result = sferr.errProtocolNotSupport()
    end
    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
end

function set_wifi_filter()
    local code = 0
    local result = {}
    local pure_config = nil
    local push_enable = luci.http.formvalue("enable")
    local push_mode   = luci.http.formvalue("mode")

    local protocol = checkversion()
    if(not protocol) then
        result = sferr.errProtocolNotFound()
    elseif(protocol == sysconfig.SF_PROTOCOL_VERSION_01) then

        pure_config = "basic_setting"
        basic_setting = _uci_real:get_all(pure_config)
        if basic_setting.wifi_filter == nil then
            _uci_real:set(pure_config, "wifi_filter", "setting")
        end

        if push_enable then _uci_real:set(pure_config, "wifi_filter", "enable", push_enable) end
        if push_mode then _uci_real:set(pure_config, "wifi_filter", "mode", push_mode) end

        _uci_real:set("notify", "setting", "push")
        _uci_real:set("notify", "setting", "enable", push_enable or "0")
        _uci_real:set("notify", "setting", "mode", push_mode or "0")

        _uci_real:save("notify")
        _uci_real:commit("notify")
        result["code"] = code
        result["msg"] = sferr.getErrorMessage(code)
    else
        result = sferr.errProtocolNotSupport()
    end

    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
    if pure_config then
        SendToLocalServer(pure_config)
    end

end

function get_wifi_filter()

    local code = 0
    local result = {}

    local protocol = checkversion()
    if(not protocol) then
        result = sferr.errProtocolNotFound()
    elseif(protocol == sysconfig.SF_PROTOCOL_VERSION_01) then

        result["enable"] = _uci_real:get("notify", "setting", "enable") or "0"
        result["mode"] = _uci_real:get("notify", "setting", "mode") or "0"

        result["code"] = code
        result["msg"] = sferr.getErrorMessage(code)
    else
        result = sferr.errProtocolNotSupport()
    end

    luci.http.prepare_content("application/json")
    luci.http.write_json(result)

end

function upload_log()
    local uci =require "luci.model.uci".cursor()

    local function upload_file(cmd)
		local local_url = ""
		local server_url = ""
		if(CLOUD_TYPE == 2)then
			local bmob_apkkey  =uci:get("sibmob","bmobkey","apkkey")
			local bmob_restkey =uci:get("sibmob","bmobkey","restkey")
			local user_id = getfenv(1).userid or "UnbindUser"
			local_url = "/tmp/file.txt"
			server_url = "https://api.bmob.cn/1/files/"
			local upload_url = ""
			if(cmd == "logread") then
				upload_url = server_url..user_id..("syslog.txt")
			elseif(cmd == "dmesg") then
				upload_url = server_url..user_id..("klog.txt")
			elseif(cmd == "top n 1") then
				upload_url = server_url..user_id..("RouterStatus.txt")
			else
				return nil
			end
			luci.util.exec("%s >%s" %{cmd, local_url} )
			local info = luci.util.exec("curl -k -X POST -H \"X-Bmob-Application-Id: %s\" -H \"X-Bmob-REST-API-Key: %s\" -H \"Content-Type: text/plain\" --data-binary '@%s' %s" %{bmob_apkkey, bmob_restkey, local_url, upload_url})
			luci.util.exec("rm %s" %{local_url})
			local bmob_file_url = string.match(info, "url\"\:\"(.*txt)")
			if(bmob_file_url) then
				local url = "http://file.bmob.cn/"..bmob_file_url
				return url
			else
				return nil
			end
		else
			if(CLOUD_TYPE == 0)then
				server_url = "https://120.76.161.56:8090/routers/file/upload"
			else
				server_url = "https://192.168.1.12:8090/routers/file/upload"
			end
			if(cmd == "logread") then
				local_url = "/tmp/syslog.txt"
			elseif(cmd == "dmesg") then
				local_url = "/tmp/klog.txt"
			elseif(cmd == "top n 1") then
				local_url = "/tmp/RouterStatus.txt"
			else
				return nil
			end
			luci.util.exec("%s >%s" %{cmd, local_url} )
			local info = luci.util.exec("curl -k -X POST -H \"Content-Type: multipart/form-data\" -F \"file=@%s\" %s" %{local_url, server_url})
			luci.util.exec("rm %s" %{local_url})
			info = tostring(info)

			local decoder = {}
			decoder = json.decode(info);
			local result_data = decoder["data"]

			local bmob_file_url = result_data.fileId
			if(bmob_file_url) then
				local url = ""
				if(CLOUD_TYPE == 0)then
					url = "https://120.76.161.56:8090/file/download?id="..bmob_file_url
				else
					url = "https://192.168.1.12:8090/file/download?id="..bmob_file_url
				end
				return url
			else
				return nil
			end
		end
    end

    local function upload_info(userid, routerid, slogurl, klogurl, statusurl, romversion, feedback, romtype)
		local upload_info_url = ""
		local upload_info = ""
		local info = {}
		info["routerid"] = routerid
		info["romversion"] = romversion
		info["userid"] = userid
		info["romtype"] = romtype
		if(feedback) then
			info["feedback"] = feedback
		end
		if(slogurl and klogurl and statusurl) then
			info["slogurl"] = slogurl
			info["klogurl"] = klogurl
			info["statusurl"] = statusurl
		end
		if(CLOUD_TYPE == 2)then
			upload_info_url = "https://api.bmob.cn/1/classes/RouterLog"
			upload_info = json.encode(info)
			local ret_info = luci.util.exec("curl -k -X POST -H \"X-Bmob-Application-Id: %s\" -H \"X-Bmob-REST-API-Key: %s\" -H \"Content-Type: application/json\" -d \'%s\'  %s" %{bmob_apkkey, bmob_restkey, upload_info, upload_info_url})
			return(string.match(ret_info, "objectId\":\"(.*)\""))
		else
			if(CLOUD_TYPE == 0)then
				upload_info_url = "https://120.76.161.56:8090/routers/data/routerLog"
			else
				upload_info_url = "https://192.168.1.12:8090/routers/data/routerLog"
			end
			local xcloud_info = {}
			xcloud_info["method"] = "insert"
			xcloud_info["object"] = info
			upload_info = json.encode(xcloud_info)
			local ret_info = luci.util.exec("curl -k -X POST -H \"Content-Type: application/json\" -d \'%s\'  %s" %{upload_info, upload_info_url})
			nixio.syslog("crit","----------feedback-result="..ret_info)
			local ret_decoder = {}
			ret_decoder = json.decode(ret_info)
			local ret_data = ret_decoder["data"]
			return ret_data.objectId
		end

    end

    local function if_upload()
        local ret = 0
        local flag = 0
        local upload_shortest_time = 0
        if sysutil.sane("/tmp/upload_shortest_time") then
            flag = 1
            upload_shortest_time  = tonumber(fs.readfile("/tmp/upload_shortest_time"))
        end
        if(flag == 1) then
            if(os.time() - upload_shortest_time > 30) then
                ret = 1
            else
                ret = -1
            end
        else
            ret = 1
        end

        local now = os.time()
        local f = nixio.open("/tmp/upload_shortest_time", "w", 600)
        f:writeall(now)
        f:close()

        return ret
    end

    result = {}
    code = 0
    local flag_upload_file = ""
    local protocol = checkversion()
    if(not protocol) then
        result = sferr.errProtocolNotFound()
    elseif(protocol == sysconfig.SF_PROTOCOL_VERSION_01) then
        if( if_upload() == 1) then
            local rom_version = sysutil.getRomVersion()
            local rom_type = sysutil.getRomtype()
            local router_id = uci:get("siserver","bmobrouter","routerid") or "UnbindRouter"
            local nolog = luci.http.formvalue("nolog")
            local feedback_info = luci.http.formvalue("feedback")
            local slog_url = nil
            local klog_url = nil
            local status_url = nil
            if(nolog ~= "1") then
                slog_url = upload_file("logread")
                klog_url = upload_file("dmesg")
                status_url = upload_file("top n 1")
                if(not (slog_url and klog_url and status_url)) then
                    flag_upload_file = "false"
                    code = sferr.EEROR_NO_UPLOAD_FILE_FAILED
                end
            end

            if(flag_upload_file ~= "false") then
                local object_id = upload_info(user_id, router_id, slog_url, klog_url, status_url, rom_version, feedback_info, rom_type)
                if (object_id) then
                    result["object_id"] = object_id
                else
                    code = sferr.EEROR_NO_UPLOAD_INFO_FAILED
                end
            end
        else
            code = sferr.EEROR_NO_WAITING_UPLOAD_LOG
        end
        result["code"] = code
        result["msg"] = sferr.getErrorMessage(code)
    else
        result = sferr.errProtocolNotSupport()
    end

    luci.util.exec("rm /tmp/upload_shortest_time")

    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
end

function download()
    local result = {}
    local code = 0
    local protocol = checkversion()
	local info = ''
    if(not protocol) then
        result = sferr.errProtocolNotFound()
    elseif(protocol == sysconfig.SF_PROTOCOL_VERSION_01) then
        local src_url = luci.http.formvalue("src_url")
        local dst_url = "/mnt/sda1"..(luci.http.formvalue("dst_url") or "")
		nixio.syslog("warning","----------src="..src_url)
		nixio.syslog("warning","----------dst="..dst_url)
        if(src_url) then
            if(dst_url) then
                info = os.execute("wget -c -P %s %s" %{dst_url, src_url})
            else
                code = sferr.ERROR_DSTURL_LOST
            end
        else
            code = sferr.ERROR_SRCURL_LOST
        end
        result["code"] = code
        result["msg"] = sferr.getErrorMessage(code)
    else
        result = sferr.errProtocolNotSupport()
    end
    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
end

function get_zigbee_dev()
    local code = 0
    local result = {}
    local device_table = {}
    local protocol = checkversion()
    if(not protocol) then
        result = sferr.errProtocolNotFound()
    elseif(protocol == sysconfig.SF_PROTOCOL_VERSION_01) then
        local req_type = luci.http.formvalue("type")
        if req_type then
            if req_type=="1" then
               device_table = sfgw.getAllZigbeeDev()
               nixio.syslog("crit", "====device_table===="..json.encode(device_table))
            end
        else
           device_table = sfgw.getAllZigbeeDev()
           nixio.syslog("crit", "====device_table===="..json.encode(device_table))
        end
        if device_table then
            result["list"] = device_table
        else
            -----TODO-----
            --errorcode---
        end
        result["code"] = code
        result["msg"] = sferr.getErrorMessage(code)
    else
        result = sferr.errProtocolNotSupport()
    end

    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
end
--[[
function zigbee_permit_join()
    local code = 0
    local result = {}
    local duration_str = luci.http.formvalue("time")
    local duration = nil
    if duration_str then
        duration = tonumber(duration_str)
    end
    local protocol = checkversion()
    if(not protocol) then
        result = sferr.errProtocolNotFound()
    elseif(protocol == sysconfig.SF_PROTOCOL_VERSION_01) then
        if duration then
            sfgw.permitJoinZigbeeDev(duration)
            for i=0,duration do
                sfgw.getAllZigbeeDev()
            end
        else

        end
    else
        result = sferr.errProtocolNotSupport()
    end

    luci.http.prepare_content("application/json")
    luci.http.write_json(result)

end
]]

function set_zigbee_dev()
--function set_zigbee_dev_not_use()
    local code = 0
    local result = {}
    local setting_param = {}
    local setting_str = luci.http.formvalue("setting")

    local protocol = checkversion()
    if(not protocol) then
        result = sferr.errProtocolNotFound()
    elseif(protocol == sysconfig.SF_PROTOCOL_VERSION_01) then
        local setting_json = json.decode(setting_str)
        for i=1,#setting_json do
            local res = 0
            local id = nil
            local location = nil
            setting_param["data"] = setting_json[i].data
            id = setting_json[i].id
            if setting_json[i].location then setting_param.location = setting_json[i].location end
            res = sfgw.setZigbeeDev(setting_param, id)
            if res ~= 0 then break end
        end
        result["code"] = code
        result["msg"] = sferr.getErrorMessage(code)
    else
        result = sferr.errProtocolNotSupport()
    end

    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
end

function check_rule_param(rule_str)
    local res = 0
    local rule_json = json.decode(rule_str)
    local rule_cond_flag = 0
    local rule_act_flag = 0
    if type(rule_json) == "table" then
        for i,val in pairs(rule_json) do
            if rule_json[i] then
                if rule_json[i].cond then
                    rule_cond_flag = 1
                end
                if rule_json[i].act then
                    rule_act_flag = 1
                end
                if not rule_json[i].cond and not rule_json[i].act == 0 then
                    res = sferr.ERROR_GW_NO_RULE_PARAM_ILLEGAL   ------for each item, neither rule_cond nor rule_act
                end
            else
                res = sferr.ERROR_NO_GW_RULE_PARAM_ILLEGAL
            end
        end
        if not rule_cond_flag or not rule_act_flag then
            res = sferr.ERROR_NO_GW_RULE_PARAM_ILLEGAL     ------for the total rule, either rule_cond or rule_act
        end
    else
        res = sferr.ERROR_NO_GW_RULE_PARAM_ILLEGAL
    end
    if res ~= 0 then
        rule_json = nil
    end
    return res, rule_json
end

function create_zigbee_rule()
    local code = 0
    local result = {}
    local protocol = checkversion()
    if(not protocol) then
        result = sferr.errProtocolNotFound()
    elseif(protocol == sysconfig.SF_PROTOCOL_VERSION_01) then
        local rule_str = luci.http.formvalue("rule")
        local rule_name = luci.http.formvalue("name")
        local rule_json = nil
        if not rule_str then
            code = sferr.ERROR_NO_GW_NIL_PARAMETER
        else
            code, rule_json = check_rule_param(rule_str)
        end
        if code == 0 then
            local res,ruleid = sfgw.createZigbeeRule(rule_name, rule_json)
            if res ~= -1 then
                result["ruleid"] = ruleid
            else
                code = sferr.ERROR_NO_GW_CREATE_RULE_FAILED
            end
        end
        result["code"] = code
        result["msg"] = sferr.getErrorMessage(code)
    else
        result = sferr.errProtocolNotSupport()
    end

    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
end



function del_zigbee_rule()
    local code = 0
    local result = {}
    local protocol = checkversion()
    if(not protocol) then
        result = sferr.errProtocolNotFound()
    elseif(protocol == sysconfig.SF_PROTOCOL_VERSION_01) then
        local ruleid = luci.http.formvalue("ruleid")
        if not ruleid then
            code = sferr.ERROR_NO_GW_NIL_PARAMETER
        else
            sfgw.delZigbeeRule(ruleid)
        end
        result["code"] = code
        result["msg"] = sferr.getErrorMessage(code)
    else
        result = sferr.errProtocolNotSupport()
    end
    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
end


function get_zigbee_rule()
    local test_data_all = '[{"create_time": "2015-09-24T15:02:56","update_time": "2015-09-24T15:02:56","last_trigger": "2015-09-24T15:02:56","trigger_count": 6,"id": "a3412215f4d","name": "rule1","rule": [{"id": "00124b0008f24488","online": 1,"endpointid": 20,"profileid": 260,"deviceid": 1026,"manufacturerName": "Shuncom","swversion": "zigbee2007Pro","hwversion": "shuncomV1.0","type": 2001,"location": "bedroom","cond": {"event": 1}},{"id": "00124b0008f22cd7","online": 1,"endpointid": 11,"profileid": 49246,"deviceid": 528,"manufacturerName": "Shuncom","swversion": "zigbee2007Pro","hwversion": "shuncomV1.0","type": 1001,"location": "bedroom","act": {"on": 1,"bri": 99,"sat": 1,"colortemp": 499}}]}]'
    local test_data_single = '{"create_time": "2015-09-24T15:02:56","update_time": "2015-09-24T15:02:56","last_trigger": "2015-09-24T15:02:56","trigger_count": 6,"id": "a3412215f4d","name": "rule1","rule": [{"id": "00124b0008f24488","online": 1,"endpointid": 20,"profileid": 260,"deviceid": 1026,"manufacturerName": "Shuncom","swversion": "zigbee2007Pro","hwversion": "shuncomV1.0","type": 2001,"location": "bedroom","cond": {"event": 1}},{"id": "00124b0008f22cd7","online": 1,"endpointid": 11,"profileid": 49246,"deviceid": 528,"manufacturerName": "Shuncom","swversion": "zigbee2007Pro","hwversion": "shuncomV1.0","type": 1001,"location": "bedroom","act": {"on": 1,"bri": 99,"sat": 1,"colortemp": 499}}]}'
    local code = 0
    local result = {}
    local protocol = checkversion()
    if(not protocol) then
        result = sferr.errProtocolNotFound()
    elseif(protocol == sysconfig.SF_PROTOCOL_VERSION_01) then
        local req_type = luci.http.formvalue("type")
        local ruleid = luci.http.formvalue("ruleid")
        local id = luci.http.formvalue("id")
        local rule_ret = {}
        if not req_type then
            code = sferr.ERROR_NO_GW_NIL_PARAMETER
        else
            if req_type == "0" then
                -------TODO-------
                --get all rules---
                res, rule_ret = sfgw.getZigbeeAllRule()
--                res, rule_ret = sfgw.getZigbeeRuleByRuleid(nil)
--                rule_ret = json.decode(test_data_all)
--                result["list"] = rule_ret
            elseif req_type == "1" then
                if not ruleid then
                    code = sferr.ERROR_NO_GW_NIL_PARAMETER
                else
                    res, rule_ret = sfgw.getZigbeeRuleByRuleid(ruleid)
                    -------TODO------------------
                    --get the special ruleid rule----
                    --fetch the record by ruleid-----
--                    rule_ret = json.decode(test_data_single)
--                    util.update(result, rule_ret)
                end
            elseif req_type == "2" then
                if not id then
                    code = sferr.ERROR_NO_GW_NIL_PARAMETER
                else
                    res, rule_ret = sfgw.getZigbeeRuleById(id)
                end
            else
                code = sferr.ERROR_NO_GW_ILLEGAL_TYPE
            end
            result["list"] = rule_ret
            result["code"] = code
            result["msg"] = sferr.getErrorMessage(code)

        end
    else
        result = sferr.errProtocolNotSupport()
    end
    luci.http.prepare_content("application/json")
    luci.http.write_json(result)

end

function set_zigbee_rule()
    local code = 0
    local result = {}
    local protocol = checkversion()
    if(not protocol) then
        result = sferr.errProtocolNotFound()
    elseif(protocol == sysconfig.SF_PROTOCOL_VERSION_01) then
        local rule_str = luci.http.formvalue("rule")
        local rule_name = luci.http.formvalue("name")
        local ruleid = luci.http.formvalue("ruleid")
        local rule_json = nil
        if not rule_str then
            code = sferr.ERROR_NO_GW_NIL_PARAMETER
        else
            if not ruleid then
                code = sferr.ERROR_NO_GW_NIL_PARAMETER
            else
                code, rule_json = check_rule_param(rule_str)
                sfgw.setZigbeeRule(rule_name, ruleid, rule_json)
                ---------------TODO-------------
                ------update the old rule-------
                --------------------------------
            end
        end
        result["code"] = code
        result["msg"] = sferr.getErrorMessage(code)
    else
        result = sferr.errProtocolNotSupport()
    end

    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
end

function get_zigbee_event_record()
    local code = 0
    local result = {}
    local record_ret = {}
    local protocol = checkversion()
    if(not protocol) then
        result = sferr.errProtocolNotFound()
    elseif(protocol == sysconfig.SF_PROTOCOL_VERSION_01) then
        local id = luci.http.formvalue("id")
        local req_type = luci.http.formvalue("type")
        local count = luci.http.formvalue("count")
        local limit = luci.http.formvalue("limit")
        local skip = luci.http.formvalue("skip")
        if id and req_type and count then
            record_ret = sfgw.getZigbeeEventRecord(id)
        else
            code = sferr.ERROR_NO_GW_NIL_PARAMETER
        end

        result["record"] = record_ret
        result["code"] = code
        result["msg"] = sferr.getErrorMessage(code)
    else
        result = sferr.errProtocolNotSupport()
    end

    luci.http.prepare_content("application/json")
    luci.http.write_json(result)

end
--[==[
function set_zigbee_dev()
    local code = 0
    local result = {}
    local setting_param = {}
    local setting_str = luci.http.formvalue("setting")

    local protocol = checkversion()
    if(not protocol) then
        result = sferr.errProtocolNotFound()
    elseif(protocol == sysconfig.SF_PROTOCOL_VERSION_01) then
        local group = {}
        sfgw.createZigbeeGroup("luck", group)
        local setting_json = json.decode(setting_str)
        local setting_array = {}
        for i=1,#setting_json do
            local res = 0
            local id = nil
            local location = nil
            setting_param["data"] = setting_json[i].data
            id = setting_json[i].id
            setting_array[i] = sfgw.fetchSetting(setting_param, id)
        end
        local online_setting_array = {}
        for k,v in pairs(setting_array) do
            if v.endpointid ~= 0 then
                online_setting_array[#online_setting_array+1] = v
            end
        end
        sfgw.setZigbeeGroup(online_setting_array, group.id)
        sfgw.delZigbeeGroup(group.id)
        result["code"] = code
        result["msg"] = sferr.getErrorMessage(code)
    else
        result = sferr.errProtocolNotSupport()
    end

    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
end

function set_zigbee_dev()
    local code = 0
    local result = {}
    local setting_param = {}
    local setting_str = luci.http.formvalue("setting")

    local protocol = checkversion()
    if(not protocol) then
        result = sferr.errProtocolNotFound()
    elseif(protocol == sysconfig.SF_PROTOCOL_VERSION_01) then
        local group = {}
        local setting_json = json.decode(setting_str)
        local setting_array = {}
        for i=1,#setting_json do
            local res = 0
            local id = nil
            local location = nil
            setting_param["data"] = setting_json[i].data
            id = setting_json[i].id
            setting_array[i] = sfgw.fetchSetting(setting_param, id)
        end
        local online_setting_array = {}
        for k,v in pairs(setting_array) do
            if type(v) == "table" and v.endpointid ~= 0 then
                online_setting_array[#online_setting_array+1] = v
            end
        end
        if #setting_json == 1 and #online_setting_array == 1 then
            local id = nil
            local location = nil
            local setting_param = {}
            setting_param["data"] = setting_json[#setting_json].data
            id = setting_json[#setting_json].id
            if setting_json[#setting_json].location then setting_param.location = setting_json[#setting_json].location end
            sfgw.setZigbeeDev(setting_param, id)
        else
            if #online_setting_array > 0 then
                sfgw.createZigbeeGroup("luck", group)
                sfgw.setZigbeeGroup(online_setting_array, group.id)
                sfgw.delZigbeeGroup(group.id)
            else
                code = sferr.ERROR_NO_GW_DEVICE_NO_DEVICE_ONLINE
            end
        end
        result["code"] = code
        result["msg"] = sferr.getErrorMessage(code)
    else
        result = sferr.errProtocolNotSupport()
    end

    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
end

]==]

function get_zigbee_info()          ------just used in local
    local table_name = luci.http.formvalue("table")
    local code = 0
    local result = {}
    local ret = {}
    if table_name == "zigbee_device_basic" then
        ret = sfgw.getAllZigbeeBasic()
    elseif table_name == "zigbee_device_status" then
        ret = sfgw.getAllZigbeeStatus()
    elseif table_name == "zigbee_rule" then
        ret = sfgw.getAllZigbeeRule()
    elseif table_name == "rule_record" then
        ret = sfgw.getAllRuleRecord()
    elseif table_name == "device_record" then
        ret = sfgw.getAllDeviceRecord()
    end
    result["code"] = code
    result["msg"] = sferr.getErrorMessage(code)
    result["list"] = ret
    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
end

function get_zigbee_table()
    local zigbee_info = {}
    local device_basic_obj = sfgw.getAllZigbeeBasic()
    local device_status_obj = sfgw.getAllZigbeeStatus()
    local zigbee_rule_obj = sfgw.getAllZigbeeRule()
    local rule_record_obj = sfgw.getAllRuleRecord()
    local device_record_obj = sfgw.getAllDeviceRecord()
    zigbee_info_obj.zigbee_device_basic = device_basic_obj
    zigbee_info_obj.zigbee_device_status = device_status_obj
    zigbee_info_obj.zigbee_rule = zigbee_rule_obj
    zigbee_info_obj.zigbee_rule_record = rule_record_obj
    zigbee_info_obj.zigbee_device_record = device_record_obj

end

function post_zigbee_changes()
    local code = 0
    local result = {}
    local data = luci.http.formvalue("data")
    local req_type = luci.http.formvalue("type")
    local protocol = checkversion()
    if(not protocol) then
        result = sferr.errProtocolNotFound()
    elseif(protocol == sysconfig.SF_PROTOCOL_VERSION_01) then
        if req_type == "0" then               -----sync all siwifi.db
            local post_data_obj = {}
            local device_basic_obj = sfgw.getAllZigbeeBasic()
            local device_status_obj = sfgw.getAllZigbeeStatus()
            local zigbee_rule_obj = sfgw.getAllZigbeeRule()
            local rule_record_obj = sfgw.getAllRuleRecord()
            local device_record_obj = sfgw.getAllDeviceRecord()
            post_data_obj.zigbee_device_basic = device_basic_obj
            post_data_obj.zigbee_device_status = device_status_obj
            post_data_obj.zigbee_rule = zigbee_rule_obj
            post_data_obj.zigbee_rule_record = rule_record_obj
            post_data_obj.zigbee_device_record = device_record_obj
            local post_data = json.encode(post_data_obj)
            SendDataToLocalServer(post_data)
        elseif req_type == "1" then           -----just sync the data
            if data then
                local data_obj = json.decode(data)
                post_data_obj = data_obj
                local post_data = json.encode(post_data_obj)
                SendDataToLocalServer(post_data)
            else
                code = sferr.ERROR_NO_GW_NIL_DATA
            end
        end
        result["code"] = code
        result["msg"] = sferr.getErrorMessage(code)
    else
        result = sferr.errProtocolNotSupport()
    end

    luci.http.prepare_content("application/json")
    luci.http.write_json(result)
end
