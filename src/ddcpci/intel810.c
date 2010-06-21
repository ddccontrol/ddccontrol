/*
    ddc/ci direct PCI memory interface for intel i810/815/865...
    Copyright(c) 2005 Nicolas Boichat (nicolas@boichat.ch)

    Based on i810-i2c.c from the kernel source patch available
    at http://i810fb.sourceforge.net/.
    
    Copyright (C) 2004 Antonino Daplas <adaplas@pol.net>
    
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

    Doc : http://www.intel.com/design/chipsets/manuals/29802601.pdf
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/io.h>

#include "ddcpci.h"

#define SCL_DIR_MASK	0x0001
#define SCL_DIR			0x0002
#define SCL_VAL_MASK	0x0004
#define SCL_VAL_OUT		0x0008
#define SCL_VAL_IN		0x0010
#define SDA_DIR_MASK	0x0100
#define SDA_DIR			0x0200
#define SDA_VAL_MASK	0x0400
#define SDA_VAL_OUT		0x0800
#define SDA_VAL_IN		0x1000

static inline unsigned int readl(const volatile void *addr)
{
	return *(volatile unsigned int*) addr;
}

static inline void writel(unsigned int b, volatile void *addr)
{
	*(volatile unsigned int*) addr = b;
}

#define i810_readl(where, mmio) readl(mmio + where)
#define i810_writel(where, mmio, val) writel(val, mmio + where)

struct mem_data {
	int fd; // /dev/mem fd
	char* memory; // mmaped memory
	size_t length; // mmaped memory length
};

struct i2c_data {
	char* mmio;
	int gpio;
};

static void i810i2c_setscl(void *data, int state)
{
	i810_writel(((struct i2c_data*)data)->mmio, ((struct i2c_data*)data)->gpio, (state ? SCL_VAL_OUT : 0) | SCL_DIR | SCL_DIR_MASK | SCL_VAL_MASK);
	i810_readl(((struct i2c_data*)data)->mmio, ((struct i2c_data*)data)->gpio);	/* flush posted write */
}

static void i810i2c_setsda(void *data, int state)
{	
 	i810_writel(((struct i2c_data*)data)->mmio, ((struct i2c_data*)data)->gpio, (state ? SDA_VAL_OUT : 0) | SDA_DIR | SDA_DIR_MASK | SDA_VAL_MASK);
	i810_readl(((struct i2c_data*)data)->mmio, ((struct i2c_data*)data)->gpio);	/* flush posted write */
}

static int i810i2c_getscl(void *data)
{
	i810_writel(((struct i2c_data*)data)->mmio, ((struct i2c_data*)data)->gpio, SCL_DIR_MASK);
	i810_writel(((struct i2c_data*)data)->mmio, ((struct i2c_data*)data)->gpio, 0);
	return (0 != (i810_readl(((struct i2c_data*)data)->mmio, ((struct i2c_data*)data)->gpio) & SCL_VAL_IN));
}

static int i810i2c_getsda(void *data)
{
	i810_writel(((struct i2c_data*)data)->mmio, ((struct i2c_data*)data)->gpio, SDA_DIR_MASK);
	i810_writel(((struct i2c_data*)data)->mmio, ((struct i2c_data*)data)->gpio, 0);
	return (0 != (i810_readl(((struct i2c_data*)data)->mmio, ((struct i2c_data*)data)->gpio) & SDA_VAL_IN));
}

static int init_i2c_bus(struct i2c_algo_bit_data* algo, char* mmio, int gpio)
{
	//fprintf(stderr, "init_i2c_bus: (ddc_base: %#x)\n", ddc_base);
	struct i2c_data* data = malloc(sizeof(struct i2c_data));
	if (!data) {
		fprintf(stderr, _("%s: Malloc error."), "intel810.c:init_i2c_bus");
		exit(-1);
	}
	data->mmio = mmio;
	data->gpio = gpio;
	
	algo->setsda		= i810i2c_setsda;
	algo->setscl		= i810i2c_setscl;
	algo->getsda		= i810i2c_getsda;
	algo->getscl		= i810i2c_getscl;
	algo->udelay		= 40;
	algo->timeout		= 100;
	algo->data 		= data;
	
	/* Raise SCL and SDA */
	i810i2c_setsda(algo->data, 1);
	i810i2c_setscl(algo->data, 1);
	usleep(200000);
	
	//test_bus(algo, "i810");
	
	return 1;
}

struct card* i810_open(struct pci_dev *dev)
{
	if (dev->vendor_id != 0x8086) { // PCI_VENDOR_ID_INTEL
		return 0;
	}
	
	switch (dev->device_id) {
	case 0x1102: // PCI_DEVICE_ID_INTEL_82815_100
	case 0x1112: // PCI_DEVICE_ID_INTEL_82815_NOAGP
	case 0x1132: // 82815 CGC [Chipset Graphics Controller]
	case 0x2562: // 82845G/GL[Brookdale-G]/GE Chipset Integrated Graphics Device
	case 0x2572: // 82865G Integrated Graphics Controller [TESTED]
	case 0x2582: // 82915G/GV/910GL Express Chipset Family Graphics Controller
	case 0x2592: // Mobile 915GM/GMS/910GML Express Graphics Controller
	case 0x2772: // 945G Integrated Graphics Controller
	case 0x2776: // 945G Integrated Graphics Controller
	case 0x2782: // 82915G Express Chipset Family Graphics Controller
	case 0x2792: // Mobile 915GM/GMS/910GML Express Graphics Controller
	case 0x27AE: // Mobile 945GME Express Graphics Controller
	case 0x3577: // 82830 CGC [Chipset Graphics Controller]
	case 0x3582: // 82852/855GM Integrated Graphics Device
	case 0x7121: // 82810 CGC [Chipset Graphics Controller]
	case 0x7123: // 82810 DC-100 CGC [Chipset Graphics Controller]
	case 0x7125: // 82810E DC-133 CGC [Chipset Graphics Controller]
		break;
	default:
		return 0;
	}
	
	
	struct card* i810_card = malloc(sizeof(struct card));
	struct mem_data* data = malloc(sizeof(struct mem_data));
	if ((!i810_card) || (!data)) {
		fprintf(stderr, _("%s: Malloc error.\n"), "i810_open");
		exit(-1);
	}
	memset(i810_card, 0, sizeof(struct card));
	
	i810_card->data = data;
	
	data->fd = open("/dev/mem", O_RDWR);
	
	if (data->fd < 0) {
		perror(_("i810_open: cannot open /dev/mem"));
		i810_close(i810_card);
		return 0;
	}
	
	data->length = 512*1024; //MMIO_SIZE
	data->memory = 0;
	
	int reg;
	int found = 0;
	
	for (reg = 0; reg < 6; reg++) {
		if (dev->size[reg] == data->length) {
			data->memory = mmap(data->memory, data->length, PROT_READ|PROT_WRITE, MAP_SHARED, data->fd, dev->base_addr[reg]);
			if (get_verbosity())
				printf("i810_open: Using region %d (%lx, %lx)\n", reg, (long unsigned int)dev->base_addr[reg], (long unsigned int)dev->size[reg]);
			found = 1;
			break;
		}
	}
	
	if (!found) { /* We did not find a region with a valid size, so we take the first one with a zero size (915G) */
		if (get_verbosity())
			printf("i810_open: Cannot find any 512k region, searching for a 0 one.\n");
		for (reg = 0; reg < 6; reg++) {
			if (dev->size[reg] == 0) {
				data->memory = mmap(data->memory, data->length, PROT_READ|PROT_WRITE, MAP_SHARED, data->fd, dev->base_addr[reg]);
				if (get_verbosity())
					printf("i810_open: Using region %d (%lx, %lx)\n", reg, (long unsigned int)dev->base_addr[reg], (long unsigned int)dev->size[reg]);
				found = 1;
				break;
			}
		}
		
		if (!found) {
			fprintf(stderr, _("i810_open: Error: cannot find any valid MMIO PCI region.\n"));
			i810_close(i810_card);
			return 0;
		}
	}
	
	if (data->memory == MAP_FAILED) {
		perror(_("i810_open: mmap failed"));
		i810_close(i810_card);
		return 0;
	}
	
	char* mmio = data->memory;
	
	/* I2C registers see http://mail.directfb.org/pipermail/directfb-dev/2005-July/000457.html */
	i810_card->nbusses = 6;
	i810_card->i2c_busses = malloc(6*sizeof(struct i2c_algo_bit_data));
	init_i2c_bus(&i810_card->i2c_busses[0], mmio, 0x05010); // GPIOA
	init_i2c_bus(&i810_card->i2c_busses[1], mmio, 0x05014); // GPIOB
	init_i2c_bus(&i810_card->i2c_busses[2], mmio, 0x05018); // GPIO"C"
	init_i2c_bus(&i810_card->i2c_busses[3], mmio, 0x0501C); // GPIO"D" (82865G DVI daughter card) 
	init_i2c_bus(&i810_card->i2c_busses[4], mmio, 0x05020); // GPIO"E"
	init_i2c_bus(&i810_card->i2c_busses[5], mmio, 0x05024); // GPIO"F"

	return i810_card;
}

void i810_close(struct card* i810_card)
{
	int i;
	
	if (i810_card->data) {
		struct mem_data* data = i810_card->data;
		if ((data->memory) && (data->memory == MAP_FAILED) && (data->length)) {
			munmap(data->memory, data->length);
		}
		free(i810_card->data);
	}
	
	for (i = 0; i < i810_card->nbusses; i++) {
		free((&i810_card->i2c_busses[i])->data);
	}
	
	free(i810_card->i2c_busses);
	
	free(i810_card);
}

