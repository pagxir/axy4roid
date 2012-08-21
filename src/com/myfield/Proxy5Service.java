package com.myfield;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.util.Log;
import android.widget.Toast;

public class Proxy5Service extends Service {
	static final String TAG = "PROXY5";

	@Override
	public IBinder onBind(Intent arg0) {
		Toast.makeText(this , "Hello World", Toast.LENGTH_SHORT);
		return null;
	}
	
	@Override
	public void onCreate() {
		Log.i(TAG, "onCreate");
		super.onCreate();
	}

	@Override
	public void onDestroy() {
		Log.i(TAG, "onDestroy");
		super.onDestroy();
	}

	@Override
	public void onStart(Intent intent, int startId) {
		Log.i(TAG, "onStart");
		super.onStart(intent, startId);
	}
}

