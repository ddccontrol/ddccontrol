/*
    ddc/ci D-Bus client library headers

    Copyright(c) 2018 Miroslav Kravec (kravec.miroslav@gmail.com)

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

#ifndef DDCCONTROL_DBUS_CLIENT_H
#define DDCCONTROL_DBUS_CLIENT_H

#include "ddcci.h"

typedef struct _DDCControl DDCControl;

DDCControl *ddcci_dbus_open_proxy();

struct monitorlist *ddcci_dbus_rescan_monitors(DDCControl *proxy);

int ddcci_dbus_open(DDCControl *proxy, struct monitor **mon, const char *filename);
int ddcci_dbus_readctrl(DDCControl *proxy, const char *fn,
                        unsigned char ctrl, unsigned short *value, unsigned short *maximum);
int ddcci_dbus_writectrl(DDCControl *proxy, const char *fn,
                         unsigned char ctrl, unsigned short value);


#endif // DDCCONTROL_DBUS_CLIENT_H
