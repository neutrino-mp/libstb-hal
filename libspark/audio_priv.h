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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * private audio stuff, to be used inside libstb-hal only
 */

#ifndef __audio_priv__
#define __audio_priv__

#include <audio_hal.h>
class ADec
{
public:
	/* construct & destruct */
	ADec(void);
	~ADec(void);

	int SetMute(bool enable, bool remember = true);

	/* volume, min = 0, max = 255 */
	int setVolume(unsigned int left, unsigned int right);

	/* start and stop audio */
	int Start(void);
	int Stop(void);
	void SetStreamType(AUDIO_FORMAT type);
	void SetSyncMode(AVSYNC_TYPE Mode);

	/* select channels */
	int PrepareClipPlay(int uNoOfChannels, int uSampleRate, int uBitsPerSample, int bLittleEndian);
	int WriteClip(unsigned char * buffer, int size);
	int StopClip();
	void setBypassMode(bool disable);
	void openDevice();
	void closeDevice();

	/* variables */
	int fd;
	int clipfd; /* for pcm playback */
	int mixer_fd;  /* if we are using the OSS mixer */
	int mixer_num; /* oss mixer to use, if any */
	AUDIO_FORMAT StreamType;
	bool started;
	int volume;
	bool muted;
};

#endif

