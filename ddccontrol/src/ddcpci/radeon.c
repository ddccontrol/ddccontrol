/*
 *  ddc/ci direct PCI memory interface for ATI cards
 *  Copyright(c) 2005 Christian Schilling
 *
 *  Based on radeon_i2c.c from the kernel source.
 * 
 *  Original copyright:
 *
 *	framebuffer driver for ATI Radeon chipset video boards
 *
 *	Copyright 2003	Ben. Herrenschmidt <benh@kernel.crashing.org>
 *	Copyright 2000	Ani Joshi <ajoshi@kernel.crashing.org>
 *
 *	i2c bits from Luca Tettamanti <kronos@kronoz.cjb.net>
 *	
 *	Special thanks to ATI DevRel team for their hardware donations.
 *
 *	...Insert GPL boilerplate here...
 *
 *	Significant portions of this driver apdated from XFree86 Radeon
 *	driver which has the following copyright notice:
 *
 *	Copyright 2000 ATI Technologies Inc., Markham, Ontario, and
 *                     VA Linux Systems Inc., Fremont, California.
 *
 *	All Rights Reserved.
 *
 *	Permission is hereby granted, free of charge, to any person obtaining
 *	a copy of this software and associated documentation files (the
 *	"Software"), to deal in the Software without restriction, including
 *	without limitation on the rights to use, copy, modify, merge,
 *	publish, distribute, sublicense, and/or sell copies of the Software,
 *	and to permit persons to whom the Software is furnished to do so,
 *	subject to the following conditions:
 *
 *	The above copyright notice and this permission notice (including the
 *	next paragraph) shall be included in all copies or substantial
 *	portions of the Software.
 *
 *	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * 	EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *	MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *	NON-INFRINGEMENT.  IN NO EVENT SHALL ATI, VA LINUX SYSTEMS AND/OR
 *	THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *	WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *	DEALINGS IN THE SOFTWARE.
 *
 *	XFree86 driver authors:
 *
 *	   Kevin E. Martin <martin@xfree86.org>
 *	   Rickard E. Faith <faith@valinux.com>
 *	   Alan Hourihane <alanh@fairlite.demon.co.uk>
 *
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



#define RADEON_REGSIZE			0x4000

/* GPIO bit constants */
#define GPIO_A_0                (1 <<  0)
#define GPIO_A_1                (1 <<  1)
#define GPIO_Y_0                (1 <<  8)
#define GPIO_Y_1                (1 <<  9)
#define GPIO_EN_0               (1 << 16)
#define GPIO_EN_1               (1 << 17)
#define GPIO_MASK_0             (1 << 24)
#define GPIO_MASK_1             (1 << 25)
#define VGA_DDC_DATA_OUTPUT     GPIO_A_0
#define VGA_DDC_CLK_OUTPUT      GPIO_A_1
#define VGA_DDC_DATA_INPUT      GPIO_Y_0
#define VGA_DDC_CLK_INPUT       GPIO_Y_1
#define VGA_DDC_DATA_OUT_EN     GPIO_EN_0
#define VGA_DDC_CLK_OUT_EN      GPIO_EN_1


/* Radeons i2c busses */
#define GPIO_VGA_DDC                           0x0060  
#define GPIO_DVI_DDC                           0x0064  
#define GPIO_MONID                             0x0068  
#define GPIO_CRT2_DDC                          0x006c


static inline unsigned int readl(const volatile void *addr)
{
	return *(volatile unsigned int*) addr;
}

static inline void writel(unsigned int b, volatile void *addr)
{
	*(volatile unsigned int*) addr = b;
}

#define INREG(addr)		readl((ldata->PCIO)+addr)
#define OUTREG(addr,val)	writel(val, (ldata->PCIO)+addr)

struct mem_data {
	int fd; // /dev/mem fd
	char* memory; // mmaped memory
	size_t length; // mmaped memory length
};

struct i2c_data {
	char* PCIO;
	int ddc_base;
};


static void radeon_gpio_setscl(void* data, int state)
{
	struct i2c_data *ldata = data;
	u32			val;
	
	val = INREG(ldata->ddc_base) & ~(VGA_DDC_CLK_OUT_EN);
	if (!state)
		val |= VGA_DDC_CLK_OUT_EN;

	OUTREG(ldata->ddc_base, val);
	(void)INREG(ldata->ddc_base);
}

static void radeon_gpio_setsda(void* data, int state)
{
	struct i2c_data 	*ldata = data;
	u32			val;
	
	val = INREG(ldata->ddc_base) & ~(VGA_DDC_DATA_OUT_EN);
	if (!state)
		val |= VGA_DDC_DATA_OUT_EN;

	OUTREG(ldata->ddc_base, val);
	(void)INREG(ldata->ddc_base);
}

static int radeon_gpio_getscl(void* data)
{
	struct i2c_data 	*ldata = data;
	u32			val;
	
	val = INREG(ldata->ddc_base);

	return (val & VGA_DDC_CLK_INPUT) ? 1 : 0;
}

static int radeon_gpio_getsda(void* data)
{
	struct i2c_data 	*ldata = data;
	u32			val;
	
	val = INREG(ldata->ddc_base);

	return (val & VGA_DDC_DATA_INPUT) ? 1 : 0;
}

static int radeon_setup_i2c_bus(struct i2c_algo_bit_data* algo, char* PCIO, int ddc_base)
{
//fprintf(stderr, "init_i2c_bus: (ddc_base: %#x)\n", ddc_base);
	struct i2c_data* data = malloc(sizeof(struct i2c_data));
	if (!data) {
		fprintf(stderr, _("radeon.c:init_i2c_bus: Malloc error."));
		exit(-1);
	}
	data->PCIO = PCIO;
	data->ddc_base = ddc_base;
	
	algo->setsda		= radeon_gpio_setsda;
	algo->setscl		= radeon_gpio_setscl;
	algo->getsda		= radeon_gpio_getsda;
	algo->getscl		= radeon_gpio_getscl;
	algo->udelay		= 40;
	algo->timeout		= 100;
	algo->data 		= data;	
	
	
	/* Raise SCL and SDA */
	radeon_gpio_setsda(data, 1);
	radeon_gpio_setscl(data, 1);
	usleep(200000);


//	test_bus(algo, "Radeon");
	return 1;
}


struct card* radeon_open(struct pci_dev *dev)
{
	if (dev->vendor_id != 0x1002) {
		return 0;
	}

	
	struct card* radeon_card = malloc(sizeof(struct card));
	struct mem_data* data = malloc(sizeof(struct mem_data));
	if ((!radeon_card) || (!data)) {
		fprintf(stderr, _("%s: Malloc error.\n"), "radeon_open");
		exit(-1);
	}
	memset(radeon_card, 0, sizeof(struct card));


	
	radeon_card->data = data;
	
	data->fd = open("/dev/mem", O_RDWR);
	
	if (data->fd < 0) {
		perror(_("radeon_open: cannot open /dev/mem"));
		radeon_close(radeon_card);
		return 0;
	}
	
	data->length = RADEON_REGSIZE;
	data->memory = mmap(data->memory, data->length, PROT_READ|PROT_WRITE, MAP_SHARED, data->fd, dev->base_addr[2]);

	if (data->memory == MAP_FAILED) {
		perror(_("radeon_open: mmap failed"));
		radeon_close(radeon_card);
		return 0;
	}
	
	
	char* PCIO = data->memory;
	
	radeon_card->nbusses = 4;
	radeon_card->i2c_busses = malloc(4*sizeof(struct i2c_algo_bit_data));
	radeon_setup_i2c_bus(&radeon_card->i2c_busses[0], PCIO, GPIO_VGA_DDC);
	radeon_setup_i2c_bus(&radeon_card->i2c_busses[1], PCIO, GPIO_DVI_DDC);
	radeon_setup_i2c_bus(&radeon_card->i2c_busses[2], PCIO, GPIO_MONID);
	radeon_setup_i2c_bus(&radeon_card->i2c_busses[3], PCIO, GPIO_CRT2_DDC);
	
	return radeon_card;
}


void radeon_close(struct card* radeon_card)
{
	int i;
	
	if (radeon_card) {
		struct mem_data* data = radeon_card->data;
		if ((data->memory) && (data->memory == MAP_FAILED) && (data->length)) {
			munmap(data->memory, data->length);
		}
		free(radeon_card->data);
	}
	
	for (i = 0; i < radeon_card->nbusses; i++) {
		free((&radeon_card->i2c_busses[i])->data);
	}
	
	free(radeon_card->i2c_busses);
	
	free(radeon_card);

}
