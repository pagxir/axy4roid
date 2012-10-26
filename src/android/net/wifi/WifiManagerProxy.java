package android.net.wifi;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;

import android.net.wifi.WifiManager;
import android.net.wifi.WifiConfiguration;

public class WifiManagerProxy {
	WifiManager mWifiManager;
	public static final String WIFI_AP_STATE_CHANGED_ACTION =
		"android.net.wifi.WIFI_AP_STATE_CHANGED";
	public static final String EXTRA_WIFI_AP_STATE = "wifi_state";

	public static final int WIFI_AP_STATE_FAILED = 4;
	public static final int WIFI_AP_STATE_ENABLED = 3;
	public static final int WIFI_AP_STATE_ENABLING = 2;
	public static final int WIFI_AP_STATE_DISABLED = 1;
	public static final int WIFI_AP_STATE_DISABLING = 0;

	private static int STAT_WIFI_AP_STATE_FAILED = stateFromName("WIFI_AP_STATE_FAILED");
	private static int STAT_WIFI_AP_STATE_ENABLED = stateFromName("WIFI_AP_STATE_ENABLED");
	private static int STAT_WIFI_AP_STATE_ENABLING = stateFromName("WIFI_AP_STATE_ENABLING");
	private static int STAT_WIFI_AP_STATE_DISABLED = stateFromName("WIFI_AP_STATE_DISABLED");
	private static int STAT_WIFI_AP_STATE_DISABLING = stateFromName("WIFI_AP_STATE_DISABLING");

	private static int stateFromName(String name) {
		Field field;
		Class<WifiManager> wifiClass = WifiManager.class;

		try {
			field = wifiClass.getDeclaredField(name);
			return field.getInt(null);
		} catch (SecurityException e) {
			e.printStackTrace();
		} catch (NoSuchFieldException e) {
			e.printStackTrace();
		} catch (IllegalArgumentException e) {
			e.printStackTrace();
		} catch (IllegalAccessException e) {
			e.printStackTrace();
		}

		return 0;
	}

	public WifiManagerProxy(WifiManager manager) {
		mWifiManager = manager;
	}

	public WifiConfiguration getWifiApConfiguration() {
		Class<WifiManager> wifiClass;
		Method getWifiApConfig;
		WifiConfiguration wifiApConfig;

		wifiClass = WifiManager.class;
		wifiApConfig = null;

		try {
			getWifiApConfig = wifiClass.getDeclaredMethod("getWifiApConfiguration");
			wifiApConfig = (WifiConfiguration)getWifiApConfig.invoke(mWifiManager);
		} catch (SecurityException e) {
			e.printStackTrace();
		} catch (NoSuchMethodException e) {
			e.printStackTrace();
		} catch (IllegalArgumentException e) {
			e.printStackTrace();
		} catch (IllegalAccessException e) {
			e.printStackTrace();
		} catch (InvocationTargetException e) {
			e.printStackTrace();
		}

		return wifiApConfig;
	}

	public int convertApState(int state) {
		if (state == STAT_WIFI_AP_STATE_FAILED)
			return WIFI_AP_STATE_FAILED;

		if (state == STAT_WIFI_AP_STATE_ENABLED)
			return WIFI_AP_STATE_ENABLED;

		if (state == STAT_WIFI_AP_STATE_ENABLING)
			return WIFI_AP_STATE_ENABLING;

		if (state == STAT_WIFI_AP_STATE_DISABLED)
			return WIFI_AP_STATE_DISABLED;

		if (state == STAT_WIFI_AP_STATE_DISABLING)
			return WIFI_AP_STATE_DISABLING;

		return state;
	}

	public int getWifiApState() {
		int wifiState;

		Class<WifiManager> wifiClass;
		Method getApState;

		wifiState = WIFI_AP_STATE_FAILED;
		wifiClass = WifiManager.class;

		try {
			getApState =wifiClass.getDeclaredMethod("getWifiApState");
			wifiState = (Integer)getApState.invoke(mWifiManager);
		} catch (SecurityException e) {
			e.printStackTrace();
		} catch (NoSuchMethodException e) {
			e.printStackTrace();
		} catch (IllegalArgumentException e) {
			e.printStackTrace();
		} catch (IllegalAccessException e) {
			e.printStackTrace();
		} catch (InvocationTargetException e) {
			e.printStackTrace();
		}

		return wifiState;
	}

	public void setWifiApConfiguration(WifiConfiguration configuration) {
		Class<WifiManager> wifiClass;
		Method setWifiApConfig;

		wifiClass = WifiManager.class;

		try {
			setWifiApConfig = wifiClass.getDeclaredMethod("setWifiApConfiguration", WifiConfiguration.class);
			setWifiApConfig.invoke(mWifiManager, configuration);
		} catch (SecurityException e) {
			e.printStackTrace();
		} catch (NoSuchMethodException e) {
			e.printStackTrace();
		} catch (IllegalArgumentException e) {
			e.printStackTrace();
		} catch (IllegalAccessException e) {
			e.printStackTrace();
		} catch (InvocationTargetException e) {
			e.printStackTrace();
		}

		return;
	}

	public boolean setWifiApEnabled(WifiConfiguration configuration, boolean enable) {
		Class<WifiManager> wifiClass;
		Method setApEnabled;

		wifiClass = WifiManager.class;

		try {
			setApEnabled = wifiClass.getDeclaredMethod("setWifiApEnabled", WifiConfiguration.class, boolean.class);
			return (Boolean) setApEnabled.invoke(mWifiManager, configuration, enable);
		} catch (SecurityException e) {
			e.printStackTrace();
		} catch (NoSuchMethodException e) {
			e.printStackTrace();
		} catch (IllegalArgumentException e) {
			e.printStackTrace();
		} catch (IllegalAccessException e) {
			e.printStackTrace();
		} catch (InvocationTargetException e) {
			e.printStackTrace();
		}

		return false;
	}
}
