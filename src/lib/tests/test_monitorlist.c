/*
    Copyright(c) 2004-2026 DDCcontrol authors and contributors (see AUTHORS and CONTRIBUTORS)
*/

#include "config.h"
#include "../conf.h"
#include "../rust_ffi.h"

#include <assert.h>
#include <libxml/parser.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CACHE_START "<?xml version=\"1.0\"?>\n<monitorlist ddccontrolversion=\"" PACKAGE_VERSION "\">"
#define CACHE_END "</monitorlist>"
#define MONITOR "<monitor filename=\"dev:/dev/i2c-1\" supported=\"1\" name=\"Test\" digital=\"0x80\"/>"

static void write_text(const char *path, const char *text)
{
	FILE *file = fopen(path, "w");
	assert(file != NULL);
	assert(fputs(text, file) >= 0);
	assert(fclose(file) == 0);
}

static void assert_lists_equal(const struct monitorlist *actual,
	const struct monitorlist *expected)
{
	while (expected != NULL) {
		assert(actual != NULL);
		assert(strcmp(actual->filename, expected->filename) == 0);
		assert(actual->supported == expected->supported);
		assert(strcmp(actual->name, expected->name) == 0);
		assert(actual->digital == expected->digital);
		actual = actual->next;
		expected = expected->next;
	}
	assert(actual == NULL);
}

static void test_cache(void)
{
	/* Exercise Unix paths that cannot be converted to UTF-8 as well. */
	char template[] = "/tmp/ddccontrol-monitorlist-\xff-XXXXXX";
	char *directory = mkdtemp(template);
	char config_path[512], profiles_path[512], cache_path[512], trailing_home[512];
	struct monitorlist second = {"dev:/dev/i2c-2", 255, "Analog", 0, NULL};
	struct monitorlist first = {"dev:/dev/i2c-1", 1,
		"Skjerm Æøå <&>\"'\n\r\t", 128, &second};
	struct monitorlist *loaded;
	struct monitorlist *output;
	xmlDocPtr document;
	xmlNodePtr root;
	xmlChar *version;
	const char *invalid[] = {
		"", "<monitorlist/>", "<profile/>",
		"<monitorlist ddccontrolversion=\"different-version\"/>",
		CACHE_START MONITOR "<monitor supported=\"1\" name=\"x\" digital=\"0\"/>" CACHE_END,
		CACHE_START MONITOR "<monitor filename=\"x\" name=\"x\" digital=\"0\"/>" CACHE_END,
		CACHE_START MONITOR "<monitor filename=\"x\" supported=\"1\" digital=\"0\"/>" CACHE_END,
		CACHE_START MONITOR "<monitor filename=\"x\" supported=\"1\" name=\"x\"/>" CACHE_END,
		CACHE_START MONITOR "<monitor filename=\"x\" supported=\"1\" name=\"x\" digital=\"256\"/>" CACHE_END,
		CACHE_START MONITOR
	};
	size_t i;
	int repeat;

	assert(directory != NULL);
	assert(setenv("HOME", directory, 1) == 0);
	snprintf(config_path, sizeof(config_path), "%s/.ddccontrol", directory);
	snprintf(profiles_path, sizeof(profiles_path), "%s/.ddccontrol/profiles", directory);
	snprintf(cache_path, sizeof(cache_path), "%s/.ddccontrol/monitorlist", directory);
	snprintf(trailing_home, sizeof(trailing_home), "%s/", directory);

	assert(ddcci_load_list() == NULL); /* Missing cache, no hardware probe. */
	assert(ddcci_save_list(&first) == 1);
	loaded = ddcci_load_list();
	assert_lists_equal(loaded, &first);
	ddcci_free_list(loaded);

	/* The existing libxml2 consumer can still read the saved schema. */
	document = xmlReadFile(cache_path, NULL, 0);
	assert(document != NULL);
	root = xmlDocGetRootElement(document);
	assert(xmlStrEqual(root->name, BAD_CAST "monitorlist"));
	version = xmlGetProp(root, BAD_CAST "ddccontrolversion");
	assert(version != NULL && strcmp((const char *)version, PACKAGE_VERSION) == 0);
	xmlFree(version);
	xmlFreeDoc(document);

	/* A bad borrowed list must not truncate an existing valid cache. */
	second.name = NULL;
	assert(ddcci_save_list(&first) == 0);
	second.name = "bad\1name";
	assert(ddcci_save_list(&first) == 0);
	second.name = "bad\xffname";
	assert(ddcci_save_list(&first) == 0);
	second.name = "Analog";
	loaded = ddcci_load_list();
	assert_lists_equal(loaded, &first);
	ddcci_free_list(loaded);

	/* Read a cache written in the old format, including hexadecimal flags. */
	write_text(cache_path, CACHE_START MONITOR CACHE_END);
	loaded = ddcci_load_list();
	assert(loaded != NULL && loaded->next == NULL);
	assert(strcmp(loaded->filename, "dev:/dev/i2c-1") == 0);
	assert(strcmp(loaded->name, "Test") == 0);
	assert(loaded->supported == 1 && loaded->digital == 128);
	ddcci_free_list(loaded);

	for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
		write_text(cache_path, invalid[i]);
		for (repeat = 0; repeat < 32; repeat++) {
			output = &first;
			assert(ddccontrol_monitorlist_load(cache_path, PACKAGE_VERSION, &output) == -1);
			assert(output == &first);
		}
		assert(ddcci_load_list() == NULL);
	}

	assert(setenv("HOME", trailing_home, 1) == 0);
	assert(ddcci_save_list(NULL) == 1);
	output = &first;
	assert(ddccontrol_monitorlist_load(cache_path, PACKAGE_VERSION, &output) == 0);
	assert(output == NULL);
	assert(ddcci_load_list() == NULL);
	ddcci_free_list(output);

	assert(unlink(cache_path) == 0);
	assert(mkdir(cache_path, 0700) == 0);
	assert(ddcci_save_list(&first) == 0);
	assert(ddcci_load_list() == NULL);
	assert(rmdir(cache_path) == 0);
	assert(rmdir(profiles_path) == 0);
	assert(rmdir(config_path) == 0);
	assert(rmdir(directory) == 0);
}

int main(void)
{
	test_cache();
	assert(unsetenv("HOME") == 0);
	assert(ddcci_load_list() == NULL);
	assert(ddcci_save_list(NULL) == 0);
	assert(setenv("HOME", "", 1) == 0);
	assert(ddcci_load_list() == NULL);
	assert(ddcci_save_list(NULL) == 0);
	return 0;
}
