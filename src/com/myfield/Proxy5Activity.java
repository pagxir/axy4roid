package com.myfield;

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
import android.widget.EditText;
import android.widget.TextView;
import android.util.Log;
import android.view.View;
import android.view.View.OnClickListener;

public class Proxy5Activity extends Activity implements OnClickListener {
	static final String LOG_TAG ="Proxy5Activity";
	static final String SETTINGS_KEY = "com.myfield.SETTINGS";
	private static final Intent proxy5Service = new Intent("com.myfield.PROXY5");
	
	boolean binded = false;
	SharedPreferences prefs = null;
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
		
		Button enableTether = (Button)findViewById(R.id.enable_usb_tether);
		enableTether.setOnClickListener(this);
		
		prefs = getSharedPreferences(SETTINGS_KEY, Context.MODE_PRIVATE);
		
		EditText etPort = (EditText)findViewById(R.id.port);
		etPort.setText("" + prefs.getInt("PORT", 1800));

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
				saveUserConfig();
				startService(proxy5Service);
				if (!binded)
					binded = bindService(proxy5Service, proxy5Connection, 0);
				updateNetworkDisplay();
				break;
				
			case R.id.stop:
				stopService(proxy5Service);
				updateNetworkDisplay();
				break;
				
			case R.id.enable_usb_tether:
				enableUsbTether();
				break;
				
			default:
				break;
		}
	}
	
	private void enableUsbTether() {
		ConnectivityManagerProxy cm =
			new ConnectivityManagerProxy((ConnectivityManager)getSystemService(Context.CONNECTIVITY_SERVICE));
		String[] available = cm.getTetherableIfaces();
		String[] mUsbRegexs = cm.getTetherableUsbRegexs();
		String usbIface = findIface(available, mUsbRegexs);
		int s = cm.tether(usbIface);
		Log.d(LOG_TAG, "tether " + s);
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

	private void saveUserConfig() {
		EditText tePort = (EditText)findViewById(R.id.port);
		int port = Integer.valueOf(tePort.getText().toString());

		if (port > 0 && port < 65536)
			prefs.edit().putInt("PORT", port).commit();

		return;
	}

	void updateNetworkDisplay() {
		TextView tvAddress = (TextView)findViewById(R.id.networkaddress);

		if (proxy5Controler == null) {
			tvAddress.setText("service not running");
			return;		
		}

		try {
			for (Enumeration<NetworkInterface> en = NetworkInterface
					.getNetworkInterfaces(); en.hasMoreElements();) {
				NetworkInterface intf = en.nextElement();
				for (Enumeration<InetAddress> ipAddr = intf.getInetAddresses();
						ipAddr.hasMoreElements();) {
					InetAddress inetAddress = ipAddr.nextElement();
					if (!inetAddress.isLoopbackAddress()) {
						String address = inetAddress.getHostAddress();
						tvAddress.setText(address + ":" + proxy5Controler.getPort());
						return;
					}
				}
			}
		} catch (SocketException ex) {
			tvAddress.setText("could not get network address");
			ex.printStackTrace();
		} catch (Exception e) {
			e.printStackTrace();
		}

		return;
	}
}
