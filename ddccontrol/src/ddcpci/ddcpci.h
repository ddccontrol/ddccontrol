/*
    ddc/ci direct PCI memory interface - header file
    Copyright(c) 2004 Nicolas Boichat (nicolas@boichat.ch)

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

#ifndef DDCPCI_H
#define DDCPCI_H

#include <pci/pci.h>

#include "i2c-algo-bit.h"

struct card {
	void* data;
	int nbusses;
	struct i2c_algo_bit_data* i2c_busses;
};

typedef struct card* (*card_open)(struct pci_dev*);
typedef void (*card_close)(struct card*);

/* nVidia functions */
struct card* nvidia_open (struct pci_dev *dev);
void         nvidia_close(struct card* nvidia_card);

#endif //DDCPCI_H
