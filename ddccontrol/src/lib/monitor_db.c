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

#include "monitor_db.h"

#define DDCCI_RETURN_IF_NULL(cond, value, message) \
	if (!cond) { \
		fprintf(stderr, "Error %s @%s:%d\n", message, __FILE__, __LINE__); \
		return value; \
	}
	
#define DDCCI_RETURN_IF_NOT_NULL(cond, value, message) \
	if (cond) { \
		fprintf(stderr, "Error : %s @%s:%d\n", message, __FILE__, __LINE__); \
		return value; \
	}

int ddcci_get_value_list(xmlNodePtr options_control, xmlNodePtr mon_control, struct control_db *current_control) {
	xmlNodePtr value, cur;
	xmlChar *options_valueid, *options_valuename, *mon_valueid;
	xmlChar *tmp;
	char *endptr;
	
	struct value_db *current_value = malloc(sizeof(struct value_db));
	struct value_db **last_value_ref = &current_control->value_list;
	memset(current_value, 0, sizeof(struct value_db));
	
	value = options_control->xmlChildrenNode;
	while (value != NULL) {
		if (!xmlStrcmp(value->name, (const xmlChar *) "value")) {
			options_valueid   = xmlGetProp(value, "id");
			DDCCI_RETURN_IF_NULL(options_valueid, -1, "Can't find id property.");
			options_valuename = xmlGetProp(value, "name");
			DDCCI_RETURN_IF_NULL(options_valuename, -1, "Can't find name property.");
			
			//printf("!!control id=%s group=%s name=%s\n", options_ctrlid, options_groupname, options_ctrlname);
			
			cur = mon_control->xmlChildrenNode;
			while (1) {
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
						current_value->name = options_valuename;
						
						tmp = xmlGetProp(cur, "value");
						DDCCI_RETURN_IF_NULL(tmp, -1, "Can't find value property.");
						current_value->value = strtol(tmp, &endptr, 16);
						DDCCI_RETURN_IF_NOT_NULL(*endptr, -1, "Can't convert hex value to int.");
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


/* recursionlevel: Protection against looping includes */
struct monitor_db* ddcci_create_db_protected(const char* pnpname, int recursionlevel) {
	struct monitor_db* mon_db;
	xmlDocPtr options_doc, mon_doc;
	xmlNodePtr cur, controls;
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
	DDCCI_RETURN_IF_NULL(mon_name, NULL, "Can't find name property.");
	
	if ((tmp = xmlGetProp(cur, "include"))) {
		recursionlevel++;
		if (recursionlevel > 15) {
			fprintf(stderr, "Error, include recursion level > 15 (file: %s).\n", pnpname);
			xmlFree(mon_name);
			mon_db = NULL;
			return NULL;
		}

		mon_db = ddcci_create_db_protected(tmp, recursionlevel);
		if (mon_db) {
			xmlFree(mon_db->name);
			mon_db->name = mon_name;
		}
		xmlFree(tmp);
		
		return mon_db;
	}
	
	mon_db = malloc(sizeof(struct monitor_db));
	mon_db->name = mon_name;
	
	tmp = xmlGetProp(cur, "init");
	DDCCI_RETURN_IF_NULL(tmp, NULL, "Can't find init property.");
	if (!(xmlStrcmp(tmp, (const xmlChar *)"standard"))) {
		mon_db->init = standard;
	}
	else if (!(xmlStrcmp(tmp, (const xmlChar *)"samsung"))) {
		mon_db->init = samsung;
	}
	else {
		DDCCI_RETURN_IF_NULL(NULL, NULL, "Invalid type.");
	}
	xmlFree(tmp);
	
	cur = cur->xmlChildrenNode;
	controls = NULL;
	
	while (cur != NULL) {
		if (!xmlStrcmp(cur->name, (const xmlChar *) "controls")) {
			controls = cur;
		}
		
		cur = cur->next;
	}
	
	if (!controls) {
		fprintf(stderr,"document of the wrong type, can't find controls.\n");
		xmlFreeDoc(mon_doc);
		xmlFreeDoc(options_doc);
		return NULL;
	}
	
	/* Find, if possible, each element of options_doc in mon_doc. */
	
	xmlChar *mon_ctrlid;
	xmlChar *options_groupname, *options_ctrlid, *options_ctrlname;
	char *endptr;
	xmlNodePtr group, control;
	
	struct group_db *current_group = malloc(sizeof(struct group_db));
	struct group_db **last_group_ref = &mon_db->group_list;
	memset(current_group, 0, sizeof(struct group_db));
	
	/* List groups */
	group = xmlDocGetRootElement(options_doc)->xmlChildrenNode;
	while (group != NULL) {
		options_groupname = NULL;
		if (!xmlStrcmp(group->name, (const xmlChar *) "group")) {
			options_groupname = xmlGetProp(group, "name");
			//printf("*group name=%s\n", options_groupname);
			
			struct control_db *current_control = malloc(sizeof(struct control_db));
			struct control_db **last_control_ref = &current_group->control_list;
			memset(current_control, 0, sizeof(struct control_db));
			
			control = group->xmlChildrenNode;
			while (control != NULL) {
				if (!xmlStrcmp(control->name, (const xmlChar *) "control")) {
					options_ctrlid   = xmlGetProp(control, "id");
					DDCCI_RETURN_IF_NULL(options_ctrlid, NULL, "Can't find id property.");
					options_ctrlname = xmlGetProp(control, "name");
					DDCCI_RETURN_IF_NULL(options_ctrlname, NULL, "Can't find name property.");
					
					//printf("!!control id=%s group=%s name=%s\n", options_ctrlid, options_groupname, options_ctrlname);
					
					cur = controls->xmlChildrenNode;
					while (1) {
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
								current_control->id   = options_ctrlid;
								current_control->name = options_ctrlname;
								
								tmp = xmlGetProp(cur, "address");
								DDCCI_RETURN_IF_NULL(tmp, NULL, "Can't find address property.")
								current_control->address = strtol(tmp, &endptr, 16);
								DDCCI_RETURN_IF_NOT_NULL(*endptr, NULL, "Can't convert hex address to int.");
								xmlFree(tmp);
								
								tmp = xmlGetProp(cur, "delay");
								if (tmp) {
									current_control->delay = strtol(tmp, &endptr, 10);
									DDCCI_RETURN_IF_NOT_NULL(*endptr, NULL, "Can't convert delay to int.");
									xmlFree(tmp);
								}
								else {
									current_control->delay = -1;
								}
								
								tmp = xmlGetProp(control, "type");
								DDCCI_RETURN_IF_NULL(tmp, NULL, "Can't find type property.");
								if (!(xmlStrcmp(tmp, (const xmlChar *)"value"))) {
									current_control->type = value;
								}
								else if (!(xmlStrcmp(tmp, (const xmlChar *)"command"))) {
									current_control->type = command;
									ddcci_get_value_list(control, cur, current_control);
								}
								else if (!(xmlStrcmp(tmp, (const xmlChar *)"list"))) {
									current_control->type = list;
									ddcci_get_value_list(control, cur, current_control);
								}
								else {
									DDCCI_RETURN_IF_NULL(NULL, NULL, "Invalid type.");
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
			
			if (current_group->control_list) {
				current_group->name = options_groupname;
				*last_group_ref = current_group;
				last_group_ref = &current_group->next;
				current_group = malloc(sizeof(struct group_db));
				memset(current_group, 0, sizeof(struct group_db));
				options_groupname = NULL; /* So it is not freed */
			}
		}
		
		if (options_groupname) {
			xmlFree(options_groupname);
		}
		group = group->next;
	}
	
	free(current_group);
	
	xmlFreeDoc(mon_doc);
	xmlFreeDoc(options_doc);
	
	return mon_db;
}

struct monitor_db* ddcci_create_db(const char* pnpname) {
	return ddcci_create_db_protected(pnpname, 0);
}

void ddcci_free_db(struct monitor_db* mon_db) {
	/* TODO: implement... */
}
