/*
 * determine the capabilities of the hardware.
 * part of libstb-hal
 *
 * (C) 2010-2012,2016 Stefan Seyfried
 *
 * License: GPL v2 or later
 */

#include <stdio.h>
#include <string.h>
#include <hardware_caps.h>
#include <sys/utsname.h>

static int initialized = 0;
static hw_caps_t caps;

hw_caps_t *get_hwcaps(void)
{
	struct utsname u;
	if (initialized)
		return &caps;

	memset(&caps, 0, sizeof(hw_caps_t));

	initialized = 1;
	caps.can_shutdown = 1;	/* for testing */
	caps.display_type = HW_DISPLAY_LINE_TEXT;
	caps.has_HDMI = 1;
	caps.display_xres = 8;
	strcpy(caps.boxvendor, "Generic");
	strcpy(caps.boxname, "PC");
	if (! uname(&u))
		strncpy(caps.boxarch, u.machine, sizeof(caps.boxarch));
	else
		fprintf(stderr, "%s: uname() failed: %m\n", __func__);

	return &caps;
}
