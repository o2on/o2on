/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: 
 * filename		: barray.h
 * description	: 
 *
 */

#pragma once
#include "typedef.h"
#include "../cryptopp/osrng.h"




#if 1
template<size_t N, typename T = byte>
class barrayT
{
public:
	typedef T block_type;
	static const size_t block_size = sizeof(T);
	static const size_t block_length = N/sizeof(T);
	T block[block_length];

	// constructor
	barrayT(void)
    {
		clear();
	}
	barrayT(const barrayT<N> &src)
	{
		assign(src);
	}
	barrayT(const byte *in, size_t len)
	{
		assign(in, len);
	}

	// =
	barrayT& operator=(const barrayT<N> &src)
	{
		assign(src);
		return (*this);
	}

	// assign
	void assign(const barrayT<N> &src)
	{
        for (size_t i = 0; i < block_length; i++)
			block[i] = src.block[i];
	}
	bool assign(const byte *in, size_t len)
	{
		if (!len || len != N)
			return false;
        memcpy((byte*)block, in, N);
		return true;
	}
	bool assign(const char *in, size_t len)
	{
		if (!len || len/2 != N)
			return false;
		hex2byte(in, len, (byte*)block);
		return true;
	}
	bool assign(const wchar_t *in, size_t len)
	{
		if (!len || len/2 != N)
			return false;
		whex2byte(in, len, (byte*)block);
		return true;
	}

	// to_string
	void to_string(std::string &out) const
	{
		byte2hex((byte*)block, N, out);
	}
	void to_string(std::wstring &out) const
	{
		byte2whex((byte*)block, N, out);
	}

	// clear
	void clear(void)
	{
        for (size_t i = 0; i < block_length; i++)
			block[i] = 0;
	}

	// random
	void random(void)
	{
		CryptoPP::AutoSeededRandomPool rng;
		rng.GenerateBlock((byte*)block, N);
	}

	// size
	size_t size(void) const
	{
		return (N);
	}

	// data
	const byte *data(void) const
	{
		return ((byte*)block);
	}
};

template<size_t N>
bool operator==(const barrayT<N> &x, const barrayT<N> &y)
{
    return (memcmp(x.block, y.block, N) == 0 ? true : false);
}
template<size_t N>
bool operator!=(const barrayT<N> &x, const barrayT<N> &y)
{
    return !(x == y);
}
template<size_t N>
bool operator<(const barrayT<N> &x, const barrayT<N> &y)
{
    return std::lexicographical_compare(x.data(), x.data()+N, y.data(), y.data()+N);
}

#else

template<size_t N>
class barrayT
{
public:
	unsigned char block[N];

	barrayT(void)
    {
        memset(block, 0, N);
    }
	barrayT(const barrayT<N> &src)
	{
		memcpy(block, src.block, N);
	}
	barrayT(const unsigned char *in, size_t len)
	{
		assign(in, len);
	}

	barrayT& operator=(const barrayT<N> &src)
	{
		memcpy(block, src.block, N);
		return (*this);
	}

	bool assign(const unsigned char *in, size_t len)
	{
		if (!len || len != N)
			return false;
        memcpy(block, in, N);
		return true;
	}

	bool assign(const char *in, size_t len)
	{
		if (!len || len/2 != N)
			return false;
		hex2byte(in, len, block);
		return true;
	}

	bool assign(const wchar_t *in, size_t len)
	{
		if (!len || len/2 != N)
			return false;
		whex2byte(in, len, block);
		return true;
	}

	void to_string(std::string &out) const
	{
		byte2hex(block, N, out);
	}

	void to_string(std::wstring &out) const
	{
		byte2whex(block, N, out);
	}

	void clear(void)
	{
        memset(block, 0, N);
	}

	void random(void)
	{
		CryptoPP::AutoSeededRandomPool rng;
		rng.GenerateBlock(block, N);
	}

	size_t size(void) const
	{
		return (N);
	}

	const unsigned char *data(void) const
	{
		return (block);
	}
};

template<size_t N>
bool operator==(const barrayT<N> &x, const barrayT<N> &y)
{
    return (memcmp(x.block, y.block, N) == 0 ? true : false);
}
template<size_t N>
bool operator!=(const barrayT<N> &x, const barrayT<N> &y)
{
    return !(x == y);
}
template<size_t N>
bool operator<(const barrayT<N> &x, const barrayT<N> &y)
{
    return std::lexicographical_compare(x.data(), x.data()+N, y.data(), y.data()+N);
}
#endif