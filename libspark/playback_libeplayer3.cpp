/*
 * CPlayback implementation for SH4 using libeplayer3
 *
 * (C) 2010-2015 Stefan Seyfried
 *
 * original code is from tdt git:
 *   git://gitorious.org/open-duckbox-project-sh4/tdt.git
 *
 * lots of changes and huge improvements are
 * (C) 2013,2014 'martii' <m4rtii@gmx.de>
 *
 * License: GPLv2 or later, as the rest of libstb-hal
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <audio_priv.h>
#include <video_priv.h>

#include "player.h"
//#include "playback_libeplayer3.h"
#include "playback_hal.h"

extern ADec *adec;
extern cVideo *videoDecoder;
static bool decoders_closed = false;

static const char * FILENAME = "playback_libeplayer3.cpp";
static playmode_t pm;
static std::string fn_ts;
static std::string fn_xml;
static off_t last_size;
static int init_jump;

class PBPrivate
{
public:
	bool enabled;
	bool playing;
	int speed;
	int astream;
	Player *player;
	PBPrivate() {
		enabled = false;
		playing = false;
		speed = 0;
		astream = -1;
		player = new Player;
	};
	~PBPrivate() {
		delete player;
	};
};

bool cPlayback::Open(playmode_t PlayMode)
{
	const char *aPLAYMODE[] = {
		"PLAYMODE_TS",
		"PLAYMODE_FILE"
	};

	if (PlayMode != PLAYMODE_TS)
	{
		adec->closeDevice();
		videoDecoder->vdec->closeDevice();
		decoders_closed = true;
	}

	printf("%s:%s - PlayMode=%s\n", FILENAME, __FUNCTION__, aPLAYMODE[PlayMode]);
	pm = PlayMode;
	fn_ts = "";
	fn_xml = "";
	last_size = 0;
	pd->speed = 0;
	init_jump = -1;

	return 0;
}

void cPlayback::Close(void)
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);

	//Dagobert: movieplayer does not call stop, it calls close ;)
	Stop();
	if (decoders_closed)
	{
		adec->openDevice();
		videoDecoder->vdec->openDevice();
		decoders_closed = false;
	}
}

bool cPlayback::Start(char *filename, unsigned short vpid, int vtype, unsigned short apid, int ac3, unsigned int)
{
	bool ret = false;
	bool isHTTP = false;
	bool no_probe = false;

	printf("%s:%s - filename=%s vpid=%u vtype=%d apid=%u ac3=%d\n",
		FILENAME, __FUNCTION__, filename, vpid, vtype, apid, ac3);

	Player *player = pd->player;
	init_jump = -1;

	std::string file;
	if (*filename == '/')
		file = "file://";
	file += filename;

	if (file.substr(0, 7) == "file://") {
		if (file.substr(file.length() - 3) ==  ".ts") {
			fn_ts = file.substr(7);
			fn_xml = file.substr(7, file.length() - 9);
			fn_xml += "xml";
			no_probe = true;
		}
	} else
		isHTTP = true;

	if (player->Open(file.c_str(), no_probe)) {
		if (pm == PLAYMODE_TS) {
			struct stat s;
			if (!stat(file.c_str(), &s))
				last_size = s.st_size;
			ret = true;
			videoDecoder->vdec->Stop(false);
			adec->Stop();
		} else {
#if 0
			std::vector<std::string> keys, values;
			int selected_program = 0;
			if (vpid || apid) {
				;
			} else if (player->GetPrograms(keys, values) && (keys.size() > 1) && ProgramSelectionCallback) {
				const char *key = ProgramSelectionCallback(ProgramSelectionCallbackData, keys, values);
				if (!key) {
					player->Close();
					return false;
				}
				selected_program = atoi(key);
			} else if (keys.size() > 0)
				selected_program = atoi(keys[0].c_str());

			if (!keys.size() || !player->SelectProgram(selected_program)) {
				if (apid)
					SetAPid(apid);
				if (vpid)
					SetVPid(vpid);
			}
#endif
			if (apid)
				player->SwitchAudio(apid);
			if (vpid)
				player->SwitchVideo(vpid);
			pd->playing = true;
			player->output.Open();
			ret = player->Play();
			if (ret && !isHTTP)
				pd->playing = ret = player->Pause();
		}
	}

	return ret;
}

bool cPlayback::Stop(void)
{
	printf("%s:%s playing %d\n", FILENAME, __func__, pd->playing);
	Player *player = pd->player;

	player->Stop();
	player->output.Close();
	player->Close();

	pd->playing = false;
	return true;
}

bool cPlayback::SetAPid(unsigned short pid, int /*ac3*/)
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
	return pd->player->SwitchAudio(pid);
}

bool cPlayback::SetSpeed(int speed)
{
	printf("%s:%s playing %d speed %d\n", FILENAME, __func__, pd->playing, speed);

	Player *player = pd->player;

	if (! decoders_closed)
	{
		adec->closeDevice();
		videoDecoder->vdec->closeDevice();
		decoders_closed = true;
		usleep(500000);
		player->output.Open();
		pd->playing = player->Play();
	}

	if (!pd->playing)
		return false;

	bool res = true;

	pd->speed = speed;

	if (speed > 1) {
		/* direction switch ? */
		if (player->isBackWard)
			player->FastBackward(0);
		res = player->FastForward(speed);
	} else if (speed < 0) {
		/* direction switch ? */
		if (player->isForwarding)
			player->Continue();
		res = player->FastBackward(speed);
	} else if (speed == 0) {
		/* konfetti: hmmm accessing the member isn't very proper */
		if ((player->isForwarding) || (!player->isBackWard))
			/* res = */ player->Pause();
		else
			/* res = */ player->FastForward(0);
	} else /* speed == 1 */ {
		res = player->Continue();
	}

	if (init_jump > -1) {
		SetPosition(init_jump);
		init_jump = -1;
	}

	return res;
}

bool cPlayback::GetSpeed(int &speed) const
{
	//printf("%s:%s\n", FILENAME, __FUNCTION__);
	speed = pd->speed;
	return true;
}

#if 0
void cPlayback::GetPts(uint64_t &pts)
{
	pd->player->GetPts((int64_t &) pts);
}
#endif

// in milliseconds
bool cPlayback::GetPosition(int &position, int &duration)
{
	bool got_duration = false;
	printf("%s:%s %d %d\n", FILENAME, __FUNCTION__, position, duration);
	Player *player = pd->player;

	/* hack: if the file is growing (timeshift), then determine its length
	 * by comparing the mtime with the mtime of the xml file */
	if (pm == PLAYMODE_TS)
	{
		struct stat s;
		if (!stat(fn_ts.c_str(), &s))
		{
			if (!pd->playing || last_size != s.st_size)
			{
				last_size = s.st_size;
				time_t curr_time = s.st_mtime;
				if (!stat(fn_xml.c_str(), &s))
				{
					duration = (curr_time - s.st_mtime) * 1000;
					if (!pd->playing)
						return true;
					got_duration = true;
				}
			}
		}
	}

	if (!pd->playing)
		return false;

	if (!player->isPlaying) {
		printf("cPlayback::%s !!!!EOF!!!! < -1\n", __func__);
		position = duration + 1000;
		// duration = 0;
		// this is stupid
		return true;
	}

	int64_t vpts = 0;
	player->GetPts(vpts);

	if(vpts <= 0) {
		//printf("ERROR: vpts==0");
	} else {
		/* len is in nanoseconds. we have 90 000 pts per second. */
		position = vpts/90;
	}

	if (got_duration)
		return true;

	int64_t length = 0;

	player->GetDuration(length);

	if(length <= 0)
		duration = position + AV_TIME_BASE / 1000;
	else
		duration = length * 1000 / AV_TIME_BASE;

	return true;
}

bool cPlayback::SetPosition(int position, bool absolute)
{
	printf("%s:%s %d\n", FILENAME, __FUNCTION__,position);
	if (!pd->playing)
	{
		/* the calling sequence is:
		 * Start()       - paused
		 * SetPosition() - which fails if not running
		 * SetSpeed()    - to start playing
		 * so let's remember the initial jump position and later jump to it
		 */
		init_jump = position;
		return false;
	}
	pd->player->Seek((int64_t)position * (AV_TIME_BASE / 1000), absolute);
	return true;
}

void cPlayback::FindAllPids(uint16_t *pids, unsigned short *ac3flags, uint16_t *numpida, std::string *language)
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
	unsigned int i = 0;

	std::vector<Track> tracks = pd->player->manager.getAudioTracks();
	for (std::vector<Track>::iterator it = tracks.begin(); it != tracks.end() && i < MAX_PLAYBACK_PIDS; ++it) {
		pids[i] = it->pid;
		ac3flags[i] = it->ac3flags;
		language[i] = it->title;
		i++;
	}
	*numpida = i;
}

#if 0
void cPlayback::FindAllSubtitlePids(uint16_t *pids, uint16_t *numpids, std::string *language)
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
	unsigned int i = 0;

	std::vector<Track> tracks = player->manager.getSubtitleTracks();
	for (std::vector<Track>::iterator it = tracks.begin(); it != tracks.end() && i < *numpids; ++it) {
		pids[i] = it->pid;
		language[i] = it->title;
		i++;
	}

	*numpids = i;
}

void cPlayback::FindAllTeletextsubtitlePids(int *pids, unsigned int *numpids, std::string *language, int *mags, int *pages)
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
	unsigned int i = 0;

	std::vector<Track> tracks = player->manager.getTeletextTracks();
	for (std::vector<Track>::iterator it = tracks.begin(); it != tracks.end() && i < *numpids; ++it) {
		if (it->type != 2 && it->type != 5) // return subtitles only
			continue;
		pids[i] = it->pid;
		language[i] = it->title;
		mags[i] = it->mag;
		pages[i] = it->page;
		i++;
	}

	*numpids = i;
}

int cPlayback::GetFirstTeletextPid(void)
{
	std::vector<Track> tracks = player->manager.getTeletextTracks();
	for (std::vector<Track>::iterator it = tracks.begin(); it != tracks.end(); ++it) {
		if (it->type == 1)
			return it->pid;
	}
	return -1;
}

unsigned short cPlayback::GetTeletextPid(void)
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
	return pd->player->GetTeletextPid();
}
#endif

/* dummy functions for subtitles */
void cPlayback::FindAllSubs(uint16_t * /*pids*/, unsigned short * /*supp*/, uint16_t *num, std::string * /*lang*/)
{
	*num = 0;
}

bool cPlayback::SelectSubtitles(int pid)
{
	printf("%s:%s pid %d\n", FILENAME, __func__, pid);
	return false;
}

void cPlayback::GetChapters(std::vector<int> &positions, std::vector<std::string> &titles)
{
	positions.clear();
	titles.clear();

	pd->player->GetChapters(positions, titles);
}

void cPlayback::GetTitles(std::vector<int> &playlists, std::vector<std::string> &titles, int &current)
{
	playlists.clear();
	titles.clear();
	current = 0;
}

void cPlayback::SetTitle(int /*title*/)
{
}

void cPlayback::RequestAbort(void)
{
}

//
cPlayback::cPlayback(int /*num*/)
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
	pd = new PBPrivate();
}

cPlayback::~cPlayback()
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
	delete pd;
	pd = NULL;
}

#if 0
bool cPlayback::IsPlaying(void) const
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);

	/* konfetti: there is no event/callback mechanism in libeplayer2
	 * so in case of ending playback we have no information on a 
	 * terminated stream currently (or did I oversee it?).
	 * So let's ask the player the state.
	 */
	if (playing)
	{
	   return player->playback->isPlaying;
	}

	return playing;
}
#endif
