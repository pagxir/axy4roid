package com.ovenstone.axy4roid;

import java.util.ArrayList;
import android.net.Uri;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.provider.MediaStore;
import android.database.Cursor;

public class SharedDialog extends Activity {
	static final String LOG_TAG ="SharedDialog";
	static final String[] mProj = { MediaStore.Images.Media.DATA };

	/** Called when the activity is first created. */
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		// Get intent, action and MIME type
		Intent intent = getIntent();

		String type = intent.getType();
		String action = intent.getAction();

		if (type != null) {
			if (Intent.ACTION_SEND.equals(action)) {
				if ("text/plain".equals(type)) {
					String sharedText = intent.getStringExtra(Intent.EXTRA_TEXT);
					Log.v(LOG_TAG, "sharedText: " + sharedText);
				} else {
					Uri imageUri = (Uri) intent.getParcelableExtra(Intent.EXTRA_STREAM);
					Log.v(LOG_TAG, "imageUri: " + imageUri);
					outputMediaPath(imageUri);
				}
			} else if (Intent.ACTION_SEND_MULTIPLE.equals(action)) {
				if (type.startsWith("image/")) {
					ArrayList<Uri> imageUris = intent.getParcelableArrayListExtra(Intent.EXTRA_STREAM);
					for (Uri uri: imageUris) {
						Log.v(LOG_TAG, "imageUri: " + uri);
						outputMediaPath(uri);
					}
				}
			} else {
				Log.v(LOG_TAG, "UNKOWN Action: " + action);
			}
		}
	}

	private void outputMediaPath(Uri uri) {

		Log.v(LOG_TAG, "scheme " + uri.getScheme());
		if (uri.getScheme().equals("content")) {
			Cursor cursor = this.managedQuery(uri, mProj, null, null, null);  
			int index = cursor.getColumnIndexOrThrow(MediaStore.Images.Media.DATA);   

			if (cursor.moveToFirst()) {
				String path = cursor.getString(index);  
				Log.v(LOG_TAG, "File Path: " + path);
			}
		}

		return;
	}

	@Override
	public void onDestroy() {
		super.onDestroy();
	}
}
