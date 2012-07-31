/* //device/content/providers/media/src/com/android/providers/media/MediaScannerReceiver.java
**
** Copyright 2007, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License"); 
** you may not use this file except in compliance with the License. 
** You may obtain a copy of the License at 
**
**     http://www.apache.org/licenses/LICENSE-2.0 
**
** Unless required by applicable law or agreed to in writing, software 
** distributed under the License is distributed on an "AS IS" BASIS, 
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
** See the License for the specific language governing permissions and 
** limitations under the License.
*/

package com.android.providers.media;

import android.content.Context;
import android.content.Intent;
import android.content.BroadcastReceiver;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;

import java.io.File;


public class MediaScannerReceiver extends BroadcastReceiver
{
    private final static String TAG = "MediaScannerReceiver";

    @Override
    public void onReceive(Context context, Intent intent) {
        String action = intent.getAction();
        Uri uri = intent.getData();
        if (action.equals(Intent.ACTION_BOOT_COMPLETED)) {
            // scan internal storage
            scan(context, MediaProvider.INTERNAL_VOLUME);
            scan(context, MediaProvider.EXTERNAL_VOLUME);
        } else {
            if (uri.getScheme().equals("file")) {
                // handle intents related to external storage
                String path = uri.getPath();
                String externalStoragePath = Environment.getExternalStorageDirectory().getPath();

                Log.d(TAG, "action: " + action + " path: " + path);
                if (action.equals(Intent.ACTION_MEDIA_MOUNTED)) {
                    // scan whenever any volume is mounted
                    /* m@nufront start */
                    // scan(context, MediaProvider.EXTERNAL_VOLUME);
                    scanVolume(context, path);
                    /* m@nufront end */
                } else if (action.equals(Intent.ACTION_MEDIA_SCANNER_SCAN_FILE) &&
                        path != null && path.startsWith(externalStoragePath + "/")) {
                    scanFile(context, path);
                /* a@nufront start: update database when bad removed or unmounted storages */
                } else if (action.equals(Intent.ACTION_MEDIA_BAD_REMOVAL)
                            || action.equals(Intent.ACTION_MEDIA_UNMOUNTED)) {
                    //Log.d(TAG, "action is ACTION_MEDIA_BAD_REMOVAL:"+path);
                    updateDatabase(context, path);
                }
                /* a@nufront end */
            }
        }
    }

    private void scan(Context context, String volume) {
        Bundle args = new Bundle();
        args.putString("volume", volume);
        context.startService(
                new Intent(context, MediaScannerService.class).putExtras(args));
    }

    private void scanFile(Context context, String path) {
        Bundle args = new Bundle();
        args.putString("filepath", path);
        context.startService(
                new Intent(context, MediaScannerService.class).putExtras(args));
    }

    /* a@nufront start: update database */
    private void updateDatabase(Context context, String path) {
        Bundle args = new Bundle();
        args.putString("updatepath", path);
        context.startService(
                new Intent(context, MediaScannerService.class).putExtras(args));
    }

    /* scan the path where the volume mounted */
    private void scanVolume(Context context, String path) {
        Bundle args = new Bundle();
        args.putString("scanVolumePath", path);
        context.startService(
                new Intent(context, MediaScannerService.class).putExtras(args));
    }
    /* a@nufront end */

}

