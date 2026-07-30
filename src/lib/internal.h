/*
    ddc/ci interface functions header
    Copyright(c) 2004 Oleg I. Vdovikin (oleg@cs.msu.su)
    Copyright(c) 2004-2005 Nicolas Boichat (nicolas@boichat.ch)
    Copyright(c) 2004-2026 DDCcontrol authors and contributors (see AUTHORS and CONTRIBUTORS)

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#ifndef DDCCI_INTERNAL_H
#define DDCCI_INTERNAL_H

#ifdef HAVE_GETTEXT
#include <libintl.h>
#include <locale.h>
#define _(String) gettext (String)
#define gettext_noop(String) String
#define N_(String) gettext_noop (String)
#else
#define _(String) String
#define N_(String) String
#endif

struct monitor_vtable {
	int (*readctrl)(struct monitor* mon, unsigned char ctrl, unsigned short *value, unsigned short *maximum);
	int (*writectrl)(struct monitor* mon, unsigned char ctrl, unsigned short value, int delay);
	int (*close)(struct monitor* mon);
};

/* A complete, parsed capabilities exchange is itself proof of DDC/CI support.
 * The legacy presence command is only needed when that exchange fails. */
static inline int ddcci_caps_prove_support(int caps_result)
{
	return caps_result >= 0;
}

/* I2C_RDWR reports completed messages on Linux and zero on FreeBSD, not the
 * number of bytes transferred. A successful read completes the requested
 * single-message buffer. */
static inline int ddcci_i2c_read_length(int ioctl_result, unsigned char requested_len)
{
#ifdef __FreeBSD__
	return ioctl_result == 0 ? requested_len : -1;
#else
	return ioctl_result == 1 ? requested_len : -1;
#endif
}

#endif //DDCCI_INTERNAL_H
