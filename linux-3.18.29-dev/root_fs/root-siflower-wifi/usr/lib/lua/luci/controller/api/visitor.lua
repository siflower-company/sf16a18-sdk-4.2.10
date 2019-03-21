--[[
LuCI - Lua Configuration Interface

Description:
Offers an interface for handle app request
]]--

module("luci.controller.api.visitor", package.seeall)

local sysutil = require "luci.siwifi.sf_sysutil"
local sysconfig = require "luci.siwifi.sf_sysconfig"
local disk = require "luci.siwifi.sf_disk"
local sferr = require "luci.siwifi.sf_error"
local nixio = require "nixio"
local fs = require "nixio.fs"
local uci = require "luci.model.uci".cursor()

function index()
    local page   = node("api","visitor")
    page.target  = firstchild()
    page.title   = ("")
    page.order   = 100
    page.index = true
    entry({"api", "visitor"}, firstchild(), (""), 100)
    entry({"api", "visitor", "welcome"}, call("welcome"), nil)
    entry({"api", "visitor", "get_bindinfo"}, call("get_bindinfo"), nil)

end

function checkversion()
    return luci.http.formvalue("version")
end

function get_bindinfo()

    local code = 0
    local result = {}
    local passwdset = 0
    local pwh,pwe = luci.sys.user.getpasswd("root")
    local protocol = checkversion()
    if(not protocol) then
        result = sferr.errProtocolNotFound()
    elseif(protocol == sysconfig.SF_PROTOCOL_VERSION_01) then
        if pwh then
            passwdset = 1
        else
            passwdset = 0
        end
        result["passwdset"] = passwdset
        result["bind"] = tonumber(uci:get("siserver","bmobrouter","bind"))
        result["binderid"] = uci:get("siserver","bmobrouter","binder") or ''
        result["routerid"] = uci:get("siserver","bmobrouter","routerid") or ''

        result["code"] = code
        result["msg"]  = sferr.getErrorMessage(code)

    else
        result = sferr.errProtocolNotSupport()
    end

    luci.http.prepare_content("application/json")
    luci.http.write_json(result)

end


function welcome()
    local rv = { }
    rv["msg"] = "welcome to sf-system"
    rv["code"] = code
    luci.http.prepare_content("application/json")
    luci.http.write_json(rv)
end
