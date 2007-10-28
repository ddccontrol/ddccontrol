/*
	Note: this file is not built by Makefile

    ddc/ci direct PCI memory interface for some Intel chips (i740)
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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/io.h>
#include <unistd.h>

#include "ddcpci.h"
 
static void intel740_setscl(void* data, int state)
{
	outb(0x62, 0x3d6);
	u8 val = (inb(0x3d7) & 0xF7); //Remove SCL bit
	if (state) {
		val = val | 0x08;
	}
	outb(0x62, 0x3d6);
	outb(val,  0x3d7);
}

static void intel740_setsda(void* data, int state)
{
	outb(0x62, 0x3d6);
	u8 val = (inb(0x3d7) & 0xFB); //Remove SDA bit
	if (state) {
		val = val | 0x04;
	}
	outb(0x62, 0x3d6);
	outb(val,  0x3d7);
}

static int intel740_getscl(void* data)
{
	outb(0x63, 0x3d6);
	u8 val = inb(0x3d7);
	
	return ((val >> 3) & 0x01);
}

static int intel740_getsda(void* data)
{
	outb(0x63, 0x3d6);
	u8 val = inb(0x3d7);
	
	return ((val >> 2) & 0x01);
}

static int init_i2c_bus(struct i2c_algo_bit_data* algo)
{
	algo->setsda		= intel740_setsda;
	algo->setscl		= intel740_setscl;
	algo->getsda		= intel740_getsda;
	algo->getscl		= intel740_getscl;
	algo->udelay		= 40;
	algo->timeout		= 100;
	algo->data 		= NULL;
	
	/* Raise SCL and SDA */
	intel740_setsda(algo->data, 1);
	intel740_setscl(algo->data, 1);
	usleep(200000);
	
	test_bus(algo, "i740");
	
	return 1;
}

struct card* intel740_open(struct pci_dev *dev)
{
    return 0;

	if (dev->vendor_id != 0x8086) {
		return 0;
	}
	
	struct card* intel740_card = malloc(sizeof(struct card));
	if (!intel740_card) {
		fprintf(stderr, _("%s: Malloc error.\n"), "intel740_open");
		exit(-1);
	}
	memset(intel740_card, 0, sizeof(struct card));
	
	if (ioperm(0x3d6, 2, 1)) {
		perror(_("%s: ioperm failed"), "intel740_open");
		intel740_close(intel740_card);
		return 0;
	}
	
	switch (dev->device_id) {
	case 0x2572:
		intel740_card->nbusses = 1;
		intel740_card->i2c_busses = malloc(1*sizeof(struct i2c_algo_bit_data));
		init_i2c_bus(&intel740_card->i2c_busses[0]);
		break;
	default:
		fprintf(stderr, _("%s: Error: unknown card type (%#x)\n"), "intel740_open", dev->device_id);
		intel740_close(intel740_card);
		return 0;
	}
	
	return intel740_card;
}

void intel740_close(struct card* intel740_card)
{
	int i;
	
	ioperm(0x3d6, 2, 0);
	
	for (i = 0; i < intel740_card->nbusses; i++) {
		free((&intel740_card->i2c_busses[i])->data);
	}
	
	free(intel740_card->i2c_busses);
	
	free(intel740_card);
}

