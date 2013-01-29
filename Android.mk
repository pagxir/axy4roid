LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(call all-subdir-java-files)

LOCAL_PACKAGE_NAME := Axy4roid
LOCAL_JNI_SHARED_LIBRARIES := libproxy5

include $(BUILD_PACKAGE)

include $(CLEAR_VARS)
include $(call all-makefiles-under,$(LOCAL_PATH))


