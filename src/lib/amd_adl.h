/*
    ddc/ci interface functions header
    Copyright(c) 2012 Vitaly V. Bursov (vitaly@bursov.com)

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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef AMD_ADL_H
#define AMD_ADL_H

int amd_adl_init();
void amd_adl_free();

int amd_adl_get_displays_count();
int amd_adl_get_display(int idx, int *adapter, int *display);
int amd_adl_check_display(int adapter, int display);

int amd_adl_i2c_read(int adapter, int display, unsigned int addr, unsigned char *buf, unsigned int len);
int amd_adl_i2c_write(int adapter, int display, unsigned int addr, unsigned char *buf, unsigned int len);

#endif /* AMD_ADL_H */
