/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: 
 * filename		: zlib.h
 * description	: deflate (RFC 1950,1951)
 *
 */

#pragma once
#include <zlib.h>
#include <zutil.h>
#include <string>
#include "typedef.h"

#define ZCHUNKSIZE (16*1024)




/*
 *	class ZLibCompressor
 *
 */
class ZLibCompressor
{
protected:
	int st;
	bool is_gz;
	std::string &out;
	z_stream zstream;
	ulong crc;

public:
	ZLibCompressor(string &buff, bool f) : out(buff), is_gz(f) {}
	~ZLibCompressor() {}

	bool init(int level, const char *in, uint len)
	{
		if (is_gz) {
			out.resize(10);
			out[0] = (char)0x1f;	//ID1
			out[1] = (char)0x8b;	//ID2
			out[2] = (char)0x08;	//CM 8:deflate
			out[3] = (char)0x00;	//FLG
			out[4] = (char)0x00;	//
			out[5] = (char)0x00;	//MTIME
			out[6] = (char)0x00;	//
			out[7] = (char)0x00;	//
			out[8] = (char)0x00;	//XFL
			out[9] = (char)0x0b;	//OS 0x0b:NTFS
			crc = crc32(0, (byte*)in, len);
		}
		
		zstream.zalloc = Z_NULL;
		zstream.zfree = Z_NULL;
		zstream.opaque = Z_NULL;
		st = deflateInit2(&zstream, level, Z_DEFLATED, -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY);
		if (st != Z_OK)
			return false;

		zstream.avail_in = len;
		zstream.next_in = (byte*)in;
		return true;
	}

	char *step(void)
	{
		if (st != Z_OK)
			return (NULL);

		uint curoutsize = out.size();
		uint newoutsize = curoutsize + ZCHUNKSIZE;
		out.resize(newoutsize);
		
		zstream.avail_out = ZCHUNKSIZE;
		zstream.next_out = (byte*)(&out[curoutsize]);
		
		st = deflate(&zstream, Z_FINISH);
		if (st == Z_STREAM_ERROR) {
			deflateEnd(&zstream);
			return (NULL);
		}
		
		if (st == Z_STREAM_END) {
			if (zstream.avail_out) {
				newoutsize -= zstream.avail_out;
				out.resize(newoutsize);
			}
			deflateEnd(&zstream);

			if (is_gz) {
				uint cursize = out.size();
				out.resize(cursize + 8);
				memcpy(&out[cursize], &crc, 4);
				memcpy(&out[cursize+4], &zstream.total_in, 4);
			}
		}

		return (&out[curoutsize]);
	}
	
	bool onetime(void)
	{
		while (step());
		if (st != Z_STREAM_END) return false;
		return true;
	}
};




/*
 *	class ZLibUncompressor
 *
 */
class ZLibUncompressor
{
protected:
	int st;
	bool is_gz;
	string &out;
	z_stream zstream;

public:
	ZLibUncompressor(string &buff, bool f) : out(buff), is_gz(f) {}
	~ZLibUncompressor() {}

	bool init(const char *in, uint len)
	{
		int offset = 0;
		if (is_gz) {
			const byte ASCII_FLAG	= 0x01; // bit 0 set: file probably ascii text
			const byte HEAD_CRC		= 0x02; // bit 1 set: header CRC present
			const byte EXTRA_FIELD	= 0x04; // bit 2 set: extra field present
			const byte ORIG_NAME	= 0x08; // bit 3 set: original file name present
			const byte COMMENT		= 0x10; // bit 4 set: file comment present
			const byte RESERVED		= 0xE0; // bits 5..7: reserved
			const byte gz_magic[2]	= {0x1f, 0x8b};

			byte		method; // method byte
			byte		flags;	// flags byte
			
			if ((byte)in[0] != gz_magic[0] && (byte)in[1] != gz_magic[1])
				return false;

			method = (byte)in[2];
			flags = (byte)in[3];
				
			if (method != Z_DEFLATED || (flags & RESERVED) != 0)
				return false;
				
			//Discard time, xflags and OS code
			offset = 10;
			if ((flags & EXTRA_FIELD) != 0) {
				//skip the extra field
				uint len;
				len  = in[offset];
				len += in[offset+1]<<8;
				offset += len;
			}
			if ((flags & ORIG_NAME) != 0) {
				//skip the original file name
				while (in[offset] != '\0') offset++;
				offset++;
			}
			if ((flags & COMMENT) != 0) {
				//skip the .gz file comment
				while (in[offset] != '\0') offset++;
				offset++;
			}
			if ((flags & HEAD_CRC) != 0) {
				//skip the header crc
				offset += 2;
			}
		}

		zstream.zalloc = Z_NULL;
		zstream.zfree = Z_NULL;
		zstream.opaque = Z_NULL;
		st = inflateInit2(&zstream, -MAX_WBITS); //windowBits is passed 
		if (st != Z_OK) //windowBits is passed
			return false;		

		zstream.avail_in = len - offset;
		zstream.next_in = (byte*)(in + offset);
		return true;
	}

	char *step(void)
	{
		if (st != Z_OK) {
			return (NULL);
		}

		uint curoutsize = out.size();
		uint newoutsize = curoutsize + ZCHUNKSIZE;
		out.resize(newoutsize);
		
		zstream.avail_out = ZCHUNKSIZE;
		zstream.next_out = (byte*)(&out[curoutsize]);
		
		st = inflate(&zstream, Z_FINISH);
		if (st == Z_STREAM_ERROR) {
			deflateEnd(&zstream);
			return (NULL);
		}
		if (st == Z_BUF_ERROR)
			st = Z_OK;
		
		if (st == Z_STREAM_END) {
			if (zstream.avail_out) {
				newoutsize -= zstream.avail_out;
				out.resize(newoutsize);
			}
			inflateEnd(&zstream);
		}

		return (&out[curoutsize]);
	}
	
	bool onetime(void)
	{
		while (step());
		if (st != Z_STREAM_END) return false;
		return true;
	}
};




#if 0
	bool zlibcompress(int level, const char *in, uint inlen, string &out)
	{
		zstream.zalloc = Z_NULL;
		zstream.zfree = Z_NULL;
		zstream.opaque = Z_NULL;
		if (deflateInit2(&zstream, level, Z_DEFLATED, -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY) != Z_OK)
			return false;

		zstream.avail_in = inlen;
		zstream.next_in = (byte*)in;

		int ret;
		do {
			uint curoutsize = out.size();
			uint newoutsize = curoutsize + ZCHUNKSIZE;
			out.resize(newoutsize);

			zstream.avail_out = ZCHUNKSIZE;
			zstream.next_out = (byte*)(&out[curoutsize]);

			ret = deflate(&zstream, Z_FINISH);
			if (ret == Z_STREAM_ERROR) {
				deflateEnd(&zstream);
				return false;
			}

			if (ret == Z_STREAM_END && zstream.avail_out) {
				newoutsize -= zstream.avail_out;
				out.resize(newoutsize);
			}
		} while (ret != Z_STREAM_END);

		deflateEnd(&zstream);
		return true;
	}

	bool zlibuncompress(const char *in, uint inlen, string &out)
	{
		z_stream zstream;
		zstream.zalloc = Z_NULL;
		zstream.zfree = Z_NULL;
		zstream.opaque = Z_NULL;
		if (inflateInit2(&zstream, -MAX_WBITS) != Z_OK) //windowBits is passed
			return false;		

		zstream.avail_in = inlen;
		zstream.next_in = (byte*)in;

		int ret;
		do {
			uint curoutsize = out.size();
			uint newoutsize = curoutsize + ZCHUNKSIZE;
			out.resize(newoutsize);

			zstream.avail_out = ZCHUNKSIZE;
			zstream.next_out = (byte*)(&out[curoutsize]);

			ret = inflate(&zstream, Z_FINISH);
			if (ret == Z_STREAM_ERROR) {
				deflateEnd(&zstream);
				return false;
			}

			if (ret == Z_STREAM_END && zstream.avail_out) {
				newoutsize -= zstream.avail_out;
				out.resize(newoutsize);
			}
		} while (ret != Z_STREAM_END);

		inflateEnd(&zstream);
		return true;
	}

	bool gzipcompress(int level, const char *in, uint inlen, string &out)
	{
		//gzip (RFC 1592)

		out.resize(10);
		out[0] = (char)0x1f;	//ID1
		out[1] = (char)0x8b;	//ID2
		out[2] = (char)0x08;	//CM 8:deflate
		out[3] = (char)0x00;	//FLG
		out[4] = (char)0x00;	//
		out[5] = (char)0x00;	//MTIME
		out[6] = (char)0x00;	//
		out[7] = (char)0x00;	//
		out[8] = (char)0x00;	//XFL
		out[9] = (char)0x0b;	//OS 0x0b:NTFS

		zlibcompress(level, in, inlen, out);

		uint cursize = out.size();
		out.resize(cursize + 8);

		ulong crc = crc32(0, (byte*)in, inlen);
		memcpy(&out[cursize], &crc, 4);
		memcpy(&out[cursize+4], &inlen, 4);

		return true;
	}

	bool gzipuncompress(const char *in, uint inlen, string &out)
	{
		const byte ASCII_FLAG	= 0x01; /* bit 0 set: file probably ascii text */
		const byte HEAD_CRC		= 0x02; /* bit 1 set: header CRC present */
		const byte EXTRA_FIELD	= 0x04; /* bit 2 set: extra field present */
		const byte ORIG_NAME	= 0x08; /* bit 3 set: original file name present */
		const byte COMMENT		= 0x10; /* bit 4 set: file comment present */
		const byte RESERVED		= 0xE0; /* bits 5..7: reserved */
		const byte gz_magic[2]	= {0x1f, 0x8b};

		byte		method; /* method byte */
		byte		flags;	/* flags byte */
		int			start = 0;
		
		if ((byte)in[0] != gz_magic[0] && (byte)in[1] != gz_magic[1])
			return false;

		method = (byte)in[2];
		flags = (byte)in[3];
			
		if (method != Z_DEFLATED || (flags & RESERVED) != 0)
			return false;
			
		/* Discard time, xflags and OS code: */
		start = 10;
		if ((flags & EXTRA_FIELD) != 0) { /* skip the extra field */
			uint len;
			len  = in[start];
			len += in[start+1]<<8;
			start += len;
		}
		if ((flags & ORIG_NAME) != 0) { /* skip the original file name */
			while (in[start] != '\0') start++;
			start++;
		}
		if ((flags & COMMENT) != 0) {	/* skip the .gz file comment */
			while (in[start] != '\0') start++;
			start++;
		}
		if ((flags & HEAD_CRC) != 0) {	/* skip the header crc */
			start += 2;
		}
		
		return (zlibuncompress(&in[start], inlen - start, out));
	}
};
#endif