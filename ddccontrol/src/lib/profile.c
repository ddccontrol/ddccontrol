/*
    Monitor profile functions.
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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
/*
 *  Profiles are stored in HOME_DIR/.ddccontrol
 */

#include <errno.h>
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

#include <libintl.h>

#include "profile.h"

/* Localize, and alloc in libxml */
#define _D(text) xmlCharStrdup(dgettext(DBPACKAGE, text))
#include <libintl.h>
#define _(String) gettext (String)
#define gettext_noop(String) String
#define N_(String) gettext_noop (String)

#define PROFILE_RETURN_IF_RUN(cond, value, message, run) \
	if (cond) { \
		fprintf(stderr, _("Error: %s @%s:%d\n"), message, __FILE__, __LINE__); \
		run \
		return value; \
	}

#define PROFILE_RETURN_IF(cond, value, message) \
	PROFILE_RETURN_IF_RUN(cond, value, message, {})

#define RETRYS 3 // number of read retrys

struct profile* ddcci_create_profile(struct monitor* mon, const char* address, int size)
{
	int retry, i;
	
	struct profile* profile = malloc(sizeof(struct profile));
	memset(profile, 0, sizeof(struct profile));
	
	profile->size = size;
	
	for (i = 0; i < size; i++) {
		profile->address[i] = address[i];
		for(retry = RETRYS; retry; retry--)
		{
			if (ddcci_readctrl(mon, address[i], &profile->value[i], NULL) >= 0)
			{
				break;
			}
		}
		PROFILE_RETURN_IF_RUN(!retry, 0, _("Cannot read control value\n"), {free(profile);})
	}
	
	profile->pnpid = xmlCharStrdup(mon->pnpid);
	
	char date[32];
	int len, ret;
	char* home;
	int trailing;
	
	time_t tm = time(NULL);
	len      = strftime(&date[0], 32, N_("%Y%m%d-%H%M%S"), localtime(&tm));
	home     = getenv(N_("HOME"));
	trailing = (home[strlen(home)-1] == '/');
	
	len += strlen(home) + 32;
	
	profile->filename = malloc(len);
	ret = snprintf(profile->filename, len, N_("%s%s.ddccontrol/profiles/%s.xml"), home, trailing ? "" : N_("/"), date);
	PROFILE_RETURN_IF_RUN(ret == len, 0, _("Cannot create filename (buffer too small)\n"), {ddcci_free_profile(profile);})
	
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
		PROFILE_RETURN_IF(!retry, 0, _("Cannot write control value\n"))
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
	
	home     = getenv(N_("HOME"));
	trailing = (home[strlen(home)-1] == '/');
	
	len = strlen(home) + 64;
	
	dirname = malloc(len);
	ret = snprintf(dirname, len, N_("%s%s.ddccontrol/profiles/"), home, trailing ? "" : N_("/"));
	PROFILE_RETURN_IF_RUN(ret == len, 0, _("Cannot create filename (buffer too small)\n"), {free(dirname);})
	
	dir = opendir(dirname);
	
	if (!dir) {
		perror(_("Error while opening ddccontrol home directory."));
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
		return 0;
	}
	
	root = xmlDocGetRootElement(profile_doc);
	
	if (root == NULL) {
		fprintf(stderr,  _("empty profile file\n"));
		xmlFreeDoc(profile_doc);
		return 0;
	}
	
	if (xmlStrcmp(root->name, (const xmlChar *) N_("profile"))) {
		fprintf(stderr,  _("profile of the wrong type, root node %s != profile"), root->name);
		xmlFreeDoc(profile_doc);
		return 0;
	}
	
	profile->pnpid = xmlGetProp(root, N_("pnpid"));
	DDCCI_RETURN_IF_RUN(profile->pnpid == NULL, 0, _("Can't find pnpid property."), root, {free(profile); xmlFreeDoc(profile_doc);});
	
	profile->name = xmlGetProp(root, N_("name"));
	DDCCI_RETURN_IF_RUN(profile->name == NULL, 0, _("Can't find name property."), root, {free(profile); xmlFreeDoc(profile_doc);});
	
	tmp = xmlGetProp(root, N_("version"));
	DDCCI_RETURN_IF_RUN(tmp == NULL, 0, _("Can't find version property."), root, {free(profile); xmlFreeDoc(profile_doc);});
	itmp = strtol(tmp, &endptr, 0);
	DDCCI_RETURN_IF_RUN(*endptr != 0, 0, _("Can't convert version to int."), root, {free(profile); xmlFreeDoc(profile_doc);});
	DDCCI_RETURN_IF_RUN(itmp != PROFILEVERSION, 0, _("Can't find version property."), root, {free(profile); xmlFreeDoc(profile_doc);});
	if (itmp > PROFILEVERSION) {
		fprintf(stderr,  _("profile version (%d) is not supported (should be %d).\n"), itmp, PROFILEVERSION);
		xmlFreeDoc(profile_doc);
		return 0;
	}
	xmlFree(tmp);
	
	cur = root->xmlChildrenNode;
	while (1)
	{
		if (cur == NULL) {
			break;
		}
		if (!(xmlStrcmp(cur->name, (const xmlChar *)N_("control")))) {
			tmp = xmlGetProp(cur, N_("address"));
			DDCCI_RETURN_IF_RUN(tmp == NULL, 0, _("Can't find address property."), cur, {free(profile); xmlFreeDoc(profile_doc);});
			profile->address[profile->size] = strtol(tmp, &endptr, 0);
			DDCCI_RETURN_IF_RUN(*endptr != 0, 0, _("Can't convert address to int."), cur, {xmlFree(tmp); free(profile); xmlFreeDoc(profile_doc);});
			xmlFree(tmp);
			
			tmp = xmlGetProp(cur, N_("value"));
			DDCCI_RETURN_IF_RUN(tmp == NULL, 0, _("Can't find value property."), cur, {free(profile); xmlFreeDoc(profile_doc);});
			profile->value[profile->size] = strtol(tmp, &endptr, 0);
			DDCCI_RETURN_IF_RUN(*endptr != 0, 0, _("Can't convert value to int."), cur, {xmlFree(tmp); free(profile); xmlFreeDoc(profile_doc);});
			xmlFree(tmp);
			
			profile->size++;
		}
		cur = cur->next;
	}
	
	profile->filename = strdup(filename);
	
	return profile;
}

/* Create $HOME/.ddccontrol if necessary */
static int ddcci_create_profiledir() {
	int len, ret;
	char* home;
	char* filename;
	int trailing;
	struct stat buf;
	
	home     = getenv(N_("HOME"));
	trailing = (home[strlen(home)-1] == '/');
	
	len = strlen(home) + 32;
	
	filename = malloc(len);
	ret = snprintf(filename, len, N_("%s%s.ddccontrol"), home, trailing ? "" : N_("/"));
	PROFILE_RETURN_IF_RUN(ret == len, 0, _("Cannot create filename (buffer too small)\n"), {free(filename);})
	
	if (stat(filename, &buf) < 0) {
		if (errno != ENOENT) {
			perror(_("Error while getting informations about ddccontrol home directory."));
			return 0;
		}
		
		if (mkdir(filename, 0750) < 0) {
			perror(_("Error while creating ddccontrol home directory."));
			return 0;
		}
		
		if (stat(filename, &buf) < 0) {
			perror(_("Error while getting informations about ddccontrol home directory after creating it."));
			return 0;
		}
	}
	
	if (!S_ISDIR(buf.st_mode)) {
		errno = ENOTDIR;
		perror(_("Error: '.ddccontrol' in your home directory is not a directory."));
		return 0;
	}
	
	strcat(filename, N_("/profiles"));
	
	if (stat(filename, &buf) < 0) {
		if (errno != ENOENT) {
			perror(_("Error while getting informations about ddccontrol profile directory."));
			return 0;
		}
		
		if (mkdir(filename, 0750) < 0) {
			perror(_("Error while creating ddccontrol profile directory."));
			return 0;
		}
		
		if (stat(filename, &buf) < 0) {
			perror(_("Error while getting informations about ddccontrol profile directory after creating it."));
			return 0;
		}
	}
	
	if (!S_ISDIR(buf.st_mode)) {
		errno = ENOTDIR;
		perror(_("Error: '.ddccontrol/profiles' in your home directory is not a directory."));
		return 0;
	}
	
	free(filename);
	
	return 1;
}

/* Save profile and add it to the profiles list of the given monitor if necessary */
int ddcci_save_profile(struct profile* profile, struct monitor* monitor) {
	int rc;
	xmlTextWriterPtr writer;
	int i;

	ddcci_create_profiledir();

	writer = xmlNewTextWriterFilename(profile->filename, 0);
	PROFILE_RETURN_IF_RUN(writer == NULL, 0, _("Cannot create the xml writer\n"), {xmlFreeTextWriter(writer);})

	xmlTextWriterSetIndent(writer, 1);

	rc = xmlTextWriterStartDocument(writer, NULL, NULL, NULL);
	PROFILE_RETURN_IF_RUN(rc < 0, 0, D_("xmlTextWriterStartDocument\n"), {xmlFreeTextWriter(writer);})

	rc = xmlTextWriterStartElement(writer, BAD_CAST N_("profile"));
	PROFILE_RETURN_IF_RUN(rc < 0, 0, D_("xmlTextWriterStartElement profile\n"), {xmlFreeTextWriter(writer);})

	rc = xmlTextWriterWriteAttribute(writer, BAD_CAST N_("name"), profile->name);
	PROFILE_RETURN_IF_RUN(rc < 0, 0, D_("xmlTextWriterWriteAttribute name\n"), {xmlFreeTextWriter(writer);})

	rc = xmlTextWriterWriteAttribute(writer, BAD_CAST N_("pnpid"), profile->pnpid);
	PROFILE_RETURN_IF_RUN(rc < 0, 0, D_("xmlTextWriterWriteAttribute pnpid\n"), {xmlFreeTextWriter(writer);})

	rc = xmlTextWriterWriteAttribute(writer, BAD_CAST N_("version"), BAD_CAST N_("1"));
	PROFILE_RETURN_IF_RUN(rc < 0, 0, D_("xmlTextWriterWriteAttribute version\n"), {xmlFreeTextWriter(writer);})

	/*rc = xmlTextWriterWriteComment(writer, BAD_CAST "My comment");
	PROFILE_RETURN_IF(rc < 0, 0, "xmlTextWriterWriteComment\n")*/

	for (i = 0; i < profile->size; i++) {
		rc = xmlTextWriterStartElement(writer, BAD_CAST N_("control"));
		PROFILE_RETURN_IF_RUN(rc < 0, 0, D_("xmlTextWriterStartElement control\n"), {xmlFreeTextWriter(writer);})

		rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST N_("address"), BAD_CAST N_("%#x"), profile->address[i]);
		PROFILE_RETURN_IF_RUN(rc < 0, 0, D_("xmlTextWriterWriteFormatAttribute address\n"), {xmlFreeTextWriter(writer);})

		rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST N_("value"), BAD_CAST N_("%#x"), profile->value[i]);
		PROFILE_RETURN_IF_RUN(rc < 0, 0, D_("xmlTextWriterWriteFormatAttribute value\n"), {xmlFreeTextWriter(writer);})

		rc = xmlTextWriterEndElement(writer);
		PROFILE_RETURN_IF_RUN(rc < 0, 0, D_("xmlTextWriterEndElement\n"), {xmlFreeTextWriter(writer);})
	}

	rc = xmlTextWriterEndDocument(writer);
	PROFILE_RETURN_IF_RUN(rc < 0, 0, D_("testXmlwriterFilename\n"), {xmlFreeTextWriter(writer);})

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
