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

#include <stdint.h>
#include <stdlib.h>

#include <libxml/tree.h>

struct value_db_private {
	struct value_db public_value;
	uint16_t value16;
};

static inline struct value_db *ddcci_value_db_new(void)
{
	return (struct value_db *)calloc(1, sizeof(struct value_db_private));
}

static inline uint16_t ddcci_value_db_value16(const struct value_db *value)
{
	return ((const struct value_db_private *)value)->value16;
}

static inline void ddcci_value_db_set_value16(struct value_db *value, uint16_t value16)
{
	((struct value_db_private *)value)->value16 = value16;
	value->value = (unsigned char)(value16 & 0xFF);
}

int ddcci_get_value_list(xmlNodePtr options_control, xmlNodePtr mon_control,
                         struct control_db *current_control, int command, int faulttolerance);

#endif
