/* ------------------------------------------------------------------------- */
/* i2c-algo-bit.h i2c driver algorithms for bit-shift adapters               */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 1995-99 Simon G. Vogl

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
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                */
/* ------------------------------------------------------------------------- */

/* With some changes from Kyösti Mälkki <kmalkki@cc.hut.fi> and even
   Frodo Looijaard <frodol@dds.nl> */

/* Adapted to user space by Nicolas Boichat <nicolas@boichat.ch> */

/* $Id$ */

#ifndef I2C_ALGO_BIT_H
#define I2C_ALGO_BIT_H

#include "config.h"
#include "lib/i2c-dev.h"

// Old headers (2.4) doesn't define some constants
#ifndef I2C_M_NO_RD_ACK
#define I2C_M_NO_RD_ACK 0x0800
#endif
#ifndef I2C_M_IGNORE_NAK
#define I2C_M_IGNORE_NAK 0x1000
#endif

/* --- Defines for bit-adapters ---------------------------------------	*/
/*
 * This struct contains the hw-dependent functions of bit-style adapters to 
 * manipulate the line states, and to init any hw-specific features. This is
 * only used if you have more than one hw-type of adapter running. 
 */
struct i2c_algo_bit_data {
	void *data;		/* private data for lowlevel routines */
	void (*setsda) (void *data, int state);
	void (*setscl) (void *data, int state);
	int  (*getsda) (void *data);
	int  (*getscl) (void *data);

	/* local settings */
	int udelay;		/* half-clock-cycle time in microsecs */
				/* i.e. clock is (500 / udelay) KHz */
	int timeout;		/* in microsecs */
};

#define I2C_BIT_ADAP_MAX	16

int bit_xfer(struct i2c_algo_bit_data *adap, struct i2c_msg msgs[], int num);
int test_bus(struct i2c_algo_bit_data *adap, char* name);

//int i2c_bit_add_bus(struct i2c_adapter *);
//int i2c_bit_del_bus(struct i2c_adapter *);

#endif /* I2C_ALGO_BIT_H */
