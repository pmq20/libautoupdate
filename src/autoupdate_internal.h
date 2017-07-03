/*
 * Copyright (c) 2017 Minqi Pan <pmq2001@gmail.com>
 *
 * This file is part of libautoupdate, distributed under the MIT License
 * For full terms see the included LICENSE file
 */

#ifndef AUTOUPDATE_INTERNAL_H_A40E122A
#define AUTOUPDATE_INTERNAL_H_A40E122A

#ifdef _WIN32

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

#else
#endif // _WIN32

short autoupdate_should_proceed();

#endif /* end of include guard: AUTOUPDATE_INTERNAL_H_A40E122A */
