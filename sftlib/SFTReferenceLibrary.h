/* $Id$ */
/* This is a reference library for the SFT data format
 *
 * Copyright (C) 2004 Bruce Allen <ballen@uwm.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example COPYING); if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef REFERENCESFTLIBRARY_H_
#define REFERENCESFTLIBRARY_H_

/* header for version 1 SFTs (not part of SPEC, but here for backwards
   compatibility and clarity) */
struct headertag1 {
  double version;
  int    gps_sec;
  int    gps_nsec;
  double tbase;
  int    firstfreqindex;
  int    nsamples;
};

/* header for version 2 SFTs */
struct headertag2 {
  double              version;
  int                 gps_sec;
  int                 gps_nsec;
  double              tbase;
  int                 firstfreqindex;
  int                 nsamples;
  unsigned long long  crc64;
  int                 padding;
  int                 comment_length;
};

int WriteSFT(FILE   *fp,            /* stream to write to.  On return, is at the end of SFT */
	     int    gps_sec,        /* GPS sec of first sample */
	     int    gps_nsec,       /* GPS nsec of first sample */
	     double tbase,          /* time baseline of SFTs */
	     int    firstfreqindex, /* index of first frequency bin included in data (0=DC)*/
	     int    nsamples,       /* number of frequency bins to include in SFT */
	     char   *comment,       /* null-terminated comment string to include in SFT */
	     float  *data           /* points to nsamples x 2 x floats (Real/Imag)  */
	     );

int ReadSFTHeader(FILE              *fp,          /* stream to read. Position unchanged on return */
		  struct headertag2 *info,        /* address to return header */
		  char              **comment,    /* if non-NULL, put pointer to comment */
		  int               *swapendian,  /* set nonzero if data in reverse endian order */
		  int                validate);   /* validate checksum of the file */

int ReadSFTData(FILE              *fp,       /* data file.  Position unchanged on return */
		float             *data,     /* location where data should be written */
		int               offset,    /* starting offset (in frequency bins) into data set */
		int               nsamples,  /* number of frequency bin samples to retrieve */
		char              **comment, /* if non-NULL, will contain pointer to comment string */
		struct headertag2 *info      /* if non-NULL, will contain header information */
		);

/* various possible error codes.  See SFTErrorMessage() for decodings */
#define SFTENULLFP            1
#define SFTESEEK              2
#define SFTEGETSTREAMPOS      3
#define SFTERESTORESTREAMPOS  4
#define SFTEREAD              5
#define SFTEUNKNOWN           6
#define SFTEGPSNSEC           7
#define SFTEBADCOMMENT        8
#define SFTEBADCRC64          9
#define SFTENOMEM            10
#define SFTESIZEWRONG        11
#define SFTEWRITE            12
#define SFTENULLPOINTER      13
#define SFTENONE             14
#define SFTEHIDDENCOMMENT    15
#define SFTENONULLINCOMMENT  16

/* takes error code from above list and returns static human-readable
   description as null-terminated string */
const char *SFTErrorMessage(int errorcode);

/* returns a null-terminated string with the package version (eg, 2.0) */
const char *ReferenceSFTLibraryVersion();

#endif /* REFERENCESFTLIBRARY_H_ */
