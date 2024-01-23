/*
    ddc/ci interface functions
    Copyright(c) 2004-2005 Nicolas Boichat (nicolas@boichat.ch)
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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "monitor_db.h"
#include "ddcci.h"
#include "internal.h"

/* Localize, and alloc in libxml */
#ifdef HAVE_GETTEXT
#define _D(text) xmlCharStrdup(dgettext(DBPACKAGE, text))
#else
#define _D(text) xmlCharStrdup(text)
#endif

#define DBPACKAGE "ddccontrol-db"

char* datadir = NULL;

xmlDocPtr options_doc = NULL;

int get_verbosity(); /* Defined in ddcci.c */

/* End of CAPS structs/functions */

int ddcci_get_value_list(xmlNodePtr options_control, xmlNodePtr mon_control, struct control_db *current_control, int command, int faulttolerance)
{
	xmlNodePtr value, cur;
	xmlChar *options_valueid, *options_valuename, *mon_valueid;
	xmlChar *tmp;
	char *endptr;
	int nvalues = 0; /* Number of values in monitor control */
	char *matchedvalues; /* Array of monitor values with (=1) or without (=0) corresponding global value */
	int i;
	
	/* Compute nvalues */
	cur = mon_control->xmlChildrenNode;
	while (cur) {
		if (cur->type == XML_ELEMENT_NODE) {
			nvalues++;
		}
		cur = cur->next;
	}
	
	matchedvalues = malloc((nvalues+1)*sizeof(char)); /* Will not be freed on error, no problem */
	memset(matchedvalues, 0, nvalues*sizeof(char));
	
	struct value_db *current_value = malloc(sizeof(struct value_db));
	struct value_db **last_value_ref = &current_control->value_list;
	memset(current_value, 0, sizeof(struct value_db));
	
	value = options_control->xmlChildrenNode;
	while (value != NULL)
	{
		if (!xmlStrcmp(value->name, (const xmlChar *) "value")) {
			options_valueid   = xmlGetProp(value, BAD_CAST "id");
			DDCCI_DB_RETURN_IF(options_valueid == NULL, -1, _("Can't find id property."), value);
			options_valuename = xmlGetProp(value, BAD_CAST "name");
			if (command) {
				if (options_valuename == NULL) {
					options_valuename = current_control->name;
				}
			}
			else {
				DDCCI_DB_RETURN_IF(options_valuename == NULL, -1, _("Can't find name property."), value);
			}
			
			//printf("!!control id=%s group=%s name=%s\n", options_ctrlid, options_groupname, options_ctrlname);
			
			i = 0;
			cur = mon_control->xmlChildrenNode;
			while (1)
			{
				if (cur == NULL) {
					/* Control not found */
					/* TODO: return */
					break;
				}
				if (!(xmlStrcmp(cur->name, (const xmlChar *)"value"))) {
					mon_valueid = xmlGetProp(cur, BAD_CAST "id");
					if (!xmlStrcmp(mon_valueid, options_valueid)) {
						current_value->id   = xmlStrdup(options_valueid);
						current_value->name = _D((char*)options_valuename);
						
						tmp = xmlGetProp(cur, BAD_CAST "value");
						
						DDCCI_DB_RETURN_IF(tmp == NULL, -1, _("Can't find value property."), cur);
						current_value->value = strtol((char*)tmp, &endptr, 0);
						DDCCI_DB_RETURN_IF(*endptr != 0, -1, _("Can't convert value to int."), cur);
						xmlFree(tmp);
						
						/*printf("**control id=%s group=%s name=%s address=%s\n", 
						options_ctrlid, options_groupname, options_ctrlname, mon_address);*/
						
						*last_value_ref = current_value;
						last_value_ref = &current_value->next;
						current_value = malloc(sizeof(struct value_db));
						memset(current_value, 0, sizeof(struct value_db));
						
						matchedvalues[i] = 1;
						
						xmlFree(mon_valueid);
						break;
					}
					else {
						xmlFree(mon_valueid);
					}
				}
				if (cur->type == XML_ELEMENT_NODE) i++;
				cur = cur->next;
			}

			xmlFree(options_valueid);
			xmlFree(options_valuename);
		}
		
		value = value->next;
	} // controls loop
	
	i = 0;
	cur = mon_control->xmlChildrenNode;
	while (cur) {
		if (cur->type == XML_ELEMENT_NODE) {
			if (!matchedvalues[i]) {
				tmp = xmlGetProp(cur, BAD_CAST "id");
				fprintf(stderr, _("Element %s (id=%s) has not been found (line %ld).\n"), cur->name, tmp, XML_GET_LINE(cur));
				xmlFree(tmp);
				if (!faulttolerance)
					return -1;
			}
			i++;
		}
		cur = cur->next;
	}
	
	free(matchedvalues);
	free(current_value);
	
	return 0;
}

int ddcci_add_controls_to_subgroup(xmlNodePtr control, xmlNodePtr mon_control, 
		struct subgroup_db *current_group, struct vcp_entry** vcp, char* defined, char *matchedcontrols, int faulttolerance) {
	xmlNodePtr cur;
	xmlChar *mon_ctrlid;
	xmlChar *options_ctrlid, *options_ctrlname;
	enum refresh_type options_refresh;
	xmlChar *tmp;
	char *endptr;
	int i;
	
	struct control_db *current_control = malloc(sizeof(struct control_db));
	struct control_db **last_control_ref = &current_group->control_list;	
	memset(current_control, 0, sizeof(struct control_db));
	
	/* TODO: fix it, don't break order. */
	/* This might break control order, but it is no big deal... */
	while (*last_control_ref) {
		last_control_ref = &(*last_control_ref)->next;
	}
	
	/* List controls in group (options.xml) */
	while (control != NULL)
	{
		if (!xmlStrcmp(control->name, (const xmlChar *) "control")) {
			options_ctrlid   = xmlGetProp(control, BAD_CAST "id");
			DDCCI_DB_RETURN_IF(options_ctrlid == NULL, 0, _("Can't find id property."), control);
			options_ctrlname = xmlGetProp(control, BAD_CAST "name");
			DDCCI_DB_RETURN_IF(options_ctrlname == NULL, 0, _("Can't find name property."), control);
			
			tmp = xmlGetProp(control, BAD_CAST "refresh");
			if (tmp) {
				if (!xmlStrcmp(tmp, BAD_CAST "none")) {
					options_refresh = none;
				}
				else if (!xmlStrcmp(tmp, BAD_CAST "all")) {
					options_refresh = all;
				}
				else {
					DDCCI_DB_RETURN_IF(1, 0, _("Invalid refresh type (!= none, != all)."), control);
				}
				xmlFree(tmp);
			}
			else {
				options_refresh = none;
			}
			
			//printf("!!control id=%s group=%s name=%s\n", options_ctrlid, options_groupname, options_ctrlname);
			
			/* Find the related control in monitor specifications */
			i = 0;
			cur = mon_control->xmlChildrenNode;
			while (1)
			{
				if (cur == NULL) {
					/* Control not found */
					/* TODO: return */
					break;
				}
				if (!(xmlStrcmp(cur->name, (const xmlChar *)"control"))) {
					mon_ctrlid = xmlGetProp(cur, BAD_CAST "id");
					if (!xmlStrcmp(mon_ctrlid, options_ctrlid)) {
						xmlFree(mon_ctrlid);

						tmp = xmlGetProp(cur, BAD_CAST "address");
						DDCCI_DB_RETURN_IF(tmp == NULL, 0, _("Can't find address property."), cur);
						current_control->address = strtol((char*)tmp, &endptr, 0);
						DDCCI_DB_RETURN_IF(*endptr != 0, 0, _("Can't convert address to int."), cur);
						xmlFree(tmp);
						
						matchedcontrols[i] = 1;
						
						if (vcp[current_control->address] == NULL) {
							if (get_verbosity()) {
								printf(_("Control %s has been discarded by the caps string.\n"), options_ctrlid);
							}
							memset(current_control, 0, sizeof(struct control_db));
							break;
						}
						
						if (defined[current_control->address]) {
							if (get_verbosity() > 1) {
								printf(_("Control %s (0x%02x) has already been defined.\n"), options_ctrlid, current_control->address);
							}
							memset(current_control, 0, sizeof(struct control_db));
							break;
						}
						
						current_control->id   = xmlStrdup(options_ctrlid);
						current_control->name = _D((char*)options_ctrlname);
						current_control->refresh = options_refresh;
						
						tmp = xmlGetProp(cur, BAD_CAST "delay");
						if (tmp) {
							current_control->delay = strtol((char*)tmp, &endptr, 10);
							DDCCI_DB_RETURN_IF(*endptr != 0, 0, _("Can't convert delay to int."), cur);
							xmlFree(tmp);
						}
						else {
							current_control->delay = -1;
						}
						
						tmp = xmlGetProp(control, BAD_CAST "type");
						DDCCI_DB_RETURN_IF(tmp == NULL, 0, _("Can't find type property."), control);
						if (!(xmlStrcmp(tmp, (const xmlChar *)"value"))) {
							current_control->type = value;
						}
						else if (!(xmlStrcmp(tmp, (const xmlChar *)"command"))) {
							current_control->type = command;
							if (ddcci_get_value_list(control, cur, current_control, 1, faulttolerance) < 0) {
								return 0;
							}
							if (current_control->value_list == NULL) { /* No value defined, use the default 0x01 value */
								struct value_db *current_value = malloc(sizeof(struct value_db));
								current_value->id = xmlCharStrdup("default");
								current_value->name = _D((char*)options_ctrlname);
								current_value->value = 0x01;
								current_value->next = NULL;
								current_control->value_list = current_value;
							}
						}
						else if (!(xmlStrcmp(tmp, (const xmlChar *)"list"))) {
							current_control->type = list;
							if (ddcci_get_value_list(control, cur, current_control, 0, faulttolerance) < 0) {
								return 0;
							}
						}
						else {
							DDCCI_DB_RETURN_IF(1, 0, _("Invalid type."), control);
						}
						xmlFree(tmp);
						
						/*printf("**control id=%s group=%s name=%s address=%s\n", 
						options_ctrlid, options_groupname, options_ctrlname, mon_address);*/
						
						defined[current_control->address] = 1;
						
						*last_control_ref = current_control;
						last_control_ref = &current_control->next;
						current_control = malloc(sizeof(struct control_db));
						memset(current_control, 0, sizeof(struct control_db));
						
						break;
					}
					else {
						xmlFree(mon_ctrlid);
					}
				}
				if (cur->type == XML_ELEMENT_NODE) i++;
				cur = cur->next;
			}
		
			xmlFree(options_ctrlid);
			xmlFree(options_ctrlname);
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
int ddcci_create_db_protected(
	struct monitor_db* mon_db, const char* pnpname, struct caps* caps, int recursionlevel,
	char* defined, int faulttolerance)
{
	xmlDocPtr mon_doc;
	xmlNodePtr root, mon_child, mon_control;
	xmlChar *tmp;
	char buffer[256];
	
	if (options_doc == NULL) {
		fprintf(stderr, _("Database must be inited before reading a monitor file.\n"));
		return 0;
	}
	
	snprintf(buffer, 256, "%s/monitor/%s.xml", datadir, pnpname);
	mon_doc = xmlParseFile(buffer);
	if (mon_doc == NULL) {
		fprintf(stderr, _("Document not parsed successfully.\n"));
		return 0;
	}
	
	root = xmlDocGetRootElement(mon_doc);
	
	if (root == NULL) {
		fprintf(stderr,  _("empty monitor/%s.xml\n"), pnpname);
		xmlFreeDoc(mon_doc);
		return 0;
	}
	
	if (xmlStrcmp(root->name, (const xmlChar *) "monitor")) {
		fprintf(stderr,  _("monitor/%s.xml of the wrong type, root node %s != monitor"), pnpname, root->name);
		xmlFreeDoc(mon_doc);
		return 0;
	}
		
	if (!mon_db->name) {
		mon_db->name = xmlGetProp(root, BAD_CAST "name");
		DDCCI_DB_RETURN_IF(mon_db->name == NULL, 0,  _("Can't find name property."), root);
	}
	
	if ((mon_db->init == unknown) && (tmp = xmlGetProp(root, BAD_CAST "init"))) {
		if (!(xmlStrcmp(tmp, (const xmlChar *)"standard"))) {
			mon_db->init = standard;
		}
		else if (!(xmlStrcmp(tmp, (const xmlChar *)"samsung"))) {
			mon_db->init = samsung;
		}
		else {
			DDCCI_DB_RETURN_IF(1, 0,  _("Invalid type."), root);
		}
		xmlFree(tmp);
	}
	
	if ((tmp = xmlGetProp(root, BAD_CAST "caps"))) {
		if (faulttolerance)
			fprintf(stderr, "Warning: caps property is deprecated.\n");
		else {
			fprintf(stderr, "Error: caps property is deprecated.\n");
			return 0;
		}
	}
	
	if ((tmp = xmlGetProp(root, BAD_CAST "include"))) {
		if (faulttolerance)
			fprintf(stderr, "Warning: include property is deprecated.\n");
		else {
			fprintf(stderr, "Error: include property is deprecated.\n");
			return 0;
		}
	}
	
	/* Create group-subgroup structure (we'll clean it up later) */
	if (!recursionlevel) {
		/*printf("Creating struct...\n");*/
		xmlNodePtr group, subgroup;
		xmlChar *options_groupname, *options_subgroupname;
		
		struct group_db *current_group;
		struct group_db **last_group_ref = &mon_db->group_list;
		
		/* List groups (options.xml) */
		for (group = xmlDocGetRootElement(options_doc)->xmlChildrenNode; group != NULL; group = group->next)
		{
			options_groupname = NULL;
			if (xmlStrcmp(group->name, (const xmlChar *) "group")) { // Not a group
				continue;
			}
			*last_group_ref = current_group = malloc(sizeof(struct group_db));
			memset(current_group, 0, sizeof(struct group_db));
			last_group_ref = &current_group->next;
			/*printf("On group %p\n", current_group);*/
			
			options_groupname = xmlGetProp(group, BAD_CAST "name");
			DDCCI_DB_RETURN_IF(options_groupname == NULL, 0,  _("Can't find name property."), group);
			current_group->name = _D((char*)options_groupname); /* Note: copy string, so we can free options_groupname */
			xmlFree(options_groupname);
			
			struct subgroup_db *current_subgroup;
			struct subgroup_db **last_subgroup_ref = &current_group->subgroup_list;
			
			/* List subgroups (options.xml) */
			for (subgroup = group->xmlChildrenNode; subgroup != NULL; subgroup = subgroup->next)
			{
				options_subgroupname = NULL;
				if (xmlStrcmp(subgroup->name, (const xmlChar *) "subgroup")) { // Not a subgroup
					continue;
				}
				*last_subgroup_ref = current_subgroup = malloc(sizeof(struct subgroup_db));
				memset(current_subgroup, 0, sizeof(struct subgroup_db));
				last_subgroup_ref = &current_subgroup->next;
				
				/*printf("On subgroup %p\n", current_subgroup);*/
				
				options_subgroupname = xmlGetProp(subgroup, BAD_CAST "name");
				DDCCI_DB_RETURN_IF(options_subgroupname == NULL, 0,  _("Can't find name property."), subgroup);
				
				current_subgroup->name = _D((char*)options_subgroupname); /* Note: copy string, so we can free options_subgroupname */
				xmlFree(options_subgroupname);
				current_subgroup->pattern = xmlGetProp(subgroup, BAD_CAST "pattern");
			}
		}
	}
	
	mon_child = root->xmlChildrenNode;
	mon_control = NULL;
	
	int controls_or_include = 0;
	while (mon_child != NULL) {
		if (!xmlStrcmp(mon_child->name, (const xmlChar *) "caps")) {
			xmlChar* remove = xmlGetProp(mon_child, BAD_CAST "remove");
			xmlChar* add = xmlGetProp(mon_child, BAD_CAST "add");
			DDCCI_DB_RETURN_IF(!remove && !add, 0,  _("Can't find add or remove property in caps."), mon_child);
			if (remove)
				DDCCI_DB_RETURN_IF(ddcci_parse_caps((char*)remove, caps, 0) <= 0, 0,  _("Invalid remove caps."), mon_child);
			if (add)
				DDCCI_DB_RETURN_IF(ddcci_parse_caps((char*)add, caps, 1) <= 0, 0,  _("Invalid add caps."), mon_child);
		}
		else if (!xmlStrcmp(mon_child->name, (const xmlChar *) "include")) {
			controls_or_include = 1;
			if (recursionlevel > 15) {
				fprintf(stderr,  _("Error, include recursion level > 15 (file: %s).\n"), pnpname);
				mon_db = NULL;
				return 0;
			}
			
			xmlChar* file = xmlGetProp(mon_child, BAD_CAST "file");
			DDCCI_DB_RETURN_IF(file == NULL, 0,  _("Can't find file property."), mon_child);
			if (!ddcci_create_db_protected(mon_db, (char*)file, caps, recursionlevel+1, defined, faulttolerance)) {
				xmlFree(file);
				return 0;
			}
			xmlFree(file);
		}
		else if (!xmlStrcmp(mon_child->name, (const xmlChar *) "controls")) {
			DDCCI_DB_RETURN_IF(mon_control != NULL, 0,  _("Two controls part in XML file."), root);
			controls_or_include = 1;
			mon_control = mon_child;
			/* Find, if possible, each element of options_doc in mon_doc. */
			xmlNodePtr mon_control_child, group, subgroup, control;
			
			struct group_db *current_group = mon_db->group_list;
			
			int ncontrols = 0; /* Number of controls in monitor file */
			char *matchedcontrols; /* Array of monitor controls with (=1) or without (=0) corresponding global control */
			
			/* Compute nvalues */
			mon_control_child = mon_control->xmlChildrenNode;
			while (mon_control_child) {
				if (mon_control_child->type == XML_ELEMENT_NODE) {
					ncontrols++;
				}
				mon_control_child = mon_control_child->next;
			}
			
			matchedcontrols = malloc((ncontrols+1)*sizeof(char)); /* Will not be freed on error, no problem */
			memset(matchedcontrols, 0, ncontrols*sizeof(char));
			
			/*printf("Filling struct...\n");*/
			
			/* List groups (options.xml) */
			for (group = xmlDocGetRootElement(options_doc)->xmlChildrenNode; group != NULL; group = group->next)
			{
				if (xmlStrcmp(group->name, (const xmlChar *) "group")) { // Not a group
					continue;
				}
				/*printf("On group %p\n", current_group);*/
				
				struct subgroup_db *current_subgroup = current_group->subgroup_list;
				
				/* List subgroups (options.xml) */
				for (subgroup = group->xmlChildrenNode; subgroup != NULL; subgroup = subgroup->next)
				{
					if (xmlStrcmp(subgroup->name, (const xmlChar *) "subgroup")) { // Not a subgroup
						continue;
					}
					/*printf("On subgroup %p\n", current_subgroup);*/
					
					control = subgroup->xmlChildrenNode;
					
					DDCCI_DB_RETURN_IF(
						!ddcci_add_controls_to_subgroup(control, mon_control, current_subgroup, caps->vcp,
							defined, matchedcontrols, faulttolerance), 0,  _("Error enumerating controls in subgroup."), control);
					current_subgroup = current_subgroup->next;
				}
				current_group = current_group->next;
			}
			
			int i = 0;
			mon_control_child = mon_control->xmlChildrenNode;
			while (mon_control_child) {
				if (mon_control_child->type == XML_ELEMENT_NODE) {
					if (!matchedcontrols[i]) {
						tmp = xmlGetProp(mon_control_child, BAD_CAST "id");
						fprintf(stderr, _("Element %s (id=%s) has not been found (line %ld).\n"), mon_control_child->name, tmp, XML_GET_LINE(mon_control_child));
						xmlFree(tmp);
						if (!faulttolerance)
							return 0;
					}
					i++;
				}
				mon_control_child = mon_control_child->next;
			}
			
			free(matchedcontrols);
		} /* mon_child->name == "controls" */
		
		mon_child = mon_child->next;
	}
	
	if (!recursionlevel) {
		struct group_db **group, *group_to_free;
		struct subgroup_db **subgroup, *subgroup_to_free;
		
		/* loop through groups */
		group = &mon_db->group_list;
		while (*group != NULL)
		{
			/* loop through subgroups inside group */
			subgroup = &((*group)->subgroup_list);
			while (*subgroup != NULL)
			{
				if (!(*subgroup)->control_list) {
					subgroup_to_free = *subgroup;
					*subgroup = (*subgroup)->next;

					xmlFree(subgroup_to_free->name);
					xmlFree(subgroup_to_free->pattern);
					free(subgroup_to_free);
					continue;
				}
				subgroup = &(*subgroup)->next;
			}
			if (!(*group)->subgroup_list) {
				group_to_free = *group;
				*group = (*group)->next;

				xmlFree(group_to_free->name);
				free(group_to_free);
				continue;
			}
			group = &(*group)->next;
		}
	}
	
	if (!controls_or_include) {
		fprintf(stderr,  _("document of the wrong type, can't find controls or include.\n"));
		xmlFreeDoc(mon_doc);
		return 0;
	}
	
	xmlFreeDoc(mon_doc);
	
	return 1;
}

/* 
 * Logic concerning CAPS, see documentation Appendix D.
 * faulttolerance :
 *  - 0 : fail on every database error
 "  - 1 : do not fail on minor errors
 */
struct monitor_db* ddcci_create_db(const char* pnpname, struct caps* caps, int faulttolerance)
{
	struct monitor_db* mon_db = malloc(sizeof(struct monitor_db));
	memset(mon_db, 0, sizeof(struct monitor_db));
	
	/* defined controls, when including another file, we don't define the same control 2 times.  */
	char defined[256];
	memset(defined, 0, 256*sizeof(char));
	
	if (!ddcci_create_db_protected(mon_db, pnpname, caps, 0, defined, faulttolerance)) {
		free(mon_db);
		mon_db = NULL;
	}
	
	if (mon_db && (mon_db->init == unknown)) {
		if (faulttolerance) {
			fprintf(stderr, "Warning: init mode not set, using standard.\n");
			mon_db->init = standard;
		}
		else {
			fprintf(stderr, "Error: init mode not set.\n");
			free(mon_db);
			mon_db = NULL;
		}
	}
		
	return mon_db;
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
			xmlFree(subgroup->pattern);
			
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
	
	free(monitor);
}

/* usedatadir : data directory to use (if NULL, uses DATADIR preprocessor symbol) */
int ddcci_init_db(char* usedatadir) {
	xmlChar *version;
	xmlChar *date;
	char buffer[256];
	xmlNodePtr cur;
	char* endptr;
	int iversion;
	
	if (usedatadir) {
		datadir = malloc(strlen(usedatadir)+1);
		strcpy(datadir, usedatadir);
	}
	else {
		datadir = malloc(strlen(DATADIR)+1);
		strcpy(datadir, DATADIR);
	}
	
	snprintf(buffer, 256, "%s/options.xml", datadir);
	options_doc = xmlParseFile(buffer);
	if (options_doc == NULL) {
		fprintf(stderr,  _("Document not parsed successfully.\n"));
		return 0;
	}

	// Version check
	cur = xmlDocGetRootElement(options_doc);
	if (cur == NULL) {
		fprintf(stderr,  _("empty options.xml\n"));
		xmlFreeDoc(options_doc);
		free(datadir);
		return 0;
	}
	
	if (xmlStrcmp(cur->name, (const xmlChar *) "options")) {
		fprintf(stderr,  _("options.xml of the wrong type, root node %s != options"), cur->name);
		xmlFreeDoc(options_doc);
		free(datadir);
		return 0;
	}
	
	version = xmlGetProp(cur, BAD_CAST "dbversion");
	date = xmlGetProp(cur, BAD_CAST "date");
	
	if (!version) {
		fprintf(stderr,  _("options.xml dbversion attribute missing, please update your database.\n"));
		xmlFreeDoc(options_doc);
		free(datadir);
		return 0;
	}
	
	if (!date) {
		fprintf(stderr,  _("options.xml date attribute missing, please update your database.\n"));
		xmlFreeDoc(options_doc);
		free(datadir);
		return 0;
	}
	
	iversion = strtol((char*)version, &endptr, 0);
	DDCCI_DB_RETURN_IF(*endptr != 0, 0, _("Can't convert version to int."), cur);
	
	if (iversion > DBVERSION) {
		fprintf(stderr,  _("options.xml dbversion (%d) is greater than the supported version (%d).\n"), iversion, DBVERSION);
		fprintf(stderr,  _("Please update ddccontrol program.\n"));
		xmlFreeDoc(options_doc);
		free(datadir);
		return 0;
	}
	
	if (iversion < DBVERSION) {
		fprintf(stderr,  _("options.xml dbversion (%d) is less than the supported version (%d).\n"), iversion, DBVERSION);
		fprintf(stderr,  _("Please update ddccontrol database.\n"));
		xmlFreeDoc(options_doc);
		free(datadir);
		return 0;
	}
	
	/* TODO: do something with the date */

	xmlFree(version);
	xmlFree(date);
	
	return 1;
}

void ddcci_release_db() {
	if (options_doc) {
		xmlFreeDoc(options_doc);
	}
	free(datadir);
}
