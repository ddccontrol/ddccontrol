/*
    ddc/ci direct PCI memory interface for some Intel chipsets (855GM)
    Copyright(c) 2005 Nicolas Boichat (nicolas@boichat.ch)

    Completely experimental: Never tested.
    
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/io.h>
#include <unistd.h>

#include <pci/pci.h>
#include "ddcpci.h"
 
static void intel855_setscl(void* data, int state)
{
	outb(0x62, 0x3d6);
	u8 val = (inb(0x3d7) & 0xF7); //Remove SCL bit
	if (state) {
		val = val | 0x08;
	}
	outb(0x62, 0x3d6);
	outb(val,  0x3d7);
}

static void intel855_setsda(void* data, int state)
{
	outb(0x62, 0x3d6);
	u8 val = (inb(0x3d7) & 0xFB); //Remove SDA bit
	if (state) {
		val = val | 0x04;
	}
	outb(0x62, 0x3d6);
	outb(val,  0x3d7);
}

static int intel855_getscl(void* data)
{
	outb(0x63, 0x3d6);
	u8 val = inb(0x3d7);
	
	return ((val >> 3) & 0x01);
}

static int intel855_getsda(void* data)
{
	outb(0x63, 0x3d6);
	u8 val = inb(0x3d7);
	
	return ((val >> 2) & 0x01);
}

static int init_i2c_bus(struct i2c_algo_bit_data* algo)
{
	algo->setsda		= intel855_setsda;
	algo->setscl		= intel855_setscl;
	algo->getsda		= intel855_getsda;
	algo->getscl		= intel855_getscl;
	algo->udelay		= 40;
	algo->timeout		= 100;
	algo->data 		= NULL;
	
	/* Raise SCL and SDA */
	intel855_setsda(algo->data, 1);
	intel855_setscl(algo->data, 1);
	usleep(200000);
	
	test_bus(algo, "Boubou");
	
	return 1;
}

struct card* intel855_open(struct pci_dev *dev)
{
	return 0;
	
	if (dev->vendor_id != 0x0000) { //FIXME
		return 0;
	}
	
	struct card* intel855_card = malloc(sizeof(struct card));
	if (!intel855_card) {
		fprintf(stderr, "intel855_open: Malloc error.\n");
		exit(-1);
	}
	memset(intel855_card, 0, sizeof(struct card));
	
	if (ioperm(0x3d6, 2, 1)) {
		perror("intel855_open: Error: ioperm failed");
		intel855_close(intel855_card);
		return 0;
	}
	
	switch (dev->device_id) { //FIXME
	case 0x0000:
		//fprintf(stderr, "nvidia_open: initing 1 bus\n");
		intel855_card->nbusses = 1;
		intel855_card->i2c_busses = malloc(1*sizeof(struct i2c_algo_bit_data));
		init_i2c_bus(&intel855_card->i2c_busses[0]);
		break;
	default:
		//fprintf(stderr, "nvidia_open: Error: unknown card type (%#x)\n", (dev->device_id >> 4) & 0xff);
		intel855_close(intel855_card);
		return 0;
	}
	
	//fprintf(stderr, "nvidia_open: OK\n");
	return intel855_card;
}

void intel855_close(struct card* intel855_card)
{
	int i;
	
	ioperm(0x3d6, 2, 0);
	
	for (i = 0; i < intel855_card->nbusses; i++) {
		free((&intel855_card->i2c_busses[i])->data);
	}
	
	free(intel855_card->i2c_busses);
	
	free(intel855_card);
}

