/* Copyright (C) 2006 o2on project. All rights reserved.
 * http://o2on.net/
 */

/*
 * project		: 
 * filename		: rsa.h
 * description	: 
 *
 */

#pragma once
#include "barray.h"

#define RSA_KEYLENGTH		1024	// 1024 bit
#define RSA_PRIVKEY_SIZE	632		// RSA-1024
#define RSA_PUBKEY_SIZE		160		// RSA-1024
#define SIGNATURE_SIZE		46		// RSA/PKCS1-1.5(SHA-1)

typedef barrayT<RSA_PRIVKEY_SIZE>	privT;
typedef barrayT<RSA_PUBKEY_SIZE>	pubT;
