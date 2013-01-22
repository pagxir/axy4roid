LOCAL_PATH := $(call my-dir)

#include $(CLEAR_VARS)
#LOCAL_MODULE := uhttpd
#LOCAL_SRC_FILES := $(USRV_OBJ) $(OBJECTS) 
#include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE := libproxy5
LOCAL_SRC_FILES := appface.c proxy5.cpp tcp_listen.cpp tcp_socks.cpp
LOCAL_C_INCLUDES +=  $(JNI_H_INCLUDE) $(LOCAL_PATH)/../libwait/include
LOCAL_CXX_INCLUDES +=  $(JNI_H_INCLUDE) $(LOCAL_PATH)/../libwait/include
LOCAL_STATIC_LIBRARIES = libwait liblog
include $(BUILD_SHARED_LIBRARY)

