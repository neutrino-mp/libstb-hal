#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <linux/dvb/audio.h>

#include <proc_tools.h>

#include "audio_hal.h"
#include "lt_debug.h"

#define AUDIO_DEVICE	"/dev/dvb/adapter0/audio0"
#define lt_debug(args...) _lt_debug(TRIPLE_DEBUG_AUDIO, this, args)
#define lt_info(args...) _lt_info(TRIPLE_DEBUG_AUDIO, this, args)

#include <linux/soundcard.h>

cAudio * audioDecoder = NULL;

typedef struct audio_pdata
{
	int fd;
	int clipfd;
	int mixer_fd;
	int mixer_num;
} audio_pdata;
#define P ((audio_pdata *)pdata)

cAudio::cAudio(void *, void *, void *)
{
	pdata = calloc(1, sizeof(audio_pdata));
	P->clipfd = -1;
	P->mixer_fd = -1;
	P->fd = open(AUDIO_DEVICE, O_RDONLY|O_CLOEXEC);
	if (P->fd < 0)
		lt_info("%s: open failed (%m)\n", __func__);
	muted = false;
}

cAudio::~cAudio(void)
{
	if (P->fd >= 0) {
		ioctl(P->fd, AUDIO_CONTINUE); /* enigma2 also does CONTINUE before close... */
		close(P->fd);
		P->fd = -1;
	}
	if (P->clipfd >= 0)
		close(P->clipfd);
	if (P->mixer_fd >= 0)
		close(P->mixer_fd);
	free(pdata);
}

int cAudio::mute(void)
{
	return SetMute(true);
}

int cAudio::unmute(void)
{
	return SetMute(false);
}

int cAudio::SetMute(bool enable)
{
	lt_debug("%s(%d)\n", __func__, enable);

	muted = enable;
#if 0
	/* does not work? */
	if (ioctl(fd, AUDIO_SET_MUTE, enable) < 0 )
		lt_info("%s: AUDIO_SET_MUTE failed (%m)\n", __func__);
#else
	char s[2] = { 0, 0 };
	s[0] = '0' + (int)enable;
	proc_put("/proc/stb/audio/j1_mute", s, 2);
#endif
	return 0;
}

int map_volume(const int volume)
{
	unsigned char vol = volume;
	if (vol > 100)
		vol = 100;

	vol = 63 - vol * 63 / 100;
	return vol;
}


int cAudio::setVolume(unsigned int left, unsigned int right)
{
	lt_debug("%s(%d, %d)\n", __func__, left, right);

	volume = (left + right) / 2;
	if (P->clipfd != -1 && P->mixer_fd != -1) {
		int tmp = 0;
		/* not sure if left / right is correct here, but it is always the same anyways ;-) */
		if (! muted)
			tmp = left << 8 | right;
		int ret = ioctl(P->mixer_fd, MIXER_WRITE(P->mixer_num), &tmp);
		if (ret == -1)
			lt_info("%s: MIXER_WRITE(%d),%04x: %m\n", __func__, P->mixer_num, tmp);
		return ret;
	}

	audio_mixer_t mixer;
	mixer.volume_left  = map_volume(left);
	mixer.volume_right = map_volume(right);

	if (ioctl(P->fd, AUDIO_SET_MIXER, &mixer) < 0)
		lt_info("%s: AUDIO_SET_MIXER failed (%m)\n", __func__);

	return 0;
}

int cAudio::Start(void)
{
	lt_debug("%s\n", __func__);
	int ret;
	ioctl(P->fd, AUDIO_CONTINUE);
	ret = ioctl(P->fd, AUDIO_PLAY);
	return ret;
}

int cAudio::Stop(void)
{
	lt_debug("%s\n", __func__);
	ioctl(P->fd, AUDIO_STOP);
	ioctl(P->fd, AUDIO_CONTINUE); /* no idea why we have to stop and then continue => enigma2 does it, too */
	return 0;
}

bool cAudio::Pause(bool /*Pcm*/)
{
	return true;
};

void cAudio::SetSyncMode(AVSYNC_TYPE Mode)
{
	lt_debug("%s %d\n", __func__, Mode);
	ioctl(P->fd, AUDIO_SET_AV_SYNC, Mode);
};

//AUDIO_ENCODING_AC3
#define AUDIO_STREAMTYPE_AC3 0
//AUDIO_ENCODING_MPEG2
#define AUDIO_STREAMTYPE_MPEG 1
//AUDIO_ENCODING_DTS
#define AUDIO_STREAMTYPE_DTS 2

#define AUDIO_ENCODING_LPCM 2
#define AUDIO_ENCODING_LPCMA 11
void cAudio::SetStreamType(AUDIO_FORMAT type)
{
	int bypass = AUDIO_STREAMTYPE_MPEG;
	lt_debug("%s %d\n", __func__, type);

	switch (type)
	{
		case AUDIO_FMT_DOLBY_DIGITAL:
			bypass = AUDIO_STREAMTYPE_AC3;
			break;
		case AUDIO_FMT_DTS:
			bypass = AUDIO_STREAMTYPE_DTS;
			break;
		case AUDIO_FMT_MPEG:
		default:
			break;
	}

	// Normaly the encoding should be set using AUDIO_SET_ENCODING
	// But as we implemented the behavior to bypass (cause of e2) this is correct here
	if (ioctl(P->fd, AUDIO_SET_BYPASS_MODE, bypass) < 0)
		lt_info("%s: AUDIO_SET_BYPASS_MODE failed (%m)\n", __func__);
};

int cAudio::setChannel(int channel)
{
	return 0;
};

int cAudio::PrepareClipPlay(int ch, int srate, int bits, int little_endian)
{
	int fmt;
	unsigned int devmask, stereo, usable;
	const char *dsp_dev = getenv("DSP_DEVICE");
	const char *mix_dev = getenv("MIX_DEVICE");
	lt_debug("%s ch %d srate %d bits %d le %d\n", __FUNCTION__, ch, srate, bits, little_endian);
	if (P->clipfd >= 0) {
		lt_info("%s: clipfd already opened (%d)\n", __func__, P->clipfd);
		return -1;
	}
	P->mixer_num = -1;
	P->mixer_fd = -1;
	/* a different DSP device can be given with DSP_DEVICE and MIX_DEVICE
	 * if this device cannot be opened, we fall back to the internal OSS device
	 * Example:
	 *   modprobe snd-usb-audio
	 *   export DSP_DEVICE=/dev/sound/dsp2
	 *   export MIX_DEVICE=/dev/sound/mixer2
	 *   neutrino
	 */
	if ((!dsp_dev) || (access(dsp_dev, W_OK))) {
		if (dsp_dev)
			lt_info("%s: DSP_DEVICE is set (%s) but cannot be opened,"
				" fall back to /dev/dsp1\n", __func__, dsp_dev);
		dsp_dev = "/dev/dsp1";
	}
	lt_info("%s: dsp_dev %s mix_dev %s\n", __func__, dsp_dev, mix_dev); /* NULL mix_dev is ok */
	/* the tdoss dsp driver seems to work only on the second open(). really. */
	P->clipfd = open(dsp_dev, O_WRONLY|O_CLOEXEC);
	if (P->clipfd < 0) {
		lt_info("%s open %s: %m\n", dsp_dev, __FUNCTION__);
		return -1;
	}
	/* no idea if we ever get little_endian == 0 */
	if (little_endian)
		fmt = AFMT_S16_BE;
	else
		fmt = AFMT_S16_LE;
	if (ioctl(P->clipfd, SNDCTL_DSP_SETFMT, &fmt))
		perror("SNDCTL_DSP_SETFMT");
	if (ioctl(P->clipfd, SNDCTL_DSP_CHANNELS, &ch))
		perror("SNDCTL_DSP_CHANNELS");
	if (ioctl(P->clipfd, SNDCTL_DSP_SPEED, &srate))
		perror("SNDCTL_DSP_SPEED");
	if (ioctl(P->clipfd, SNDCTL_DSP_RESET))
		perror("SNDCTL_DSP_RESET");

	if (!mix_dev)
		return 0;

	P->mixer_fd = open(mix_dev, O_RDWR|O_CLOEXEC);
	if (P->mixer_fd < 0) {
		lt_info("%s: open mixer %s failed (%m)\n", __func__, mix_dev);
		/* not a real error */
		return 0;
	}
	if (ioctl(P->mixer_fd, SOUND_MIXER_READ_DEVMASK, &devmask) == -1) {
		lt_info("%s: SOUND_MIXER_READ_DEVMASK %m\n", __func__);
		devmask = 0;
	}
	if (ioctl(P->mixer_fd, SOUND_MIXER_READ_STEREODEVS, &stereo) == -1) {
		lt_info("%s: SOUND_MIXER_READ_STEREODEVS %m\n", __func__);
		stereo = 0;
	}
	usable = devmask & stereo;
	if (usable == 0) {
		lt_info("%s: devmask: %08x stereo: %08x, no usable dev :-(\n",
			__func__, devmask, stereo);
		close(P->mixer_fd);
		P->mixer_fd = -1;
		return 0; /* TODO: should we treat this as error? */
	}
	/* __builtin_popcount needs GCC, it counts the set bits... */
	if (__builtin_popcount (usable) != 1) {
		/* TODO: this code is not yet tested as I have only single-mixer devices... */
		lt_info("%s: more than one mixer control: devmask %08x stereo %08x\n"
			"%s: querying MIX_NUMBER environment variable...\n",
			__func__, devmask, stereo, __func__);
		const char *tmp = getenv("MIX_NUMBER");
		if (tmp)
			P->mixer_num = atoi(tmp);
		lt_info("%s: mixer_num is %d -> device %08x\n",
			__func__, P->mixer_num, (P->mixer_num >= 0) ? (1 << P->mixer_num) : 0);
		/* no error checking, you'd better know what you are doing... */
	} else {
		P->mixer_num = 0;
		while (!(usable & 0x01)) {
			P->mixer_num++;
			usable >>= 1;
		}
	}
	setVolume(volume, volume);

	return 0;
};

int cAudio::WriteClip(unsigned char *buffer, int size)
{
	int ret;
	// lt_debug("cAudio::%s\n", __FUNCTION__);
	if (P->clipfd <= 0) {
		lt_info("%s: clipfd not yet opened\n", __FUNCTION__);
		return -1;
	}
	ret = write(P->clipfd, buffer, size);
	if (ret < 0)
		lt_info("%s: write error (%m)\n", __FUNCTION__);
	return ret;
};

int cAudio::StopClip()
{
	lt_debug("%s\n", __FUNCTION__);
	if (P->clipfd <= 0) {
		lt_info("%s: clipfd not yet opened\n", __FUNCTION__);
		return -1;
	}
	close(P->clipfd);
	P->clipfd = -1;
	if (P->mixer_fd >= 0)
		close(P->mixer_fd);
	P->mixer_fd = -1;
	setVolume(volume, volume);
	return 0;
};

void cAudio::getAudioInfo(int &type, int &layer, int &freq, int &bitrate, int &mode)
{
	lt_debug("%s\n", __FUNCTION__);
	type = 0;
	layer = 0;
	freq = 0;
	bitrate = 0;
	mode = 0;
#if 0
	unsigned int atype;
	static const int freq_mpg[] = {44100, 48000, 32000, 0};
	static const int freq_ac3[] = {48000, 44100, 32000, 0};
	scratchl2 i;
	if (ioctl(fd, MPEG_AUD_GET_DECTYP, &atype) < 0)
		perror("cAudio::getAudioInfo MPEG_AUD_GET_DECTYP");
	if (ioctl(fd, MPEG_AUD_GET_STATUS, &i) < 0)
		perror("cAudio::getAudioInfo MPEG_AUD_GET_STATUS");

	type = atype;
#if 0
/* this does not work, some of the values are negative?? */
	AMPEGStatus A;
	memcpy(&A, &i.word00, sizeof(i.word00));
	layer   = A.audio_mpeg_layer;
	mode    = A.audio_mpeg_mode;
	bitrate = A.audio_mpeg_bitrate;
	switch(A.audio_mpeg_frequency)
#endif
	/* layer and bitrate are not used anyway... */
	layer   = 0; //(i.word00 >> 17) & 3;
	bitrate = 0; //(i.word00 >> 12) & 3;
	switch (type)
	{
		case 0:	/* MPEG */
			mode = (i.word00 >> 6) & 3;
			freq = freq_mpg[(i.word00 >> 10) & 3];
			break;
		case 1:	/* AC3 */
			mode = (i.word00 >> 28) & 7;
			freq = freq_ac3[(i.word00 >> 16) & 3];
			break;
		default:
			mode = 0;
			freq = 0;
	}
	//fprintf(stderr, "type: %d layer: %d freq: %d bitrate: %d mode: %d\n", type, layer, freq, bitrate, mode);
#endif
};

void cAudio::SetSRS(int /*iq_enable*/, int /*nmgr_enable*/, int /*iq_mode*/, int /*iq_level*/)
{
	lt_debug("%s\n", __FUNCTION__);
};

void cAudio::SetHdmiDD(bool enable)
{
	lt_debug("%s %d\n", __func__, enable);
};

#define AUDIO_BYPASS_ON  0
#define AUDIO_BYPASS_OFF 1
void cAudio::SetSpdifDD(bool enable)
{
	lt_debug("%s %d\n", __func__, enable);
	//setBypassMode(!enable);
	int mode = enable ? AUDIO_BYPASS_ON : AUDIO_BYPASS_OFF;
	if (ioctl(P->fd, AUDIO_SET_BYPASS_MODE, mode) < 0)
		lt_info("%s AUDIO_SET_BYPASS_MODE %d: %m\n", __func__, mode);
};

void cAudio::ScheduleMute(bool On)
{
	lt_debug("%s %d\n", __FUNCTION__, On);
};

void cAudio::EnableAnalogOut(bool enable)
{
	lt_debug("%s %d\n", __FUNCTION__, enable);
};

#if 0
void cAudio::setBypassMode(bool disable)
{
	lt_debug("%s %d\n", __func__, disable);
	int mode = disable ? AUDIO_BYPASS_OFF : AUDIO_BYPASS_ON;
	if (ioctl(fd, AUDIO_SET_BYPASS_MODE, mode) < 0)
		lt_info("%s AUDIO_SET_BYPASS_MODE %d: %m\n", __func__, mode);
	return;
}
#endif
