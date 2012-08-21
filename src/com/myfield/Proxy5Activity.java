package com.myfield;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.widget.Button;
import android.view.View;
import android.view.View.OnClickListener;

public class Proxy5Activity extends Activity implements OnClickListener {
	private static final Intent proxy5Service = new Intent("com.myfield.PROXY5");

	/** Called when the activity is first created. */
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.main);

		Button start = (Button)findViewById(R.id.start);
		start.setOnClickListener(this);

		Button stop = (Button)findViewById(R.id.stop);
		stop.setOnClickListener(this);
	}

	@Override
	public void onClick(View view) {
		switch (view.getId()) {
			case R.id.start:
				startService(proxy5Service);
				break;
				
			case R.id.stop:
				stopService(proxy5Service);
				break;
				
			default:
				break;
		}		
	}
}
