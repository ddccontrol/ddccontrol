/*
    ddc/ci direct PCI memory interface
    Copyright(c) 2004 Nicolas Boichat (nicolas@boichat.ch)

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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include <sys/msg.h>
#include <sys/ipc.h>

#include <errno.h>

#include "config.h"

#include <pci/pci.h>

#include "ddcpci.h"
#include "ddcpci-ipc.h"

/* card list */
card_open cards_open[] = {
	&nvidia_open,
	NULL
};

card_close cards_close[] = {
	&nvidia_close,
	NULL
};
/* end of card list */

struct pci_access *pacc;

static int msqid;

static void open_card(struct i2c_bus* bus) {
	printf("==>Opening...\n");
	printf("==>%02x:%02x.%d-%d\n",
				bus->bus, bus->dev, bus->func, bus->i2cbus);
	
	struct answer aopen;
	aopen.mtype = 2;
	aopen.status = 0;
	if (msgsnd(msqid, &aopen, sizeof(struct answer), IPC_NOWAIT) < 0) {
		perror("Error while sending open message");
	}
}

static void list()
{
	printf("Listing...\n");
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
			printf("==>%02x:%02x.%d vendor=%04x device=%04x class=%04x irq=%d base0=%lx size0=%lx\n",
				dev->bus, dev->dev, dev->func, dev->vendor_id, dev->device_id,
				c, dev->irq, dev->base_addr[0], dev->size[0]);
			for (i = 0; cards_open[i] != NULL; i++) {
				if ((thecard = cards_open[i](dev))) {
					printf("Supported\n");
					for (j = 0; j < thecard->nbusses; j++) {
						struct answer alist;
						alist.mtype = 2;
						alist.status = 0;
						alist.last = 0;
						alist.bus.bus    = dev->bus;
						alist.bus.dev    = dev->dev;
						alist.bus.func   = dev->func;
						alist.bus.i2cbus = j;
						printf("==>%02x:%02x.%d-%d\n",
							alist.bus.bus, alist.bus.dev, alist.bus.func, alist.bus.i2cbus);
						if (msgsnd(msqid, &alist, sizeof(struct answer), IPC_NOWAIT) < 0) {
							perror("==>Error while sending list message");
						}
					}
					cards_close[i](thecard);
				}
			}
		}
	}
	
	struct answer alist;
	alist.mtype = 2;
	alist.status = 0;
	alist.last = 1;
	if (msgsnd(msqid, &alist, sizeof(struct answer), IPC_NOWAIT) < 0) {
		perror("Error while sending list message");
	}
}

int main(int argc, char **argv)
{
	pacc = pci_alloc();
	pci_init(pacc);
	pci_scan_bus(pacc);
	
	struct query mquery;
	
	char* endptr;
	
	if (argc != 2) {
		fprintf(stderr, "Can't read key.\n");
		exit(1);
	}
	
	key_t key = strtol(argv[1], &endptr, 0);
	
	if (*endptr != 0) {
		fprintf(stderr, "Can't read key.\n");
		exit(1);
	}
	
	if ((msqid = msgget(key, 0666)) < 0) {
		perror("msgget");
		exit(1);
	}
	
	int cont = 1;
	while (cont) {
		if (msgrcv(msqid, &mquery, sizeof(struct query), 1, IPC_NOWAIT) < 0) {
			if (errno == ENOMSG) {
				usleep(10000);
				continue;
			}
			perror("Error while receiving query");
			break;
		}
		
		printf("==>Received!\n");
		
		switch (mquery.qtype) {
		case QUERY_LIST:
			list();
			break;
		case QUERY_OPEN:
			open_card(&mquery.bus);
			break;
		case QUERY_SEND:
			break;
		case QUERY_QUIT:
			printf("quitting...\n");
			cont = 0;
			break;
		}
	}

	printf("End.\n");
	
	pci_cleanup(pacc);
	
	exit(0);
}
