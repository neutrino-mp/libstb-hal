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
 * Audio / Video decoder wrapper class for Raspberry pi
 */

#include "codec.h"

class AVDec {
public:
	AVDec();
	~AVDec();
	int start_audio();
	int start_video();
	int stop_audio();
	int stop_video();
	int set_volume(int vol);
	int play_pcm(uint8_t *buffer, int size, int ch = -1, int srate = -1, int bits = -1, int le = -1);
	int show_picture(const char *filename);
	int pig(int x, int y, int w, int h);
};
