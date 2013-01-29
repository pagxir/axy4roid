package com.ovenstone.axy4roid;

import android.util.Log;
import android.util.Base64;
import android.os.Bundle;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;
import android.preference.PreferenceActivity;

public class Proxy5Settings extends PreferenceActivity {
	static final String LOG_TAG ="Proxy5Settings";

	SharedPreferences prefs = null;

	/** Called when the activity is first created. */
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		addPreferencesFromResource(R.xml.settings);
	}

	@Override
	public void onDestroy() {
		super.onDestroy();
	}

	public static int apply(Context context) {
		int val;
		String port;
		String user;
		String password;
		SharedPreferences pref;

		val = 1800;
		pref = PreferenceManager.getDefaultSharedPreferences(context);
		port = pref.getString("port", "1800");
		user = pref.getString("user", "proxy");
		password = pref.getString("password", "AdZnGWTM0dLT");

		try {
			val = Integer.valueOf(port);
			if (val >= 0 && val <= 65535)
				AppFace.setPort(val);
		} catch (NumberFormatException e) {
			e.printStackTrace();
		}

		if (!isEnabled(context, "enable_authorization")) {
			AppFace.setHTTPAuthorization("");
			AppFace.setSocks5UserPassword("");
			Log.v(LOG_TAG, "enable anonymous");
		} else {
			int ul, pl;
			StringBuilder sb;
			ul = user.length();
			pl = password.length();
			sb = new StringBuilder(ul + pl + 2);
			sb.append((char)ul);
			sb.append(user);
			sb.append((char)pl);
			sb.append(password);
			Log.v(LOG_TAG, "sock_auth: " + sb.toString());
			AppFace.setSocks5UserPassword(sb.toString());

			String info = user + ":" + password;
			info = Base64.encodeToString(info.getBytes(), Base64.NO_WRAP);
			AppFace.setHTTPAuthorization(info);
			Log.v(LOG_TAG, "http_auth: " + info);
		}

		return val;
	}

	public static boolean isEnabled(Context context, String key) {
		return PreferenceManager.getDefaultSharedPreferences(context).getBoolean(key, false);
	}
}
