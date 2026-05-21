/*
    ddc/ci monitor database internals
    Copyright(c) 2004-2005 Nicolas Boichat (nicolas@boichat.ch)
    Copyright(c) 2004 Oleg I. Vdovikin (oleg@cs.msu.su)

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#ifndef MONITOR_DB_INTERNAL_H
#define MONITOR_DB_INTERNAL_H

#include "monitor_db.h"

#include <libxml/tree.h>

int ddcci_get_value_list(xmlNodePtr options_control, xmlNodePtr mon_control,
                         struct control_db *current_control, int command, int faulttolerance);

#endif
