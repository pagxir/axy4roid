#include <stdio.h>
#include <assert.h>
#include <android/log.h>
#include <jni.h>

#define  LOG_TAG  "PROXY5-JNI"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

int loop_proxy(void);
int stop_proxy(void);
int start_proxy(void);
int proxy_setport(int port);

static void setProxyPort(JNIEnv *env, jclass clazz, jint port)
{
	LOGE("call setProxyPort");
	proxy_setport(port);
}

static void startProxy(JNIEnv *env, jclass clazz)
{
	LOGE("call startProxy");
	start_proxy();
}

static void loopProxy(JNIEnv *env, jclass clazz)
{
	LOGE("call loopProxy");
	loop_proxy();
}

static void stopProxy(JNIEnv *env, jclass clazz)
{
	LOGE("call stopProxy");
	stop_proxy();
}

//定义目标类名称
static const char *className = "com/myfield/AppFace";

//定义方法隐射关系
static JNINativeMethod methods[] = {
	{"setPort", "(I)V", (void*)setProxyPort},
	{"start", "()V", (void*)startProxy},
	{"loop", "()V", (void*)loopProxy},
	{"stop", "()V", (void*)stopProxy},
};

jint JNI_OnLoad(JavaVM* vm, void* reserved){
	//声明变量
	jclass clazz;
	JNIEnv* env = NULL;
	jint result = JNI_ERR;
	int methodsLenght;

	//获取JNI环境对象
	if ((*vm)->GetEnv(vm, (void**) &env, JNI_VERSION_1_4) != JNI_OK) {
		LOGE("ERROR: GetEnv failed\n");
		return JNI_ERR;
	}
	assert(env != NULL);

	//注册本地方法.Load 目标类
	clazz = (*env)->FindClass(env, className);
	if (clazz == NULL) {
		LOGE("Native registration unable to find class '%s'", className);
		return JNI_ERR;
	}

	//建立方法隐射关系
	//取得方法长度
	methodsLenght = sizeof(methods) / sizeof(methods[0]);
	if ((*env)->RegisterNatives(env, clazz, methods, methodsLenght) < 0) {
		LOGE("RegisterNatives failed for '%s'", className);
		return JNI_ERR;
	}
	//
	result = JNI_VERSION_1_4;
	return result;
}

//onUnLoad方法，在JNI组件被释放时调用
void JNI_OnUnload(JavaVM* vm, void* reserved){
	LOGE("call JNI_OnUnload ~~!!");
}
