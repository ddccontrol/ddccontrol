/*
    ddc/ci interface functions
    Copyright(c) 2004 Nicolas Boichat (nicolas@boichat.ch)
    Copyright(c) 2004 Oleg I. Vdovikin (oleg@cs.msu.su)

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

#include <errno.h>
#include <stdio.h>

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include <libintl.h>

#include "monitor_db.h"

#define DBPACKAGE "ddccontrol-db"

/* Localize, and alloc in libxml */
#define _D(text) xmlCharStrdup(dgettext(DBPACKAGE, text))

#define DDCCI_RETURN_IF(cond, value, message, node) \
	if (cond) { \
		if (node) \
			fprintf(stderr, "Error %s @%s:%d (%s:%ld)\n", message, __FILE__, __LINE__, \
				((xmlNodePtr)node)->doc->name, XML_GET_LINE(node)); \
		else \
			fprintf(stderr, "Error %s @%s:%d\n", message, __FILE__, __LINE__); \
		return value; \
	}

/* Structure to store CAPS entry (control and related values) */
struct caps_entry {
	int values_len; /* -1 if values were not specified */
	unsigned short* values;
};

/* See doc/xml-design.txt
 * Returns :
 * -1 if an error occured 
 *  0 otherwise
 */
int ddcci_parse_caps(const char* caps_str, struct caps_entry** caps)
{
//	printf("Parsing CAPS (%s).\n", caps_str);
	int pos = 0; /* position in caps_str */
	
	int level = 0; /* CAPS parenthesis level */
	int vcp = 0; /* Current CAPS section is vcp */
	
	int containsvcp = 0;
	
	char buf[8];
	char* endptr;
	long ind = -1;
	long val = -1;
	int i;
	
	for (pos = 0; caps_str[pos] != 0; pos++)
	{
		if (caps_str[pos] == '(') {
			level++;
		}
		else if (caps_str[pos] == ')')
		{
			level--;
			if (level == 0) {
				vcp = 0;
			}
		}
		else if (caps_str[pos] != ' ')
		{
			if ((level == 1) && (strncmp(caps_str+pos, "vcp", 3) == 0)) {
				vcp = 1;
				pos += 2;
				containsvcp = 1;
			}
			else if ((vcp == 1) && (level == 2)) {
				buf[0] = caps_str[pos];
				buf[1] = caps_str[++pos];
				buf[2] = 0;
				ind = strtol(buf, &endptr, 16);
				DDCCI_RETURN_IF(*endptr != 0, -1, "Can't convert value to int, invalid CAPS.", NULL);
				//printf("Index %lx in CAPS.\n", ind);
				caps[ind] = malloc(sizeof(struct caps_entry));
				caps[ind]->values_len = -1;
				caps[ind]->values = NULL;
			}
			else if ((vcp == 1) && (level == 3)) {
				i = 0;
				while ((caps_str[pos+i] != ' ') && (caps_str[pos+i] != ')')) {
					buf[i] = caps_str[pos+i];
					i++;
				}
				buf[i] = 0;
				val = strtol(buf, &endptr, 16);
				DDCCI_RETURN_IF(*endptr != 0, -1, "Can't convert value to int, invalid CAPS.", NULL);
				if (caps[ind]->values_len == -1) {
					caps[ind]->values_len = 1;
				}
				else {
					caps[ind]->values_len++;
				}
				caps[ind]->values = realloc(caps[ind]->values, caps[ind]->values_len*sizeof(unsigned short));
				caps[ind]->values[caps[ind]->values_len-1] = val;
			}
		}
	}
	
	/* If no vcp is specified, then every control is accepted */
	if (!containsvcp) {
		for (i = 0; i < 256; i++) {
			caps[i] = malloc(sizeof(struct caps_entry));
			caps[i]->values_len = -1;
			caps[i]->values = NULL;
		}
	}
	
	return 0;
}

/* End of CAPS structs/functions */

int ddcci_get_value_list(xmlNodePtr options_control, xmlNodePtr mon_control, struct control_db *current_control, int command)
{
	xmlNodePtr value, cur;
	xmlChar *options_valueid, *options_valuename, *mon_valueid;
	xmlChar *tmp;
	char *endptr;
	
	struct value_db *current_value = malloc(sizeof(struct value_db));
	struct value_db **last_value_ref = &current_control->value_list;
	memset(current_value, 0, sizeof(struct value_db));
	
	value = options_control->xmlChildrenNode;
	while (value != NULL)
	{
		if (!xmlStrcmp(value->name, (const xmlChar *) "value")) {
			options_valueid   = xmlGetProp(value, "id");
			DDCCI_RETURN_IF(options_valueid == NULL, -1, "Can't find id property.", value);
			options_valuename = xmlGetProp(value, "name");
			if (command) {
				if (options_valuename == NULL) {
					options_valuename = current_control->name;
				}
			}
			else {
				DDCCI_RETURN_IF(options_valuename == NULL, -1, "Can't find name property.", value);
			}
			
			//printf("!!control id=%s group=%s name=%s\n", options_ctrlid, options_groupname, options_ctrlname);
			
			cur = mon_control->xmlChildrenNode;
			while (1)
			{
				if (cur == NULL) {
					/* Control not found, free strings */
					xmlFree(options_valueid);
					xmlFree(options_valuename);
					/* TODO: return */
					break;
				}
				if (!(xmlStrcmp(cur->name, (const xmlChar *)"value"))) {
					mon_valueid = xmlGetProp(cur, "id");
					if (!xmlStrcmp(mon_valueid, options_valueid)) {
						current_value->id   = options_valueid;
						current_value->name = _D(options_valuename);
						
						tmp = xmlGetProp(cur, "value");
						
						DDCCI_RETURN_IF(tmp == NULL, -1, "Can't find value property.", cur);
						current_value->value = strtol(tmp, &endptr, 0);
						DDCCI_RETURN_IF(*endptr != 0, -1, "Can't convert value to int.", cur);
						xmlFree(tmp);
						
						/*printf("**control id=%s group=%s name=%s address=%s\n", 
						options_ctrlid, options_groupname, options_ctrlname, mon_address);*/
						
						*last_value_ref = current_value;
						last_value_ref = &current_value->next;
						current_value = malloc(sizeof(struct value_db));
						memset(current_value, 0, sizeof(struct value_db));
						
						xmlFree(mon_valueid);
						break;
					}
					else {
						xmlFree(mon_valueid);
					}
				}
				cur = cur->next;
			}
		}
		
		value = value->next;
	} // controls loop
	
	return 0;
}

int ddcci_add_controls_to_subgroup(xmlNodePtr control, xmlNodePtr mon_control, struct subgroup_db *current_group, struct caps_entry** caps) {
	xmlNodePtr cur;
	xmlChar *mon_ctrlid;
	xmlChar *options_ctrlid, *options_ctrlname;
	xmlChar *tmp;
	char *endptr;
	
	struct control_db *current_control = malloc(sizeof(struct control_db));
	struct control_db **last_control_ref = &current_group->control_list;
	memset(current_control, 0, sizeof(struct control_db));
	
	/* List controls in group (options.xml) */
	while (control != NULL)
	{
		if (!xmlStrcmp(control->name, (const xmlChar *) "control")) {
			options_ctrlid   = xmlGetProp(control, "id");
			DDCCI_RETURN_IF(options_ctrlid == NULL, 0, "Can't find id property.", control);
			options_ctrlname = xmlGetProp(control, "name");
			DDCCI_RETURN_IF(options_ctrlname == NULL, 0, "Can't find name property.", control);
			
			//printf("!!control id=%s group=%s name=%s\n", options_ctrlid, options_groupname, options_ctrlname);
			
			/* Find the related control in monitor specifications */
			cur = mon_control->xmlChildrenNode;
			while (1)
			{
				if (cur == NULL) {
					/* Control not found, free strings */
					xmlFree(options_ctrlid);
					xmlFree(options_ctrlname);
					/* TODO: return */
					break;
				}
				if (!(xmlStrcmp(cur->name, (const xmlChar *)"control"))) {
					mon_ctrlid = xmlGetProp(cur, "id");
					if (!xmlStrcmp(mon_ctrlid, options_ctrlid)) {					
						tmp = xmlGetProp(cur, "address");
						DDCCI_RETURN_IF(tmp == NULL, 0, "Can't find address property.", cur);
						current_control->address = strtol(tmp, &endptr, 0);
						DDCCI_RETURN_IF(*endptr != 0, 0, "Can't convert address to int.", cur);
						xmlFree(tmp);
						if (caps[current_control->address] == NULL) {
							memset(current_control, 0, sizeof(struct control_db));
							xmlFree(options_ctrlid);
							xmlFree(options_ctrlname);
							break;
						}
						
						current_control->id   = options_ctrlid;
						current_control->name = _D(options_ctrlname);
						
						tmp = xmlGetProp(cur, "delay");
						if (tmp) {
							current_control->delay = strtol(tmp, &endptr, 10);
							DDCCI_RETURN_IF(endptr != NULL, 0, "Can't convert delay to int.", cur);
							xmlFree(tmp);
						}
						else {
							current_control->delay = -1;
						}
						
						tmp = xmlGetProp(control, "type");
						DDCCI_RETURN_IF(tmp == NULL, 0, "Can't find type property.", control);
						if (!(xmlStrcmp(tmp, (const xmlChar *)"value"))) {
							current_control->type = value;
						}
						else if (!(xmlStrcmp(tmp, (const xmlChar *)"command"))) {
							current_control->type = command;
							if (ddcci_get_value_list(control, cur, current_control, 1) < 0) {
								return 0;
							}
							if (current_control->value_list == NULL) { /* No value defined, use the default 0x01 value */
								struct value_db *current_value = malloc(sizeof(struct value_db));
								current_value->id = xmlCharStrdup("default");
								current_value->name = _D(options_ctrlname);
								current_value->value = 0x01;
								current_value->next = NULL;
								current_control->value_list = current_value;
							}
						}
						else if (!(xmlStrcmp(tmp, (const xmlChar *)"list"))) {
							current_control->type = list;
							if (ddcci_get_value_list(control, cur, current_control, 0) < 0) {
								return 0;
							}
						}
						else {
							DDCCI_RETURN_IF(1, 0, "Invalid type.", control);
						}
						xmlFree(tmp);
						
						/*printf("**control id=%s group=%s name=%s address=%s\n", 
						options_ctrlid, options_groupname, options_ctrlname, mon_address);*/
						
						*last_control_ref = current_control;
						last_control_ref = &current_control->next;
						current_control = malloc(sizeof(struct control_db));
						memset(current_control, 0, sizeof(struct control_db));
						
						xmlFree(mon_ctrlid);
						break;
					}
					else {
						xmlFree(mon_ctrlid);
					}
				}
				cur = cur->next;
			}
		}
		
		control = control->next;
	} // controls loop
	
	free(current_control);
	
	return 1;
}

/* recursionlevel: Protection against looping includes
 * default_caps: CAPS passed to ddcci_create_db (read from the monitor)
 * prof_caps: CAPS read from one of the profile (NULL if none has been read yet)
 */
struct monitor_db* ddcci_create_db_protected(const char* pnpname, int recursionlevel, const char* default_caps, xmlChar* prof_caps)
{
	struct monitor_db* mon_db;
	xmlDocPtr options_doc, mon_doc;
	xmlNodePtr cur, mon_control;
	xmlChar *mon_name;
	xmlChar *tmp;
	char buffer[256];

	snprintf(buffer, 256, "%s/monitor/%s.xml", DATADIR, pnpname);
	mon_doc = xmlParseFile(buffer);
	if (mon_doc == NULL) {
		fprintf(stderr,"Document not parsed successfully. \n");
		return NULL;
	}
	
	snprintf(buffer, 256, "%s/options.xml", DATADIR);
	options_doc = xmlParseFile(buffer);
	if (options_doc == NULL) {
		fprintf(stderr,"Document not parsed successfully. \n");
		xmlFreeDoc(mon_doc);
		return NULL;
	}
	
	cur = xmlDocGetRootElement(mon_doc);
	
	if (cur == NULL) {
		fprintf(stderr,"empty document\n");
		xmlFreeDoc(mon_doc);
		xmlFreeDoc(options_doc);
		return NULL;
	}
	
	if (xmlStrcmp(cur->name, (const xmlChar *) "monitor")) {
		fprintf(stderr,"document of the wrong type, root node %s != monitor", cur->name);
		xmlFreeDoc(mon_doc);
		xmlFreeDoc(options_doc);
		return NULL;
	}
	
	mon_name = xmlGetProp(cur, "name");
	DDCCI_RETURN_IF(mon_name == NULL, NULL, "Can't find name property.", cur);
	if (prof_caps == NULL) {
		prof_caps = xmlGetProp(cur, "caps");
	}
	
	if ((tmp = xmlGetProp(cur, "include"))) {
		recursionlevel++;
		if (recursionlevel > 15) {
			fprintf(stderr, "Error, include recursion level > 15 (file: %s).\n", pnpname);
			xmlFree(mon_name);
			mon_db = NULL;
			return NULL;
		}

		mon_db = ddcci_create_db_protected(tmp, recursionlevel, default_caps, prof_caps);
		if (mon_db) {
			xmlFree(mon_db->name);
			mon_db->name = mon_name;
		}
		xmlFree(tmp);
		
		return mon_db;
	}
	
	mon_db = malloc(sizeof(struct monitor_db));
	memset(mon_db, 0, sizeof(struct monitor_db));
	mon_db->name = mon_name;
	
	tmp = xmlGetProp(cur, "init");
	DDCCI_RETURN_IF(tmp == NULL, NULL, "Can't find init property.", cur);
	if (!(xmlStrcmp(tmp, (const xmlChar *)"standard"))) {
		mon_db->init = standard;
	}
	else if (!(xmlStrcmp(tmp, (const xmlChar *)"samsung"))) {
		mon_db->init = samsung;
	}
	else {
		DDCCI_RETURN_IF(1, NULL, "Invalid type.", cur);
	}
	xmlFree(tmp);
	
	cur = cur->xmlChildrenNode;
	mon_control = NULL;
	
	while (cur != NULL) {
		if (!xmlStrcmp(cur->name, (const xmlChar *) "controls")) {
			mon_control = cur;
		}
		
		cur = cur->next;
	}
	
	if (!mon_control) {
		fprintf(stderr,"document of the wrong type, can't find controls.\n");
		xmlFreeDoc(mon_doc);
		xmlFreeDoc(options_doc);
		return NULL;
	}
	
	/* Parse caps, and fill structure array. */
	struct caps_entry* caps[256];
	int i;
	
	for (i = 0; i < 256; i++) {
		caps[i] = NULL;
	}
	
	ddcci_parse_caps((prof_caps == NULL) ? default_caps: (const char*)prof_caps, caps);
	
	/* Find, if possible, each element of options_doc in mon_doc. */
	
	xmlChar *options_groupname, *options_subgroupname;
	xmlNodePtr group, subgroup, control;
	
	struct group_db *current_group = malloc(sizeof(struct group_db));
	struct group_db **last_group_ref = &mon_db->group_list;
	memset(current_group, 0, sizeof(struct group_db));
	
	/* List groups (options.xml) */
	for (group = xmlDocGetRootElement(options_doc)->xmlChildrenNode; group != NULL; group = group->next)
	{
		options_groupname = NULL;
		if (xmlStrcmp(group->name, (const xmlChar *) "group")) { // Not a group
			continue;
		}
		
		options_groupname = xmlGetProp(group, "name");
		//printf("*group name=%s\n", options_groupname);
		
		struct subgroup_db *current_subgroup = malloc(sizeof(struct subgroup_db));
		struct subgroup_db **last_subgroup_ref = &current_group->subgroup_list;
		memset(current_subgroup, 0, sizeof(struct subgroup_db));
		
		/* List subgroups (options.xml) */
		for (subgroup = group->xmlChildrenNode; subgroup != NULL; subgroup = subgroup->next)
		{
			options_subgroupname = NULL;
			if (xmlStrcmp(subgroup->name, (const xmlChar *) "subgroup")) { // Not a subgroup
				continue;
			}
			
			options_subgroupname = xmlGetProp(subgroup, "name");
			//printf("*group name=%s\n", options_groupname);
		
			control = subgroup->xmlChildrenNode;
			
			DDCCI_RETURN_IF(
				!ddcci_add_controls_to_subgroup(control, mon_control, current_subgroup, (struct caps_entry**)&caps), 
					NULL, "Error enumerating controls in group.", control);
			
			if (current_subgroup->control_list) {
				current_subgroup->name = _D(options_subgroupname);
				*last_subgroup_ref = current_subgroup;
				last_subgroup_ref = &current_subgroup->next;
				current_subgroup = malloc(sizeof(struct subgroup_db));
				memset(current_subgroup, 0, sizeof(struct subgroup_db));
				options_subgroupname = NULL; /* So it is not freed */
			}
			
			if (options_subgroupname) {
				xmlFree(options_subgroupname);
			}
		}
		
		if (current_group->subgroup_list) {
			current_group->name = _D(options_groupname);
			*last_group_ref = current_group;
			last_group_ref = &current_group->next;
			current_group = malloc(sizeof(struct group_db));
			memset(current_group, 0, sizeof(struct group_db));
			options_groupname = NULL; /* So it is not freed */
		}
		
		if (options_groupname) {
			xmlFree(options_groupname);
		}
	}
	
	free(current_group);
	
	xmlFreeDoc(mon_doc);
	xmlFreeDoc(options_doc);
	
	for (i = 0; i < 256; i++) {
		if(caps[i]) {
			if (caps[i]->values) {
				free(caps[i]->values);
			}
			free(caps[i]);
		}
	}
	
	return mon_db;
}

/* Logic concerning CAPS, see doc/xml-design.txt */
struct monitor_db* ddcci_create_db(const char* pnpname, const char* default_caps)
{
	return ddcci_create_db_protected(pnpname, 0, default_caps, NULL);
}

void ddcci_free_db(struct monitor_db* monitor)
{
	struct group_db    *group,    *ogroup;
	struct subgroup_db *subgroup, *osubgroup;
	struct control_db  *control,  *ocontrol;
	struct value_db    *value,    *ovalue;
	
	xmlFree(monitor->name);
	
	/* free groups */
	group = monitor->group_list;
	while (group != NULL) 
	{
		xmlFree(group->name);
		
		/* free subgroups inside group */
		subgroup = group->subgroup_list;
		while (subgroup != NULL)
		{
			xmlFree(subgroup->name);
			
			/* free controls inside subgroup */
			control = subgroup->control_list;
			while (control != NULL)
			{
				xmlFree(control->name);
				xmlFree(control->id);
				
				/* free controls' value list */
				value = control->value_list;
				while (value != NULL)
				{
					xmlFree(value->name);
					xmlFree(value->id);
					
					ovalue = value;
					value = ovalue->next;
					free(ovalue);
				}
				
				ocontrol = control;
				control = ocontrol->next;
				free(ocontrol);
			}
			
			osubgroup = subgroup;
			subgroup = osubgroup->next;
			free(osubgroup);
		}
		
		ogroup = group;
		group = ogroup->next;
		free(ogroup);
	}
}
