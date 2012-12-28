package com.myfield;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.ConnectivityManager;
import android.net.ConnectivityManagerProxy;
import android.util.Log;

public class USBPlugReceiver extends BroadcastReceiver {
	static final String LOG_TAG = "USBPlugReceiver";

	@Override
	public void onReceive(Context context, Intent intent) {

		boolean autoUSB, autoTether;
		SharedPreferences prefs;
		String action = intent.getAction();
		prefs = context.getSharedPreferences(Proxy5Activity.SETTINGS_KEY, Context.MODE_PRIVATE);
		autoUSB = prefs.getBoolean("AutoUSB", false);
		autoTether = prefs.getBoolean("AutoTether", false);

		if (action.equals(Intent.ACTION_UMS_CONNECTED)) {
			Log.d(LOG_TAG, "onReceive " + action);
		} else if (action.equals(Intent.ACTION_UMS_DISCONNECTED)) {
			Log.d(LOG_TAG, "onReceive " + action);
		}

		if (autoUSB == false) {
			Log.d(LOG_TAG, "Auto USB not enable");
		} else if (action.equals(Intent.ACTION_POWER_CONNECTED)) {
			Intent proxy5Service = new Intent("com.myfield.PROXY5");
			context.startService(proxy5Service);
		} else if (action.equals(Intent.ACTION_POWER_DISCONNECTED)) {
			Intent proxy5Service = new Intent("com.myfield.PROXY5");
			context.stopService(proxy5Service);
		}

		if ((autoUSB || autoTether) && action.equals(Intent.ACTION_POWER_CONNECTED)) {
			int state;
			ConnectivityManager cm =
					(ConnectivityManager)context.getSystemService(Context.CONNECTIVITY_SERVICE);
			ConnectivityManagerProxy wrapper =	new ConnectivityManagerProxy(cm);

			String[] available = wrapper.getTetherableIfaces();
			String[] mUsbRegexs = wrapper.getTetherableUsbRegexs();
			String usbIface = findIface(available, mUsbRegexs);

			state = wrapper.tether(usbIface);
			Log.d(LOG_TAG, "USB state " + state + " " + usbIface);
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
}