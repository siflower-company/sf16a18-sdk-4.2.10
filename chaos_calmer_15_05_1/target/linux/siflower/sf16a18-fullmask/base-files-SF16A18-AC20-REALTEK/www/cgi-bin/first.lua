#! /usr/bin/lua
io.write("Status: 302 \r\n")
io.write("Location:  http://portal.siwifi.cn/cgi-bin/luci\r\n")
io.write("Content-Type: text/html;charset=UTF-8\r\n")
io.write("\r\n")
io.write("hello world\r\n")
