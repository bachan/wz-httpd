PROJECT(wz-httpd)

CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

#SET(CMAKE_C_FLAGS "-O3 -Wall -fomit-frame-pointer -funroll-loops -pedantic -Wno-long-long -Wno-variadic-macros -fPIC")
#SET(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}")

IF(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS)
  SET(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS)
ENDIF(CMAKE_SYSTEM_NAME STREQUAL "Linux")

FIND_PATH(EXPAT_INCLUDE_DIR expat.h /usr/include/ /usr/local/include)
INCLUDE_DIRECTORIES(include ./ ${EXPAT_INCLUDE_DIR})

FIND_PATH(CODA_INCLUDE_DIR coda/coda.h)
INCLUDE_DIRECTORIES(${CODA_INCLUDE_DIR})

IF(CMAKE_SYSTEM_NAME STREQUAL Linux)
  ADD_DEFINITIONS(-DMODEL_LINUX_EPOLL)
ELSE(CMAKE_SYSTEM_NAME STREQUAL Linux)
  ADD_DEFINITIONS(-DMODEL_FREEBSD_KQUEUE)
ENDIF(CMAKE_SYSTEM_NAME STREQUAL Linux)

ADD_EXECUTABLE(wz-httpd src/listener.cpp src/plugins.cpp src/server_status.cpp src/talker.cpp src/wz-httpd.cpp)

FIND_LIBRARY(CODA_LIBRARY NAMES coda PATHS /usr/lib /usr/local/lib PATHS /usr/lib64 /usr/local/lib64)
FIND_LIBRARY(EXPAT_LIBRARY NAMES expat PATHS /usr/lib /usr/local/lib)

TARGET_LINK_LIBRARIES(wz-httpd ${CODA_LIBRARY} ${EXPAT_LIBRARY} pthread)

IF(CMAKE_SYSTEM_NAME STREQUAL Linux)
	TARGET_LINK_LIBRARIES(wz-httpd dl)
ENDIF(CMAKE_SYSTEM_NAME STREQUAL Linux)

INSTALL(TARGETS wz-httpd DESTINATION bin)
INSTALL(FILES include/wz_handler.h DESTINATION include)
INSTALL(FILES conf/wzconfig.xml.example DESTINATION etc)

