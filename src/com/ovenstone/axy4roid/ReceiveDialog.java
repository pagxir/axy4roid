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
			mbroadcastThread.interrupt();
			mbroadcastThread.join();
		} catch (Exception e) {
			e.printStackTrace();
		}
		super.onDestroy();
	}
}
