/*
    ddc/ci direct PCI memory interface for some SIS video cards

    Copyright 2006 Johannes Deisenhofer j.deisenhofer@tomorrow-focus.de
    Based on nvidia.c and the sis framebuffer driver from the Kernel source.
	Copyright (C) 2001-2005 by Thomas Winischhofer, Vienna, Austria

    Copyright(c) 2004 Nicolas Boichat (nicolas@boichat.ch)
    Based on rivafb-i2c.c from the kernel source.
    Copyright 2004 Antonino A. Daplas <adaplas @pol.net>
    
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
#include <sys/io.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "ddcpci.h"
 
struct mem_data {
	char* memory; // mmaped memory
	size_t length; // mmaped memory length
};

struct i2c_data {
	unsigned short int ddc_base;
	u32		   cstate; // Register_state
};

#define VGA_WR08(p,i,d)  (outb(i,d))
#define VGA_RD08(p,i)    (inb(i))

#define VGA_SR_IX       0x3c4
#define VGA_SR_DATA     0x3c5

// There is only one bus probed. More busses are probably accessible in the same register,
// probably in pairs at bit positions 2/3, 4/5 

#define SIS_I2C_SCL     0x01
#define SIS_I2C_SDA     0x02


static void sis_gpio_setscl(void* data, int state)
{
	u32			val;
	struct i2c_data*	ldata = (struct i2c_data*)data;

	outb(ldata->ddc_base,VGA_SR_IX );
	val = inb(VGA_SR_DATA) & 0xfc;

	if (state)
		ldata->cstate |= SIS_I2C_SCL;
	else
		ldata->cstate &= ~SIS_I2C_SCL;

	val |= ldata->cstate;

	outb(val ,VGA_SR_DATA);
}

static void sis_gpio_setsda(void* data, int state)
{
	u32	val;
	struct i2c_data*	ldata = (struct i2c_data*)data;

	outb(ldata->ddc_base, VGA_SR_IX );
	val = inb(VGA_SR_DATA) & 0xfc;

	if (state)
		ldata->cstate |= SIS_I2C_SDA;
	else
		ldata->cstate &= ~SIS_I2C_SDA;

	val |= ldata->cstate;

	outb(val, VGA_SR_DATA);
}

static int sis_gpio_getscl(void* data)
{
	u32			val = 0;
	struct i2c_data*	ldata = (struct i2c_data*)data;

	outb(ldata->ddc_base, VGA_SR_IX);
	if (inb(VGA_SR_DATA) & SIS_I2C_SCL)
		val = 1;

	return val;
}

static int sis_gpio_getsda(void* data)
{
	u32			val = 0;
	struct 	i2c_data*	ldata = (struct i2c_data*)data;

	outb( ldata->ddc_base, VGA_SR_IX);
	if (inb(VGA_SR_DATA) & SIS_I2C_SDA)
		val = 1;

	return val;
}

static int init_i2c_bus(struct i2c_algo_bit_data* algo, char* PCIO, int ddc_base)
{
	//fprintf(stderr, "init_i2c_bus: (ddc_base: %#x)\n", ddc_base);
	// Request io Permission
	if(ioperm(VGA_SR_IX,2,1)<0)
	{
		perror("Request IO Perm");
		return 0;
	}
	struct i2c_data* data = malloc(sizeof(struct i2c_data));
	if (!data) {
		fprintf(stderr, _("sis.c:init_i2c_bus: Malloc error."));
		exit(-1);
	}
	data->ddc_base = ddc_base;
	data->cstate   = 0;	
	
	algo->setsda		= sis_gpio_setsda;
	algo->setscl		= sis_gpio_setscl;
	algo->getsda		= sis_gpio_getsda;
	algo->getscl		= sis_gpio_getscl;
	algo->udelay		= 40;
	algo->timeout		= 100;
	algo->data 		= data;
	
	/* Raise SCL and SDA */
	sis_gpio_setsda(algo->data, 1);
	sis_gpio_setscl(algo->data, 1);
	usleep(200000);
	
	//test_bus(algo, "Boubou");
	
	return 1;
}


struct card* sis_open(struct pci_dev *dev)
{
	// SIS
	if (dev->vendor_id != 0x1039) {
		return 0;
	}
/*       switch (dev->device_id) {
        case 0x6330: // PCI_DEVICE_ID_SIS_VGA_MIRAGE
                break;
        default:
                return 0;
        }*/

	
	struct card* sis_card = malloc(sizeof(struct card));
	if (!sis_card) {
		fprintf(stderr, _("%s: Malloc error.\n"), "sis_open");
		exit(-1);
	}
	memset(sis_card, 0, sizeof(struct card));
	
	sis_card->nbusses = 1;
	sis_card->i2c_busses = malloc(2*sizeof(struct i2c_algo_bit_data));

	// Note: Only first Bus is accessible
	init_i2c_bus(&sis_card->i2c_busses[0], 0, 0x11);
	
	return sis_card;
}

void sis_close(struct card* sis_card)
{
	int i;
	
	if (sis_card->data) {
		struct mem_data* data = sis_card->data;
		if ((data->memory) && (data->memory == MAP_FAILED) && (data->length)) {
			munmap(data->memory, data->length);
		}
		free(sis_card->data);
	}
	
	for (i = 0; i < sis_card->nbusses; i++) {
		free((&sis_card->i2c_busses[i])->data);
	}
	
	free(sis_card->i2c_busses);
	
	free(sis_card);
}

