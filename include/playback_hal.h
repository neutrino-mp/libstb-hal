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

#ifndef __playback_hal__
#define __playback_hal__

#include <string>
#include <stdint.h>
#include <vector>

/*
 * This is actually the max number that could be returned by
 * FindAllPids() / FindAllSubs().
 * not yet implemented, most archs return max. 10 PIDs.
 */
#define MAX_PLAYBACK_PIDS 40

typedef enum {
	PLAYMODE_TS = 0,
	PLAYMODE_FILE,
} playmode_t;

class PBPrivate;
class cPlayback
{
public:
	bool Open(playmode_t PlayMode);
	void Close(void);
	bool Start(char *filename, unsigned short vpid, int vtype, unsigned short apid, int ac3, unsigned int duration);
	bool Stop(void);
	bool SetAPid(unsigned short pid, int audio_flag);
	bool SetSpeed(int speed);
	bool GetSpeed(int &speed) const;
	bool GetPosition(int &position, int &duration);
	bool SetPosition(int position, bool absolute = false);
	void FindAllPids(uint16_t *pids, unsigned short *aud_flags, uint16_t *num, std::string *language);
	void FindAllSubs(uint16_t *pids, unsigned short *supported, uint16_t *num, std::string *language);
	bool SelectSubtitles(int pid);
	void GetChapters(std::vector<int> &positions, std::vector<std::string> &titles);
	void RequestAbort();
	void GetTitles(std::vector<int> &playlists, std::vector<std::string> &titles, int &current);
	void SetTitle(int title);
	uint64_t GetReadCount(void);
	//
	cPlayback(int num = 0);
	~cPlayback();
private:
	PBPrivate *pd;
};

#endif
