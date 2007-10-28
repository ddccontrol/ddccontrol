/*
    ddc/ci direct PCI memory interface for VIA Unichrome cards

    Based on nvidia.c and via_i2c  from the openchrome Project openchrome@sourceforge.net
    and the savagefb-i2c.c from the Kernel sources

    Copyright 2006 Johannes Deisenhofer j.deisenhofer@tomorrow-focus.de
    Copyright 2004 The Unichrome Project  [unichrome.sf.net]
    Copyright 1998-2003 VIA Technologies, Inc. All Rights Reserved.
    Copyright 2001-2003 S3 Graphics, Inc. All Rights Reserved.
    Copyright 2004 Nicolas Boichat (nicolas@boichat.ch)
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
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "ddcpci.h"
 
struct mem_data {
	int fd; // /dev/mem fd
	char* memory; // mmaped memory
	size_t length; // mmaped memory length
};

struct i2c_data {
	char* PCIO;
	int ddc_base;
};

typedef unsigned char  U008;

#define VGA_WR08(p,i,d)  (((U008 *)(p))[i]=(d))
#define VGA_RD08(p,i)    (((U008 *)(p))[i])

#define VGA_SR_IX       0x3c4
#define VGA_SR_DATA     0x3c5

#define UNICHROME_I2C_ENAB      0x01
#define UNICHROME_I2C_SCL_OUT   0x20
#define UNICHROME_I2C_SDA_OUT   0x10
#define UNICHROME_I2C_SCL_IN    0x08
#define UNICHROME_I2C_SDA_IN    0x04


static void via_gpio_setscl(void* data, int state)
{
	u32			val;

	VGA_WR08(((struct i2c_data*)data)->PCIO, VGA_SR_IX, ((struct i2c_data*)data)->ddc_base );
	val = VGA_RD08(((struct i2c_data*)data)->PCIO, VGA_SR_DATA);
	val |= UNICHROME_I2C_ENAB;

	if (state)
		val |= UNICHROME_I2C_SCL_OUT;
	else
		val &= ~UNICHROME_I2C_SCL_OUT;

	VGA_WR08(((struct i2c_data*)data)->PCIO, VGA_SR_DATA, val);
}

static void via_gpio_setsda(void* data, int state)
{
	u32			val;

	VGA_WR08(((struct i2c_data*)data)->PCIO, VGA_SR_IX, ((struct i2c_data*)data)->ddc_base );
	val = VGA_RD08(((struct i2c_data*)data)->PCIO, VGA_SR_DATA);
	val |= UNICHROME_I2C_ENAB;

	if (state)
		val |= UNICHROME_I2C_SDA_OUT;
	else
		val &= ~UNICHROME_I2C_SDA_OUT;

	VGA_WR08(((struct i2c_data*)data)->PCIO, VGA_SR_DATA, val);
}

static int via_gpio_getscl(void* data)
{
	u32			val = 0;

	VGA_WR08(((struct i2c_data*)data)->PCIO, VGA_SR_IX, ((struct i2c_data*)data)->ddc_base);
	if (VGA_RD08(((struct i2c_data*)data)->PCIO, VGA_SR_DATA) & UNICHROME_I2C_SCL_IN)
		val = 1;

	return val;
}

static int via_gpio_getsda(void* data)
{
	u32			val = 0;

	VGA_WR08(((struct i2c_data*)data)->PCIO, VGA_SR_IX, ((struct i2c_data*)data)->ddc_base);
	if (VGA_RD08(((struct i2c_data*)data)->PCIO, VGA_SR_DATA) & UNICHROME_I2C_SDA_IN)
		val = 1;

	return val;
}

static int init_i2c_bus(struct i2c_algo_bit_data* algo, char* PCIO, int ddc_base)
{
	//fprintf(stderr, "init_i2c_bus: (ddc_base: %#x)\n", ddc_base);
	struct i2c_data* data = malloc(sizeof(struct i2c_data));
	if (!data) {
		fprintf(stderr, _("via.c:init_i2c_bus: Malloc error."));
		exit(-1);
	}
	data->PCIO = PCIO;
	data->ddc_base = ddc_base;
	
	algo->setsda		= via_gpio_setsda;
	algo->setscl		= via_gpio_setscl;
	algo->getsda		= via_gpio_getsda;
	algo->getscl		= via_gpio_getscl;
	algo->udelay		= 40;
	algo->timeout		= 100;
	algo->data 		= data;
	
	/* Raise SCL and SDA */
	via_gpio_setsda(algo->data, 1);
	via_gpio_setscl(algo->data, 1);
	usleep(200000);
	
	//test_bus(algo, "Boubou");
	
	return 1;
}


struct card* via_open(struct pci_dev *dev)
{
	if (dev->vendor_id != 0x1106) {
		return 0;
	}
/*       switch (dev->device_id) {
        case 0x3122: // PCI_DEVICE_ID_VIA_UNICHROME
        case 0x3118: // PCI_DEVICE_ID_VIA_UNICHROME_PRO
                break;
        default:
                return 0;
        }*/

	
	struct card* via_card = malloc(sizeof(struct card));
	struct mem_data* data = malloc(sizeof(struct mem_data));
	if ((!via_card) || (!data)) {
		fprintf(stderr, _("%s: Malloc error.\n"), "via_open");
		exit(-1);
	}
	memset(via_card, 0, sizeof(struct card));
	
	via_card->data = data;
	
	data->fd = open("/dev/mem", O_RDWR);
	
	if (data->fd < 0) {
		perror(_("via_open: cannot open /dev/mem"));
		via_close(via_card);
		return 0;
	}
	
	data->length = 0x00001000;
	data->memory = mmap(data->memory, data->length, PROT_READ|PROT_WRITE, MAP_SHARED, data->fd, dev->base_addr[1]+0x8000 );

	if (data->memory == MAP_FAILED) {
		perror(_("via_open: mmap failed"));
		via_close(via_card);
		return 0;
	}
	
	char* PCIO = data->memory;

	// Note: There is a third bus, which uses the GPIO bits. This one is not supported here!

	via_card->nbusses = 2;
	via_card->i2c_busses = malloc(2*sizeof(struct i2c_algo_bit_data));
	init_i2c_bus(&via_card->i2c_busses[0], PCIO, 0x26);
	init_i2c_bus(&via_card->i2c_busses[1], PCIO, 0x31);
	
	return via_card;
}

void via_close(struct card* via_card)
{
	int i;
	
	if (via_card->data) {
		struct mem_data* data = via_card->data;
		if ((data->memory) && (data->memory == MAP_FAILED) && (data->length)) {
			munmap(data->memory, data->length);
		}
		free(via_card->data);
	}
	
	for (i = 0; i < via_card->nbusses; i++) {
		free((&via_card->i2c_busses[i])->data);
	}
	
	free(via_card->i2c_busses);
	
	free(via_card);
}

