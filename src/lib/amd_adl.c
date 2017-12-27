/*
    ddc/ci interface functions header
    Copyright(c) 2012 Vitaly V. Bursov (vitaly@bursov.com)

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

#include "config.h"

#ifdef HAVE_AMDADL
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <dlfcn.h>

#define MAX_DISPLAYS (64)

#ifndef LINUX
#define LINUX /* not Windows */
#endif
#include <ADL/adl_sdk.h>

#include "amd_adl.h"
#include "ddcci.h"
#include "internal.h"

#if 1
# define D(x)
#else
# define D(x) x
#endif

struct adl_state {
    int initialized;

    void *lib;

    int (*ADL_Main_Control_Create)(ADL_MAIN_MALLOC_CALLBACK, int );
    int (*ADL_Main_Control_Destroy)();

    int (*ADL_Adapter_NumberOfAdapters_Get)(int *lpNumAdapters);
    int (*ADL_Adapter_AdapterInfo_Get)(LPAdapterInfo lpInfo, int iInputSize);
    int (*ADL_Display_NumberOfDisplays_Get)(int iAdapterIndex, int *lpNumDisplays);
    int (*ADL_Display_DisplayInfo_Get)(int  iAdapterIndex, int *lpNumDisplays, ADLDisplayInfo **lppInfo, int iForceDetect);
    int (*ADL_Display_DDCBlockAccess_Get)(int iAdapterIndex, int iDisplayIndex, int iOption, int iCommandIndex, int iSendMsgLen, char *lpucSendMsgBuf, int *lpulRecvMsgLen, char *lpucRecvMsgBuf);

    struct _displays {
	int adapter_index;
	int display_index;
    } displays[MAX_DISPLAYS];
    int displays_count;
};

static struct adl_state *adl;

static void* __stdcall adl_malloc (int size)
{
    void* buffer = malloc (size);
    if (buffer)
	memset(buffer, 0, size);
    return buffer;
}

static void __stdcall adl_free ( void **buffer )
{
    if (*buffer != NULL) {
	free (*buffer);
	*buffer = NULL;
    }
}

int amd_adl_get_displays_count()
{
    if (!adl->initialized)
	return -1;

    return adl->displays_count;
}

int amd_adl_get_display(int idx, int *adapter, int *display)
{
    if (!adl->initialized)
	return -1;

    if (idx < 0 || idx >= adl->displays_count)
	return -1;

    if (adapter)
	*adapter = adl->displays[idx].adapter_index;
    if (display)
	*display = adl->displays[idx].display_index;

    return 0;
}

int amd_adl_check_display(int adapter, int display)
{
    int i;

    if (!adl->initialized)
	return -1;

    for (i=0;i<adl->displays_count;i++){
	if (adl->displays[i].adapter_index == adapter &&
	    adl->displays[i].display_index == display)
	    return 0;
    }
    return -1;
}

int amd_adl_i2c_read(int adapter, int display, unsigned int addr, unsigned char *buf, unsigned int len)
{
    int res;
    char wbuf = addr << 1 | 1;

    res = adl->ADL_Display_DDCBlockAccess_Get(adapter, display, 0, 0, 1, &wbuf, (int*)&len, (char*)buf);

    D(fprintf(stderr, " >>>>>>>> adl i2c r on %d:%d a %x l %d err %d\n", adapter,  display, addr, len, res));

    if (res != ADL_OK){
	return -1;
    }

    return len;
}

int amd_adl_i2c_write(int adapter, int display, unsigned int addr, unsigned char *buf, unsigned int len)
{
    int res, rlen;
    char *wbuf = alloca(len+1);

    wbuf[0] = addr << 1;
    memcpy(&wbuf[1], buf, len);

    rlen = 0;
    res = adl->ADL_Display_DDCBlockAccess_Get(adapter, display, 0, 0, len+1, wbuf, &rlen, NULL);

    D(fprintf(stderr, " >>>>>>>> adl i2c w on %d:%d a %x l %d err %d\n", adapter,  display, addr, len, res));

    if (res != ADL_OK){
	return -1;
    }

    return len;
}


int amd_adl_init()
{
    int i;
    int res;
    int adapters_count;
    AdapterInfo *adapter_info;

    adl = adl_malloc(sizeof(struct adl_state));

    if (!adl){
	fprintf(stderr, "ADL error: malloc failed\n");
	return 0;
    }

    adl->lib = dlopen("libatiadlxx.so", RTLD_LAZY|RTLD_GLOBAL);
    if (!adl->lib){
	if (get_verbosity())
	    perror("ADL error: dlopen() failed\n");
	return 0;
    }
#define LOADFUNC(_n_) \
    do { \
	adl->_n_ = dlsym(adl->lib, #_n_); \
	if (!adl->_n_) { \
	    fprintf(stderr, "ADL error: loading symbol %s\n", #_n_); \
	    return 0; \
	} \
    } while (0)

    LOADFUNC(ADL_Main_Control_Create);
    LOADFUNC(ADL_Main_Control_Destroy);

    LOADFUNC(ADL_Adapter_NumberOfAdapters_Get);
    LOADFUNC(ADL_Adapter_AdapterInfo_Get);
    LOADFUNC(ADL_Display_NumberOfDisplays_Get);
    LOADFUNC(ADL_Display_DisplayInfo_Get);
    LOADFUNC(ADL_Display_DDCBlockAccess_Get);

#undef LOADFUNC

    res = adl->ADL_Main_Control_Create(adl_malloc, 1); // retrieve adapter information only for adapters that are physically present and enabled

    if (res != ADL_OK){
	if (get_verbosity())
		fprintf(stderr, "Failed to initialize ADL: %d\n", res);
	return 0;
    }

    res = adl->ADL_Adapter_NumberOfAdapters_Get(&adapters_count);
    if (res != ADL_OK){
	if (get_verbosity())
	    fprintf(stderr, "Failed to get number of ADL adapters: %d\n", res);
	return 0;
    }

    if (adapters_count < 1){
	if (get_verbosity())
	    fprintf(stderr, "No ADL adapters found.\n");
	return 0;
    }

    adapter_info = adl_malloc(sizeof(AdapterInfo) * adapters_count);
    if (!adapter_info){
	fprintf(stderr, "ADL error: malloc failed\n");
	return 0;
    }

    res = adl->ADL_Adapter_AdapterInfo_Get(adapter_info, sizeof(AdapterInfo) * adapters_count);
    if (res != ADL_OK){
	fprintf(stderr, "Failed to get ADL adapters info: %d\n", res);
	return 0;
    }

    for (i=0;i<adapters_count;i++){
	int aidx = adapter_info[i].iAdapterIndex;
	int numdisplays = -1;
	int j;
	ADLDisplayInfo *display_info = NULL;

	if (adl->ADL_Display_DisplayInfo_Get(aidx, &numdisplays, &display_info, 1) != ADL_OK) {
		printf("adl error: ADL_Display_DisplayInfo_Get: %d\n", res);
	    continue;
	}
	D(printf("\t ================================\n"));
	D(printf("\t %d: %s - %s %d %x:%x.%x %s\n", adapter_info[i].iAdapterIndex, adapter_info[i].strAdapterName, adapter_info[i].strDisplayName,
		adapter_info[i].iPresent,
		adapter_info[i].iBusNumber,
		adapter_info[i].iDeviceNumber,
		adapter_info[i].iFunctionNumber,
		adapter_info[i].strUDID));

	for (j=0;j<numdisplays;j++){
	    int didx;

	    if ((display_info[j].iDisplayInfoValue & ADL_DISPLAY_DISPLAYINFO_DISPLAYCONNECTED) &&
		(display_info[j].iDisplayInfoValue & ADL_DISPLAY_DISPLAYINFO_DISPLAYMAPPED)){

		didx = display_info[j].displayID.iDisplayLogicalIndex;

		D(printf("\t\t found display %s at %d:%d\n",
		    display_info[j].strDisplayName, aidx, didx));

		adl->displays[adl->displays_count].adapter_index = aidx;
		adl->displays[adl->displays_count].display_index = didx;
		adl->displays_count++;
		if (adl->displays_count >= MAX_DISPLAYS){
		    break;
		}
	    }
	}

	adl_free((void**)&display_info);

	if (adl->displays_count >= MAX_DISPLAYS){
	    break;
	}
    }

    adl_free((void**)&adapter_info);

    D(fprintf(stderr, "adl initialized, %d displays\n", adl->displays_count));

    adl->initialized = 1;
    return 1;
}

void amd_adl_free()
{
    if (!adl)
	return;

    adl->ADL_Main_Control_Destroy();

    if (adl->lib){
	dlclose(adl->lib);
	adl->lib = NULL;
    }

    adl_free((void**)&adl);
}

#endif /* HAVE_AMDADL */

