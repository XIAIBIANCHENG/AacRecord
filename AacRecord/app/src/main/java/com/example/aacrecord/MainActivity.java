package com.example.aacrecord;

import android.Manifest;
import android.app.Activity;
import android.app.Dialog;
import android.content.DialogInterface;
import android.content.pm.PackageManager;
import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaPlayer;
import android.media.MediaRecorder;
import android.os.Build;
import android.os.Environment;
import android.support.annotation.NonNull;
import android.support.v4.app.FragmentActivity;
import android.support.v7.app.AlertDialog;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;
import java.util.ArrayList;

public class MainActivity extends AppCompatActivity {
    private final int           REQUEST_PREMISSION=2;

    private String filePath;
    PcmRecordThread thread;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        findViewById(R.id.startrecord).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                checkPremission();
            }
        });

        findViewById(R.id.endrecord).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                thread.stopRecord();
                AacRecord.closeRecord();
            }
        });

        findViewById(R.id.playrecord).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                MediaPlayer player  =   new MediaPlayer();

                try {
                    player.setDataSource(filePath);

                    player.prepare();

                    player.start();
                }catch (Exception e){

                }

            }
        });
    }

    private void start(){
        filePath=getSDPath()+"/outAudio.aac";
        File file = new File(filePath);
        if (file.exists())
            file.delete();
        AacRecord.initRecord(filePath);

        thread=new PcmRecordThread();
        thread.start();
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
       if(grantResults[0]==0)
           start();
    }

    public static String getSDPath() {
        String str = "";
        if (Environment.getExternalStorageState().equals("mounted"))
            str = Environment.getExternalStorageDirectory().toString();
        return str;
    }

    private  void checkPremission(){
        if (Build.VERSION.SDK_INT >= 23) {
            Activity activity =this;
            if (activity != null) {
                if (activity.checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
                    askForPermissons();
                }else{
                    start();
                }
            }
        }else{
            start();
        }
    }

    private void askForPermissons() {
        Activity activity =this;
        if (activity == null) {
            return;
        }
        ArrayList<String> permissons = new ArrayList<>();

        if (activity.checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
            permissons.add(Manifest.permission.READ_EXTERNAL_STORAGE);
            permissons.add(Manifest.permission.WRITE_EXTERNAL_STORAGE);
            permissons.add(Manifest.permission.RECORD_AUDIO);
        }
        String[] items = permissons.toArray(new String[permissons.size()]);
        activity.requestPermissions(items, REQUEST_PREMISSION);
    }


    private class PcmRecordThread extends Thread {
        private int sampleRate = 16000;
        private AudioRecord audioRecord;
        private int minBufferSize = 0;
        private boolean isRecording = false;

        public PcmRecordThread() {
            minBufferSize = AudioRecord.getMinBufferSize(sampleRate, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT);
            minBufferSize = 2048;
            audioRecord = new AudioRecord(MediaRecorder.AudioSource.MIC, sampleRate, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT, minBufferSize);

        }

        @Override
        public synchronized void start() {
            audioRecord.startRecording();
            isRecording = true;
            super.start();
        }

        @Override
        public void run() {
            while (isRecording == true) {
                byte[] bytes = new byte[minBufferSize];
                if (audioRecord == null)
                    return;
                int res = audioRecord.read(bytes, 0, minBufferSize);
                if (res > 0 && isRecording == true) {
                    AacRecord.writeAudioData(bytes);
                }
            }
        }

        public void stopRecord() {
            isRecording = false;
            audioRecord.stop();
            audioRecord.release();
            audioRecord = null;
        }
    }
}
