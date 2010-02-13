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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#ifndef DDCPCI_H
#define DDCPCI_H

#include <pci/pci.h>

#include "i2c-algo-bit.h"

#include <libintl.h>
#include <locale.h>
#define _(String) gettext (String)
#define gettext_noop(String) String
#define N_(String) gettext_noop (String)

struct card {
	void* data;
	int nbusses;
	struct i2c_algo_bit_data* i2c_busses;
};

int get_verbosity();

typedef struct card* (*card_open)(struct pci_dev*);
typedef void (*card_close)(struct card*);

/* nVidia functions */
struct card* nvidia_open (struct pci_dev *dev);
void         nvidia_close(struct card* nvidia_card);

/* nVidia functions */
struct card* radeon_open (struct pci_dev *dev);
void         radeon_close(struct card* radeon_card);

/* Intel 810 functions */
struct card* i810_open (struct pci_dev *dev);
void         i810_close(struct card* intel810_card);

/* Via Unichrome functions */

struct card* via_open(struct pci_dev*dev);
void	     via_close(struct card* via_card);

/* SIS functions */
struct card* sis_open(struct pci_dev*dev);
void	     sis_close(struct card* sis_card);

#endif //DDCPCI_H
