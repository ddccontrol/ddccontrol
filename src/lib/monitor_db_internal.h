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

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <libxml/tree.h>

struct value_db_private {
	struct value_db public_value;
	uint16_t value16;
};

typedef char value_db_private_public_value_must_be_first[
	offsetof(struct value_db_private, public_value) == 0 ? 1 : -1];

static inline struct value_db_private *ddcci_value_db_private_from_public(struct value_db *value)
{
	return (struct value_db_private *)((char *)value - offsetof(struct value_db_private, public_value));
}

static inline const struct value_db_private *ddcci_value_db_private_from_const_public(const struct value_db *value)
{
	return (const struct value_db_private *)((const char *)value - offsetof(struct value_db_private, public_value));
}

static inline struct value_db *ddcci_value_db_new(void)
{
	struct value_db_private *value = calloc(1, sizeof(struct value_db_private));
	return value ? &value->public_value : NULL;
}

static inline void ddcci_value_db_free(struct value_db *value)
{
	if (value) {
		free(ddcci_value_db_private_from_public(value));
	}
}

static inline uint16_t ddcci_value_db_value16(const struct value_db *value)
{
	return ddcci_value_db_private_from_const_public(value)->value16;
}

static inline void ddcci_value_db_set_value16(struct value_db *value, uint16_t value16)
{
	ddcci_value_db_private_from_public(value)->value16 = value16;
	value->value = (unsigned char)(value16 & 0xFF);
}

int ddcci_get_value_list(xmlNodePtr options_control, xmlNodePtr mon_control,
                         struct control_db *current_control, int command, int faulttolerance);

#endif
