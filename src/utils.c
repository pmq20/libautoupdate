/*
 * Copyright (c) 2017 Minqi Pan <pmq2001@gmail.com>
 *
 * This file is part of libautoupdate, distributed under the MIT License
 * For full terms see the included LICENSE file
 */

#include "autoupdate.h"
#include "autoupdate_internal.h"

#ifdef _WIN32

#include <Windows.h>

short autoupdate_should_proceed()
{
	TCHAR lpBuffer[32767 + 1];
	if (0 == GetEnvironmentVariable("CI", lpBuffer, 32767)) {
		return 1;
	} else {
		return 0;
	}
}

short autoupdate_should_proceed_24_hours(int argc, wchar_t *wargv[], short will_write)
{
	return 1;
}

#else

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <string.h>
#include <time.h>
#include <limits.h>

short autoupdate_should_proceed()
{
	if (NULL == getenv("CI")) {
		return 1;
	} else {
		return 0;
	}
}

short autoupdate_should_proceed_24_hours(int argc, char *argv[], short will_write)
{
	char *writting = NULL;
	short has_written = 0;
	time_t time_now;
	long item_time;
	char *item_string = NULL;
	char *item_space;
	char *cursor;
	char *filepath = NULL;
	char *string = NULL;
	char *string0 = NULL;
	long fsize;
	FILE *f = NULL;
	struct passwd *pw;
	const char *homedir;
	int ret;
	size_t size_t_ret;
	const char *filename = "/.libautoupdate";
	size_t exec_path_len = 2 * PATH_MAX;
	char exec_path[2 * PATH_MAX];

	if (autoupdate_exepath(exec_path, &exec_path_len) != 0) {
		if (!argv[0]) {
			goto exit;
		}
		assert(strlen(argv[0]) < 2 * PATH_MAX);
		memcpy(exec_path, argv[0], strlen(argv[0]));
	}
	time_now = time(NULL);
	if ((time_t)-1 == time_now) {
		goto exit;
	}
	pw = getpwuid(getuid());
	if (NULL == pw) {
		goto exit;
	}
	homedir = pw->pw_dir;
	if (NULL == homedir) {
		goto exit;
	}
	filepath = malloc(strlen(homedir) + strlen(filename) + 1);
	if (NULL == filepath) {
		goto exit;
	}
	memcpy(filepath, homedir, strlen(homedir));
	memcpy(filepath + strlen(homedir), filename, strlen(filename));
	filepath[strlen(homedir) + strlen(filename)] = 0;
	f = fopen(filepath, "r");
	if (NULL == f) {
		goto exit;
	}
	ret = fseek(f, 0, SEEK_END);
	if (0 != ret) {
		goto exit;
	}
	fsize = ftell(f);
	if (fsize < 0) {
		goto exit;
	}
	ret = fseek(f, 0, SEEK_SET);
	if (0 != ret) {
		goto exit;
	}
	
	string = malloc(fsize + 1);
	if (NULL == string) {
		goto exit;
	}
	size_t_ret = fread(string, fsize, 1, f);
	if (1 != size_t_ret) {
		goto exit;
	}
	ret = fclose(f);
	if (0 != ret) {
		goto exit;
	}
	f = NULL;
	string[fsize] = 0;
	string0 = string;
	while (string < string0 + fsize) {
		cursor = strchr(string, '\n');
		if (cursor) {
			*cursor = 0;
		}
		item_space = strchr(string, ' ');
		if (!item_space) {
			goto exit;
		}
		*item_space = 0;
		item_time = atol(string);
		item_string = item_space + 1;
		if (0 == strcmp(item_string, exec_path)) {
			if (will_write) {
				if (item_time >= 1000000000 && time_now >= 1000000000) {
					has_written = 1;
					ret = sprintf(string, "%ld ", time_now);
					*cursor = '\n';
					break;
				}
			} else if (time_now - item_time < 24 * 3600) {
				return 0;
			}
		}
		if (cursor) {
			*cursor = '\n';
			string = cursor + 1;
		} else {
			break;
		}
	}
	
	if (will_write) {
		f = fopen(filepath, "w");
		if (NULL == f) {
			goto exit;
		}
		ret = fwrite(string0, fsize, 1, f);
		if (1 != ret) {
			goto exit;
		}
		if (!has_written) {
			ret = asprintf(&writting, "%ld %s\n", time_now, exec_path);
			ret = fwrite(writting, strlen(writting), 1, f);
			if (1 != ret) {
				goto exit;
			}
		}
	}

exit:
	if (f) {
		fclose(f);
	}
	if (filepath) {
		free(filepath);
	}
	if (string) {
		free(string);
	}
	if (writting) {
		free(writting);
	}
	return 1;
}

#endif // _WIN32
