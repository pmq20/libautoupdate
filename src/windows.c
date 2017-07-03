/*
 * Copyright (c) 2017 Minqi Pan <pmq2001@gmail.com>
 *
 * This file is part of libautoupdate, distributed under the MIT License
 * For full terms see the included LICENSE file
 */

#ifdef _WIN32

#include "autoupdate.h"
#include "autoupdate_internal.h"
#include "zlib.h"

#include <assert.h>
#include <Windows.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <conio.h>
#include <stdint.h>
#include <stdlib.h> /* exit */

int autoupdate(
	int argc,
	wchar_t *wargv[],
	const char *host,
	const char *port,
	const char *path,
	const char *current
)
{
	WSADATA wsaData;
	intptr_t intptr_ret;

	if (!autoupdate_should_proceed()) {
		return 1;
	}

	// Initialize Winsock
	int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	if (iResult != 0) {
		fprintf(stderr, "Auto-update Failed: WSAStartup failed with %d\n", iResult);
		return 2;
	}

	struct addrinfo *result = NULL,
		*ptr = NULL,
		hints;

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// Resolve the server address and port
	iResult = getaddrinfo(host, port, &hints, &result);
	if (iResult != 0) {
		fprintf(stderr, "Auto-update Failed: getaddrinfo failed with %d\n", iResult);
		WSACleanup();
		return 2;
	}

	SOCKET ConnectSocket = INVALID_SOCKET;

	// Attempt to connect to the first address returned by
	// the call to getaddrinfo
	ptr = result;

	// Create a SOCKET for connecting to server
	ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
		ptr->ai_protocol);

	if (ConnectSocket == INVALID_SOCKET) {
		fprintf(stderr, "Auto-update Failed: Error at socket() with %d\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return 2;
	}

	// Connect to server.
	iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		closesocket(ConnectSocket);
		ConnectSocket = INVALID_SOCKET;
	}
	freeaddrinfo(result);
	if (ConnectSocket == INVALID_SOCKET) {
		fprintf(stderr, "Auto-update Failed: connect failed on %s and port %s\n", host, port);
		WSACleanup();
		return 2;
	}
	if (5 != send(ConnectSocket, "HEAD ", 5, 0) ||
	    strlen(path) != send(ConnectSocket, path, strlen(path), 0) ||
	    13 != send(ConnectSocket, " HTTP/1.0\r\n\r\n", 13, 0)) {
		fprintf(stderr, "Auto-update Failed: send failed with %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
		return 2;
	}

	char response[1024 * 10 + 1]; // 10KB
	int bytes, total;
	total = sizeof(response) - 2;
	long long received = 0;
	do {
		bytes = recv(ConnectSocket, response + received, total - received, 0);
		if (bytes < 0) {
			fprintf(stderr, "Auto-update Failed: recv failed with %d\n", WSAGetLastError());
			closesocket(ConnectSocket);
			WSACleanup();
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
		fprintf(stderr, "Auto-update Failed: read causes buffer full\n");
		closesocket(ConnectSocket);
		WSACleanup();
		return 2;
	}

	// shutdown the connection for sending since no more data will be sent
	// the client can still use the ConnectSocket for receiving data
	iResult = shutdown(ConnectSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		fprintf(stderr, "Auto-update Failed: shutdown failed with %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
		return 2;
	}

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
	char *port2 = "80";
	if (strlen(url) >= 8 && 0 == strncmp("https://", url, 8)) {
		host2 = url + 8;
	} else if (strlen(url) >= 7 && 0 == strncmp("http://", url, 7)) {
		host2 = url + 7;
	} else {
		fprintf(stderr, "Auto-update Failed: failed to find http:// or https:// at the beginning of URL %s\n", url);
		return 2;
	}
	char *found_slash = host2.strchr('/');
	char *request_path;
	if (NULL == found_slash) {
		request_path = "/";
	} else {
		request_path = found_slash;
		*found_slash = 0;
	}

	result = NULL;
	ptr = NULL;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// Resolve the server address and port
	iResult = getaddrinfo(host2.c_str(), port2.c_str(), &hints, &result);
	if (iResult != 0) {
		fprintf(stderr, "Auto-update Failed: getaddrinfo failed with %d\n", iResult);
		WSACleanup();
		return 2;
	}

	ConnectSocket = INVALID_SOCKET;

	// Attempt to connect to the first address returned by
	// the call to getaddrinfo
	ptr = result;

	// Create a SOCKET for connecting to server
	ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

	if (ConnectSocket == INVALID_SOCKET) {
		fprintf(stderr, "Auto-update Failed: Error at socket() with %d\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return 2;
	}
	// Connect to server.
	iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		closesocket(ConnectSocket);
		ConnectSocket = INVALID_SOCKET;
	}
	freeaddrinfo(result);
	if (ConnectSocket == INVALID_SOCKET) {
		fprintf(stderr, "Auto-update Failed: connect failed on %s and port %s\n", host2, port2);
		WSACleanup();
		return 2;
	}
	if (NULL != found_slash) {
		*found_slash = '/';
	}
	if (4 != send(ConnectSocket, "GET ", 4, 0) ||
		strlen(request_path) != send(ConnectSocket, request_path, strlen(request_path), 0) ||
		11 != send(ConnectSocket, " HTTP/1.0\r\n", 11, 0)) {
			fprintf(stderr, "Auto-update Failed: send failed with %d\n", WSAGetLastError());
			closesocket(ConnectSocket);
			WSACleanup();
			return 2;
	}
	if (NULL != found_slash) {
		*found_slash = 0;
	}
	if (6 != send(ConnectSocket, "Host: ", 6, 0) ||
		strlen(host2) != send(ConnectSocket, host2, strlen(host2), 0) ||
		4 != send(ConnectSocket, "\r\n\r\n", 4, 0)) {
			fprintf(stderr, "Auto-update Failed: send failed with %d\n", WSAGetLastError());
			closesocket(ConnectSocket);
			WSACleanup();
			return 2;
	}

	// Read the header
	total = sizeof(response) - 2;
	response[sizeof(response) - 1] = 0;
	received = 0;
	char *header_end = NULL;
	do {
		bytes = recv(ConnectSocket, response + received, total - received, 0);
		if (bytes < 0) {
			fprintf(stderr, "Auto-update Failed: recv failed with %d\n", WSAGetLastError());
			closesocket(ConnectSocket);
			WSACleanup();
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
		fprintf(stderr, "Auto-update Failed: failed to find the end of the response header\n");
		closesocket(ConnectSocket);
		WSACleanup();
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
			found_length = std::stoll(response + i + 16);
			break;
		}
		*new_line = '\r';
		i = new_line - response + 2;
	}
	if (-1 == found_length) {
		fprintf(stderr, "Auto-update Failed: failed to find a Content-Length header\n");
		closesocket(ConnectSocket);
		WSACleanup();
		return 2;
	}
	if (0 == found_length) {
		fprintf(stderr, "Auto-update Failed: found a Content-Length header of zero\n");
		closesocket(ConnectSocket);
		WSACleanup();
		return 2;
	}
	assert(found_length > 0);
	// Read the body
	// header_end -> \r\n\r\n
	assert(header_end);
	assert(header_end + 4 <= response + received);
	// put the rest of over-read content when reading header
	size_t the_rest = response + received - (header_end + 4);
	char *body_buffer = static_cast<char *>(malloc(found_length));
	if (NULL == body_buffer) {
		fprintf(stderr, "Auto-update Failed: Insufficient memory\n");
		closesocket(ConnectSocket);
		WSACleanup();
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
		bytes = recv(ConnectSocket, body_buffer_ptr, space, 0);
		if (bytes < 0) {
			fprintf(stderr, "Auto-update Failed: read failed\n");
			free(body_buffer);
			closesocket(ConnectSocket);
			WSACleanup();
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
		closesocket(ConnectSocket);
		WSACleanup();
		free(body_buffer);
		return 2;
	}
	fprintf(stderr, "\n");
	fflush(stderr);
	// shutdown the connection for sending since no more data will be sent
	// the client can still use the ConnectSocket for receiving data
	iResult = shutdown(ConnectSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		fprintf(stderr, "Auto-update Failed: shutdown failed with %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
		return 2;
	}
	// Inflate to a file
	fprintf(stderr, "Inflating...");
	fflush(stderr);
	ZIPLocalFileHeader *h = (ZIPLocalFileHeader *)body_buffer;
	if (!(0x04034b50 == h->signature && 8 == h->compressionMethod)) {
		fprintf(stderr, "Auto-update Failed: We only support a zip file containing "
			"one Deflate compressed file for the moment.\n"
			"Pull requests are welcome on GitHub at "
			"https://github.com/pmq20/libautoupdate\n");
	}
	// skip the Local File Header
	unsigned full_length = found_length - sizeof(ZIPLocalFileHeader) - h->fileNameLength;
	unsigned half_length = full_length / 2;
	unsigned uncompLength = full_length;

	/* windowBits is passed < 0 to tell that there is no zlib header.
	* Note that in this case inflate *requires* an extra "dummy" byte
	* after the compressed stream in order to complete decompression and
	* return Z_STREAM_END.
	*/
	char* uncomp = (char*)calloc(sizeof(char), uncompLength + 1);
	if (NULL == uncomp) {
		fprintf(stderr, "Auto-update Failed: Insufficient memory\n");
		free(body_buffer);
		return 2;
	}

	z_stream strm;
	strm.next_in = (z_const Bytef *)(body_buffer + sizeof(ZIPLocalFileHeader) + h->fileNameLength);
	strm.avail_in = found_length;
	strm.total_out = 0;
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;

	short done = 0;

	if (inflateInit2(&strm, -MAX_WBITS) != Z_OK) {
		free(uncomp);
		free(body_buffer);
		fprintf(stderr, "Auto-update Failed: inflateInit2 failed\n");
		return 2;
	}

	while (!done) {
		// If our output buffer is too small
		if (strm.total_out >= uncompLength) {
			// Increase size of output buffer
			char* uncomp2 = (char*)calloc(sizeof(char), uncompLength + half_length + 1);
			if (NULL == uncomp2) {
				free(uncomp);
				free(body_buffer);
				fprintf(stderr, "Auto-update Failed: calloc failed\n");
				return 2;
			}
			memcpy(uncomp2, uncomp, uncompLength);
			uncompLength += half_length;
			free(uncomp);
			uncomp = uncomp2;
		}

		strm.next_out = (Bytef *)(uncomp + strm.total_out);
		strm.avail_out = uncompLength - strm.total_out;

		// Inflate another chunk.
		int err = inflate(&strm, Z_SYNC_FLUSH);
		if (err == Z_STREAM_END) {
			done = 1;
		}
		else if (err != Z_OK) {
			fprintf(stderr, "Auto-update Failed: inflate failed with %d\n", err);
			free(uncomp);
			free(body_buffer);
			return 2;
		}
	}

	if (inflateEnd(&strm) != Z_OK) {
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
	SQUASH_OS_PATH tmpf = squash_tmpf(tmpdir, "exe");
	if (NULL == tmpf) {
		fprintf(stderr, "Auto-update Failed: no temporary file available\n");
		free((void*)(tmpdir));
		free(uncomp);
		free(body_buffer);
		return 2;
	}
	FILE *fp = _wfopen(tmpf, L"wb");
	if (NULL == fp) {
		fprintf(stderr, "Auto-update Failed: cannot open temporary file %S\n", tmpf);
		free((void*)(tmpdir));
		free((void*)(tmpf));
		free(uncomp);
		free(body_buffer);
		return 2;
	}
	fprintf(stderr, " to %S\n", tmpf);
	size_t fwrite_ret = fwrite(uncomp, sizeof(char), strm.total_out, fp);
	if (fwrite_ret != strm.total_out) {
		fprintf(stderr, "Auto-update Failed: fwrite failed %S\n", tmpf);
		fclose(fp);
		DeleteFileW(tmpf);
		free((void*)(tmpdir));
		free((void*)(tmpf));
		free(uncomp);
		free(body_buffer);
		return 2;
	}
	fclose(fp);
	free(uncomp);
	free(body_buffer);
	/* Windows paths can never be longer than this. */
	const size_t utf16_buffer_len = 32768;
	wchar_t utf16_buffer[utf16_buffer_len];
	DWORD utf16_len = GetModuleFileNameW(NULL, utf16_buffer, utf16_buffer_len);
	if (0 == utf16_len) {
		fprintf(stderr, "Auto-update Failed: GetModuleFileNameW failed with GetLastError=%d\n", GetLastError());
		DeleteFileW(tmpf);
		free((void*)(tmpdir));
		free((void*)(tmpf));
		return 2;
	}
	// Moving
	SQUASH_OS_PATH selftmpf = squash_tmpf(tmpdir, "exe");
	if (NULL == selftmpf) {
		fprintf(stderr, "Auto-update Failed: no temporary file available\n");
		DeleteFileW(tmpf);
		free((void*)(tmpdir));
		free((void*)(tmpf));
		return 2;
	}
	fprintf(stderr, "Moving %S to %S\n", utf16_buffer, selftmpf);
	BOOL ret = MoveFileW(utf16_buffer, selftmpf);
	if (!ret) {
		fprintf(stderr, "Auto-update Failed: MoveFileW failed with GetLastError=%d\n", GetLastError());
		DeleteFileW(tmpf);
		DeleteFileW(selftmpf);
		free((void*)(tmpdir));
		free((void*)(tmpf));
		free((void*)(selftmpf));
		return 2;
	}
	fprintf(stderr, "Moving %S to %S \n", tmpf, utf16_buffer);
	ret = MoveFileW(tmpf, utf16_buffer);
	if (!ret) {
		fprintf(stderr, "Auto-update Failed: MoveFileW failed with GetLastError=%d\n", GetLastError());
		DeleteFileW(tmpf);
		DeleteFileW(selftmpf);
		free((void*)(tmpdir));
		free((void*)(tmpf));
		free((void*)(selftmpf));
		return 2;
	}
	// Restarting
	fprintf(stderr, "Restarting\n");
	fflush(stderr);
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));
	ret = CreateProcess(
		NULL,	     // No module name (use command line)
		GetCommandLine(), // Command line
		NULL,	     // Process handle not inheritable
		NULL,	     // Thread handle not inheritable
		FALSE,	    // Set handle inheritance to FALSE
		0,		// No creation flags
		NULL,	     // Use parent's environment block
		NULL,	     // Use parent's starting directory 
		&si,	      // Pointer to STARTUPINFO structure
		&pi	       // Pointer to PROCESS_INFORMATION structure
	);
	if (!ret) {
		fprintf(stderr, "Auto-update Failed: CreateProcess failed with GetLastError=%d\n", GetLastError());
		DeleteFileW(tmpf);
		DeleteFileW(selftmpf);
		free((void*)(tmpdir));
		free((void*)(tmpf));
		free((void*)(selftmpf));
		return 2;
	}
	exit(0);
}

#endif // _WIN32
