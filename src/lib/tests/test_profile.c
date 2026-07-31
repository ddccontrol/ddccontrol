/*
    Copyright(c) 2004-2026 DDCcontrol authors and contributors (see AUTHORS and CONTRIBUTORS)
*/

#include "../conf.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void write_text(const char *path, const char *text)
{
	FILE *file = fopen(path, "w");
	assert(file != NULL);
	assert(fputs(text, file) >= 0);
	assert(fclose(file) == 0);
}

static void test_load_and_save_profile(void)
{
	char template[] = "/tmp/ddccontrol-profile-test-XXXXXX";
	char *directory = mkdtemp(template);
	char input_path[512];
	char output_path[512];
	char config_path[512];
	char profiles_path[1024];
	struct monitor monitor;
	struct profile *profile;
	struct profile *round_trip;

	assert(directory != NULL);
	assert(setenv("HOME", directory, 1) == 0);
	snprintf(input_path, sizeof(input_path), "%s/input.xml", directory);
	snprintf(output_path, sizeof(output_path), "%s/output.xml", directory);
	snprintf(config_path, sizeof(config_path), "%s/.ddccontrol", directory);
	snprintf(profiles_path, sizeof(profiles_path), "%s/profiles", config_path);
	write_text(input_path,
		"<?xml version=\"1.0\"?>\n"
		"<profile name=\"Office\" pnpid=\"DEL1234\" version=\"1\">\n"
		"  <control address=\"0x10\" value=\"75\"/>\n"
		"  <control address=\"014\" value=\"0xffff\"/>\n"
		"</profile>\n");

	profile = ddcci_load_profile(input_path);
	assert(profile != NULL);
	assert(strcmp((const char*)profile->name, "Office") == 0);
	assert(strcmp((const char*)profile->pnpid, "DEL1234") == 0);
	assert(profile->size == 2);
	assert(profile->address[0] == 0x10);
	assert(profile->value[0] == 75);
	assert(profile->address[1] == 014);
	assert(profile->value[1] == 0xffff);
	assert(ddcci_load_profile(NULL) == NULL);

	free(profile->filename);
	profile->filename = strdup(output_path);
	assert(profile->filename != NULL);
	ddcci_set_profile_name(profile, "Work & \"Play\"\nSecond\tcolumn\rreturn");
	memset(&monitor, 0, sizeof(monitor));
	assert(ddcci_save_profile(profile, &monitor) == 1);
	assert(monitor.profiles == profile);

	round_trip = ddcci_load_profile(output_path);
	assert(round_trip != NULL);
	assert(strcmp((const char*)round_trip->name,
		"Work & \"Play\"\nSecond\tcolumn\rreturn") == 0);
	assert(strcmp((const char*)round_trip->pnpid, "DEL1234") == 0);
	assert(round_trip->size == 2);
	assert(round_trip->address[0] == 0x10);
	assert(round_trip->value[1] == 0xffff);

	ddcci_free_profile(round_trip);
	ddcci_free_profile(profile);
	assert(unlink(output_path) == 0);
	assert(unlink(input_path) == 0);
	assert(rmdir(profiles_path) == 0);
	assert(rmdir(config_path) == 0);
	assert(rmdir(directory) == 0);
}

static void test_rejects_oversized_profile(void)
{
	char template[] = "/tmp/ddccontrol-profile-limit-test-XXXXXX";
	char *directory = mkdtemp(template);
	char path[512];
	FILE *file;
	int index;

	assert(directory != NULL);
	snprintf(path, sizeof(path), "%s/oversized.xml", directory);
	file = fopen(path, "w");
	assert(file != NULL);
	assert(fputs("<profile name=\"x\" pnpid=\"DEL1234\" version=\"1\">", file) >= 0);
	for (index = 0; index <= 256; index++)
		assert(fputs("<control address=\"1\" value=\"2\"/>", file) >= 0);
	assert(fputs("</profile>", file) >= 0);
	assert(fclose(file) == 0);

	assert(ddcci_load_profile(path) == NULL);
	assert(unlink(path) == 0);
	assert(rmdir(directory) == 0);
}

int main(void)
{
	test_load_and_save_profile();
	test_rejects_oversized_profile();
	return 0;
}
