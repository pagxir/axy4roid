package android.net;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class ConnectivityManagerProxy {
	ConnectivityManager mConnectivityManager;
	public static final String ACTION_TETHER_STATE_CHANGED =
		"android.net.conn.TETHER_STATE_CHANGED";

	public static final String ACTION_USB_TETHER =
		"android.intent.action.USB_TETHER";

	public static final String EXTRA_AVAILABLE_TETHER = "availableArray";
	public static final String EXTRA_ACTIVE_TETHER = "activeArray";
	public static final String EXTRA_ERRORED_TETHER = "erroredArray";

	public ConnectivityManagerProxy(ConnectivityManager cm) {
		mConnectivityManager = cm;
	}

	public String[] getTetherableIfaces() {
		Method getIfaces;
		String[] ifaces = null;
		Class<ConnectivityManager> connectivityManagerClass;

		connectivityManagerClass = ConnectivityManager.class;

		try {
			getIfaces = connectivityManagerClass.getDeclaredMethod("getTetherableIfaces");
			ifaces = (String[])getIfaces.invoke(mConnectivityManager);
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

		return ifaces;		
	}

	public String[] getTetheringErroredIfaces() {
		Method getIfaces;
		String[] ifaces = null;
		Class<ConnectivityManager> connectivityManagerClass;

		connectivityManagerClass = ConnectivityManager.class;

		try {
			getIfaces = connectivityManagerClass.getDeclaredMethod("getTetheringErroredIfaces");
			ifaces = (String[])getIfaces.invoke(mConnectivityManager);
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

		return ifaces;		
	}

	public String[] getTetheredIfaces() {
		Method getIfaces;
		String[] ifaces = null;
		Class<ConnectivityManager> connectivityManagerClass;

		connectivityManagerClass = ConnectivityManager.class;

		try {
			getIfaces = connectivityManagerClass.getDeclaredMethod("getTetheredIfaces");
			ifaces = (String[])getIfaces.invoke(mConnectivityManager);
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

		return ifaces;		
	}

	public String[] getTetherableUsbRegexs() {		
		Method getUsbRegexs;
		String[] UsbRegexs = null;
		Class<ConnectivityManager> connectivityManagerClass;

		connectivityManagerClass = ConnectivityManager.class;

		try {
			getUsbRegexs = connectivityManagerClass.getDeclaredMethod("getTetherableUsbRegexs");
			UsbRegexs = (String[])getUsbRegexs.invoke(mConnectivityManager);
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

		return UsbRegexs;
	}

	public int tether(String name) {
		int state = 0;
		Method mtether;
		Class<ConnectivityManager> connectivityManagerClass;

		connectivityManagerClass = ConnectivityManager.class;

		try {
			mtether = connectivityManagerClass.getDeclaredMethod("tether", String.class);
			state = (Integer)mtether.invoke(mConnectivityManager, name);
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

		return state;		
	}

	public int untether(String name) {
		int state = 0;
		Method muntether;
		Class<ConnectivityManager> connectivityManagerClass;

		connectivityManagerClass = ConnectivityManager.class;

		try {
			muntether = connectivityManagerClass.getDeclaredMethod("untether", String.class);
			state = (Integer)muntether.invoke(mConnectivityManager, name);
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

		return state;		
	}

	/*
	 * ArrayList<String> available = intent.getStringArrayListExtra(
	 * ConnectivityManager.EXTRA_AVAILABLE_TETHER);
	 * ArrayList<String> active = intent.getStringArrayListExtra(
	 * ConnectivityManager.EXTRA_ACTIVE_TETHER);
	 * ArrayList<String> errored = intent.getStringArrayListExtra(
	 * ConnectivityManager.EXTRA_ERRORED_TETHER);
	 */
}
