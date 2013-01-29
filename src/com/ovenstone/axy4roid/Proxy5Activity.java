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
	}

	@Override
	public void onDestroy() {
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
}
