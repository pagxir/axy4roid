package com.ovenstone.axy4roid;

public class AppFace {
	static native void start();
	static native void loop();
	static native void stop();
	static native void setPort(int port);
	static native void setHTTPAuthorization(String info);
	static native void setSocks5UserPassword(String info);
	static native void setHTTPAuthorizationURL(String info);

	static {
		System.loadLibrary("proxy5");
	}
}

