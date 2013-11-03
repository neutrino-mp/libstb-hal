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
#ifndef __record_hal__
#define __record_hal__

#define REC_STATUS_OK 0
#define REC_STATUS_SLOW 1
#define REC_STATUS_OVERFLOW 2

class RecData;
class cRecord
{
public:
	cRecord(int num = 0);
	~cRecord();

	bool Open();
	bool Start(int fd, unsigned short vpid, unsigned short *apids, int numapids, uint64_t ch = 0);
	bool Stop(void);
	bool AddPid(unsigned short pid);
	int  GetStatus();
	void ResetStatus();
	bool ChangePids(unsigned short vpid, unsigned short *apids, int numapids);
private:
	RecData *pd;
};
#endif
