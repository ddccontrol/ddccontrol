/*
    ddc/ci direct PCI memory interface
    Copyright(c) 2004-2005 Nicolas Boichat (nicolas@boichat.ch)

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

#include <config.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include <time.h>

#include <sys/msg.h>
#include <sys/ipc.h>

#include <errno.h>

#include "ddcpci.h"
#include "ddcpci-ipc.h"

/* card list */
card_open cards_open[] = {
	&nvidia_open,
	&radeon_open,
	&via_open,
#ifdef HAVE_SYS_IO_H
	&i810_open,
	&sis_open,
#endif
	NULL
};

card_close cards_close[] = {
	&nvidia_close,
	&radeon_close,
	&via_close,
#ifdef HAVE_SYS_IO_H
	&i810_close,
	&sis_close,
#endif
	NULL
};
/* end of card list */

/* debugging */
/*static void dumphex(FILE *f, unsigned char *buf, int len)
{
	int i, j;
	
	for (j = 0; j < len; j +=16) {
		if (len > 16) {
			fprintf(f, "%04x: ", j);
		}
		
		for (i = 0; i < 16; i++) {
			if (i + j < len) fprintf(f, "%02x ", buf[i + j]);
			else fprintf(f, "   ");
		}

		fprintf(f, "| ");

		for (i = 0; i < 16; i++) {
			if (i + j < len) fprintf(f, "%c", 
				buf[i + j] >= ' ' && buf[i + j] < 127 ? buf[i + j] : '.');
			else fprintf(f, " ");
		}
		
		fprintf(f, "\n");
	}
}*/

struct pci_access *pacc;

struct card* current_card = NULL;
card_close current_card_close = NULL;
struct i2c_algo_bit_data* current_algo = NULL;

static int msqid;

static int verbosity = 0;

int get_verbosity() {
	return verbosity;
}

static void open_card(struct i2c_bus* bus) {
	if (verbosity == 2) {
		printf("==>Opening...\n");
		printf("==>%02x:%02x.%d-%d\n",
			bus->bus, bus->dev, bus->func, bus->i2cbus);
	}
	
	struct answer aopen;
	memset(&aopen, 0, sizeof(struct answer));
	aopen.mtype = 2;
	aopen.status = -1;
	
	struct pci_dev *dev;
	
	unsigned int c;
	int i;
	if (current_card && current_card_close) {
		current_card_close(current_card);
	}
	current_card = NULL;
	current_card_close = NULL;
	current_algo = NULL;
	
	for(dev=pacc->devices; dev && (aopen.status != 0); dev=dev->next)	/* Iterate over all devices */
	{
		pci_fill_info(dev, PCI_FILL_IDENT | PCI_FILL_BASES);
		c = pci_read_word(dev, PCI_CLASS_DEVICE);
		if ((c == 0x0300) && (bus->bus == dev->bus) && (bus->dev == dev->dev) && (bus->func == dev->func)) // Right card
		{
			for (i = 0; cards_open[i] != NULL; i++) {
				if ((current_card = cards_open[i](dev))) {
					current_card_close = cards_close[i];
					if (bus->i2cbus < current_card->nbusses) {
						current_algo = &current_card->i2c_busses[bus->i2cbus];
						aopen.status = 0;
						if (verbosity == 2) {
							printf("==>%02x:%02x.%d vendor=%04x device=%04x class=%04x irq=%d base0=%lx size0=%lx\n",
								dev->bus, dev->dev, dev->func, dev->vendor_id, dev->device_id,
								c, dev->irq, (long unsigned int)dev->base_addr[0], (long unsigned int)dev->size[0]);
						}
						break;
					}
					else {
						current_card_close(current_card);
					}
				}
			}
		}
	}
	
	if (verbosity == 2) {
		printf("==>Opened (status=%d)...\n", aopen.status);
	}
	
	if (msgsnd(msqid, &aopen, ANSWER_SIZE, IPC_NOWAIT) < 0) {
		perror(_("==>Error while sending open message"));
	}
}

static void data(struct query* mquery, int len) {
	if (verbosity == 2) {
		printf("==>Data, mquery->flags = %d\n", mquery->flags);
	}
	if (mquery->flags & I2C_M_RD) {
		struct answer adata;
		memset(&adata, 0, sizeof(struct answer));
		adata.mtype = 2;
		
		int ret;
		struct i2c_msg i2cmsg[1];
		i2cmsg[0].addr  = mquery->addr;
		i2cmsg[0].flags = mquery->flags;
		i2cmsg[0].len   = mquery->len;
		i2cmsg[0].buf   = adata.buffer;
		ret = bit_xfer(current_algo, i2cmsg, 1);
		
		if (ret < 0) {
			adata.status = -1;
			if (msgsnd(msqid, &adata, ANSWER_SIZE, IPC_NOWAIT) < 0) {
				perror(_("==>Error while sending data answer message"));
			}
		}
		else {
			adata.status = 0;
			if (msgsnd(msqid, &adata, ANSWER_SIZE + mquery->len, IPC_NOWAIT) < 0) {
				perror(_("==>Error while sending data answer message"));
			}
		}
	}
	else { // Write
		int ret;
		struct i2c_msg i2cmsg[1];
		i2cmsg[0].addr  = mquery->addr;
		i2cmsg[0].flags = mquery->flags;
		i2cmsg[0].len   = len - QUERY_SIZE;
		i2cmsg[0].buf   = mquery->buffer;
		ret = bit_xfer(current_algo, i2cmsg, 1);
		
		struct answer adata;
		memset(&adata, 0, sizeof(struct answer));
		adata.mtype = 2;
		adata.status = (ret < 0) ? -1 : ret;
		if (msgsnd(msqid, &adata, ANSWER_SIZE, IPC_NOWAIT) < 0) {
			perror(_("==>Error while sending data answer message"));
		}
	}
}

static void list()
{
	if (verbosity == 2) {
		printf("==>Listing...\n");
	}
	struct pci_dev *dev;
	
	unsigned int c;
	int i, j;
	struct card* thecard;
	//char buffer[256];
	
	for(dev=pacc->devices; dev; dev=dev->next)	/* Iterate over all devices */
	{
		pci_fill_info(dev, PCI_FILL_IDENT | PCI_FILL_BASES);
		c = pci_read_word(dev, PCI_CLASS_DEVICE);
		if (c == 0x0300) // VGA
		{
			if (verbosity == 2) {
				printf("==>%02x:%02x.%d vendor=%04x device=%04x class=%04x irq=%d base0=%lx size0=%lx\n",
					dev->bus, dev->dev, dev->func, dev->vendor_id, dev->device_id,
					c, dev->irq, (long unsigned int)dev->base_addr[0], (long unsigned int)dev->size[0]);
			}
			for (i = 0; cards_open[i] != NULL; i++)
			{
				if ((thecard = cards_open[i](dev))) {
					if (verbosity == 2) {
						printf("==>Supported\n");
					}
					for (j = 0; j < thecard->nbusses; j++) {
						struct answer alist;
						memset(&alist, 0, sizeof(struct answer));
						alist.mtype = 2;
						alist.status = 0;
						alist.last = 0;
						alist.bus.bus    = dev->bus;
						alist.bus.dev    = dev->dev;
						alist.bus.func   = dev->func;
						alist.bus.i2cbus = j;
						if (verbosity == 2) {
							printf("==>%02x:%02x.%d-%d\n",
								alist.bus.bus, alist.bus.dev, alist.bus.func, alist.bus.i2cbus);
						}
						if (msgsnd(msqid, &alist, ANSWER_SIZE, IPC_NOWAIT) < 0) {
							perror(_("==>Error while sending list message"));
						}
					}
					cards_close[i](thecard);
				}
			}
		}
	}
	
	struct answer alist;
	memset(&alist, 0, sizeof(struct answer));
	alist.mtype = 2;
	alist.status = 0;
	alist.last = 1;
	if (msgsnd(msqid, &alist, ANSWER_SIZE, IPC_NOWAIT) < 0) {
		perror(_("==>Error while sending list message"));
	}
	if (verbosity == 2) {
		printf("==>EOL\n");
	}
}

int main(int argc, char **argv)
{
	pacc = pci_alloc();
	pci_init(pacc);
	pci_scan_bus(pacc);
	
	struct query mquery;
	
	char* endptr;
	
	if (argc != 3) {
		fprintf(stderr, _("Invalid arguments.\n"));
		exit(1);
	}
	
	verbosity = strtol(argv[1], &endptr, 0);
	if (*endptr != 0) {
		fprintf(stderr, _("==>Can't read verbosity.\n"));
		exit(1);
	}
	
	key_t key = strtol(argv[2], &endptr, 0);
	
	if (*endptr != 0) {
		fprintf(stderr, _("==>Can't read key.\n"));
		exit(1);
	}
	
	if ((msqid = msgget(key, 0666)) < 0) {
		fprintf(stderr, _("==>Can't open key %u\n"), key);
		perror("msgget");
		exit(1);
	}
	
	int cont = 1;
	int len = 0;
	
	time_t last = time(NULL);
	
	while (cont) {
		if ((len = msgrcv(msqid, &mquery, sizeof(struct query) - sizeof(long), 1, IPC_NOWAIT)) < 0) {
			if (errno == ENOMSG) {
				if ((time(NULL) - last) > (IDLE_TIMEOUT*1.1)) {
					fprintf(stderr, _("==>No command received for %ld seconds, aborting.\n"), (time(NULL) - last));
					cont = 0;
					msgctl(msqid, IPC_RMID, NULL);
					break;
				}
				usleep(10000);
				continue;
			}
			perror(_("==>Error while receiving query\n"));
			break;
		}
		
		if (verbosity == 2) {
			printf("==>Received!\n");
		}
		
		last = time(NULL);
		
		switch (mquery.qtype) {
		case QUERY_LIST:
			list();
			break;
		case QUERY_OPEN:
			open_card(&mquery.bus);
			break;
		case QUERY_DATA:
			data(&mquery, len);
			break;
		case QUERY_HEARTBEAT:
			if (verbosity == 2) {
				printf("==>Heartbeat received.\n");
			}
			break;
		case QUERY_QUIT:
			if (verbosity == 2) {
				printf("==>Quitting...\n");
			}
			if (current_card && current_card_close) {
				current_card_close(current_card);
			}
			cont = 0;
			break;
		default:
			fprintf(stderr, _("==>Invalid query...\n"));
			break;
		}
	}

	if (verbosity) {
		printf("==>ddcpci is quitting.\n");
	}
	
	pci_cleanup(pacc);
	
	exit(0);
}
