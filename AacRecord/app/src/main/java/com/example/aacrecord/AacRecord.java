package com.example.aacrecord;

/**
 * Created by Administrator on 2017/5/13.
 */

public class AacRecord {
    static {
        System.loadLibrary("aacrecord");
    }
    public static native int  initRecord(String outputPath);
    public static native int  writeAudioData(byte[] pcmData);
    public static native void closeRecord();
}
