/*
    read/write configuration files in ~/.ddccontrol (profiles, cached monitor list)
    Copyright(c) 2005 Nicolas Boichat (nicolas@boichat.ch)

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
/*
 *  Profiles are stored in HOME_DIR/.ddccontrol
 */

#include "config.h"
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>

#include "conf.h"
#include "internal.h"

#define RETRYS 3 // number of read retrys

/* Read/write monitor list */

static char* get_monitorlist_filename() {
	char* filename;
	int trailing;
	int len, ret;
	char* home;
	
	ddcci_create_config_dir();
	
	home     = getenv("HOME");
	trailing = (home[strlen(home)-1] == '/');
	
	len = strlen(home) + 64;
	
	filename = malloc(len);
	ret = snprintf(filename, len, "%s%s.ddccontrol/monitorlist", home, trailing ? "" : "/");
	DDCCI_RETURN_IF_RUN(ret == len, NULL, _("Cannot create filename (buffer too small)\n"), {free(filename);})
	
	return filename;
}

/* Load a saved monitorlist */
struct monitorlist* ddcci_load_list() {
	xmlNodePtr cur, root;
	xmlDocPtr list_doc;
	
	char* filename;
	xmlChar *tmp;
	char *endptr;
	
	struct monitorlist* list = NULL;
	struct monitorlist* current = NULL;
	struct monitorlist** last = &list;
	
	filename = get_monitorlist_filename();
	if (!filename)
		return 0;
	
	list_doc = xmlParseFile(filename);
	free(filename);
	if (list_doc == NULL) {
		fprintf(stderr, _("Document not parsed successfully.\n"));
		return 0;
	}
	
	root = xmlDocGetRootElement(list_doc);
	
	if (root == NULL) {
		fprintf(stderr,  _("empty profile file\n"));
		xmlFreeDoc(list_doc);
		return 0;
	}
	
	if (xmlStrcmp(root->name, BAD_CAST "monitorlist")) {
		fprintf(stderr,  _("profile of the wrong type, root node %s != profile"), root->name);
		xmlFreeDoc(list_doc);
		return 0;
	}
	
	tmp = xmlGetProp(root, BAD_CAST "ddccontrolversion");
	DDCCI_DB_RETURN_IF_RUN(tmp == NULL, 0, _("Can't find ddccontrolversion property."), root, {xmlFreeDoc(list_doc);});
	if (strcmp((const char*)tmp, PACKAGE_VERSION)) {
		fprintf(stderr,  _("ddccontrol has been upgraded since monitorlist was saved (%s vs %s).\n"), tmp, PACKAGE_VERSION);
		xmlFreeDoc(list_doc);
		xmlFree(tmp);
		return 0;
	}
	xmlFree(tmp);
	
	cur = root->xmlChildrenNode;
	while (1)
	{
		if (cur == NULL) {
			break;
		}
		if (!(xmlStrcmp(cur->name, BAD_CAST "monitor"))) {
			current = malloc(sizeof(struct monitorlist));
			
			tmp = xmlGetProp(cur, BAD_CAST "filename");
			DDCCI_DB_RETURN_IF_RUN(tmp == NULL, 0, _("Can't find filename property."), cur,
					{ddcci_free_list(list);	free(current); xmlFreeDoc(list_doc);});
			current->filename = strdup((const char*)tmp);
			xmlFree(tmp);
			
			tmp = xmlGetProp(cur, BAD_CAST "supported");
			DDCCI_DB_RETURN_IF_RUN(tmp == NULL, 0, _("Can't find supported property."), cur,
				{ddcci_free_list(list);	free(current); xmlFreeDoc(list_doc);});
			current->supported = strtol((const char*)tmp, &endptr, 0);
			DDCCI_DB_RETURN_IF_RUN(*endptr != 0, 0, _("Can't convert supported property to int."), cur, 
					{xmlFree(tmp); ddcci_free_list(list);	free(current); xmlFreeDoc(list_doc);});
			xmlFree(tmp);
			
			tmp = xmlGetProp(cur, BAD_CAST "name");
			DDCCI_DB_RETURN_IF_RUN(tmp == NULL, 0, _("Can't find name property."), cur,
					{ddcci_free_list(list);	free(current); xmlFreeDoc(list_doc);});
			current->name = strdup((const char*)tmp);
			xmlFree(tmp);
			
			tmp = xmlGetProp(cur, BAD_CAST "digital");
			DDCCI_DB_RETURN_IF_RUN(tmp == NULL, 0, _("Can't find digital property."), cur,
				{ddcci_free_list(list);	free(current); xmlFreeDoc(list_doc);});
			current->digital = strtol((const char*)tmp, &endptr, 0);
			DDCCI_DB_RETURN_IF_RUN(*endptr != 0, 0, _("Can't convert digital property to int."), cur, 
					{xmlFree(tmp); ddcci_free_list(list);	free(current); xmlFreeDoc(list_doc);});
			xmlFree(tmp);
			
			current->next = NULL;
			*last = current;
			last = &current->next;
		}
		cur = cur->next;
	}
	
	xmlFreeDoc(list_doc);
	
	return list;
}

/* Save the monitorlist */
int ddcci_save_list(struct monitorlist* monlist) {
	char* filename;
	struct monitorlist* current;
	int rc;
	xmlTextWriterPtr writer;
	
	filename = get_monitorlist_filename();
	if (!filename)
		return 0;
	
	writer = xmlNewTextWriterFilename(filename, 0);
	DDCCI_RETURN_IF_RUN(writer == NULL, 0, _("Cannot create the xml writer\n"), {xmlFreeTextWriter(writer);})

	free(filename);

	xmlTextWriterSetIndent(writer, 1);

	rc = xmlTextWriterStartDocument(writer, NULL, NULL, NULL);
	DDCCI_RETURN_IF_RUN(rc < 0, 0, "xmlTextWriterStartDocument\n", {xmlFreeTextWriter(writer);})

	rc = xmlTextWriterStartElement(writer, BAD_CAST "monitorlist");
	DDCCI_RETURN_IF_RUN(rc < 0, 0, "xmlTextWriterStartElement monitorlist\n", {xmlFreeTextWriter(writer);})

	rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "ddccontrolversion", BAD_CAST PACKAGE_VERSION);
	DDCCI_RETURN_IF_RUN(rc < 0, 0, "xmlTextWriterWriteAttribute ddccontrolversion\n", {xmlFreeTextWriter(writer);})
	
	for (current = monlist; current != NULL; current = current->next)
	{
		rc = xmlTextWriterStartElement(writer, BAD_CAST "monitor");
		DDCCI_RETURN_IF_RUN(rc < 0, 0, "xmlTextWriterStartElement monitor\n", {xmlFreeTextWriter(writer);})

		rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "filename", "%s", current->filename);
		DDCCI_RETURN_IF_RUN(rc < 0, 0, "xmlTextWriterWriteFormatAttribute filename\n", {xmlFreeTextWriter(writer);})

		rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "supported", "%d", current->supported);
		DDCCI_RETURN_IF_RUN(rc < 0, 0, "xmlTextWriterWriteFormatAttribute supported\n", {xmlFreeTextWriter(writer);})

		rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "name", "%s", current->name);
		DDCCI_RETURN_IF_RUN(rc < 0, 0, "xmlTextWriterWriteFormatAttribute name\n", {xmlFreeTextWriter(writer);})

		rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "digital", "%d", current->digital);
		DDCCI_RETURN_IF_RUN(rc < 0, 0, "xmlTextWriterWriteFormatAttribute digital\n", {xmlFreeTextWriter(writer);})

		rc = xmlTextWriterEndElement(writer);
		DDCCI_RETURN_IF_RUN(rc < 0, 0, "xmlTextWriterEndElement\n", {xmlFreeTextWriter(writer);})
	}

	rc = xmlTextWriterEndDocument(writer);
	DDCCI_RETURN_IF_RUN(rc < 0, 0, "testXmlwriterFilename\n", {xmlFreeTextWriter(writer);})

	xmlFreeTextWriter(writer);
	
	return 1;
}

/* Profile functions */

struct profile* ddcci_create_profile(struct monitor* mon, const unsigned char* address, int size)
{
	int retry, i;
	
	struct profile* profile = malloc(sizeof(struct profile));
	memset(profile, 0, sizeof(struct profile));
	
	profile->size = size;
	
	for (i = 0; i < size; i++) {
		profile->address[i] = address[i];
		for(retry = RETRYS; retry; retry--)
		{
			if (ddcci_readctrl (mon, address[i], &profile->value[i], NULL) >= 0)
			{
				break;
			}
		}
		DDCCI_RETURN_IF_RUN(!retry, 0, _("Cannot read control value\n"), {free(profile);})
	}
	
	profile->pnpid = xmlCharStrdup ((const char*) mon->pnpid);
	
	char date[32];
	int len, ret;
	char* home;
	int trailing;
	
	time_t tm = time(NULL);
	len      = strftime(&date[0], 32, "%Y%m%d-%H%M%S", localtime(&tm));
	home     = getenv("HOME");
	trailing = (home[strlen(home)-1] == '/');
	
	len += strlen(home) + 32;
	
	profile->filename = malloc(len);
	ret = snprintf(profile->filename, len, "%s%s.ddccontrol/profiles/%s.xml", home, trailing ? "" : "/", date);
	DDCCI_RETURN_IF_RUN(ret == len, 0, _("Cannot create filename (buffer too small)\n"), {ddcci_free_profile(profile);})
	
	profile->name = xmlCharStrdup(date);
	
	return profile;
}

int ddcci_apply_profile(struct profile* profile, struct monitor* mon) {
	int retry, i;
	
	for (i = 0; i < profile->size; i++) {
		for(retry = RETRYS; retry; retry--)
		{
			if (ddcci_writectrl(mon, profile->address[i], profile->value[i], -1) >= 0)
			{
				break;
			}
		}
		DDCCI_RETURN_IF(!retry, 0, _("Cannot write control value\n"))
	}
	
	return 1;
}

void ddcci_set_profile_name(struct profile* profile, const char* name) {
	/* FIXME: What happens if the profile name contains chars like '"<>'? */
	profile->name = xmlCharStrdup(name);
}

/* Get all profiles available for a given monitor */
int ddcci_get_all_profiles(struct monitor* mon) {
	int len, ret, pos;
	char* home;
	char* dirname;
	char* filename;
	int trailing;
	DIR* dir;
	struct dirent* entry;
	struct stat buf;
	
	struct profile** next = &mon->profiles;
	struct profile* profile;
	
	home     = getenv("HOME");
	trailing = (home[strlen(home)-1] == '/');
	
	len = strlen(home) + 64;
	
	dirname = malloc(len);
	ret = snprintf(dirname, len, "%s%s.ddccontrol/profiles/", home, trailing ? "" : "/");
	DDCCI_RETURN_IF_RUN(ret == len, 0, _("Cannot create filename (buffer too small)\n"), {free(dirname);})
	
	dir = opendir(dirname);
	
	if (!dir) {
		perror(_("Error while opening ddccontrol home directory."));
		free(dirname);
		return 0;
	}
	
	filename = malloc(len);
	strcpy(filename, dirname);
	pos = strlen(filename);
	while ((entry = readdir(dir))) {
		strcpy(filename+pos, entry->d_name);
		if (!stat(filename, &buf)) {
			if (S_ISREG(buf.st_mode)) { /* Is a regular file ? */
				profile = ddcci_load_profile(filename);
				if (!xmlStrcmp(profile->pnpid, BAD_CAST mon->pnpid)) {
					*next = profile;
					next = &profile->next;
				}
				else {
					ddcci_free_profile(profile);
				}
			}
		}
	}
	
	if (errno) {
		perror(_("Error while reading ddccontrol home directory."));
		free(dirname);
		free(filename);
		closedir(dir);
		return 0;
	}
	
	closedir(dir);
	free(dirname);
	free(filename);
	
	return 1;
}

struct profile* ddcci_load_profile(const char* filename) {
	xmlNodePtr cur, root;
	xmlDocPtr profile_doc;
	
	xmlChar *tmp;
	char *endptr;
	int itmp;
	
	struct profile* profile = malloc(sizeof(struct profile));
	memset(profile, 0, sizeof(struct profile));
	
	profile_doc = xmlParseFile(filename);
	if (profile_doc == NULL) {
		fprintf(stderr, _("Document not parsed successfully.\n"));
		free(profile);
		return 0;
	}
	
	root = xmlDocGetRootElement(profile_doc);
	
	if (root == NULL) {
		fprintf(stderr,  _("empty profile file\n"));
		xmlFreeDoc(profile_doc);
		free(profile);
		return 0;
	}
	
	if (xmlStrcmp(root->name, BAD_CAST "profile")) {
		fprintf(stderr,  _("profile of the wrong type, root node %s != profile"), root->name);
		xmlFreeDoc(profile_doc);
		free(profile);
		return 0;
	}
	
	profile->pnpid = xmlGetProp(root,BAD_CAST "pnpid");
	DDCCI_DB_RETURN_IF_RUN(profile->pnpid == NULL, 0, _("Can't find pnpid property."), root, {free(profile); xmlFreeDoc(profile_doc);});
	
	profile->name = xmlGetProp(root,BAD_CAST "name");
	DDCCI_DB_RETURN_IF_RUN(profile->name == NULL, 0, _("Can't find name property."), root, {free(profile); xmlFreeDoc(profile_doc);});
	
	tmp = xmlGetProp(root,BAD_CAST "version");
	DDCCI_DB_RETURN_IF_RUN(tmp == NULL, 0, _("Can't find version property."), root, {free(profile); xmlFreeDoc(profile_doc);});
	itmp = strtol ((const char*)tmp, &endptr, 0);
	DDCCI_DB_RETURN_IF_RUN(*endptr != 0, 0, _("Can't convert version to int."), root, {free(profile); xmlFreeDoc(profile_doc);});
	DDCCI_DB_RETURN_IF_RUN(itmp != PROFILEVERSION, 0, _("Can't find version property."), root, {free(profile); xmlFreeDoc(profile_doc);});
	if (itmp > PROFILEVERSION) {
		fprintf(stderr,  _("profile version (%d) is not supported (should be %d).\n"), itmp, PROFILEVERSION);
		xmlFreeDoc(profile_doc);
		free(profile);
		return 0;
	}
	xmlFree(tmp);
	
	cur = root->xmlChildrenNode;
	while (1)
	{
		if (cur == NULL) {
			break;
		}
		if (!(xmlStrcmp(cur->name, BAD_CAST "control"))) {
			tmp = xmlGetProp (cur, BAD_CAST "address");
			DDCCI_DB_RETURN_IF_RUN(tmp == NULL, 0, _("Can't find address property."), cur, {free(profile); xmlFreeDoc(profile_doc);});
			profile->address[profile->size] = strtol ((const char*)tmp, &endptr, 0);
			DDCCI_DB_RETURN_IF_RUN(*endptr != 0, 0, _("Can't convert address to int."), cur, {xmlFree(tmp); free(profile); xmlFreeDoc(profile_doc);});
			xmlFree(tmp);
			
			tmp = xmlGetProp(cur, BAD_CAST "value");
			DDCCI_DB_RETURN_IF_RUN(tmp == NULL, 0, _("Can't find value property."), cur, {free(profile); xmlFreeDoc(profile_doc);});
			profile->value[profile->size] = strtol ((const char*)tmp, &endptr, 0);
			DDCCI_DB_RETURN_IF_RUN(*endptr != 0, 0, _("Can't convert value to int."), cur, {xmlFree(tmp); free(profile); xmlFreeDoc(profile_doc);});
			xmlFree(tmp);
			
			profile->size++;
		}
		cur = cur->next;
	}
	
	profile->filename = strdup(filename);
	
	xmlFreeDoc(profile_doc);
	
	return profile;
}

/* Save profile and add it to the profiles list of the given monitor if necessary */
int ddcci_save_profile(struct profile* profile, struct monitor* monitor) {
	int rc;
	xmlTextWriterPtr writer;
	int i;

	ddcci_create_config_dir();

	writer = xmlNewTextWriterFilename(profile->filename, 0);
	DDCCI_RETURN_IF_RUN(writer == NULL, 0, _("Cannot create the xml writer\n"), {xmlFreeTextWriter(writer);})

	xmlTextWriterSetIndent(writer, 1);

	rc = xmlTextWriterStartDocument(writer, NULL, NULL, NULL);
	DDCCI_RETURN_IF_RUN(rc < 0, 0, "xmlTextWriterStartDocument\n", {xmlFreeTextWriter(writer);})

	rc = xmlTextWriterStartElement(writer, BAD_CAST "profile");
	DDCCI_RETURN_IF_RUN(rc < 0, 0, "xmlTextWriterStartElement profile\n", {xmlFreeTextWriter(writer);})

	rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "name", profile->name);
	DDCCI_RETURN_IF_RUN(rc < 0, 0, "xmlTextWriterWriteAttribute name\n", {xmlFreeTextWriter(writer);})

	rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "pnpid", profile->pnpid);
	DDCCI_RETURN_IF_RUN(rc < 0, 0, "xmlTextWriterWriteAttribute pnpid\n", {xmlFreeTextWriter(writer);})

	rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "version", BAD_CAST "1");
	DDCCI_RETURN_IF_RUN(rc < 0, 0, "xmlTextWriterWriteAttribute version\n", {xmlFreeTextWriter(writer);})

	/*rc = xmlTextWriterWriteComment(writer, BAD_CAST "My comment");
	PROFILE_RETURN_IF(rc < 0, 0, "xmlTextWriterWriteComment\n")*/

	for (i = 0; i < profile->size; i++) {
		rc = xmlTextWriterStartElement(writer, BAD_CAST "control");
		DDCCI_RETURN_IF_RUN(rc < 0, 0, "xmlTextWriterStartElement control\n", {xmlFreeTextWriter(writer);})

		rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "address", "%#x", profile->address[i]);
		DDCCI_RETURN_IF_RUN(rc < 0, 0, "xmlTextWriterWriteFormatAttribute address\n", {xmlFreeTextWriter(writer);})

		rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "value", "%#x", profile->value[i]);
		DDCCI_RETURN_IF_RUN(rc < 0, 0, "xmlTextWriterWriteFormatAttribute value\n", {xmlFreeTextWriter(writer);})

		rc = xmlTextWriterEndElement(writer);
		DDCCI_RETURN_IF_RUN(rc < 0, 0, "xmlTextWriterEndElement\n", {xmlFreeTextWriter(writer);})
	}

	rc = xmlTextWriterEndDocument(writer);
	DDCCI_RETURN_IF_RUN(rc < 0, 0, "testXmlwriterFilename\n", {xmlFreeTextWriter(writer);})

	xmlFreeTextWriter(writer);
	
	/* Update database */
	struct profile** profileptr = &monitor->profiles;
	
	while (*profileptr) {
		if (*profileptr == profile) /* We are already in the database... */
			return 1;
		profileptr = &((*profileptr)->next);
	}
	
	*profileptr = profile;
	
	return 1;
}

/* Deletes the profile file, and remove it from the monitor database. */
void ddcci_delete_profile(struct profile* profile, struct monitor* monitor) {
	/* Delete the file */
	
	if (unlink(profile->filename) < 0) {
		perror(_("ddcci_delete_profile: Error, cannot delete profile.\n"));
		return;
	}
	
	/* Delete the database entry */
	struct profile** profileptr = &monitor->profiles;
	
	while (*profileptr) {
		if (*profileptr == profile) { /* We found the profile to delete. */
			*profileptr = profile->next;
			
			ddcci_free_profile(profile);
			
			return;
		}
		profileptr = &((*profileptr)->next);
	}
	
	*profileptr = profile;
	
	fprintf(stderr, _("ddcci_delete_profile: Error, could not find the profile to delete.\n"));
}

void ddcci_free_profile(struct profile* profile) {
	if (profile->next)
		ddcci_free_profile(profile->next);
	
	free(profile->filename);
	xmlFree(profile->pnpid);
	xmlFree(profile->name);
	free(profile);
}
