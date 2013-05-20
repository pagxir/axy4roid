package com.ovenstone.axy4roid;

import java.util.List;
import java.util.ArrayList;
import java.util.Enumeration;
import android.net.Uri;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.provider.MediaStore;
import android.database.Cursor;
import android.widget.TextView;
import android.os.Handler;
import android.os.Looper; 
import android.os.Message;

import java.io.*;
import java.nio.*;
import java.net.*;
import java.nio.channels.*;

public class SharedDialog extends Activity {
	static final String LOG_TAG ="SharedDialog";
	static final String[] mProj = { MediaStore.Images.Media.DATA };

	private Thread mbroadcastThread = null;
	/** Called when the activity is first created. */
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.dialog);

		// Get intent, action and MIME type
		Intent intent = getIntent();

		String type = intent.getType();
		String action = intent.getAction();

		mbroadcastThread = new Thread(broadcastor);
		mbroadcastThread.start();
		try {
			while (mHandler == null)
				Thread.sleep(20);
		} catch (Exception e) {
			e.printStackTrace();
		}

		if (type != null) {
			String lines = "";
			if (Intent.ACTION_SEND.equals(action)) {
				if ("text/plain".equals(type)) {
					String sharedText = intent.getStringExtra(Intent.EXTRA_TEXT);
					Log.v(LOG_TAG, "sharedText: " + sharedText);
				} else {
					Uri imageUri = (Uri) intent.getParcelableExtra(Intent.EXTRA_STREAM);
					Log.v(LOG_TAG, "imageUri: " + imageUri);
					lines += outputMediaPath(imageUri);
					lines += "\n";
				}
			} else if (Intent.ACTION_SEND_MULTIPLE.equals(action)) {
				if (type.startsWith("image/")) {
					ArrayList<Uri> imageUris = intent.getParcelableArrayListExtra(Intent.EXTRA_STREAM);
					for (Uri uri: imageUris) {
						Log.v(LOG_TAG, "imageUri: " + uri);
						lines += outputMediaPath(uri);
						lines += "\n";
					}
				}
			} else {
				Log.v(LOG_TAG, "UNKOWN Action: " + action);
			}

			if (!lines.equals("")) {
				TextView views = (TextView)findViewById(R.id.path);
				views.setText(lines);
			}
		}
	}

	private class BroadcastHandler extends Handler {
		public void handleMessage(Message msg) {
			String path;
			ByteBuffer buffer;
			DatagramChannel channel;

			switch (msg.what) {
				case EVENT_BROADCAST_PATH:
					try {
						path = (String)msg.obj;
						channel = DatagramChannel.open();
						channel.socket().setBroadcast(true);
						for (Enumeration<NetworkInterface> en = NetworkInterface
								.getNetworkInterfaces(); en.hasMoreElements();) {
							NetworkInterface intf = en.nextElement();
							List<InterfaceAddress> ifaddrs = intf.getInterfaceAddresses();
							for (InterfaceAddress ifaddr: ifaddrs) {
								InetAddress iaddr = ifaddr.getBroadcast();
								if (iaddr != null && !iaddr.isLoopbackAddress()) {
									String http_prefix = "http://" + ifaddr.getAddress().getHostAddress() + ":" + Proxy5Service.PORT + "/";
									String url_path = path.replace("file:///", http_prefix);
									buffer = ByteBuffer.wrap(url_path.getBytes());
									channel.send(buffer, new InetSocketAddress(iaddr, 8899));
									Log.v(LOG_TAG, "subnetwork address is " + iaddr);
									Log.v(LOG_TAG, "send file url " + url_path);
								}   
							}   
						}   
						channel.close();
					} catch (IOException e) {
						e.printStackTrace();
					}
					break;
			}
		}
	}

	private Handler mHandler = null;
	private final static int EVENT_BROADCAST_PATH = 0x0001;

	final private Runnable broadcastor = new Runnable() {
		public void run() {
			Looper.prepare();
			mHandler = new BroadcastHandler();
			Looper.loop();
			return;
		}
	};

	private void broadcastFilePath(String line) {
		Message msg = mHandler.obtainMessage(EVENT_BROADCAST_PATH);
		msg.obj = line;
		mHandler.sendMessage(msg);
		return;
	}

	private String outputMediaPath(Uri uri) {

		Log.v(LOG_TAG, "scheme " + uri.getScheme());
		if (uri.getScheme().equals("content")) {
			Cursor cursor = this.managedQuery(uri, mProj, null, null, null);  
			int index = cursor.getColumnIndexOrThrow(MediaStore.Images.Media.DATA);   

			if (cursor.moveToFirst()) {
				String path = cursor.getString(index);  
				Log.v(LOG_TAG, "File Path: " + path);
				broadcastFilePath(path);
				return path;
			}
		}

		broadcastFilePath(uri.toString());
		return uri.toString();
	}

	@Override
	public void onDestroy() {
		mHandler.getLooper().quit();
		try {
			mbroadcastThread.join();
		} catch (Exception e) {
			e.printStackTrace();
		}
		mHandler = null;
		super.onDestroy();
	}
}
