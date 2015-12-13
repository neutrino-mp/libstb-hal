#include <stdio.h>

#include "playback_hal.h"

static const char * FILENAME = "playback-dummy";

bool cPlayback::Open(playmode_t)
{
	return 0;
}

void cPlayback::Close(void)
{
}

bool cPlayback::Start(char * filename, unsigned short vpid, int vtype, unsigned short apid, int ac3, unsigned int duration)
{
	printf("%s:%s - filename=%s vpid=%u vtype=%d apid=%u ac3=%d duration=%i\n",
		FILENAME, __func__, filename, vpid, vtype, apid, ac3, duration);
	return true;
}

bool cPlayback::SetAPid(unsigned short pid, int /*ac3*/)
{
	printf("%s:%s pid %i\n", FILENAME, __func__, pid);
	return true;
}

bool cPlayback::SelectSubtitles(int pid)
{
	printf("%s:%s pid %i\n", FILENAME, __func__, pid);
	return true;
}

bool cPlayback::SetSpeed(int speed)
{
	printf("%s:%s speed %d\n", FILENAME, __func__, speed);
	return true;
}

bool cPlayback::GetSpeed(int &/*speed*/) const
{
	return true;
}

bool cPlayback::GetPosition(int &position, int &duration)
{
	printf("%s:%s %d %d\n", FILENAME, __func__, position, duration);
	position = 0;
	duration = 0;
	return true;
}

bool cPlayback::SetPosition(int position, bool)
{
	printf("%s:%s %d\n", FILENAME, __func__,position);
	return true;
}

void cPlayback::FindAllPids(uint16_t *, unsigned short *, uint16_t *numpida, std::string *)
{
	printf("%s:%s\n", FILENAME, __func__);
	*numpida = 0;
}

void cPlayback::FindAllSubs(uint16_t *, unsigned short *, uint16_t *numpida, std::string *)
{
	printf("%s:%s\n", FILENAME, __func__);
	*numpida = 0;
}

void cPlayback::GetChapters(std::vector<int> &positions, std::vector<std::string> &titles)
{
	positions.clear();
	titles.clear();
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

uint64_t cPlayback::GetReadCount(void)
{
	return 0;
}

cPlayback::cPlayback(int /*num*/)
{
	printf("%s:%s\n", FILENAME, __func__);
}

cPlayback::~cPlayback()
{
	printf("%s:%s\n", FILENAME, __func__);
}
