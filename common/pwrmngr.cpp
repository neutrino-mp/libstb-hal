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
 */
#include <config.h>

#include "pwrmngr.h"
#include "lt_debug.h"

#if HAVE_TRIPLEDRAGON
#include <stdio.h>
#include <cstdlib>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <avs/avs_inf.h>
#include <tdpanel/lcdstuff.h>
#endif

#define lt_debug(args...) _lt_debug(TRIPLE_DEBUG_PWRMNGR, this, args)
#define lt_info(args...) _lt_info(TRIPLE_DEBUG_PWRMNGR, this, args)

/* cpufreqmanager */
void cCpuFreqManager::Up(void)
{
	lt_debug("%s\n", __func__);
}

void cCpuFreqManager::Down(void)
{
	lt_debug("%s\n", __func__);
}

void cCpuFreqManager::Reset(void)
{
	lt_debug("%s\n", __func__);
}

/* those function dummies return true or "harmless" values */
bool cCpuFreqManager::SetDelta(unsigned long)
{
	lt_debug("%s\n", __func__);
	return true;
}

unsigned long cCpuFreqManager::GetDelta(void)
{
	lt_debug("%s\n", __func__);
	return 0;
}

unsigned long cCpuFreqManager::GetCpuFreq(void)
{
	lt_debug("%s\n", __func__);
	return 0;
}

bool cCpuFreqManager::SetCpuFreq(unsigned long f)
{
	lt_info("%s(%lu) => set standby = %s\n", __func__, f, f?"true":"false");
#if HAVE_TRIPLEDRAGON
	/* actually SetCpuFreq is used to determine if the system is in standby
	   this is an "elegant" hack, because:
	   * during a recording, cpu freq is kept "high", even if the box is sent to standby
	   * the "SetStandby" call is made even if a recording is running
	   On the TD, setting standby disables the frontend, so we must not do it
	   if a recording is running.
	   For now, the values in neutrino are hardcoded:
	   * f == 0        => max => not standby
	   * f == 50000000 => min => standby
	 */
	int fd = open("/dev/stb/tdsystem", O_RDONLY);
	if (fd < 0)
	{
		perror("open tdsystem");
		return false;
	}
	if (f)
	{
		ioctl(fd, IOC_AVS_SET_VOLUME, 31); /* mute AVS to avoid ugly noise */
		ioctl(fd, IOC_AVS_STANDBY_ENTER);
		if (getenv("TRIPLE_LCDBACKLIGHT"))
		{
			lt_info("%s: TRIPLE_LCDBACKLIGHT is set: keeping LCD backlight on\n", __func__);
			close(fd);
			fd = open("/dev/stb/tdlcd", O_RDONLY);
			if (fd < 0)
				lt_info("%s: open tdlcd error: %m\n", __func__);
			else
				ioctl(fd, IOC_LCD_BACKLIGHT_ON);
		}
	}
	else
	{
		ioctl(fd, IOC_AVS_SET_VOLUME, 31); /* mute AVS to avoid ugly noise */
		ioctl(fd, IOC_AVS_STANDBY_LEAVE);
		/* unmute will be done by cAudio::do_mute(). Ugly, but prevents pops */
		// ioctl(fd, IOC_AVS_SET_VOLUME, 0); /* max gain */
	}

	close(fd);
#endif
	return true;
}

cCpuFreqManager::cCpuFreqManager(void)
{
	lt_debug("%s\n", __func__);
}

/* powermanager */
bool cPowerManager::Open(void)
{
	lt_debug("%s\n", __func__);
	return true;
}

void cPowerManager::Close(void)
{
	lt_debug("%s\n", __func__);
}
bool cPowerManager::SetStandby(bool Active, bool Passive)
{
	lt_debug("%s(%d, %d)\n", __func__, Active, Passive);
	return true;
}

cPowerManager::cPowerManager(void)
{
	lt_debug("%s\n", __func__);
}

cPowerManager::~cPowerManager()
{
	lt_debug("%s\n", __func__);
}
