package com.myfield;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Binder;
import android.os.IBinder;
import android.util.Log;
import android.widget.Toast;

public class Proxy5Service extends Service implements Runnable {
	private int PORT = 1800;
	private Thread worker = null;
	private boolean exited = true;
	private boolean running = false;
	
	static final String TAG = "PROXY5";
	static final String SETTINGS_KEY = "com.myfield.SETTINGS";

	private Proxy5Controler proxy5Controler = new Proxy5Controler();
	public class Proxy5Controler extends Binder implements IProxy5Control {
		@Override
		public int getPort() {
			return PORT;
		}
	}

	@Override
	public IBinder onBind(Intent arg0) {
		Toast.makeText(this , "Hello World", Toast.LENGTH_SHORT);
		return proxy5Controler;
	}
	
	@Override
	public void onCreate() {
		Log.i(TAG, "onCreate");
		super.onCreate();
		
		SharedPreferences prefs = getSharedPreferences(SETTINGS_KEY, Context.MODE_PRIVATE);
		PORT = prefs.getInt("PORT", 1800);
		
		worker = new Thread(this);
		exited = false;
		running = true;
		worker.start();
	}

	@Override
	public void onDestroy() {
		Log.i(TAG, "onDestroy");
		super.onDestroy();
		
		try {
			exited = true;
			worker.join();
		} catch (InterruptedException e) {
			e.printStackTrace();
		}
		
		if (running) {
			throw new RuntimeException("worker thread should not running.");
		}
		
		worker = null;
	}

	@Override
	public void onStart(Intent intent, int startId) {
		Log.i(TAG, "onStart");
		super.onStart(intent, startId);
	}

	@Override
	public void run() {
		AppFace.setPort(PORT);
		AppFace.start();
		
		Log.i(TAG, "run prepare");
		while (!exited) {
			AppFace.loop();
		}
		
		Log.i(TAG, "run finish");
		AppFace.stop();
		running = false;
	}
}
