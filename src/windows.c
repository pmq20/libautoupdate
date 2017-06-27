/*
 * Copyright (c) 2017 Minqi Pan <pmq2001@gmail.com>
 *
 * This file is part of libautoupdate, distributed under the MIT License
 * For full terms see the included LICENSE file
 */

#ifdef _WIN32

#include "autoupdate.h"

#include <assert.h>
#include <Windows.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <conio.h>
#include "zlib.h"
#include <iostream>
#include <string>
#include <algorithm>
#include <functional>
#include <cctype>
#include <locale>
#include <stdint.h>

#define PACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop) )

PACK(
	struct ZIPLocalFileHeader
{
	uint32_t signature;
	uint16_t versionNeededToExtract; // unsupported
	uint16_t generalPurposeBitFlag; // unsupported
	uint16_t compressionMethod;
	uint16_t lastModFileTime;
	uint16_t lastModFileDate;
	uint32_t crc32;
	uint32_t compressedSize;
	uint32_t uncompressedSize;
	uint16_t fileNameLength;
	uint16_t extraFieldLength; // unsupported
});

static short should_proceed()
{
	TCHAR lpBuffer[32767 + 1];
	if (0 == GetEnvironmentVariable("LIBAUTOUPDATE_SKIP", lpBuffer, 32767)) {
		return 1;
	} else {
		return 0;
	}
}

int autoupdate(int argc, wchar_t *wargv[])
{
	WSADATA wsaData;
	intptr_t intptr_ret;

	if (!should_proceed()) {
		return 1;
	}

	// Initialize Winsock
	int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	if (iResult != 0) {
		std::cerr << "Auto-update Failed: WSAStartup failed with " << iResult << std::endl;
		return;
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
		std::cerr << "Auto-update Failed: getaddrinfo failed with " << iResult << std::endl;
		WSACleanup();
		return;
	}

	SOCKET ConnectSocket = INVALID_SOCKET;

	// Attempt to connect to the first address returned by
	// the call to getaddrinfo
	ptr = result;

	// Create a SOCKET for connecting to server
	ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
		ptr->ai_protocol);

	if (ConnectSocket == INVALID_SOCKET) {
		std::cerr << "Auto-update Failed: Error at socket() with " << WSAGetLastError() << std::endl;
		freeaddrinfo(result);
		WSACleanup();
		return;
	}

	// Connect to server.
	iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		closesocket(ConnectSocket);
		ConnectSocket = INVALID_SOCKET;
	}
	freeaddrinfo(result);
	if (ConnectSocket == INVALID_SOCKET) {
		std::cerr << "Auto-update Failed: connect failed on " << host << " and port " << port << std::endl;
		WSACleanup();
		return;
	}
	if (5 != send(ConnectSocket, "HEAD ", 5, 0) ||
	    strlen(path) != send(ConnectSocket, path, strlen(path), 0) ||
	    13 != send(ConnectSocket, " HTTP/1.0\r\n\r\n", 13, 0)) {
		std::cerr << "Auto-update Failed: send failed with " << WSAGetLastError() << std::endl;
		closesocket(ConnectSocket);
		WSACleanup();
		return;
	}

	char response[1024 * 10 + 1]; // 10KB
	int bytes, received, total;
	total = sizeof(response) - 2;
	received = 0;
	do {
		bytes = recv(ConnectSocket, response + received, total - received, 0);
		if (bytes < 0) {
			std::cerr << "Auto-update Failed: recv failed with " << WSAGetLastError() << std::endl;
			closesocket(ConnectSocket);
			WSACleanup();
			return;
		}
		if (bytes == 0) {
			/* EOF */
			*(response + received) = 0;
			break;
		}
		received += bytes;
	} while (received < total);
	if (received == total) {
		std::cerr << "Auto-update Failed: read causes buffer full" << std::endl;
		closesocket(ConnectSocket);
		WSACleanup();
		return;
	}

	// shutdown the connection for sending since no more data will be sent
	// the client can still use the ConnectSocket for receiving data
	iResult = shutdown(ConnectSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		std::cerr << "Auto-update Failed: shutdown failed with " << WSAGetLastError() << std::endl;
		closesocket(ConnectSocket);
		WSACleanup();
		return;
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
		std::cerr << "Auto-update Failed: failed to find a Location header" << std::endl;
		return;
	}
	if (strstr(found, current)) {
		/* Latest version confirmed. No need to update */
		return 0;
	}
	std::string s;
	std::cerr << "New version detected. Would you like to update? [y/N]: " << std::flush;


	static HANDLE stdinHandle;
	// Get the IO handles
	// getc(stdin);
	stdinHandle = GetStdHandle(STD_INPUT_HANDLE);
	bool cont = true;
	while (cont)
	{
		switch (WaitForSingleObject(stdinHandle, 10000))
		{
		case(WAIT_TIMEOUT):
			std::cerr << std::endl;
			std::cerr << "10 seconds timed out. Will not update." << std::endl;
			return;
		case(WAIT_OBJECT_0):
			if (_kbhit()) // _kbhit() always returns immediately
			{
				std::getline(std::cin, s);
				s.erase(s.begin(), std::find_if(s.begin(), s.end(),
					std::not1(std::ptr_fun<int, int>(std::isspace))));
				s.erase(std::find_if(s.rbegin(), s.rend(),
					std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
				if ("Y" != s && "y" != s) {
					/* The user refused to update */
					return;
				}
				cont = false;
			} else { // some sort of other events , we need to clear it from the queue
				// clear events
				INPUT_RECORD r[512];
				DWORD read;
				ReadConsoleInput(stdinHandle, r, 512, &read);
			}
			break;
		case(WAIT_FAILED):
			std::cerr << std::endl;
			std::cerr << "Auto-update Failed: WaitForSingleObject failed. WAIT FAILED." << std::endl;
			return;
		case(WAIT_ABANDONED):
			std::cerr << std::endl;
			std::cerr << "Auto-update Failed: WaitForSingleObject failed. WAIT ABANDONED." << std::endl;
			return;
		default:
			std::cerr << std::endl;
			std::cerr << "Auto-update Failed: WaitForSingleObject failed. Someting unexpected was returned." << std::endl;
			return;
		}
	}

	std::string url{ found };
	std::cerr << "Downloading from " << url << std::endl;
	// TODO https
	std::string host2;
	if (url.size() >= 8 && "https://" == url.substr(0, 8)) {
		host2 = url.substr(8);
	} else if (url.size() >= 7 && "http://" == url.substr(0, 7)) {
		host2 = url.substr(7);
	} else {
		std::cerr << "Auto-update Failed: failed to find http:// or https:// at the beginning of URL " << url << std::endl;
		return;
	}
	std::size_t found_slash = host2.find('/');
	std::string request_path;
	if (std::string::npos == found_slash) {
		request_path = '/';
	}
	else {
		request_path = host2.substr(found_slash);
		host2 = host2.substr(0, found_slash);
	}
	std::size_t found_colon = host2.find(':');
	std::string port2;
	if (std::string::npos == found_colon) {
		port2 = "80";
	}
	else {
		port2 = host2.substr(found_colon + 1);
		host2 = host2.substr(0, found_colon);
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
		std::cerr << "Auto-update Failed: getaddrinfo failed with " << iResult << std::endl;
		WSACleanup();
		return;
	}

	ConnectSocket = INVALID_SOCKET;

	// Attempt to connect to the first address returned by
	// the call to getaddrinfo
	ptr = result;

	// Create a SOCKET for connecting to server
	ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

	if (ConnectSocket == INVALID_SOCKET) {
		std::cerr << "Auto-update Failed: Error at socket() with " << WSAGetLastError() << std::endl;
		freeaddrinfo(result);
		WSACleanup();
		return;
	}
	// Connect to server.
	iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		closesocket(ConnectSocket);
		ConnectSocket = INVALID_SOCKET;
	}
	freeaddrinfo(result);
	if (ConnectSocket == INVALID_SOCKET) {
		std::cerr << "Auto-update Failed: connect failed on " << host2 << " and port " << port2 << std::endl;
		WSACleanup();
		return;
	}
	if (4 != send(ConnectSocket, "GET ", 4, 0) ||
		request_path.size() != send(ConnectSocket, request_path.c_str(), request_path.size(), 0) ||
		11 != send(ConnectSocket, " HTTP/1.0\r\n", 11, 0) ||
		6 != send(ConnectSocket, "Host: ", 6, 0) ||
		host2.size() != send(ConnectSocket, host2.c_str(), host2.size(), 0) ||
		4 != send(ConnectSocket, "\r\n\r\n", 4, 0)) {
		std::cerr << "Auto-update Failed: send failed with " << WSAGetLastError() << std::endl;
		closesocket(ConnectSocket);
		WSACleanup();
		return;
	}
	// Read the header
	total = sizeof(response) - 2;
	response[sizeof(response) - 1] = 0;
	received = 0;
	char *header_end = NULL;
	do {
		bytes = recv(ConnectSocket, response + received, total - received, 0);
		if (bytes < 0) {
			std::cerr << "Auto-update Failed: recv failed with " << WSAGetLastError() << std::endl;
			closesocket(ConnectSocket);
			WSACleanup();
			return;
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
		std::cerr << "Auto-update Failed: failed to find the end of the response header" << std::endl;
		closesocket(ConnectSocket);
		WSACleanup();
		return;
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
		std::cerr << "Auto-update Failed: failed to find a Content-Length header" << std::endl;
		closesocket(ConnectSocket);
		WSACleanup();
		return;
	}
	if (0 == found_length) {
		std::cerr << "Auto-update Failed: found a Content-Length header of zero" << std::endl;
		closesocket(ConnectSocket);
		WSACleanup();
		return;
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
		std::cerr << "Auto-update Failed: Insufficient memory" << std::endl;
		closesocket(ConnectSocket);
		WSACleanup();
		return;
	}
	memcpy(body_buffer, (header_end + 4), the_rest);
	char *body_buffer_ptr = body_buffer + the_rest;
	char *body_buffer_end = body_buffer + found_length;
	// read the remaining body
	received = the_rest;
	std::cerr << '\r' << received << " / " << found_length << " bytes finished (" << received * 100LL / found_length << "%)";
	while (received < found_length) {
		size_t space = 100 * 1024;
		if (space > body_buffer_end - body_buffer_ptr) {
			space = body_buffer_end - body_buffer_ptr;
		}
		bytes = recv(ConnectSocket, body_buffer_ptr, space, 0);
		if (bytes < 0) {
			std::cerr << "Auto-update Failed: read failed" << std::endl;
			free(body_buffer);
			closesocket(ConnectSocket);
			WSACleanup();
			return;
		}
		if (bytes == 0) {
			/* EOF */
			break;
		}
		received += bytes;
		body_buffer_ptr += bytes;
		std::cerr << '\r' << received << " / " << found_length << " bytes finished (" << received * 100LL / found_length << "%)";
	}
	if (received != found_length) {
		assert(received < found_length);
		std::cerr << "Auto-update Failed: prematurely reached EOF after reading " << received << " bytes" << std::endl;
		closesocket(ConnectSocket);
		WSACleanup();
		free(body_buffer);
		return;
	}
	std::cerr << std::endl;
	// shutdown the connection for sending since no more data will be sent
	// the client can still use the ConnectSocket for receiving data
	iResult = shutdown(ConnectSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		std::cerr << "Auto-update Failed: shutdown failed with " << WSAGetLastError() << std::endl;
		closesocket(ConnectSocket);
		WSACleanup();
		return;
	}
	// Inflate to a file
	std::cerr << "Inflating" << std::flush;
	ZIPLocalFileHeader *h = (ZIPLocalFileHeader *)body_buffer;
	if (!(0x04034b50 == h->signature && 8 == h->compressionMethod)) {
		std::cerr << "Auto-update Failed: We only support a zip file containing "
			"one Deflate compressed file for the moment.\n"
			"Pull requests are welcome on GitHub at "
			"https://github.com/pmq20/libautoupdate" << std::endl;
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
		std::cerr << "Auto-update Failed: Insufficient memory" << std::endl;
		free(body_buffer);
		return;
	}

	z_stream strm;
	strm.next_in = (z_const Bytef *)(body_buffer + sizeof(ZIPLocalFileHeader) + h->fileNameLength);
	strm.avail_in = found_length;
	strm.total_out = 0;
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;

	bool done = false;

	if (inflateInit2(&strm, -MAX_WBITS) != Z_OK) {
		free(uncomp);
		free(body_buffer);
		std::cerr << "Auto-update Failed: inflateInit2 failed" << std::endl;
		return;
	}

	while (!done) {
		// If our output buffer is too small
		if (strm.total_out >= uncompLength) {
			// Increase size of output buffer
			char* uncomp2 = (char*)calloc(sizeof(char), uncompLength + half_length + 1);
			if (NULL == uncomp2) {
				free(uncomp);
				free(body_buffer);
				std::cerr << "Auto-update Failed: calloc failed" << std::endl;
				return;
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
			done = true;
		}
		else if (err != Z_OK) {
			std::cerr << "Auto-update Failed: inflate failed with " << err << std::endl;
			free(uncomp);
			free(body_buffer);
			return;
		}
	}

	if (inflateEnd(&strm) != Z_OK) {
		std::cerr << "Auto-update Failed: inflateInit2 failed" << std::endl;
		free(uncomp);
		free(body_buffer);
		return;
	}

	SQUASH_OS_PATH tmpdir = squash_tmpdir();
	if (NULL == tmpdir) {
		std::cerr << "Auto-update Failed: no temporary folder found" << std::endl;
		free(uncomp);
		free(body_buffer);
		return;
	}
	SQUASH_OS_PATH tmpf = squash_tmpf(tmpdir, "exe");
	if (NULL == tmpf) {
		std::cerr << "Auto-update Failed: no temporary file available" << std::endl;
		free((void*)(tmpdir));
		free(uncomp);
		free(body_buffer);
		return;
	}
	FILE *fp = _wfopen(tmpf, L"wb");
	if (NULL == fp) {
		std::cerr << "Auto-update Failed: cannot open temporary file " << tmpf << std::endl;
		free((void*)(tmpdir));
		free((void*)(tmpf));
		free(uncomp);
		free(body_buffer);
		return;
	}
	std::cerr << " to ";
	std::wcerr << tmpf << std::endl;
	size_t fwrite_ret = fwrite(uncomp, sizeof(char), strm.total_out, fp);
	if (fwrite_ret != strm.total_out) {
		std::cerr << "Auto-update Failed: fwrite failed " << tmpf << std::endl;
		fclose(fp);
		DeleteFileW(tmpf);
		free((void*)(tmpdir));
		free((void*)(tmpf));
		free(uncomp);
		free(body_buffer);
		return;
	}
	fclose(fp);
	free(uncomp);
	free(body_buffer);
	/* Windows paths can never be longer than this. */
	const size_t utf16_buffer_len = 32768;
	wchar_t utf16_buffer[utf16_buffer_len];
	DWORD utf16_len = GetModuleFileNameW(NULL, utf16_buffer, utf16_buffer_len);
	if (0 == utf16_len) {
		std::cerr << "Auto-update Failed: GetModuleFileNameW failed with GetLastError=" << GetLastError() << std::endl;
		DeleteFileW(tmpf);
		free((void*)(tmpdir));
		free((void*)(tmpf));
		return;
	}
	// Moving
	SQUASH_OS_PATH selftmpf = squash_tmpf(tmpdir, "exe");
	if (NULL == selftmpf) {
		std::cerr << "Auto-update Failed: no temporary file available" << std::endl;
		DeleteFileW(tmpf);
		free((void*)(tmpdir));
		free((void*)(tmpf));
		return;
	}
	std::cerr << "Moving ";
	std::wcerr << utf16_buffer;
	std::cerr << " to ";
	std::wcerr << selftmpf;
	std::cerr << std::endl;
	BOOL ret = MoveFileW(utf16_buffer, selftmpf);
	if (!ret) {
		std::cerr << "Auto-update Failed: MoveFileW failed with GetLastError=" << GetLastError() << std::endl;
		DeleteFileW(tmpf);
		DeleteFileW(selftmpf);
		free((void*)(tmpdir));
		free((void*)(tmpf));
		free((void*)(selftmpf));
		return;
	}
	std::cerr << "Moving ";
	std::wcerr << tmpf;
	std::cerr << " to ";
	std::wcerr << utf16_buffer;
	std::cerr << std::endl;
	ret = MoveFileW(tmpf, utf16_buffer);
	if (!ret) {
		std::cerr << "Auto-update Failed: MoveFileW failed with GetLastError=" << GetLastError() << std::endl;
		DeleteFileW(tmpf);
		DeleteFileW(selftmpf);
		free((void*)(tmpdir));
		free((void*)(tmpf));
		free((void*)(selftmpf));
		return;
	}
	// Restarting
	std::cerr << "Restarting" << std::endl;
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
		std::cerr << "Auto-update Failed: CreateProcess failed with GetLastError=" << GetLastError() << std::endl;
		DeleteFileW(tmpf);
		DeleteFileW(selftmpf);
		free((void*)(tmpdir));
		free((void*)(tmpf));
		free((void*)(selftmpf));
		return;
	}
	// Wait until child process exits.
	WaitForSingleObject(pi.hProcess, INFINITE);
	// Close process and thread handles. 
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	std::cerr << "Deleting ";
	std::wcerr << selftmpf;
	std::cerr << std::endl;
	intptr_ret = _wexeclp(L"cmd", L"cmd", L"/c", L"ping", L"127.0.0.1", L"-n", L"3", L">nul", L"&", L"del", selftmpf, NULL);
	// we should never reach here
	std::cerr << "Auto-update Failed: _wexeclp failed with " << intptr_ret << "(errno " << errno << ")" << std::endl;
}

#endif // _WIN32
