/*
 * Copyright (c) 2017 Minqi Pan <pmq2001@gmail.com>
 *
 * This file is part of libautoupdate, distributed under the MIT License
 * For full terms see the included LICENSE file
 */

#include <limits.h>
#ifdef __linux__
#include <linux/limits.h>
#endif
#include <assert.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXPECT(condition) expect(condition, __FILE__, __LINE__)

static void expect(short condition, const char *file, int line)
{
	if (condition) {
		fprintf(stderr, ".");
	}
	else {
		fprintf(stderr, "x");
		fprintf(stderr, "\nFAILED: %s line %d\n", file, line);
		exit(1);
	}
	fflush(stderr);
}

int main()
{
	int ret;
	struct stat statbuf;
	size_t exec_path_len;
	char* exec_path;

	// test autoupdate_exepath
	exec_path_len = 2 * PATH_MAX;
	exec_path = malloc(exec_path_len);
	ret = autoupdate_exepath(exec_path, &exec_path_len);
	EXPECT(0 == ret);
	
	ret = stat(exec_path, &statbuf);
	EXPECT(0 == ret);
	EXPECT(S_ISREG(statbuf.st_mode));
	
	return 0;
}
