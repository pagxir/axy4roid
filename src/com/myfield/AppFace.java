package com.myfield;

public class AppFace {
	static native void setPort(int port);
	static native void loop();
	static native void stop();

	static {
		System.loadLibrary("proxy5");
	}
}
