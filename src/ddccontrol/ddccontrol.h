/*
    ddc/ci internal functions header for command line tool
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

#ifndef DDCCONTROL_H
#define DDCCONTROL_H

#include "ddcci.h"

void print_control_value(struct monitor *mon, unsigned char ctrl, unsigned short value, unsigned short maximum, int result);

#endif // DDCCONTROL_H
