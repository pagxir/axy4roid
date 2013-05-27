package com.ovenstone.axy4roid;

import java.net.InetAddress;
import java.net.NetworkInterface;
import java.net.SocketException;
import java.util.Enumeration;
import android.net.ConnectivityManager;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.SharedPreferences;
import android.net.ConnectivityManagerProxy;
import android.os.Bundle;
import android.os.IBinder;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.TextView;
import android.util.Log;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.Menu;
import android.view.MenuItem;

import java.util.List;
import java.util.ArrayList;
import java.util.Enumeration;
import android.net.Uri;
import android.provider.MediaStore;
import android.database.Cursor;
import android.os.Handler;
import android.os.Looper; 
import android.os.Message;
import android.os.Environment;
import android.app.DownloadManager;
import android.content.BroadcastReceiver;
import android.content.IntentFilter;
import android.database.Cursor;
import android.widget.Toast;

import java.io.*;
import java.nio.*;
import java.net.*;
import java.nio.channels.*;

public class Proxy5Activity extends Activity implements OnClickListener {
	static final String LOG_TAG ="Proxy5Activity";
	private static final Intent proxy5Service = new Intent("com.ovenstone.axy4roid.PROXY5");

	boolean binded = false;
	Proxy5Service.Proxy5Controler proxy5Controler = null;

	private ServiceConnection proxy5Connection = new ServiceConnection() {

		@Override
		public void onServiceConnected(ComponentName arg0, IBinder arg1) {
			proxy5Controler = (Proxy5Service.Proxy5Controler)arg1;
			updateNetworkDisplay();
		}

		@Override
		public void onServiceDisconnected(ComponentName arg0) {
			proxy5Controler = null;
			updateNetworkDisplay();
			binded = false;
		}
	};

	/** Called when the activity is first created. */
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.main);

		Button start = (Button)findViewById(R.id.start);
		start.setOnClickListener(this);

		Button stop = (Button)findViewById(R.id.stop);
		stop.setOnClickListener(this);

		Button enableTether = (Button)findViewById(R.id.tether);
		enableTether.setOnClickListener(this);

		binded = bindService(proxy5Service, proxy5Connection, 0);

		quited = false;
		mbroadcastThread = new Thread(receiver);
		mDownloadManager = (DownloadManager) getSystemService(DOWNLOAD_SERVICE);
		mbroadcastThread.start();

		IntentFilter filter = new IntentFilter(DownloadManager.ACTION_DOWNLOAD_COMPLETE);
		filter.addAction(DownloadManager.ACTION_NOTIFICATION_CLICKED);
		registerReceiver(onNotification, filter);
	}

	@Override
	public void onDestroy() {
		try {
			quited = true;
    		unregisterReceiver(onNotification);
			mbroadcastThread.interrupt();
			mbroadcastThread.join();
		} catch (Exception e) {
			e.printStackTrace();
		}
		unbindService(proxy5Connection);
		super.onDestroy();
	}

	@Override
	public void onClick(View view) {
		switch (view.getId()) {
			case R.id.start:
				startService(proxy5Service);
				if (!binded)
					binded = bindService(proxy5Service, proxy5Connection, 0);
				updateNetworkDisplay();
				break;

			case R.id.stop:
				stopService(proxy5Service);
				updateNetworkDisplay();
				break;

			case R.id.tether:
				enableUsbTether();
				break;

			default:
				break;
		}
	}

	private void enableUsbTether() {
		int state;
		ConnectivityManagerProxy cm =
			new ConnectivityManagerProxy((ConnectivityManager)getSystemService(Context.CONNECTIVITY_SERVICE));
		state = cm.setUsbTethering(true);

		if (state == -1) {
			String[] available = cm.getTetherableIfaces();
			String[] mUsbRegexs = cm.getTetherableUsbRegexs();
			String usbIface = findIface(available, mUsbRegexs);

			state = 0;
			if (usbIface != null) {
				state = cm.tether(usbIface);
				Log.d(LOG_TAG, "tether " + state);
			}
		}

		if (state != 0) {
			Intent settings = new Intent("android.settings.WIRELESS_SETTINGS");
			Log.d(LOG_TAG, "auto enable tethering failure, switch to manmual");
			startActivity(settings);
		}
	}

	private String findIface(String[] ifaces, String[] regexes) {
		for (String iface : ifaces) {
			for (String regex : regexes) {
				if (iface.matches(regex)) {
					return iface;
				}
			}
		}
		return null;
	}

	void updateNetworkDisplay() {
		TextView tvAddress = (TextView)findViewById(R.id.networkaddress);

		if (proxy5Controler == null) {
			tvAddress.setText("service not running");
			return;
		}

		try {
			StringBuilder sb = new StringBuilder();
			for (Enumeration<NetworkInterface> en = NetworkInterface
					.getNetworkInterfaces(); en.hasMoreElements();) {
				NetworkInterface intf = en.nextElement();
				for (Enumeration<InetAddress> ipAddr = intf.getInetAddresses();
						ipAddr.hasMoreElements();) {
					InetAddress inetAddress = ipAddr.nextElement();
					if (!inetAddress.isLoopbackAddress()) {
						String address = inetAddress.getHostAddress();
						sb.append(address);
						sb.append(":");
						sb.append(String.valueOf(proxy5Controler.getPort()));
						sb.append("\n");
					}
				}
			}
			tvAddress.setText(sb.toString());
		} catch (SocketException ex) {
			tvAddress.setText("could not get network address");
			ex.printStackTrace();
		} catch (Exception e) {
			e.printStackTrace();
		}

		return;
	}

	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		menu.add(Menu.NONE, Menu.FIRST + 1, 5, "Settings").setIcon(android.R.drawable.ic_menu_edit);
		return true;
	}

	@Override
	public boolean onOptionsItemSelected(MenuItem item) {
		switch (item.getItemId()) {
			case Menu.FIRST + 1:
				Intent settings = new Intent("com.ovenstone.axy4roid.SETTINGS");
				startActivity(settings);
				break;
		}

		return false;
	}

	private boolean quited = false;
	private Thread mbroadcastThread = null;
	private DownloadManager mDownloadManager = null;

	final private Runnable receiver = new Runnable() {
		public void run() {
			String path;
			ByteBuffer buffer;
			DatagramChannel channel;

			// Start download
			try {
				channel = DatagramChannel.open();
				channel.socket().setBroadcast(true);
				channel.socket().bind(new InetSocketAddress(8899));

				buffer = ByteBuffer.allocate(8000);
				while (!quited) {
					SocketAddress peer = channel.receive(buffer);
					String url = new String(buffer.array(), 0, buffer.position());
					Uri uri = Uri.parse(url);
					Log.v(LOG_TAG, "receive: " + uri.toString());
					DownloadManager.Request r = new DownloadManager.Request(uri);
					// This put the download in the same Download dir the browser uses
					String filename = uri.getPath().replaceAll(".*/", "");
					r.setDestinationInExternalPublicDir(Environment.DIRECTORY_DOWNLOADS, filename);
					r.setAllowedNetworkTypes(DownloadManager.Request.NETWORK_WIFI | DownloadManager.Request.NETWORK_MOBILE);
					r.setAllowedOverRoaming(false);
					//r.setMimeType("application/octet-stream");
					r.setVisibleInDownloadsUi(true);
					r.setShowRunningNotification(true);
					r.setTitle(filename);
					r.setDescription(uri.getPath());
					// When downloading music and videos they will be listed in the player
					// (Seems to be available since Honeycomb only)
					//r.allowScanningByMediaScanner();
					// Notify user when download is completed
					// (Seems to be available since Honeycomb only)
					//r.setNotificationVisibility(DownloadManager.Request.VISIBILITY_VISIBLE_NOTIFY_COMPLETED);
					mDownloadManager.enqueue(r);
					buffer.position(0);
				}

				channel.close();
			} catch (IOException e) {
				e.printStackTrace();
			}
			return;
		}
	};

	public void queryStatus(int lastDownload) {
		Cursor c = mDownloadManager.query(new DownloadManager.Query().setFilterById(lastDownload));

		if (c == null) {
			Toast.makeText(this, "Download not found!", Toast.LENGTH_LONG).show();
		} else {
			c.moveToFirst();

			Log.d(getClass().getName(), "COLUMN_ID: "+
					c.getLong(c.getColumnIndex(DownloadManager.COLUMN_ID)));
			Log.d(getClass().getName(), "COLUMN_BYTES_DOWNLOADED_SO_FAR: "+
					c.getLong(c.getColumnIndex(DownloadManager.COLUMN_BYTES_DOWNLOADED_SO_FAR)));
			Log.d(getClass().getName(), "COLUMN_LAST_MODIFIED_TIMESTAMP: "+
					c.getLong(c.getColumnIndex(DownloadManager.COLUMN_LAST_MODIFIED_TIMESTAMP)));
			Log.d(getClass().getName(), "COLUMN_LOCAL_URI: "+
					c.getString(c.getColumnIndex(DownloadManager.COLUMN_LOCAL_URI)));
			Log.d(getClass().getName(), "COLUMN_STATUS: "+
					c.getInt(c.getColumnIndex(DownloadManager.COLUMN_STATUS)));
			Log.d(getClass().getName(), "COLUMN_REASON: "+
					c.getInt(c.getColumnIndex(DownloadManager.COLUMN_REASON)));

			Toast.makeText(this, statusMessage(c), Toast.LENGTH_LONG).show();
		}
	}

	private String statusMessage(Cursor c) {
		String msg = "???";

		switch(c.getInt(c.getColumnIndex(DownloadManager.COLUMN_STATUS))) {
			case DownloadManager.STATUS_FAILED:
				msg = "Download failed!";
				break;

			case DownloadManager.STATUS_PAUSED:
				msg = "Download paused!";
				break;

			case DownloadManager.STATUS_PENDING:
				msg = "Download pending!";
				break;

			case DownloadManager.STATUS_RUNNING:
				msg = "Download in progress!";
				break;

			case DownloadManager.STATUS_SUCCESSFUL:
				msg = "Download complete!";
				break;

			default:
				msg = "Download is nowhere in sight";
				break;
		}

		return(msg);
	}

	BroadcastReceiver onNotification = new BroadcastReceiver() {
		public void onReceive(Context context, Intent intent) {
			if (intent.getAction().equals(DownloadManager.ACTION_DOWNLOAD_COMPLETE)) {
				Toast.makeText(context, "Download finish!", Toast.LENGTH_LONG).show();
				startActivity(new Intent(DownloadManager.ACTION_VIEW_DOWNLOADS));
			} else {
				Toast.makeText(context, "Ummmm...hi!", Toast.LENGTH_LONG).show();
			}
		}
	};
}
