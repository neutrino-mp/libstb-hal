/*
 * (C) 2010-2013 Stefan Seyfried
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Audio / Video decoder for Raspberry pi
 */

#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include <assert.h>

#include <cstdio>
#include <cstdlib>

#include <OpenThreads/Thread>

extern "C" {
#include <bcm_host.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libavutil/mathematics.h>
//#include <ao/ao.h>
#include "codec.h"
#include "avcodec_omx.h"
}
/* ffmpeg buf 8k for audio */
#define AINBUF_SIZE 0x2000
/* ffmpeg buf 256k for video */
#define VINBUF_SIZE 0x40000

#ifdef EXTRA_BUFFER
/* my own buf 16k */
#define ADMX_BUF_SZ 0x4000
/* my own buf 512k */
#define VDMX_BUF_SZ 0x80000
#endif

#include "avdec.h"
#include "dmx_lib.h"
#include "lt_debug.h"

#define lt_debug(args...) _lt_debug(HAL_DEBUG_AUDIO, this, args)
#define lt_info(args...) _lt_info(HAL_DEBUG_AUDIO, this, args)

static AVRational omx_timebase = {1,1000000};

static struct codecs_t codecs;
static struct omx_pipeline_t omxpipe;

extern cDemux *audioDemux;
extern cDemux *videoDemux;

class Dec {
	friend class aDec;
	friend class vDec;
	public:
		Dec(bool audio);
		~Dec();
		Dec *base;
		int dmx_read(uint8_t *buf, int buf_size);
	private:
		bool dec_running;
		cDemux *dmx;
#ifdef EXTRA_BUFFER
		int bufpos;
	public:
		int bufsize;
		uint8_t *dmxbuf;
#endif
};

class aDec: public Dec, public OpenThreads::Thread
{
	public:
		aDec();
		~aDec();
		int set_volume(int vol);
	private:
		void run();
		int64_t curr_pts;
};

class vDec: public Dec, public OpenThreads::Thread
{
	public:
		vDec();
		~vDec();
	private:
		void run();
};

Dec::Dec(bool audio)
{
	lt_info("calling Dec::Dec(%d)\n", audio);
	dec_running = false;
	if (audio)
		dmx = audioDemux;
	else
		dmx = videoDemux;
#ifdef EXTRA_BUFFER
	if (audio)
		bufsize = ADMX_BUF_SZ;
	else
		bufsize = VDMX_BUF_SZ;
	dmxbuf = (uint8_t *)malloc(bufsize);
	lt_info("dmxbuf: %p, bufsize: %d this %p\n", dmxbuf, bufsize, this);
	bufpos = 0;
#endif
}

Dec::~Dec()
{
#ifdef EXTRA_BUFFER
	free(dmxbuf);
#endif
	lt_info("exiting Dec::~Dec()\n");
}

aDec::aDec() : Dec(true)
{
	lt_info("calling aDec::aDec()\n");
	start();
}

aDec::~aDec()
{
	lt_info("calling aDec::~aDec()\n");
	codec_stop(&codecs.acodec);
	dec_running = false;
	join();
	lt_info("exiting aDec::~aDec()\n");
}

vDec::vDec() : Dec(false)
{
	lt_info("calling vDec::vDec()\n");
	start();
}

vDec::~vDec()
{
	lt_info("calling vDec::~vDec()\n");
	codec_stop(&codecs.vcodec);
	dec_running = false;
	join();
	lt_info("exiting vDec::~vDec()\n");
}

AVDec::AVDec()
{
	bcm_host_init();
	av_register_all();
	if (OMX_Init() != OMX_ErrorNone) /* TODO: what now? */
		lt_info("AVDec::AVDec: OMX_Init() failed!\n");

	memset(&omxpipe, 0, sizeof(omxpipe));
	memset(&codecs,  0, sizeof(codecs));

	pthread_mutex_init(&codecs.playback_mutex, NULL);
	pthread_mutex_init(&omxpipe.omx_active_mutex, NULL);
	pthread_cond_init(&omxpipe.omx_active_cv, NULL);

	codecs.is_paused = 0;
	codecs.vcodec.acodec = &codecs.acodec;
	/* TODO: this is hard coded for h264 for now */
	codecs.vcodec.vcodectype = OMX_VIDEO_CodingAVC;
	/* TODO: hard coded audio output HDMI */
	char *output = getenv("RASPI_AUDIO");
	if (! output)
		output = (char *)"hdmi";
	vcodec_omx_init(&codecs.vcodec, &omxpipe, output);
	acodec_omx_init(&codecs.acodec, &omxpipe);

	lt_info("AVDec created\n");
}

AVDec::~AVDec()
{
	stop_video();
	stop_audio();

	if (OMX_Deinit() != OMX_ErrorNone) /* TODO: what now? */
		lt_info("AVDec::~AVDec: OMX_Deinit() failed!\n");

	lt_info("AVDec destroyed\n");
}

static vDec *vdec = NULL;
static aDec *adec = NULL;

#if 0
	int play_pcm(uint8_t *buffer, int size, int ch = -1, int srate = -1, int bits = -1, int le = -1);
	int show_picture(const char *filename);
	int pig(int x, int y, int w, int h);
#endif

int AVDec::start_video()
{
	if (vdec)
		lt_info("AVDec::start_video: vdec not NULL!\n");
	else
		vdec = new vDec();
	return (vdec != NULL);
}

int AVDec::stop_video()
{
	if (! vdec)
		lt_info("AVDec::stop_video: vdec is NULL!\n");
	else
		delete vdec;
	vdec = NULL;
	return 1;
}

int AVDec::start_audio()
{
	if (adec)
		lt_info("AVdec::start_audio: adec not NULL!\n");
	else
		adec = new aDec();
	return (adec != NULL);
}

int AVDec::stop_audio()
{
	if (! adec)
		lt_info("AVDec::stop_audio: adec is NULL!\n");
	else
		delete adec;
	adec = NULL;
	return 1;
}

int AVDec::set_volume(int vol)
{
	if (adec)
		return adec->set_volume(vol);
	return -1;
}

typedef struct {
	Dec *d;
	bool audio;
} helper_struct;

static int _read(void *thiz, uint8_t *buf, int buf_size)
{
	helper_struct *h = (helper_struct *)thiz;
	Dec *me;
	if (h->audio)
		me = (aDec *)h->d;
	else
		me = (vDec *)h->d;
	//fprintf(stderr, "_read, me %p dmxbuf %p bufsize %d buf %p n %d audio %d\n", me, me->dmxbuf, me->bufsize, buf, buf_size, h->audio);
	return me->dmx_read(buf, buf_size);
}

static inline OMX_TICKS ToOMXTime(int64_t pts)
{
	OMX_TICKS ticks;
	ticks.nLowPart = pts;
	ticks.nHighPart = pts >> 32;
	return ticks;
}

static int64_t vpts = 0;
static int64_t apts = 0;

void aDec::run()
{
	hal_set_threadname("hal:adec");
	lt_info("====================== start audio decoder thread ================================\n");
	int ret = 0;
	codec_new_channel(&codecs.acodec);
	codecs.acodec.first_packet = 1;
	codecs.acodec.is_running = 0;
	struct packet_t *packet;
	/* libavcodec & friends */
	AVCodec *codec;
	AVFormatContext *avfc = NULL;
	AVCodecContext *c = NULL;
	AVInputFormat *inp;
	AVFrame *frame;
	uint8_t *inbuf;
	AVPacket avpkt;
	char tmp[128] = "unknown";

	curr_pts = 0;
	bool retry;
 again:
	inbuf = (uint8_t *)av_malloc(AINBUF_SIZE);
	retry = false;
	av_init_packet(&avpkt);
	inp = av_find_input_format("mpegts");
	helper_struct h;
	h.d = this;
	h.audio = true;
	dec_running = true;
	AVIOContext *pIOCtx = avio_alloc_context(inbuf, AINBUF_SIZE, // internal Buffer and its size
						0,	// bWriteable (1=true,0=false)
						&h,	// user data; will be passed to our callback functions
						_read,	// read callback
						NULL,	// write callback
						NULL);	// seek callback
	avfc = avformat_alloc_context();
	avfc->pb = pIOCtx;
	avfc->iformat = inp;
	avfc->probesize = 188*5;

	if (avformat_open_input(&avfc, NULL, inp, NULL) < 0) {
		lt_info("aDec: avformat_open_input() failed.\n");
		goto out;
	}
	ret = avformat_find_stream_info(avfc, NULL);
	lt_debug("aDec: avformat_find_stream_info: %d\n", ret);
	if (avfc->nb_streams != 1)
	{
		lt_info("aDec: nb_streams: %d, should be 1!\n", avfc->nb_streams);
		goto out;
	}
	if (avfc->streams[0]->codec->codec_type != AVMEDIA_TYPE_AUDIO)
		lt_info("aDec: stream 0 no audio codec? 0x%x\n", avfc->streams[0]->codec->codec_type);

	c = avfc->streams[0]->codec;
	codec = avcodec_find_decoder(c->codec_id);
	if (!codec) {
		lt_info("aDec: Codec for codec_id 0x%08x not found\n", c->codec_id);
		goto out;
	}
	if (avcodec_open2(c, codec, NULL) < 0) {
		lt_info("aDec: avcodec_open2() failed\n");
		goto out;
	}
	frame = avcodec_alloc_frame();
	if (!frame) {
		lt_info("aDec: avcodec_alloc_frame failed\n");
		goto out2;
	}
	/* output sample rate, channels, layout could be set here if necessary */
	avcodec_string(tmp, sizeof(tmp), c, 0);
	lt_info("aDec: decoding %s\n", tmp);
	while (dec_running) {
		int gotframe = 0;
		if (av_read_frame(avfc, &avpkt) < 0) {
			lt_info("aDec: av_read_frame < 0\n");
			retry = true;
			break;
		}
		if (codec_is_running(&codecs.acodec) != 1) {
			av_free_packet(&avpkt);
			continue;
		}
		int len = avcodec_decode_audio4(c, frame, &gotframe, &avpkt);
		if (gotframe && dec_running) {
			curr_pts = (avpkt.pts);
			apts = curr_pts;
			lt_debug("aDec: pts 0x%" PRIx64 " %3f\n", curr_pts, curr_pts/90000.0);
			int data_size = av_samples_get_buffer_size(NULL, c->channels,
					frame->nb_samples, c->sample_fmt, 1);
			packet = (packet_t *)malloc(sizeof(*packet));
			packet->PTS = av_rescale_q(avpkt.pts, avfc->streams[0]->time_base, omx_timebase);
			packet->DTS = -1;
			packet->packetlength = data_size;
			packet->packet = (uint8_t *)malloc(data_size);
			memcpy(packet->packet, frame->data[0], data_size);
			packet->buf = packet->packet;  /* This is what is free()ed */
			codec_queue_add_item(&codecs.acodec, packet, MSG_PACKET);
		}
		if (len != avpkt.size)
			lt_info("aDec: decoded len: %d pkt.size: %d gotframe %d\n", len, avpkt.size, gotframe);
		av_free_packet(&avpkt);
	}
	lt_info("aDec: decoder loop exited\n");
#if LIBAVCODEC_VERSION_INT >= (54 << 16 | 28 << 8)
	avcodec_free_frame(&frame);
#else
	av_free(frame);
#endif
 out2:
	avcodec_close(c);
	c = NULL;
 out:
	if (avfc)
		avformat_close_input(&avfc);
	av_free(pIOCtx->buffer);
	av_free(pIOCtx);
	if (retry && dec_running)
		goto again;
	lt_info("======================== end audio decoder thread ================================\n");
}

int aDec::set_volume(int vol)
{
	struct packet_t *packet;
	long volume = (vol - 100) * 60;
	packet = (packet_t *)malloc(sizeof(*packet));
	packet->PTS = volume;
	packet->buf = NULL;
	codec_queue_add_item(&codecs.acodec, packet, MSG_SET_VOLUME);
	return 0;
}

void vDec::run()
{
	hal_set_threadname("hal:vdec");
	lt_info("====================== start video decoder thread ================================\n");
	int ret = 0;
	codec_new_channel(&codecs.vcodec);
	codecs.vcodec.first_packet = 1;
	codecs.vcodec.is_running = 1;

	AVFormatContext *avfc = NULL;
	AVInputFormat *inp;
	uint8_t *inbuf;
	AVPacket avpkt;
	struct packet_t *packet;

	bool retry;
 again:
	retry = false;
	av_init_packet(&avpkt);
	helper_struct h;
	h.d = this;
	h.audio = false;
	dec_running = true;
 retry_open:
	inbuf = (uint8_t *)av_malloc(VINBUF_SIZE);
	inp = av_find_input_format("mpegts");
	AVIOContext *pIOCtx = avio_alloc_context(inbuf, VINBUF_SIZE, // internal Buffer and its size
						0,	// bWriteable (1=true,0=false)
						&h,	// user data; will be passed to our callback functions
						_read,	// read callback
						NULL,	// write callback
						NULL);	// seek callback
	avfc = avformat_alloc_context();
	avfc->pb = pIOCtx;
	avfc->iformat = inp;
	avfc->probesize = 188 * 100;

	if ((ret = avformat_open_input(&avfc, NULL, inp, NULL)) < 0) {
		lt_info("vDec: Could not open input: %d ctx:%p\n", ret, avfc);
		if (dec_running)
			goto retry_open;
		goto out;
	}
#if 0
	while (avfc->nb_streams < 1)
	{
		lt_info("vDec: nb_streams %d, should be 1 => retry\n", avfc->nb_streams);
		if (av_read_frame(avfc, &avpkt) < 0)
			lt_info("vDec: av_read_frame < 0\n");
		av_free_packet(&avpkt);
		if (! dec_running)
			goto out;
	}

	/* TODO: how to recover here? */
	if (avfc->streams[0]->codec->codec_type != AVMEDIA_TYPE_VIDEO)
		lt_info("vDec: no video codec? 0x%x streams: %d\n",
				avfc->streams[0]->codec->codec_type, avfc->nb_streams);
#endif
	lt_info("vDec: decoder loop starts\n");
	while (dec_running) {
		if (av_read_frame(avfc, &avpkt) < 0) {
			lt_info("vDec: av_read_frame < 0\n");
			retry = true;
			break;
		}
		if (dec_running) {
			vpts = avpkt.pts;
			lt_debug("vDec: pts 0x%" PRIx64 " %3f\n", vpts, vpts/90000.0);
			packet = (packet_t *)malloc(sizeof(*packet));
			packet->PTS = av_rescale_q(avpkt.pts, avfc->streams[0]->time_base, omx_timebase);
			packet->DTS = av_rescale_q(avpkt.dts, avfc->streams[0]->time_base, omx_timebase);
			packet->packetlength = avpkt.size;
			packet->packet = (uint8_t *)malloc(avpkt.size);
			memcpy(packet->packet, avpkt.data, avpkt.size);
			packet->buf = packet->packet;  /* This is what is free()ed */
			codec_queue_add_item(&codecs.vcodec, packet, MSG_PACKET);
		}
		av_free_packet(&avpkt);
	}
	lt_info("vDec: decoder loop ends\n");
 out:
	if (avfc)
		avformat_close_input(&avfc);
	av_free(pIOCtx->buffer);
	av_free(pIOCtx);
	if (retry && dec_running)
		goto again;
	lt_info("======================== end video decoder thread ================================\n");
}

int Dec::dmx_read(uint8_t *buf, int buf_size)
{
#ifndef EXTRA_BUFFER
	int tmp, ret = 0, cnt = 0;
	while (++cnt < 20) {
		tmp = dmx->Read(buf + ret, buf_size - ret, 10);
		if (tmp > 0)
			ret += tmp;
		if (ret > buf_size - 512)
			break;
	}
	return ret;
#else
	int tmp = 0, ret = 0;
	if (bufpos < bufsize - 4096) {
		while (bufpos < buf_size && ++tmp < 20) { /* retry max 20 times */
			ret = dmx->Read(dmxbuf + bufpos, bufsize - bufpos, 10);
			if (ret > 0)
				bufpos += ret;
			if (! dec_running)
				break;
		}
	}
	if (bufpos == 0)
		return 0;
	//lt_info("%s buf_size %d bufpos %d th %d tmp %d\n", __func__, buf_size, bufpos, thread_started, tmp);
	if (bufpos > buf_size) {
		memcpy(buf, dmxbuf, buf_size);
		memmove(dmxbuf, dmxbuf + buf_size, bufpos - buf_size);
		bufpos -= buf_size;
		return buf_size;
	}
	memcpy(buf, dmxbuf, bufpos);
	tmp = bufpos;
	bufpos = 0;
	return tmp;
#endif
}
