LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(call all-subdir-java-files)

LOCAL_PACKAGE_NAME := Axy4roid
LOCAL_JNI_SHARED_LIBRARIES := libproxy5
LOCAL_STATIC_JAVA_LIBRARIES := youmisdk

include $(BUILD_PACKAGE)

include $(CLEAR_VARS)
LOCAL_PREBUILT_STATIC_JAVA_LIBRARIES := youmisdk:libs/YoumiSdk_v4.08_2014-04-16.jar
include $(BUILD_MULTI_PREBUILT)

include $(CLEAR_VARS)
include $(call all-makefiles-under,$(LOCAL_PATH))

