/* ------------------------------------------------------------------------- */
/* i2c-algo-bit.c i2c driver algorithms for bit-shift adapters		     */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 1995-2000 Simon G. Vogl

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
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.		     */
/* ------------------------------------------------------------------------- */

/* With some changes from Frodo Looijaard <frodol@dds.nl>, Ky�sti M�lkki
   <kmalkki@cc.hut.fi> and Jean Delvare <khali@linux-fr.org> */

/* Adapted to user space by Nicolas Boichat <nicolas@boichat.ch> */

#include <errno.h>
#include <stdio.h>

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>

#include "i2c-algo-bit.h"

#include <libintl.h>
#include <locale.h>
#define _(String) gettext (String)
#define gettext_noop(String) String
#define N_(String) gettext_noop (String)

/* ----- global defines ----------------------------------------------- */
#define DEB(x) if (i2c_debug>=1) x;
#define DEB2(x) if (i2c_debug>=2) x;
#define DEBSTAT(x) if (i2c_debug>=3) x; /* print several statistical values*/
#define DEBPROTO(x) if (i2c_debug>=9) { x; }
 	/* debug the protocol by showing transferred bits */

#define RETRIES 3

/* ----- global variables ---------------------------------------------	*/

/* module parameters:
 */
static int i2c_debug = 0;
static struct timeval lasttv;
static struct timeval newtv;

/* ----- sleep function ---------------------------------------------	*/

static inline void ssleep(const int usec) {
   gettimeofday(&lasttv, NULL);
   while (1) {
      gettimeofday(&newtv, NULL);
      if (((newtv.tv_usec - lasttv.tv_usec) + ((newtv.tv_sec - lasttv.tv_sec)*1000000)) > usec) {
         break;
      }
   }
}

/* --- setting states on the bus with the right timing: ---------------	*/

#define setsda(adap,val) adap->setsda(adap->data, val)
#define setscl(adap,val) adap->setscl(adap->data, val)
#define getsda(adap) adap->getsda(adap->data)
#define getscl(adap) adap->getscl(adap->data)

static inline void sdalo(struct i2c_algo_bit_data *adap)
{
	setsda(adap,0);
	ssleep(adap->udelay);
}

static inline void sdahi(struct i2c_algo_bit_data *adap)
{
	setsda(adap,1);
	ssleep(adap->udelay);
}

static inline void scllo(struct i2c_algo_bit_data *adap)
{
	setscl(adap,0);
	ssleep(adap->udelay);
}

/*
 * Raise scl line, and do checking for delays. This is necessary for slower
 * devices.
 */
static inline int sclhi(struct i2c_algo_bit_data *adap)
{
	setscl(adap,1);

	/* Not all adapters have scl sense line... */
	if (adap->getscl == NULL ) {
		ssleep(adap->udelay);
		return 0;
	}

	gettimeofday(&lasttv, NULL);
	gettimeofday(&newtv, NULL);

	while (! getscl(adap) ) {
 		/* the hw knows how to read the clock line,
 		 * so we wait until it actually gets high.
 		 * This is safer as some chips may hold it low
 		 * while they are processing data internally. 
 		 */
		gettimeofday(&newtv, NULL);
		if (((newtv.tv_usec - lasttv.tv_usec) + ((newtv.tv_sec - lasttv.tv_sec)*1000000)) > adap->timeout) {
			return -1;
		}
	}
	DEBSTAT(printf("waited %ld usecs\n", ((newtv.tv_usec - lasttv.tv_usec) + ((newtv.tv_sec - lasttv.tv_sec)*1000000))));
	ssleep(adap->udelay);
	return 0;
} 


/* --- other auxiliary functions --------------------------------------	*/
static void i2c_start(struct i2c_algo_bit_data *adap) 
{
	/* assert: scl, sda are high */
	DEBPROTO(printf("S "));
	sdalo(adap);
	scllo(adap);
}

static void i2c_repstart(struct i2c_algo_bit_data *adap) 
{
	/* scl, sda may not be high */
	DEBPROTO(printf(" Sr "));
	setsda(adap,1);
	sclhi(adap);
	ssleep(adap->udelay);
	
	sdalo(adap);
	scllo(adap);
}


static void i2c_stop(struct i2c_algo_bit_data *adap) 
{
	DEBPROTO(printf("P\n"));
	/* assert: scl is low */
	sdalo(adap);
	sclhi(adap); 
	sdahi(adap);
}



/* send a byte without start cond., look for arbitration, 
   check ackn. from slave */
/* returns:
 * 1 if the device acknowledged
 * 0 if the device did not ack
 * -1 if an error occurred (while raising the scl line)
 */
static int i2c_outb(struct i2c_algo_bit_data *adap, char c)
{
	int i;
	int sb;
	int ack;

	/* assert: scl is low */
	for ( i=7 ; i>=0 ; i-- ) {
		sb = c & ( 1 << i );
		setsda(adap,sb);
		ssleep(adap->udelay);
		DEBPROTO(printf("%d",sb!=0));
		if (sclhi(adap)<0) { /* timed out */
			sdahi(adap); /* we don't want to block the net */
			DEB2(printf(" i2c_outb: 0x%02x, timeout at bit #%d\n", c&0xff, i));
			return -1;
		};
		/* do arbitration here: 
		 * if ( sb && ! getsda(adap) ) -> ouch! Get out of here.
		 */
		setscl(adap, 0 );
		ssleep(adap->udelay);
	}
	sdahi(adap);
	if (sclhi(adap)<0){ /* timeout */
	    DEB2(printf(" i2c_outb: 0x%02x, timeout at ack\n", c&0xff));
	    return -1;
	};
	/* read ack: SDA should be pulled down by slave */
	ack=getsda(adap);	/* ack: sda is pulled low ->success.	 */
	DEB2(printf(" i2c_outb: 0x%02x , getsda() = %d\n", c & 0xff, ack));

	DEBPROTO( printf("[%2.2x]",c&0xff) );
	DEBPROTO(if (0==ack){ printf(" A ");} else printf(" NA ") );
	scllo(adap);
	return 0==ack;		/* return 1 if device acked	 */
	/* assert: scl is low (sda undef) */
}


static int i2c_inb(struct i2c_algo_bit_data *adap) 
{
	/* read byte via i2c port, without start/stop sequence	*/
	/* acknowledge is sent in i2c_read.			*/
	int i;
	unsigned char indata=0;

	/* assert: scl is low */
	sdahi(adap);
	for (i=0;i<8;i++) {
		if (sclhi(adap)<0) { /* timeout */
			DEB2(printf(" i2c_inb: timeout at bit #%d\n", 7-i));
			return -1;
		};
		indata *= 2;
		if ( getsda(adap) ) 
			indata |= 0x01;
		scllo(adap);
	}
	/* assert: scl is low */
	DEB2(printf("i2c_inb: 0x%02x\n", indata & 0xff));

	DEBPROTO(printf(" 0x%02x", indata & 0xff));
	return (int) (indata & 0xff);
}

/*
 * Sanity check for the adapter hardware - check the reaction of
 * the bus lines only if it seems to be idle.
 */
int test_bus(struct i2c_algo_bit_data *adap, char* name) {
	int scl,sda;

	if (adap->getscl==NULL)
		printf("i2c-algo-bit.o: Testing SDA only, "
			"SCL is not readable.\n");

	sda=getsda(adap);
	scl=(adap->getscl==NULL?1:getscl(adap));
	printf("i2c-algo-bit.o: (0) scl=%d, sda=%d\n",scl,sda);
	if (!scl || !sda ) {
		printf("i2c-algo-bit.o: %s seems to be busy.\n", name);
		goto bailout;
	}

	sdalo(adap);
	sda=getsda(adap);
	scl=(adap->getscl==NULL?1:getscl(adap));
	printf("i2c-algo-bit.o: (1) scl=%d, sda=%d\n",scl,sda);
	if ( 0 != sda ) {
		printf("i2c-algo-bit.o: SDA stuck high!\n");
		goto bailout;
	}
	if ( 0 == scl ) {
		printf("i2c-algo-bit.o: SCL unexpected low "
			"while pulling SDA low!\n");
		goto bailout;
	}		

	sdahi(adap);
	sda=getsda(adap);
	scl=(adap->getscl==NULL?1:getscl(adap));
	printf("i2c-algo-bit.o: (2) scl=%d, sda=%d\n",scl,sda);
	if ( 0 == sda ) {
		printf("i2c-algo-bit.o: SDA stuck low!\n");
		goto bailout;
	}
	if ( 0 == scl ) {
		printf("i2c-algo-bit.o: SCL unexpected low "
			"while pulling SDA high!\n");
		goto bailout;
	}

	scllo(adap);
	sda=getsda(adap);
	scl=(adap->getscl==NULL?0:getscl(adap));
	printf("i2c-algo-bit.o: (3) scl=%d, sda=%d\n",scl,sda);
	if ( 0 != scl ) {
		printf("i2c-algo-bit.o: SCL stuck high!\n");
		goto bailout;
	}
	if ( 0 == sda ) {
		printf("i2c-algo-bit.o: SDA unexpected low "
			"while pulling SCL low!\n");
		goto bailout;
	}
	
	sclhi(adap);
	sda=getsda(adap);
	scl=(adap->getscl==NULL?1:getscl(adap));
	printf("i2c-algo-bit.o: (4) scl=%d, sda=%d\n",scl,sda);
	if ( 0 == scl ) {
		printf("i2c-algo-bit.o: SCL stuck low!\n");
		goto bailout;
	}
	if ( 0 == sda ) {
		printf("i2c-algo-bit.o: SDA unexpected low "
			"while pulling SCL high!\n");
		goto bailout;
	}
	printf("i2c-algo-bit.o: %s passed test.\n",name);
	return 0;
bailout:
	sdahi(adap);
	sclhi(adap);
	return -1;
}

/* ----- Utility functions
 */

/* try_address tries to contact a chip for a number of
 * times before it gives up.
 * return values:
 * 1 chip answered
 * 0 chip did not answer
 * -x transmission error
 */
static inline int try_address(struct i2c_algo_bit_data *adap, unsigned char addr, int retries)
{
	int i,ret = -1;
	for (i=0;i<=retries;i++) {
		ret = i2c_outb(adap,addr);
		if (ret==1)
			break;	/* success! */
		i2c_stop(adap);
		ssleep(5/*adap->udelay*/);
		if (i==retries)  /* no success */
			break;
		i2c_start(adap);
		ssleep(adap->udelay);
	}
	DEB2(if (i)
	     printf("i2c-algo-bit.o: Used %d tries to %s client at 0x%02x : %s\n",
		    i+1, addr & 1 ? "read" : "write", addr>>1,
		    ret==1 ? "success" : ret==0 ? "no ack" : "failed, timeout?" )
	    );
	return ret;
}

int sendbytes(struct i2c_algo_bit_data *adap, struct i2c_msg *msg)
{
	unsigned char c;
	const unsigned char *temp = msg->buf;
	int count = msg->len;
	unsigned short nak_ok = msg->flags & I2C_M_IGNORE_NAK; 
	int retval;
	int wrcount=0;

	while (count > 0) {
		c = *temp;
		DEB2(printf("sendbytes: writing %2.2X\n", c&0xff));
		retval = i2c_outb(adap,c);
		if ((retval>0) || (nak_ok && (retval==0)))  { /* ok or ignored NAK */
			count--; 
			temp++;
			wrcount++;
		} else { /* arbitration or no acknowledge */
			printf(_("sendbytes: error - bailout.\n"));
			i2c_stop(adap);
			return (retval<0)? retval : -EFAULT;
			        /* got a better one ?? */
		}
#if 0
		/* from asm/delay.h */
		__delay(adap->mdelay * (loops_per_sec / 1000) );
#endif
	}
	return wrcount;
}

static inline int readbytes(struct i2c_algo_bit_data *adap, struct i2c_msg *msg)
{
	int inval;
	int rdcount=0;   	/* counts bytes read */
	unsigned char *temp = msg->buf;
	int count = msg->len;

	while (count > 0) {
		inval = i2c_inb(adap);
/*printf("%#02x ",inval); if ( ! (count % 16) ) printf("\n"); */
		if (inval>=0) {
			*temp = inval;
			rdcount++;
		} else {   /* read timed out */
			printf(_("i2c-algo-bit.o: readbytes: i2c_inb timed out.\n"));
			break;
		}

		temp++;
		count--;

		if (msg->flags & I2C_M_NO_RD_ACK)
			continue;

		if ( count > 0 ) {		/* send ack */
			sdalo(adap);
			DEBPROTO(printf(" Am "));
		} else {
			sdahi(adap);	/* neg. ack on last byte */
			DEBPROTO(printf(" NAm "));
		}
		if (sclhi(adap)<0) {	/* timeout */
			sdahi(adap);
			printf(_("i2c-algo-bit.o: readbytes: Timeout at ack\n"));
			return -1;
		};
		scllo(adap);
		sdahi(adap);
	}
	return rdcount;
}

/* doAddress initiates the transfer by generating the start condition (in
 * try_address) and transmits the address in the necessary format to handle
 * reads, writes as well as 10bit-addresses.
 * returns:
 *  0 everything went okay, the chip ack'ed, or IGNORE_NAK flag was set
 * -x an error occurred (like: -EREMOTEIO if the device did not answer, or
 *	-1, for example if the lines are stuck...) 
 */
static inline int bit_doAddress(struct i2c_algo_bit_data *adap, struct i2c_msg *msg) 
{
	unsigned short flags = msg->flags;
	unsigned short nak_ok = msg->flags & I2C_M_IGNORE_NAK;

	unsigned char addr;
	int ret, retries;

	retries = nak_ok ? 0 : RETRIES;
	
	if ( (flags & I2C_M_TEN)  ) { 
		/* a ten bit address, never happen in ddccontrol */
		addr = 0xf0 | (( msg->addr >> 7) & 0x03);
		DEB2(printf("addr0: %d\n",addr));
		/* try extended address code...*/
		ret = try_address(adap, addr, retries);
		if ((ret != 1) && !nak_ok)  {
			printf("died at extended address code.\n");
			return -EREMOTEIO;
		}
		/* the remaining 8 bit address */
		ret = i2c_outb(adap,msg->addr & 0x7f);
		if ((ret != 1) && !nak_ok) {
			/* the chip did not ack / xmission error occurred */
			printf("died at 2nd address code.\n");
			return -EREMOTEIO;
		}
		if ( flags & I2C_M_RD ) {
			i2c_repstart(adap);
			/* okay, now switch into reading mode */
			addr |= 0x01;
			ret = try_address(adap, addr, retries);
			if ((ret!=1) && !nak_ok) {
				printf("died at extended address code.\n");
				return -EREMOTEIO;
			}
		}
	} else {		/* normal 7bit address	*/
		addr = ( msg->addr << 1 );
		if (flags & I2C_M_RD )
			addr |= 1;
		if (flags & I2C_M_REV_DIR_ADDR )
			addr ^= 1;
		ret = try_address(adap, addr, retries);
		if ((ret!=1) && !nak_ok)
			return -EREMOTEIO;
	}

	return 0;
}

int bit_xfer(struct i2c_algo_bit_data *adap,
		    struct i2c_msg msgs[], int num)
{
	struct i2c_msg *pmsg;
	
	int i,ret;
	unsigned short nak_ok;

	i2c_start(adap);
	for (i=0;i<num;i++) {
		pmsg = &msgs[i];
		nak_ok = pmsg->flags & I2C_M_IGNORE_NAK; 
		if (!(pmsg->flags & I2C_M_NOSTART)) {
			if (i) {
				i2c_repstart(adap);
			}
			ret = bit_doAddress(adap, pmsg);
			if ((ret != 0) && !nak_ok) {
			    DEB2(printf("i2c-algo-bit.o: NAK from device addr %2.2x msg #%d\n"
					,msgs[i].addr,i));
			    return (ret<0) ? ret : -EREMOTEIO;
			}
		}
		if (pmsg->flags & I2C_M_RD ) {
			/* read bytes into buffer*/
			ret = readbytes(adap, pmsg);
			DEB2(printf("i2c-algo-bit.o: read %d bytes.\n",ret));
			if (ret < pmsg->len ) {
				return (ret<0)? ret : -EREMOTEIO;
			}
		} else {
			/* write bytes from buffer */
			ret = sendbytes(adap, pmsg);
			DEB2(printf("i2c-algo-bit.o: wrote %d bytes.\n",ret));
			if (ret < pmsg->len ) {
				return (ret<0) ? ret : -EREMOTEIO;
			}
		}
	}
	i2c_stop(adap);
	return num;
}
