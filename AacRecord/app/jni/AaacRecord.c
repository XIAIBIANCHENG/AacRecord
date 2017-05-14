#include <jni.h>
#include<stdio.h>
#include<math.h>
#include <android/log.h>
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/log.h"
#include "libavutil/mathematics.h"

#define TAG "VIDEOROLATE"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,TAG,__VA_ARGS__)
#define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG,TAG,__VA_ARGS__)

AVFormatContext *outputCtx;
AVStream        *audioStream;
AVFrame         *audioFrame;
uint8_t         *frame_buf;
AVPacket        audioPkt;

int             audioSize;


/**
 * 自定义日志输出回调接口
 */
void mylog(void* p, int v, const char* str, va_list lis){

	va_list vl2;
	char line[1024];
	static int print_prefix = 1;


	va_copy(vl2, lis);
	av_log_format_line(p, v, str, vl2, line, sizeof(line), &print_prefix);
	va_end(vl2);
	LOGE("%s",line);
}

/**
 * jstring 转换为char* 字符串
 */
void jstring2Char(JNIEnv* env, jstring inputJstring, char* outputCharstr) {
	char *str = (*env)->GetStringUTFChars(env, inputJstring, 0);
	sprintf(outputCharstr, "%s", str);
	(*env)->ReleaseStringUTFChars(env, inputJstring, str);
}
/************************************************************************/
/* 创建音频流                                                    */
/************************************************************************/
AVStream *addAudioStream(AVFormatContext *oc, enum AVCodecID codec_id){
	AVCodecContext *c;
	AVStream *st;
	AVCodec *codec;
	codec = avcodec_find_encoder(codec_id);
	if (!codec) {
		LOGE("audio codec not found\n");
		return NULL;
	}
	st = avformat_new_stream(oc, codec);
	st->id =0;
	c = st->codec;
	c->sample_fmt =AV_SAMPLE_FMT_S16;
	c->bit_rate = 64000;
	c->sample_rate = 16000;
	c->channels = av_get_channel_layout_nb_channels(c->channel_layout);
	c->channel_layout = AV_CH_LAYOUT_MONO;
	if (oc->oformat->flags & AVFMT_GLOBALHEADER)
		c->flags |= CODEC_FLAG_GLOBAL_HEADER;
	return st;
}
/************************************************************************/
/* 打开音频流                                                                     */
/************************************************************************/
int openAudioStream(AVFormatContext *oc, AVStream *st){
	AVCodecContext *c;
	AVCodec *codec;
	int nb_samples, ret;
	AVDictionary *opt = NULL;
	c = st->codec;
	codec = avcodec_find_encoder(c->codec_id);
	if (!codec) {
		LOGE("codec not found\n");
		return -1;
	}
	c = st->codec;
	ret = avcodec_open2(c, codec, &opt);
	av_dict_free(&opt);
	if (ret < 0){
		LOGE("avcodec_open2 error %d", ret);
		return -1;
	}
	return 1;
}


/**
 * 写入音频数据
 */
int Java_com_example_aacrecord_AacRecord_writeAudioData(JNIEnv* env, jobject thiz, jbyteArray pcmData){
	uint8_t* srcBuf = (uint8_t*) (*env)->GetByteArrayElements(env, pcmData, 0);
	audioFrame->data[0] = srcBuf;
	int got_frame = 0;
	int ret = avcodec_encode_audio2(audioStream->codec, &audioPkt, audioFrame, &got_frame);
	if (ret < 0){
		LOGE("encoder failed");
	}
	if (got_frame==1){
		audioPkt.stream_index = 0;
		ret = av_interleaved_write_frame(outputCtx, &audioPkt);
	}
	av_free_packet(&audioPkt);
	(*env)->ReleaseByteArrayElements(env, pcmData, srcBuf, 0);
	return 1;
}


/************************************************************************/
/* 初始化ffmpeg                                                                     */
/************************************************************************/
int Java_com_example_aacrecord_AacRecord_initRecord(JNIEnv* env, jobject thiz, jstring* outputFile){
	char chOutputFilepath[128];
	audioSize=0;
	jstring2Char(env, outputFile, chOutputFilepath);

	//注册自己的日志回调接口
	av_log_set_callback(mylog);
	av_register_all();
	avformat_alloc_output_context2(&outputCtx, NULL, NULL, chOutputFilepath);
	if (!outputCtx){
		LOGE("Could not init ffmpeg env");
		return -1;
	}

	audioStream = addAudioStream(outputCtx, AV_CODEC_ID_AAC);
	if (audioStream == NULL|| openAudioStream(outputCtx, audioStream) < 1){
		LOGE("open media failed");
		return -1;
	}
	if (avio_open(&outputCtx->pb, chOutputFilepath, AVIO_FLAG_WRITE) < 0){
		LOGE("open file failed\n");
		return -1;
	}

	audioFrame = av_frame_alloc();
	audioFrame->nb_samples = audioStream->codec->frame_size;
	audioFrame->format = audioStream->codec->sample_fmt;

	audioSize = av_samples_get_buffer_size(NULL, audioStream->codec->channels, audioStream->codec->frame_size, audioStream->codec->sample_fmt, 1);
	frame_buf = (uint8_t *)av_malloc(audioSize);
	avcodec_fill_audio_frame(audioFrame, audioStream->codec->channels, audioStream->codec->sample_fmt, (const uint8_t*)frame_buf, audioSize, 1);;
	avformat_write_header(outputCtx, NULL);
	av_new_packet(&audioPkt, audioSize);
	return 1;
}


/**
 * 用于输出编码器中剩余的AVPacket。
 */
int flush_all_encoder(AVFormatContext *fmt_ctx, unsigned int stream_index){
	int ret;
	int got_frame;
	AVPacket enc_pkt;
	if (!(fmt_ctx->streams[stream_index]->codec->codec->capabilities &
		CODEC_CAP_DELAY))
		return 0;
	while (1) {
		enc_pkt.data = NULL;
		enc_pkt.size = 0;
		av_init_packet(&enc_pkt);
		ret = avcodec_encode_audio2(fmt_ctx->streams[stream_index]->codec, &enc_pkt, NULL, &got_frame);
		av_frame_free(NULL);
		if (ret < 0)
			break;
		if (!got_frame){
			ret = 0;
			break;
		}
		LOGE("Flush %d Encoder: Succeed to encode 1 frame!\tsize:%5d\n", stream_index,enc_pkt.size);
		/* mux encoded frame */

		enc_pkt.stream_index = stream_index;
		ret = av_interleaved_write_frame(fmt_ctx, &enc_pkt);
		if (ret < 0)
			break;
	}
	return ret;
}

/************************************************************************/
/* 关闭                                                                                                                                                                            */
/************************************************************************/
void Java_com_example_aacrecord_AacRecord_closeRecord(JNIEnv* env, jobject thiz){
	flush_all_encoder(outputCtx,0);
	av_write_trailer(outputCtx);
	avcodec_close(audioStream->codec);
	avio_close(outputCtx->pb);
	avformat_free_context(outputCtx);
	av_free(frame_buf);
}
