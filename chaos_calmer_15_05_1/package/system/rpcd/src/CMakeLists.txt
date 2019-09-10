cmake_minimum_required(VERSION 2.6)

INCLUDE(CheckFunctionExists)

PROJECT(rpcd C CXX)
ADD_DEFINITIONS(-Os -Wall -g3 -Wmissing-declarations)
INCLUDE_DIRECTORIES(include)

OPTION(FILE_SUPPORT "File plugin support" ON)
OPTION(IWINFO_SUPPORT "libiwinfo plugin support" ON)
OPTION(SIWIFI_P2P_API_SUPPORT "siwifi_p2p_api plugin support" ON)
OPTION(LEPTON_SUPPORT "lepton plugin support" ON)
OPTION(RPCSYS_SUPPORT "rpc-sys plugin support" ON)

SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")

IF(APPLE)
  INCLUDE_DIRECTORIES(/opt/local/include)
  LINK_DIRECTORIES(/opt/local/lib)
ENDIF()

CHECK_FUNCTION_EXISTS(getspnam HAVE_SHADOW)
IF(HAVE_SHADOW)
  ADD_DEFINITIONS(-DHAVE_SHADOW)
ENDIF()

FIND_LIBRARY(json NAMES json-c json)
FIND_LIBRARY(crypt NAMES crypt)
IF(crypt STREQUAL "crypt-NOTFOUND")
  SET(crypt "")
ENDIF()

ADD_EXECUTABLE(rpcd main.c exec.c session.c uci.c plugin.c)
TARGET_LINK_LIBRARIES(rpcd ubox ubus uci dl blobmsg_json ${json} ${crypt})

SET(PLUGINS "")

IF(FILE_SUPPORT)
  SET(PLUGINS ${PLUGINS} file_plugin)
  ADD_LIBRARY(file_plugin MODULE file.c)
  TARGET_LINK_LIBRARIES(file_plugin ubox ubus)
  SET_TARGET_PROPERTIES(file_plugin PROPERTIES OUTPUT_NAME file PREFIX "")
ENDIF()

IF(RPCSYS_SUPPORT)
  SET(PLUGINS ${PLUGINS} rpcsys_plugin)
  ADD_LIBRARY(rpcsys_plugin MODULE sys.c)
  TARGET_LINK_LIBRARIES(rpcsys_plugin ubox ubus)
  SET_TARGET_PROPERTIES(rpcsys_plugin PROPERTIES OUTPUT_NAME rpcsys PREFIX "")
ENDIF()

IF (IWINFO_SUPPORT)
  SET(PLUGINS ${PLUGINS} iwinfo_plugin)
  ADD_LIBRARY(iwinfo_plugin MODULE iwinfo.c)
  TARGET_LINK_LIBRARIES(iwinfo_plugin ubox ubus iwinfo)
  SET_TARGET_PROPERTIES(iwinfo_plugin PROPERTIES OUTPUT_NAME iwinfo PREFIX "")
ENDIF()

IF (SIWIFI_P2P_API_SUPPORT)
  SET(PLUGINS ${PLUGINS} siwifi_p2p_api_plugin)
  ADD_LIBRARY(siwifi_p2p_api_plugin MODULE siwifi_p2p.c p2papi.c p2p.cpp cJSON.c)
  TARGET_LINK_LIBRARIES(siwifi_p2p_api_plugin ubox ubus sunloginsdk pthread localhttp curl)
  SET_TARGET_PROPERTIES(siwifi_p2p_api_plugin PROPERTIES OUTPUT_NAME siwifi_p2p_api PREFIX "")
ENDIF()

IF (LEPTON_SUPPORT)
  SET(PLUGINS ${PLUGINS} lepton_plugin)
  ADD_LIBRARY(lepton_plugin MODULE lepton.c)
  TARGET_LINK_LIBRARIES(lepton_plugin ubox ubus)
  SET_TARGET_PROPERTIES(lepton_plugin PROPERTIES OUTPUT_NAME lepton PREFIX "")
ENDIF()

INSTALL(TARGETS rpcd ${PLUGINS}
	RUNTIME DESTINATION sbin
	LIBRARY DESTINATION lib
)
