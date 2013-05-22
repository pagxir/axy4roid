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
import android.os.Environment;
import android.app.DownloadManager;
import android.content.BroadcastReceiver;
import android.content.IntentFilter;
import android.database.Cursor;
import android.view.View;
import android.widget.Toast;

import java.io.*;
import java.nio.*;
import java.net.*;
import java.nio.channels.*;

public class ReceiveDialog extends Activity {
	static final String LOG_TAG ="ReceiveDialog";

	private boolean quited = false;
	private Thread mbroadcastThread = null;
	private DownloadManager mDownloadManager = null;

	/** Called when the activity is first created. */
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.dialog);
		mDownloadManager = (DownloadManager) getSystemService(DOWNLOAD_SERVICE);

		quited = false;
		mbroadcastThread = new Thread(receiver);
		mbroadcastThread.start();

		IntentFilter filter = new IntentFilter(DownloadManager.ACTION_DOWNLOAD_COMPLETE);
		filter.addAction(DownloadManager.ACTION_NOTIFICATION_CLICKED);
		registerReceiver(onNotification, filter);

	}

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
		super.onDestroy();
	}

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
