/*
    ddc/ci direct PCI memory interface for nvidia cards
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

static void riva_gpio_setscl(void* data, int state)
{
	u32			val;

	VGA_WR08(((struct i2c_data*)data)->PCIO, 0x3d4, 0x1f);
	VGA_WR08(((struct i2c_data*)data)->PCIO, 0x3d5, 0x57);
	VGA_WR08(((struct i2c_data*)data)->PCIO, 0x3d4, 0x44);
    val = VGA_RD08(((struct i2c_data*)data)->PCIO, 0x3d5) & 0xf0;

	VGA_WR08(((struct i2c_data*)data)->PCIO, 0x3d4, ((struct i2c_data*)data)->ddc_base + 1);
	val = VGA_RD08(((struct i2c_data*)data)->PCIO, 0x3d5) & 0xf0;

	if (state)
		val |= 0x20;
	else
		val &= ~0x20;

	VGA_WR08(((struct i2c_data*)data)->PCIO, 0x3d4, ((struct i2c_data*)data)->ddc_base + 1);
	VGA_WR08(((struct i2c_data*)data)->PCIO, 0x3d5, val | 0x1);
}

static void riva_gpio_setsda(void* data, int state)
{
	u32			val;

	VGA_WR08(((struct i2c_data*)data)->PCIO, 0x3d4, 0x1f);
	VGA_WR08(((struct i2c_data*)data)->PCIO, 0x3d5, 0x57);
	VGA_WR08(((struct i2c_data*)data)->PCIO, 0x3d4, 0x44);
    val = VGA_RD08(((struct i2c_data*)data)->PCIO, 0x3d5) & 0xf0;

	VGA_WR08(((struct i2c_data*)data)->PCIO, 0x3d4, ((struct i2c_data*)data)->ddc_base + 1);
	val = VGA_RD08(((struct i2c_data*)data)->PCIO, 0x3d5) & 0xf0;

	if (state)
		val |= 0x10;
	else
		val &= ~0x10;

	VGA_WR08(((struct i2c_data*)data)->PCIO, 0x3d4, ((struct i2c_data*)data)->ddc_base + 1);
	VGA_WR08(((struct i2c_data*)data)->PCIO, 0x3d5, val | 0x1);
}

static int riva_gpio_getscl(void* data)
{
	u32			val = 0;

	VGA_WR08(((struct i2c_data*)data)->PCIO, 0x3d4, ((struct i2c_data*)data)->ddc_base);
	if (VGA_RD08(((struct i2c_data*)data)->PCIO, 0x3d5) & 0x04)
		val = 1;

//	val = VGA_RD08(((struct i2c_data*)data)->PCIO, 0x3d5);

	return val;
}

static int riva_gpio_getsda(void* data)
{
	u32			val = 0;

	VGA_WR08(((struct i2c_data*)data)->PCIO, 0x3d4, ((struct i2c_data*)data)->ddc_base);
	if (VGA_RD08(((struct i2c_data*)data)->PCIO, 0x3d5) & 0x08)
		val = 1;

	return val;
}

static int init_i2c_bus(struct i2c_algo_bit_data* algo, char* PCIO, int ddc_base)
{
	//fprintf(stderr, "init_i2c_bus: (ddc_base: %#x)\n", ddc_base);
	struct i2c_data* data = malloc(sizeof(struct i2c_data));
	if (!data) {
		fprintf(stderr, _("nvidia.c:init_i2c_bus: Malloc error."));
		exit(-1);
	}
	data->PCIO = PCIO;
	data->ddc_base = ddc_base;
	
	algo->setsda		= riva_gpio_setsda;
	algo->setscl		= riva_gpio_setscl;
	algo->getsda		= riva_gpio_getsda;
	algo->getscl		= riva_gpio_getscl;
	algo->udelay		= 40;
	algo->timeout		= 100;
	algo->data 		= data;
	
	/* Raise SCL and SDA */
	riva_gpio_setsda(algo->data, 1);
	riva_gpio_setscl(algo->data, 1);
	usleep(200000);
	
	//test_bus(algo, "Boubou");
	
	return 1;
}

struct card* nvidia_open(struct pci_dev *dev)
{
	if (dev->vendor_id != 0x10de) {
		return 0;
	}
	
	struct card* nvidia_card = malloc(sizeof(struct card));
	struct mem_data* data = malloc(sizeof(struct mem_data));
	if ((!nvidia_card) || (!data)) {
		fprintf(stderr, _("%s: Malloc error.\n"), "nvidia_open");
		exit(-1);
	}
	memset(nvidia_card, 0, sizeof(struct card));
	
	nvidia_card->data = data;
	
	data->fd = open("/dev/mem", O_RDWR);
	
	if (data->fd < 0) {
		perror(_("nvidia_open: cannot open /dev/mem"));
		nvidia_close(nvidia_card);
		return 0;
	}
	
	data->memory = NULL;
	data->length = 0x00001000;
	data->memory = mmap(data->memory, data->length, PROT_READ|PROT_WRITE, MAP_SHARED, data->fd, dev->base_addr[0] + 0x00601000);
	
	if (data->memory == MAP_FAILED) {
		perror(_("nvidia_open: mmap failed"));
		nvidia_close(nvidia_card);
		return 0;
	}
	
	char* PCIO = data->memory;
	
	nvidia_card->nbusses = 3;
	nvidia_card->i2c_busses = malloc(3*sizeof(struct i2c_algo_bit_data));
	init_i2c_bus(&nvidia_card->i2c_busses[0], PCIO, 0x50);
	init_i2c_bus(&nvidia_card->i2c_busses[1], PCIO, 0x36);
	init_i2c_bus(&nvidia_card->i2c_busses[2], PCIO, 0x3e);
	
	//fprintf(stderr, "nvidia_open: OK\n");
	return nvidia_card;
}

void nvidia_close(struct card* nvidia_card)
{
	int i;
	
	if (nvidia_card->data) {
		struct mem_data* data = nvidia_card->data;
		if ((data->memory) && (data->memory == MAP_FAILED) && (data->length)) {
			munmap(data->memory, data->length);
		}
		free(nvidia_card->data);
	}
	
	for (i = 0; i < nvidia_card->nbusses; i++) {
		free((&nvidia_card->i2c_busses[i])->data);
	}
	
	free(nvidia_card->i2c_busses);
	
	free(nvidia_card);
}

