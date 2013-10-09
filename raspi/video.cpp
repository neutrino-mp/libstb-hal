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
 * along with this program; if not, write to the Free Software
 * Foundation, 51 Franklin Street, Suite 500 Boston, MA 02110-1335 USA
 *
 * cVideo dummy implementation
 */

#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#include <OpenThreads/Thread>

#include <bcm_host.h>
extern "C" {
#include "ilclient.h"
}

#include "video_lib.h"
#include "dmx_lib.h"
#include "lt_debug.h"
#define lt_debug(args...) _lt_debug(TRIPLE_DEBUG_VIDEO, this, args)
#define lt_info(args...) _lt_info(TRIPLE_DEBUG_VIDEO, this, args)
#define lt_info_c(args...) _lt_info(TRIPLE_DEBUG_VIDEO, NULL, args)

cVideo *videoDecoder = NULL;
int system_rev = 0;

extern cDemux *videoDemux;
static int dec_running = false;


class Dec: public OpenThreads::Thread
{
	public:
		Dec();
		~Dec();
	private:
		void run();
};

Dec::Dec()
{
	start();
}

Dec::~Dec()
{
	dec_running = false;
	join();
}


static Dec *dec = NULL;

cVideo::cVideo(int, void *, void *, unsigned int)
{
	lt_debug("%s\n", __func__);
	display_aspect = DISPLAY_AR_16_9;
	display_crop = DISPLAY_AR_MODE_LETTERBOX;
	v_format = VIDEO_FORMAT_MPEG2;
	bcm_host_init();
}

cVideo::~cVideo(void)
{
}

int cVideo::setAspectRatio(int vformat, int cropping)
{
	lt_info("%s(%d, %d)\n", __func__, vformat, cropping);
	return 0;
}

int cVideo::getAspectRatio(void)
{
	int ret = 0;
	return ret;
}

int cVideo::setCroppingMode(int)
{
	return 0;
}

int cVideo::Start(void *, unsigned short, unsigned short, void *)
{
	lt_debug("%s running %d >\n", __func__, thread_running);
	if (!dec) {
		dec = new Dec();
	}
	return 0;
}

int cVideo::Stop(bool)
{
	lt_debug("%s running %d >\n", __func__, thread_running);
	if (dec) {
		delete dec;
		dec = NULL;
	}
	return 0;
}

int cVideo::setBlank(int)
{
	return 1;
}

int cVideo::SetVideoSystem(int system, bool)
{
	int h;
	switch(system)
	{
		case VIDEO_STD_NTSC:
		case VIDEO_STD_480P:
			h = 480;
			break;
		case VIDEO_STD_1080I60:
		case VIDEO_STD_1080I50:
		case VIDEO_STD_1080P30:
		case VIDEO_STD_1080P24:
		case VIDEO_STD_1080P25:
		case VIDEO_STD_1080P50:
			h = 1080;
			break;
		case VIDEO_STD_720P50:
		case VIDEO_STD_720P60:
			h = 720;
			break;
		case VIDEO_STD_AUTO:
			lt_info("%s: VIDEO_STD_AUTO not implemented\n", __func__);
			// fallthrough
		case VIDEO_STD_SECAM:
		case VIDEO_STD_PAL:
		case VIDEO_STD_576P:
			h = 576;
			break;
		default:
			lt_info("%s: unhandled value %d\n", __func__, system);
			return 0;
	}
	v_std = (VIDEO_STD) system;
	output_h = h;
	return 0;
}

int cVideo::getPlayState(void)
{
	return VIDEO_PLAYING;
}

void cVideo::SetVideoMode(analog_mode_t)
{
}

void cVideo::ShowPicture(const char *fname)
{
	lt_info("%s(%s)\n", __func__, fname);
	if (access(fname, R_OK))
		return;
}

void cVideo::StopPicture()
{
}

void cVideo::Standby(unsigned int)
{
}

int cVideo::getBlank(void)
{
	return 0;
}

void cVideo::Pig(int x, int y, int w, int h, int, int)
{
	pig_x = x;
	pig_y = y;
	pig_w = w;
	pig_h = h;
}

void cVideo::getPictureInfo(int &width, int &height, int &rate)
{
	width = dec_w;
	height = dec_h;
	rate = dec_r;
}

void cVideo::SetSyncMode(AVSYNC_TYPE)
{
};

int cVideo::SetStreamType(VIDEO_FORMAT v)
{
	v_format = v;
	return 0;
}

bool cVideo::GetScreenImage(unsigned char * &data, int &xres, int &yres, bool get_video, bool get_osd, bool scale_to_video)
{
	lt_info("%s: data 0x%p xres %d yres %d vid %d osd %d scale %d\n",
		__func__, data, xres, yres, get_video, get_osd, scale_to_video);
	return false;
}

int64_t cVideo::GetPTS(void)
{
	int64_t pts = 0;
	return pts;
}

void cVideo::SetDemux(cDemux *)
{
	lt_debug("%s: not implemented yet\n", __func__);
}


void Dec::run()
{
	hal_set_threadname("hal:vdec");
	int ret = 0;
	/* write to file instead of decoding. For testing only. */
	if (getenv("DEC_OUT")) {
		lt_info_c("Dec::run %d\n", __LINE__);
		FILE *ff = fopen("/tmp/video.pes", "w");
		unsigned char buf[65536];
		dec_running = true;
		while (dec_running)
		{
			ret = videoDemux->Read(buf, 65536, 10);
			if (ret <= 0) {
				if (!dec_running)
					break;
				continue;
			}
			fwrite(buf, 1, ret, ff);
		}
		lt_info_c("Dec::run %d\n", __LINE__);
		return;
	}
	lt_info_c("Dec::run %d\n", __LINE__);

	/* this code is mostly copied from hello_pi/hello_video example */
	OMX_VIDEO_PARAM_PORTFORMATTYPE format;
	OMX_TIME_CONFIG_CLOCKSTATETYPE cstate;
	OMX_PARAM_BRCMVIDEODECODEERRORCONCEALMENTTYPE conc;
	COMPONENT_T *video_decode = NULL, *video_scheduler = NULL, *video_render = NULL, *clock = NULL;
	COMPONENT_T *list[5];
	TUNNEL_T tunnel[4];
	ILCLIENT_T *client;
	int status = 0;
	unsigned int data_len = 0;
	int packet_size = 80<<10; /* 80kB */

	memset(list, 0, sizeof(list));
	memset(tunnel, 0, sizeof(tunnel));

	if ((client = ilclient_init()) == NULL) {
		lt_info_c("Dec::run %d\n", __LINE__);
		return;
	}

	if (OMX_Init() != OMX_ErrorNone) {
		lt_info_c("Dec::run %d\n", __LINE__);
		ilclient_destroy(client);
		return;
	}
	// create video_decode
	if (ilclient_create_component(client, &video_decode, (char *)"video_decode", ILCLIENT_DISABLE_ALL_PORTS | ILCLIENT_ENABLE_INPUT_BUFFERS) != 0)
		status = -14;
	list[0] = video_decode;

	// create video_render
	if (status == 0 && ilclient_create_component(client, &video_render, (char *)"video_render", ILCLIENT_DISABLE_ALL_PORTS) != 0)
		status = -14;
	list[1] = video_render;

	// create clock
	if (status == 0 && ilclient_create_component(client, &clock, (char *)"clock", ILCLIENT_DISABLE_ALL_PORTS) != 0)
		status = -14;
	list[2] = clock;

	memset(&cstate, 0, sizeof(cstate));
	cstate.nSize = sizeof(cstate);
	cstate.nVersion.nVersion = OMX_VERSION;
	cstate.eState = OMX_TIME_ClockStateWaitingForStartTime;
	cstate.nWaitMask = 1;
	if (clock != NULL &&
	    OMX_SetParameter(ILC_GET_HANDLE(clock), OMX_IndexConfigTimeClockState, &cstate) != OMX_ErrorNone)
		status = -13;

	// create video_scheduler
	if (status == 0 &&
	    ilclient_create_component(client, &video_scheduler, (char *)"video_scheduler", ILCLIENT_DISABLE_ALL_PORTS) != 0)
		status = -14;
	list[3] = video_scheduler;

	set_tunnel(tunnel, video_decode, 131, video_scheduler, 10);
	set_tunnel(tunnel+1, video_scheduler, 11, video_render, 90);
	set_tunnel(tunnel+2, clock, 80, video_scheduler, 12);

	// setup clock tunnel first
	if (status == 0 && ilclient_setup_tunnel(tunnel+2, 0, 0) != 0)
		status = -15;
	else
		ilclient_change_component_state(clock, OMX_StateExecuting);

	if (status == 0)
		ilclient_change_component_state(video_decode, OMX_StateIdle);

	memset(&conc, 0, sizeof(conc));
	conc.nSize = sizeof(conc);
	conc.nVersion.nVersion = OMX_VERSION;
	conc.bStartWithValidFrame = OMX_FALSE;
	if (status == 0 &&
	    OMX_SetParameter(ILC_GET_HANDLE(video_decode), OMX_IndexParamBrcmVideoDecodeErrorConcealment, &conc) != OMX_ErrorNone)
		status = -16;

	memset(&format, 0, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
	format.nSize = sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE);
	format.nVersion.nVersion = OMX_VERSION;
	format.nPortIndex = 130;
	format.eCompressionFormat = OMX_VIDEO_CodingAVC;
	// format.xFramerate = 50 * (1 << 16);

	lt_info_c("Dec::run %d status %d\n", __LINE__, status);
	if (status == 0 &&
	    OMX_SetParameter(ILC_GET_HANDLE(video_decode), OMX_IndexParamVideoPortFormat, &format) == OMX_ErrorNone &&
	    ilclient_enable_port_buffers(video_decode, 130, NULL, NULL, NULL) == 0)
	{
		OMX_BUFFERHEADERTYPE *buf;
		int port_settings_changed = 0;
		int first_packet = 1;
		dec_running = true;

		ilclient_change_component_state(video_decode, OMX_StateExecuting);

		lt_info_c("Dec::run %d\n", __LINE__);
		while ((buf = ilclient_get_input_buffer(video_decode, 130, 1)) != NULL)
		{
			// feed data and wait until we get port settings changed
			unsigned char *dest = buf->pBuffer;
 again:
			//lt_info_c("Dec::run %d\n", __LINE__);
			ret = videoDemux->Read(dest, packet_size-data_len, 10);
			if (ret <= 0) {
				if (!dec_running)
					break;
				goto again;
			}

			data_len += ret;
			//lt_info_c("Dec::run %d data_len %d\n", __LINE__, data_len);

			if (port_settings_changed == 0 &&
			    ((data_len > 0 && ilclient_remove_event(video_decode, OMX_EventPortSettingsChanged, 131, 0, 0, 1) == 0) ||
			    (data_len == 0 && ilclient_wait_for_event(video_decode, OMX_EventPortSettingsChanged, 131, 0, 0, 1,
								      ILCLIENT_EVENT_ERROR | ILCLIENT_PARAMETER_CHANGED, 10000) == 0)))
			{
				port_settings_changed = 1;

				if (ilclient_setup_tunnel(tunnel, 0, 0) != 0) {
					status = -7;
					break;
				}

				ilclient_change_component_state(video_scheduler, OMX_StateExecuting);

				// now setup tunnel to video_render
				if (ilclient_setup_tunnel(tunnel+1, 0, 1000) != 0) {
					status = -12;
					break;
				}

				ilclient_change_component_state(video_render, OMX_StateExecuting);
			}
			if (!data_len)
				break;
			if (! dec_running)
				break;

			buf->nFilledLen = data_len;
				data_len = 0;

			buf->nOffset = 0;
			if (first_packet) {
				buf->nFlags = OMX_BUFFERFLAG_STARTTIME;
				first_packet = 0;
			}
			else
				buf->nFlags = OMX_BUFFERFLAG_TIME_UNKNOWN;

			if (OMX_EmptyThisBuffer(ILC_GET_HANDLE(video_decode), buf) != OMX_ErrorNone) {
				status = -6;
				break;
			}
		}
		lt_info_c("Dec::run %d\n", __LINE__);

		buf->nFilledLen = 0;
		buf->nFlags = OMX_BUFFERFLAG_TIME_UNKNOWN | OMX_BUFFERFLAG_EOS;

		if (OMX_EmptyThisBuffer(ILC_GET_HANDLE(video_decode), buf) != OMX_ErrorNone)
			status = -20;

		// wait for EOS from render
		ilclient_wait_for_event(video_render, OMX_EventBufferFlag, 90, 0, OMX_BUFFERFLAG_EOS, 0,
					ILCLIENT_BUFFER_FLAG_EOS, 10000);

		// need to flush the renderer to allow video_decode to disable its input port
		ilclient_flush_tunnels(tunnel, 0);

		ilclient_disable_port_buffers(video_decode, 130, NULL, NULL, NULL);
	}

	ilclient_disable_tunnel(tunnel);
	ilclient_disable_tunnel(tunnel+1);
	ilclient_disable_tunnel(tunnel+2);
	ilclient_teardown_tunnels(tunnel);

	ilclient_state_transition(list, OMX_StateIdle);
	ilclient_state_transition(list, OMX_StateLoaded);

	ilclient_cleanup_components(list);

	OMX_Deinit();

	ilclient_destroy(client);
	lt_info_c("Dec::run %d ends\n", __LINE__);
	// return status;
}
