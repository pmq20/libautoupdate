/*
 * Copyright (c) 2017 Minqi Pan <pmq2001@gmail.com>
 *
 * This file is part of libautoupdate, distributed under the MIT License
 * For full terms see the included LICENSE file
 */

#ifndef _WIN32

#include "autoupdate.h"
#include "autoupdate_internal.h"
#include "zlib.h"

#include <assert.h>
#include <stdio.h> /* printf, sprintf */
#include <stdlib.h> /* exit */
#include <unistd.h> /* read, write, close */
#include <string.h> /* memcpy, memset */
#include <sys/socket.h> /* socket, connect */
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <netdb.h> /* struct hostent, gethostbyname */
#include <unistd.h>
#include <sys/select.h>
#include <limits.h>  // PATH_MAX

int autoupdate(
	int argc,
	char *argv[],
	const char *host,
	uint16_t port,
	const char *path,
	const char *current
)
{
	struct hostent *server;
	struct sockaddr_in serv_addr;
	int sockfd, bytes, total;
	char response[1024 * 10 + 1]; // 10KB

	if (!autoupdate_should_proceed()) {
		return 1;
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		fprintf(stderr, "Auto-update Failed: socket creation failed\n");
		return 2;
	}
	server = gethostbyname(host);
	if (server == NULL) {
		close(sockfd);
		fprintf(stderr, "Auto-update Failed: gethostbyname failed for %s\n", host);
		return 2;
	}
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
	if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		close(sockfd);
		fprintf(stderr, "Auto-update Failed: connect failed on %s and port %d\n", host, port);
		return 2;
	}
	if (5 != write(sockfd, "HEAD ", 5) ||
	    strlen(path) != write(sockfd, path, strlen(path)) ||
	    13 != write(sockfd, " HTTP/1.0\r\n\r\n", 13)) {
		close(sockfd);
		fprintf(stderr, "Auto-update Failed: write failed\n");
		return 2;
	}
	total = sizeof(response) - 2;
	long long received = 0;
	do {
		bytes = read(sockfd, response + received, total - received);
		if (bytes < 0) {
			close(sockfd);
			fprintf(stderr, "Auto-update Failed: read failed\n");
			return 2;
		}
		if (bytes == 0) {
			/* EOF */
			*(response + received) = 0;
			break;
		}
		received += bytes;
	} while (received < total);
	if (received == total) {
		close(sockfd);
		fprintf(stderr, "Auto-update Failed: read causes buffer full\n");
		return 2;
	}
	close(sockfd);
	assert(received < total);
	size_t len = strlen(response);
	assert(len <= total);
	char *new_line = NULL;
	char *found = NULL;
	size_t i = 0;
	response[sizeof(response) - 1] = 0;
	while (i < len) {
		new_line = strstr(response + i, "\r\n");
		if (NULL == new_line) {
			break;
		}
		*new_line = 0;
		if (0 == strncmp(response + i, "Location: ", 10)) {
			found = response + i + 10;
			break;
		}
		*new_line = '\r';
		i = new_line - response + 2;
	}
	if (!found) {
		fprintf(stderr, "Auto-update Failed: failed to find a Location header\n");
		return 2;
	}
	if (strstr(found, current)) {
		/* Latest version confirmed. No need to update */
		return 0;
	}

	char *url = found;
	fprintf(stderr, "Downloading from %s\n", url);
	fflush(stderr);

	char *host2;
	uint16_t port2 = 80;
	if (strlen(url) >= 8 && 0 == strncmp("https://", url, 8)) {
		host2 = url + 8;
	} else if (strlen(url) >= 7 && 0 == strncmp("http://", url, 7)) {
		host2 = url + 7;
	} else {
		fprintf(stderr, "Auto-update Failed: failed to find http:// or https:// at the beginning of URL %s\n", url);
		return 2;
	}
	char *found_slash = strchr(host2, '/');
	char *request_path;
	if (NULL == found_slash) {
		request_path = "/";
	} else {
		request_path = found_slash;
		*found_slash = 0;
	}
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		fprintf(stderr, "Auto-update Failed: socket creation failed\n");
		return 2;
	}
	server = gethostbyname(host2);
	if (server == NULL) {
		close(sockfd);
		fprintf(stderr, "Auto-update Failed: gethostbyname failed for %s\n", host2);
		return 2;
	}
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port2);
	memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
	if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		close(sockfd);
		fprintf(stderr, "Auto-update Failed: connect failed on %s and port %d\n", host2, port2);
		return 2;
	}
	if (NULL != found_slash) {
		*found_slash = '/';
	}
	if (4 != write(sockfd, "GET ", 4) ||
		strlen(request_path) != write(sockfd, request_path, strlen(request_path)) ||
		11 != write(sockfd, " HTTP/1.0\r\n", 11)) {
			close(sockfd);
			fprintf(stderr, "Auto-update Failed: write failed\n");
			return 2;
	}
	if (NULL != found_slash) {
		*found_slash = 0;
	}
	if (6 != write(sockfd, "Host: ", 6) ||
		strlen(host2) != write(sockfd, host2, strlen(host2)) ||
		4 != write(sockfd, "\r\n\r\n", 4)) {
			close(sockfd);
			fprintf(stderr, "Auto-update Failed: write failed\n");
			return 2;
	}

	// Read the header
	total = sizeof(response) - 2;
	response[sizeof(response) - 1] = 0;
	received = 0;
	char *header_end = NULL;
	do {
		bytes = read(sockfd, response + received, total - received);
		if (bytes < 0) {
			close(sockfd);
			fprintf(stderr, "Auto-update Failed: read failed\n");
			return 2;
		}
		if (bytes == 0) {
			/* EOF */
			*(response + received) = 0;
			break;
		}
		*(response + received + bytes) = 0;
		header_end = strstr(response + received, "\r\n\r\n");
		received += bytes;
		if (header_end) {
			break;
		}
	} while (received < total);
	if (NULL == header_end) {
		close(sockfd);
		fprintf(stderr, "Auto-update Failed: failed to find the end of the response header\n");
		return 2;
	}
	assert(received <= total);
	// Parse the header
	len = received;
	assert(len <= total);
	new_line = NULL;
	long long found_length = -1;
	i = 0;
	response[sizeof(response) - 1] = 0;
	while (i < len) {
		new_line = strstr(response + i, "\r\n");
		if (NULL == new_line) {
			break;
		}
		*new_line = 0;
		if (0 == strncmp(response + i, "Content-Length: ", 16)) {
			found_length = atoll(response + i + 16);
			break;
		}
		*new_line = '\r';
		i = new_line - response + 2;
	}
	if (-1 == found_length) {
		close(sockfd);
		fprintf(stderr, "Auto-update Failed: failed to find a Content-Length header\n");
		return 2;
	}
	if (0 == found_length) {
		close(sockfd);
		fprintf(stderr, "Auto-update Failed: found a Content-Length header of zero\n");
		return 2;
	}
	assert(found_length > 0);
	// Read the body
	// header_end -> \r\n\r\n
	assert(header_end);
	assert(header_end + 4 <= response + received);
	// put the rest of over-read content when reading header
	size_t the_rest = response + received - (header_end + 4);
	char *body_buffer = (char *)(malloc(found_length));
	if (NULL == body_buffer) {
		close(sockfd);
		fprintf(stderr, "Auto-update Failed: Insufficient memory\n");
		return 2;
	}
	memcpy(body_buffer, (header_end + 4), the_rest);
	char *body_buffer_ptr = body_buffer + the_rest;
	char *body_buffer_end = body_buffer + found_length;
	// read the remaining body
	received = the_rest;
	fprintf(stderr, "\r%lld / %lld bytes finished (%lld%%)",  received, found_length, received*100LL/found_length);
	fflush(stderr);
	while (received < found_length) {
		size_t space = 100 * 1024;
		if (space > body_buffer_end - body_buffer_ptr) {
			space = body_buffer_end - body_buffer_ptr;
		}
		bytes = read(sockfd, body_buffer_ptr, space);
		if (bytes < 0) {
			fprintf(stderr, "Auto-update Failed: read failed\n");
			free(body_buffer);
			close(sockfd);
			return 2;
		}
		if (bytes == 0) {
			/* EOF */
			break;
		}
		received += bytes;
		body_buffer_ptr += bytes;
		fprintf(stderr, "\r%lld / %lld bytes finished (%lld%%)",  received, found_length, received*100LL/found_length);
		fflush(stderr);
	}
	if (received != found_length) {
		assert(received < found_length);
		fprintf(stderr, "Auto-update Failed: prematurely reached EOF after reading %lld bytes\n", received);
		close(sockfd);
		free(body_buffer);
		return 2;
	}
	fprintf(stderr, "\n");
	fflush(stderr);
	close(sockfd);
	// Inflate to a file
	fprintf(stderr, "Inflating...");
	fflush(stderr);
	unsigned full_length = found_length;
	unsigned half_length = found_length / 2;
	unsigned uncompLength = full_length;
	char* uncomp = (char*) calloc( sizeof(char), uncompLength );
	if (NULL == uncomp) {
		fprintf(stderr, "Auto-update Failed: Insufficient memory\n");
		free(body_buffer);
		return 2;
	}

	z_stream strm;
	strm.next_in = (z_const Bytef *)body_buffer;
	strm.avail_in = found_length;
	strm.total_out = 0;
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	
	short done = 0;

	if (inflateInit2(&strm, (16+MAX_WBITS)) != Z_OK) {
		free(uncomp);
		free(body_buffer);
		fprintf(stderr, "Auto-update Failed: inflateInit2 failed\n");
		return 2;
	}
	
	while (!done) {
		// If our output buffer is too small
		if (strm.total_out >= uncompLength ) {
			// Increase size of output buffer
			char* uncomp2 = (char*) calloc( sizeof(char), uncompLength + half_length );
			if (NULL == uncomp2) {
				free(uncomp);
				free(body_buffer);
				fprintf(stderr, "Auto-update Failed: calloc failed\n");
				return 2;
			}
			memcpy( uncomp2, uncomp, uncompLength );
			uncompLength += half_length ;
			free( uncomp );
			uncomp = uncomp2 ;
		}
		
		strm.next_out = (Bytef *) (uncomp + strm.total_out);
		strm.avail_out = uncompLength - strm.total_out;
		
		// Inflate another chunk.
		int err = inflate(&strm, Z_SYNC_FLUSH);
		if (err == Z_STREAM_END) {
			done = 1;
		}
		else if (err != Z_OK)  {
			fprintf(stderr, "Auto-update Failed: inflate failed with %d\n", err);
			free(uncomp);
			free(body_buffer);
			return 2;
		}
	}

	if (inflateEnd (&strm) != Z_OK) {
		fprintf(stderr, "Auto-update Failed: inflateInit2 failed\n");
		free(uncomp);
		free(body_buffer);
		return 2;
	}

	SQUASH_OS_PATH tmpdir = squash_tmpdir();
	if (NULL == tmpdir) {
		fprintf(stderr, "Auto-update Failed: no temporary folder found\n");
		free(uncomp);
		free(body_buffer);
		return 2;
	}
	SQUASH_OS_PATH tmpf = squash_tmpf(tmpdir, NULL);
	if (NULL == tmpf) {
		fprintf(stderr, "Auto-update Failed: no temporary file available\n");
		free((void*)(tmpdir));
		free(uncomp);
		free(body_buffer);
		return 2;
	}
	FILE *fp = fopen(tmpf, "wb");
	if (NULL == fp) {
		fprintf(stderr, "Auto-update Failed: cannot open temporary file %s\n", tmpf);
		free((void*)(tmpdir));
		free((void*)(tmpf));
		free(uncomp);
		free(body_buffer);
		return 2;
	}
	fprintf(stderr, " to %s\n", tmpf);
	size_t fwrite_ret = fwrite(uncomp, sizeof(char), strm.total_out, fp);
	if (fwrite_ret != strm.total_out) {
		fprintf(stderr, "Auto-update Failed: fwrite failed %s\n", tmpf);
		fclose(fp);
		unlink(tmpf);
		free((void*)(tmpdir));
		free((void*)(tmpf));
		free(uncomp);
		free(body_buffer);
		return 2;
	}
	fclose(fp);
	free(uncomp);
	free(body_buffer);
	// chmod
	size_t exec_path_len = 2 * PATH_MAX;
	char* exec_path = static_cast<char*>(malloc(exec_path_len));
	if (NULL == exec_path) {
		fprintf(stderr, "Auto-update Failed: Insufficient memory allocating exec_path\n");
		free((void*)(tmpdir));
		free((void*)(tmpf));
		unlink(tmpf);
		return 2;
	}
	if (uv_exepath(exec_path, &exec_path_len) != 0) {
		if (!argv[0]) {
			fprintf(stderr, "Auto-update Failed: missing argv[0]\n");
			free((void*)(tmpdir));
			free((void*)(tmpf));
			unlink(tmpf);
			return 2;
		}
		assert(strlen(argv[0]) < 2 * PATH_MAX);
		memcpy(exec_path, argv[0], strlen(argv[0]));
	}
	struct stat current_st;
	int ret = stat(exec_path, &current_st);
	if (0 != ret) {
		fprintf(stderr, "Auto-update Failed: stat failed for %s\n", exec_path);
		free(exec_path);
		free((void*)(tmpdir));
		free((void*)(tmpf));
		unlink(tmpf);
		return 2;
	}
	ret = chmod(tmpf, current_st.st_mode | S_IXUSR);
	if (0 != ret) {
		fprintf(stderr, "Auto-update Failed: chmod failed for %s\n", tmpf);
		free(exec_path);
		free((void*)(tmpdir));
		free((void*)(tmpf));
		unlink(tmpf);
		return 2;
	}
	// move
	fprintf(stderr, "Moving %s to %s\n", tmpf, exec_path);
	ret = rename(tmpf, exec_path);
	if (0 != ret) {
		fprintf(stderr, "Auto-update Failed: failed calling rename %s to %s\n", tmpf, exec_path);
		free(exec_path);
		free((void*)(tmpdir));
		free((void*)(tmpf));
		unlink(tmpf);
		return 2;
	}
	fprintf(stderr, "Restarting\n");
	ret = execv(exec_path, argv);
	// we should not reach this point
	fprintf(stderr, "Auto-update Failed: execv failed with %d (errno %d)\n", ret, errno);
	free(exec_path);
	free((void*)(tmpdir));
	free((void*)(tmpf));
	unlink(tmpf);
}

#endif // !_WIN32
