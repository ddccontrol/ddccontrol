/*
    ddc/ci i2c-dev header
    Copyright(c) 2004 Nicolas Boichat (nicolas@boichat.ch)
    Copyright (C) 1995-2000 Simon G. Vogl (original i2c.h headers)

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

#ifndef DDCCONTROL_I2C_DEV_H
#define DDCCONTROL_I2C_DEV_H

/* If linux/i2c-dev.h is usable, use it, otherwise, define
 * the required constants and structures. */

#ifdef __FreeBSD__

#include <sys/types.h>
#include <dev/iicbus/iic.h>

#define I2C_M_RD   IIC_M_RD
#define I2C_RDWR   I2CRDWR

#define i2c_msg iic_msg
#define i2c_rdwr_ioctl_data iic_rdwr_data

#else
#if HAVE_BUGGY_I2C_DEV
#include <linux/types.h>

/*
 * I2C Message - used for pure i2c transaction, also from /dev interface
 */
struct i2c_msg {
	__u16 addr;	/* slave address			*/
 	__u16 flags;		
#define I2C_M_TEN	0x10	/* we have a ten bit chip address	*/
#define I2C_M_RD	0x01
#define I2C_M_NOSTART	0x4000
#define I2C_M_REV_DIR_ADDR	0x2000
#define I2C_M_IGNORE_NAK	0x1000
#define I2C_M_NO_RD_ACK		0x0800
 	__u16 len;		/* msg length				*/
 	__u8 *buf;		/* pointer to msg data			*/
};

#define I2C_RDWR	0x0707	/* Combined R/W transfer (one stop only)*/

/* This is the structure as used in the I2C_RDWR ioctl call */
struct i2c_rdwr_ioctl_data {
	struct i2c_msg *msgs;		/* pointers to i2c_msgs */
	__u32 nmsgs;			/* number of i2c_msgs */
};

#else

#include <linux/types.h>
#include <linux/i2c-dev.h>

#endif
#endif // __FreeBSD__

#endif //DDCCONTROL_I2C_DEV_H
