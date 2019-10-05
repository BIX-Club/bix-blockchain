///////////////////////////////////////////////////////////////////////////////
// bix-blockchain
// Copyright (C) 2019 HUBECN LLC
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version. See LICENSE.txt 
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.


#if _WIN32

// _WIN32
#ifdef _MSC_VER
// Remove annoying warnings
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_DEPRECATE
// No Warning on: unreferenced local variable
#pragma warning (disable: 4101)
// No Warning on: conversion from 'xxx' to 'yyy', possible loss of data
#pragma warning (disable: 4244)
// No Warning on: This function or variable may be unsafe.
#pragma warning (disable: 4996)
#endif

#include <tchar.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include <malloc.h>
#include <new.h>
#include <Winsock2.h>
#include <ws2tcpip.h>
#include <errno.h>
#include <io.h>
#include <fcntl.h>
#define _SC_OPEN_MAX            1024
#define strncasecmp             _strnicmp
#define strcasecmp              _stricmp
#define va_copy(d,s)            ((d) = (s))
#define va_copy_end(a)          {}
#define snprintf                _snprintf

#define isblank(c)              ((c)==' ' || (c) =='\n' || c=='\t')
#define isprint(c)              ((c)>=0x20 && (c)!=0x7f)


#else

// !_WIN32
#include <netdb.h>
#include <unistd.h>
#include <netinet/tcp.h>
#if __GNUC__
// Remove annoying warnings
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wwrite-strings"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

#define closesocket(x)                      close(x)
#define va_copy_end(a)                      va_end(a)

// if _WIN32
#endif


#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <wchar.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "jsvar.h"
#include "sglib.h"


#if JSVAR_USE_REGEXP
#include <regex.h>
#endif

/////////////////////////////////////////////////////////////////////////////////////

// all memory allocation / freeing is going through the following macros
#ifndef JSVAR_ALLOC
#define JSVAR_ALLOC(p,t)            {(p) = (t*) malloc(sizeof(t)); if((p)==NULL) {printf("Out of memory\n"); assert(0); exit(1);}}
#define JSVAR_REALLOC(p,t)          {(p) = (t*) realloc((p), sizeof(t)); if((p)==NULL && (n)!=0) {printf("Out of memory\n"); assert(0); exit(1);}}
#define JSVAR_ALLOCC(p,n,t)         {(p) = (t*) malloc((n)*sizeof(t)); if((p)==NULL && (n)!=0) {printf("Out of memory\n"); assert(0); exit(1);}}
#define JSVAR_REALLOCC(p,n,t)       {(p) = (t*) realloc((p), (n)*sizeof(t)); if((p)==NULL && (n)!=0) {printf("Out of memory\n"); assert(0); exit(1);}}
#define JSVAR_ALLOC_SIZE(p,t,n)     {(p) = (t*) malloc(n); if((p)==NULL && (n)!=0) {printf("Out of memory\n"); assert(0); exit(1);}}
#define JSVAR_FREE(p)               {free(p); }
#endif

// Macro used to compute I/O buffer sizes when buffer needs to be expanded.
// If your memory allocator works better with specific sizes, you can specify it here.
// Next buffer size shall be approximately twice as large as previous one.
#ifndef JSVAR_NEXT_RECOMMENDED_BUFFER_SIZE
#define JSVAR_NEXT_RECOMMENDED_BUFFER_SIZE(size) (size*2+1);
#endif

// Maximum number of jsVar variable spaces.
#ifndef JSVAR_MAX_VARSPACES
#define JSVAR_MAX_VARSPACES         (1<<12)
#endif

// All messages written by jsVar are prefixed with the following string
#ifndef JSVAR_PRINT_PREFIX
#define JSVAR_PRINT_PREFIX()        "jsVar"
#endif

// If you want that undocumented jsVar functions have non static linkage
// define JSVAR_STATIC as empty macro
#ifndef JSVAR_STATIC
#define JSVAR_STATIC                static
#endif

// initial size of dynamic strings, with string growing the size is doubled
#ifndef JSVAR_INITIAL_DYNAMIC_STRING_SIZE
#define JSVAR_INITIAL_DYNAMIC_STRING_SIZE   16
#endif

//////////////////////////////////////////////////////////////////////////////////////


#define JSVAR_PATH_MAX                      256

#define JSVAR_TMP_STRING_SIZE               256
#define JSVAR_LONG_LONG_TMP_STRING_SIZE     65536

#define JSVAR_MIN(x,y)                      ((x)<(y) ? (x) : (y))
#define JSVAR_MAX(x,y)                      ((x)<(y) ? (y) : (x))


#define JsVarInternalCheck(x) {                                             \
        if (! (x)) {                                                    \
            printf("%s: Error: Internal check %s failed at %s:%d\n", JSVAR_PRINT_PREFIX(), #x, __FILE__, __LINE__); \
            fflush(stdout);                                             \
        }                                                               \
    }

#define JSVAR_CALLBACK_CALL(hook, command) {\
        int _i_; \
        for(_i_=(hook).i-1; _i_ >= 0; _i_--) {\
            jsVarCallBackHookFunType callBack = (hook).a[_i_]; \
            if (command) break;\
        }\
    }

//////////////////////////////////////////////////////////////////////////////////////////////////
// forward decls
int baioMsgInProgress(struct baio *bb) ;

//////////////////////////////////////////////////////////////////////////////////////////////////
static char *strDuplicate(char *s) {
    int     n;
    char    *res;
    if (s == NULL) return(NULL);
    n = strlen(s);
    JSVAR_ALLOCC(res, n+1, char);
    strcpy(res, s);
    return(res);
}

int hexDigitCharToInt(int hx) {
    int res;
    if (hx >= '0' && hx <= '9') return(hx - '0');
    if (hx >= 'A' && hx <= 'F') return(hx - 'A' + 10);
    if (hx >= 'a' && hx <= 'f') return(hx - 'a' + 10);
    return(-1);
}

int intDigitToHexChar(int x) {
    if (x >= 0 && x <= 9) return(x + '0');
    if (x >= 10 && x <= 15) return(x + 'A' - 10);
    return(-1);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// indexed set of small integers with fast walking through all elements

JSVAR_STATIC void jsVarIndexSetInit(struct jsVarIndexSet *ee, int maxValue) {
    ee->bitsDim = maxValue;
    ee->aDim = 16;
    JSVAR_ALLOCC(ee->bits, ee->bitsDim, uint8_t);
    JSVAR_ALLOCC(ee->a, ee->aDim, int);
    memset(ee->bits, 0, ee->bitsDim);
    ee->ai = 0;
}

JSVAR_STATIC void jsVarIndexSetClear(struct jsVarIndexSet *ee) {
    int         i, n;
    assert(ee->ai <= ee->aDim);
    n = ee->ai;
    for(i=0; i<n; i++) {
        assert(ee->a[i] < ee->bitsDim);
        ee->bits[ee->a[i]] = 0;
    }
    ee->ai = 0;
}

JSVAR_STATIC void jsVarIndexSetReinit(struct jsVarIndexSet *ee, int maxValue) {
    JSVAR_REALLOCC(ee->bits, maxValue, uint8_t);
    ee->bitsDim = maxValue;
    ee->aDim = 16;
    JSVAR_REALLOCC(ee->a, ee->aDim, int);
    memset(ee->bits, 0, ee->bitsDim);
    ee->ai = 0;
}

JSVAR_STATIC void jsVarIndexSetResize(struct jsVarIndexSet *ee, int maxValue) {
    int     oldBitsDim;
    int     i,j;
    oldBitsDim = ee->bitsDim;
    ee->bitsDim = maxValue;
    if (ee->bitsDim < oldBitsDim) {
        // Also remove all indexes smaller than max
        for(i=j=0; i<ee->ai; i++) {
            if (ee->a[i] < maxValue) ee->a[j++] = ee->a[i];
        }
        ee->ai = j;
    }
    JSVAR_REALLOCC(ee->bits, ee->bitsDim, uint8_t);
    if (ee->bitsDim > oldBitsDim) memset(ee->bits+oldBitsDim, 0, ee->bitsDim-oldBitsDim);
}

JSVAR_STATIC void jsVarIndexSetFree(struct jsVarIndexSet *ee) {
    JSVAR_FREE(ee->bits);
    JSVAR_FREE(ee->a);
    ee->bits = NULL;
    ee->a = NULL;
}

JSVAR_STATIC void jsVarIndexSetAdd(struct jsVarIndexSet *ee, int index) {
    if (ee->ai >= ee->aDim) {
        ee->aDim *= 2;
        JSVAR_REALLOCC(ee->a, ee->aDim, int);
        assert(ee->ai < ee->aDim);
    }
    if (index >= ee->bitsDim) {
        printf("%s: %s:%d: indexSetAd: index %d out of range 0 - %d.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, index, ee->bitsDim);
        return;
    }
    if (ee->bits[index] == 0) {
        // add it only if not yet inside
        ee->bits[index] = 1;
        ee->a[ee->ai] = index;
        ee->ai ++;
    }
}

// This is a very expensive
JSVAR_STATIC void jsVarIndexSetDeleteSymbol(struct jsVarIndexSet *ee, int index) {
    int         i, j;

    if (index >= ee->bitsDim) return;
    if (ee->bits[index] != 0) {
        ee->bits[index] = 0;
        // it is inside. I have to go through the whole array, find and delete it
        for(i=0,j=0; i<ee->ai; i++) {
            if (i != j) ee->a[j] = ee->a[i];
            if (ee->a[i] != index) j++;
        }
        ee->ai --;
        JsVarInternalCheck(j == ee->ai);
    }
}

JSVAR_STATIC int jsVarIndexSetIndexIsInside(struct jsVarIndexSet *ee, int index) {
    if (index >= ee->bitsDim) return(0);
    return(ee->bits[index]);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// base64 (Those two function are public domain software)

static unsigned char encoding_table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
                                'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
                                'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                                'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
                                'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                                'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
                                'w', 'x', 'y', 'z', '0', '1', '2', '3',
                                '4', '5', '6', '7', '8', '9', '+', '/'};
static unsigned char *decoding_table = NULL;
static int mod_table[] = {0, 2, 1};


JSVAR_STATIC void build_decoding_table() {
    int i;
    JSVAR_ALLOCC(decoding_table, 256, unsigned char);
    for (i = 0; i < 64; i++) decoding_table[(unsigned char) encoding_table[i]] = i;
}


JSVAR_STATIC void base64_cleanup() {
    free(decoding_table);
}


JSVAR_STATIC int base64_encode(char *data, int input_length, char *encoded_data, int output_length) {
    int             i, j, olen;
    unsigned char   *d;

    d = (unsigned char *) data;

    olen = 4 * ((input_length + 2) / 3);
    if (olen >= output_length) return(-1);

    for (i = 0, j = 0; i < input_length;) {

        uint32_t octet_a = i < input_length ? d[i++] : 0;
        uint32_t octet_b = i < input_length ? d[i++] : 0;
        uint32_t octet_c = i < input_length ? d[i++] : 0;

        uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

        encoded_data[j++] = encoding_table[(triple >> 3 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 2 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 1 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 0 * 6) & 0x3F];
    }

    for (i = 0; i < mod_table[input_length % 3]; i++)
        encoded_data[olen - 1 - i] = '=';

    encoded_data[olen] = 0;
    return(olen);
}


JSVAR_STATIC int base64_decode(char *data, int input_length, char *decoded_data, int output_length) {
    int             i, j, olen;
    unsigned char   *d;

    d = (unsigned char *) data;
    if (decoding_table == NULL) build_decoding_table();

    if (input_length % 4 != 0) return(-1);

    olen = input_length / 4 * 3;
    if (d[input_length - 1] == '=') olen--;
    if (d[input_length - 2] == '=') olen--;

    if (olen > output_length) return(-1);

    for (i = 0, j = 0; i < input_length;) {

        uint32_t sextet_a = d[i] == '=' ? 0 & i++ : decoding_table[d[i++]];
        uint32_t sextet_b = d[i] == '=' ? 0 & i++ : decoding_table[d[i++]];
        uint32_t sextet_c = d[i] == '=' ? 0 & i++ : decoding_table[d[i++]];
        uint32_t sextet_d = d[i] == '=' ? 0 & i++ : decoding_table[d[i++]];

        uint32_t triple = (sextet_a << 3 * 6)
        + (sextet_b << 2 * 6)
        + (sextet_c << 1 * 6)
        + (sextet_d << 0 * 6);

        if (j < olen) decoded_data[j++] = (triple >> 2 * 8) & 0xFF;
        if (j < olen) decoded_data[j++] = (triple >> 1 * 8) & 0xFF;
        if (j < olen) decoded_data[j++] = (triple >> 0 * 8) & 0xFF;
    }
    return(olen);
}


//////////////////////////////////////////////////////////////////////////////////////////////////
// simple resizeable strings
///////////////////////////////////

static void jsVarDstrInitContent(struct jsVarDynamicString *s) {
    if (s->allocationMethod == MSAM_PURE_MALLOC) {
        s->s = (char *) malloc(JSVAR_INITIAL_DYNAMIC_STRING_SIZE * sizeof(char));
    } else {
        JSVAR_ALLOCC(s->s, JSVAR_INITIAL_DYNAMIC_STRING_SIZE, char);
    }
    s->allocatedSize = JSVAR_INITIAL_DYNAMIC_STRING_SIZE;
    s->size = 0;
    s->s[0] = 0;
}

JSVAR_STATIC struct jsVarDynamicString *jsVarDstrCreate() {
    struct jsVarDynamicString *res;
    JSVAR_ALLOCC(res, 1, struct jsVarDynamicString);
    res->allocationMethod = MSAM_DEFAULT;
    jsVarDstrInitContent(res);
    return(res);
}

JSVAR_STATIC struct jsVarDynamicString *jsVarDstrCreatePureMalloc() {
    struct jsVarDynamicString *res;
    res = (struct jsVarDynamicString *) malloc(sizeof(*res));
    res->allocationMethod = MSAM_PURE_MALLOC;
    jsVarDstrInitContent(res);
    return(res);
}

JSVAR_STATIC void jsVarDstrFree(struct jsVarDynamicString **p) {
	if (p == NULL || *p == NULL) return;
    if ((*p)->allocationMethod == MSAM_PURE_MALLOC) {
        free((*p)->s);
        free(*p);
    } else {
        assert((*p)->allocationMethod == MSAM_DEFAULT);
        JSVAR_FREE((*p)->s);
        JSVAR_FREE(*p);
    }
    *p = NULL;
}

JSVAR_STATIC char *jsVarDstrGetStringAndReinit(struct jsVarDynamicString *s) {
    char *res;
    res = s->s;
    jsVarDstrInitContent(s);
    return(res);
}

static void jsVarDstrInternalReallocateToSize(struct jsVarDynamicString *s) {
    if (s->allocationMethod == MSAM_PURE_MALLOC) {
        s->s = (char *) realloc(s->s, s->allocatedSize * sizeof(char));
        if (s->s == NULL) {
            printf("Out of memory\n");
            assert(0);
            exit(1);
        }
    } else {
        assert(s->allocationMethod == MSAM_DEFAULT);
        JSVAR_REALLOCC(s->s, s->allocatedSize, char);
    }
}

void jsVarDstrExpandToSize(struct jsVarDynamicString *s, int size) {
    if (size < s->allocatedSize) return;
    while (size >= s->allocatedSize) {
        // adjust allocated size
        s->allocatedSize = (s->allocatedSize * 4 + 1);
    }
    jsVarDstrInternalReallocateToSize(s);
}

JSVAR_STATIC struct jsVarDynamicString *jsVarDstrTruncateToSize(struct jsVarDynamicString *s, int size) {
    s->size = size;
	s->s[s->size] = 0;
    s->allocatedSize = size + 1;
    jsVarDstrInternalReallocateToSize(s);
    return(s);
}

JSVAR_STATIC struct jsVarDynamicString *jsVarDstrTruncate(struct jsVarDynamicString *s) {
    jsVarDstrTruncateToSize(s, s->size);
    return(s);
}

JSVAR_STATIC int jsVarDstrAddCharacter(struct jsVarDynamicString *s, int c) {
    jsVarDstrExpandToSize(s, s->size+1);
    s->s[(s->size)++] = c;
    s->s[s->size] = 0;
    return(1);
}

JSVAR_STATIC int jsVarDstrDeleteLastChar(struct jsVarDynamicString *s) {
    if (s->size == 0) return(0);
    s->size --;
    s->s[s->size] = 0;
    return(-1);
}

JSVAR_STATIC int jsVarDstrAppendData(struct jsVarDynamicString *s, char *appendix, int len) {
	if (len < 0) return(len);
    jsVarDstrExpandToSize(s, s->size+len+1);
    memcpy(s->s+s->size, appendix, len);
    s->size += len;
    s->s[s->size] = 0;
    return(len);
}

JSVAR_STATIC int jsVarDstrAppend(struct jsVarDynamicString *s, char *appendix) {
    int len;
    len = strlen(appendix);
    jsVarDstrAppendData(s, appendix, len);
    return(len);
}

JSVAR_STATIC int jsVarDstrAppendVPrintf(struct jsVarDynamicString *s, char *fmt, va_list arg_ptr) {
    int         n, dsize;
    char        *d;
    va_list     arg_ptr_copy;

    n = 0;
    for(;;) {
        d = s->s+s->size;
        dsize = s->allocatedSize - s->size;
        if (dsize > 0) {
            va_copy(arg_ptr_copy, arg_ptr);
            n = vsnprintf(d, dsize, fmt, arg_ptr_copy);
#if _WIN32
            // TODO: Review this stuff
            while (n < 0) {
                jsVarDstrExpandToSize(s, s->allocatedSize * 2 + 1024);
                d = s->s+s->size;
                dsize = s->allocatedSize - s->size;
                n = vsnprintf(d, dsize, fmt, arg_ptr_copy);
            }
#endif
            JsVarInternalCheck(n >= 0);
            va_copy_end(arg_ptr_copy);
            if (n < dsize) break;                   // success
        }
        jsVarDstrExpandToSize(s, s->size+n+1);
    }
    s->size += n;
    return(n);
}

JSVAR_STATIC int jsVarDstrAppendPrintf(struct jsVarDynamicString *s, char *fmt, ...) {
    int             res;
    va_list         arg_ptr;

    va_start(arg_ptr, fmt);
    res = jsVarDstrAppendVPrintf(s, fmt, arg_ptr);
    va_end(arg_ptr);
    return(res);
}

JSVAR_STATIC int jsVarDstrAppendEscapedStringUsableInJavascriptEval(struct jsVarDynamicString *ss, char *s, int slen) {
	int							i, c;
	
	for(i=0; i<slen; i++) {
		c = ((unsigned char*)s)[i];
		jsVarDstrAddCharacter(ss, '\\');
		jsVarDstrAddCharacter(ss, 'x');
		jsVarDstrAddCharacter(ss, intDigitToHexChar(c/16));
		jsVarDstrAddCharacter(ss, intDigitToHexChar(c%16));
	}
	return(slen*4);
}

JSVAR_STATIC int jsVarDstrAppendBase64EncodedData(struct jsVarDynamicString *ss, char *s, int slen) {
	int							i, c, r;

	if (slen < 0) return(-1);
	if (slen == 0) return(0);

	jsVarDstrExpandToSize(ss, ss->size + slen*4/3 + 2);
	r = base64_encode(s, slen, ss->s+ss->size, ss->allocatedSize-ss->size);
	if (r < 0) {
		printf("%s: %s:%d: Can't encode string\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
		return(-1);
	}
	ss->size += r;
	assert(ss->size < ss->allocatedSize);
	ss->s[ss->size] = 0;
	return(r);
}

JSVAR_STATIC int jsVarDstrAppendBase64DecodedData(struct jsVarDynamicString *ss, char *s, int slen) {
	int							i, c, r;

	if (slen < 0) return(-1);
	if (slen == 0) return(0);

	jsVarDstrExpandToSize(ss, ss->size + slen / 4 * 3);
	r = base64_decode(s, slen, ss->s+ss->size, ss->allocatedSize-ss->size);
	if (r < 0) {
		printf("%s: %s:%d: Can't decode string\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
		return(-1);
	}
	ss->size += r;
	assert(ss->size < ss->allocatedSize);
	ss->s[ss->size] = 0;
	return(r);
}

JSVAR_STATIC void jsVarDstrClear(struct jsVarDynamicString *s) {
    s->size = 0;
    (s->s)[s->size] = 0;
}

// flags: 0 == ALL, otherwise single occurence
JSVAR_STATIC int jsVarDstrReplace(struct jsVarDynamicString *s, char *str, char *byStr, int allOccurencesFlag) {
    struct jsVarDynamicString   *d;
    int                         i, slen, stlen;
    char                        *ss, *cc;

    stlen = strlen(str);
    slen = s->size;
    ss = jsVarDstrGetStringAndReinit(s);
    for(i=0; i<slen-stlen+1; i++) {
        if (strncmp(ss+i, str, stlen) == 0) {
            jsVarDstrAppend(s, byStr);
            i += stlen - 1;
            if (allOccurencesFlag == 0) break;
        } else {
            jsVarDstrAddCharacter(s, ss[i]);
        }
    }
    if (i<slen) {
        jsVarDstrAppendData(s, ss+i+1, slen-i-1);
    }
    if (s->allocationMethod == MSAM_PURE_MALLOC) {
        free(ss);
    } else {
        JSVAR_FREE(ss);
    }
    return(s->size);
}

JSVAR_STATIC int jsVarDstrAppendFile(struct jsVarDynamicString *res, FILE *ff) {
	int 	n, originalSize;

	originalSize = res->size;
	n = 1;
	while (n > 0) {
		jsVarDstrExpandToSize(res, res->size + 1024);
		n = fread(res->s+res->size, 1, res->allocatedSize-res->size-1, ff);
		res->size += n;
	}
	res->s[res->size] = 0;
	return(res->size - originalSize);
}

JSVAR_STATIC struct jsVarDynamicString *jsVarDstrCreateByVPrintf(char *fmt, va_list arg_ptr) {
    struct jsVarDynamicString   *res;
    va_list                     arg_ptr_copy;

    va_copy(arg_ptr_copy, arg_ptr);
    res = jsVarDstrCreate();
    jsVarDstrAppendVPrintf(res, fmt, arg_ptr_copy);
    res = jsVarDstrTruncate(res);
    va_copy_end(arg_ptr_copy);
    return(res);
}

JSVAR_STATIC struct jsVarDynamicString *jsVarDstrCreateByPrintf(char *fmt, ...) {
    struct jsVarDynamicString   *res;
    va_list                     arg_ptr;

    va_start(arg_ptr, fmt);
    res = jsVarDstrCreateByVPrintf(fmt, arg_ptr);
    va_end(arg_ptr);
    return(res);
}

JSVAR_STATIC struct jsVarDynamicString *jsVarDstrCreateFromCharPtr(char *s, int slen) {
    struct jsVarDynamicString   *res;

	if (slen < 0) return(NULL);
    res = jsVarDstrCreate();
    jsVarDstrAppendData(res, s, slen);
    return(res);
}

JSVAR_STATIC struct jsVarDynamicString *jsVarDstrCreateCopy(struct jsVarDynamicString *ss) {
    struct jsVarDynamicString   *res;
    res = jsVarDstrCreate();
    jsVarDstrAppendData(res, ss->s, ss->size);
    return(res);
}

struct jsVarDynamicString *jsVarDstrCreateByVFileLoad(int useCppFlag, char *fileNameFmt, va_list arg_ptr) {
	FILE 						*ff;
	int							n, r;
	char						fname[JSVAR_PATH_MAX];
	char						ccc[JSVAR_PATH_MAX+32];
	struct jsVarDynamicString 	*res;

	r = vsnprintf(fname, sizeof(fname), fileNameFmt, arg_ptr);
	if (r < 0 || r >= sizeof(fname)) {
		strcpy(fname+sizeof(fname)-5, "...");
		printf("%s: %s:%d: Error: File name too long %s\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, fname);
		return(NULL);
	}

	if (useCppFlag) {
#if _WIN32
		printf("%s: %s:%d: Error: file preprocessor is not available for Windows: %s\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, fname);
		return(NULL);
#else
		sprintf(ccc, "cpp -P %s", fname);
		ff = popen(ccc, "r");
#endif
	} else {
		ff = fopen(fname, "r");
	}
	if (ff == NULL) return(NULL);
	res = jsVarDstrCreate();
	jsVarDstrAppendFile(res, ff);
	fclose(ff);
	return(res);
}

JSVAR_STATIC struct jsVarDynamicString *jsVarDstrCreateByFileLoad(int useCppFlag, char *fileNameFmt, ...) {
	va_list 					arg_ptr;
	struct jsVarDynamicString 	*res;

	va_start(arg_ptr, fileNameFmt);
	res = jsVarDstrCreateByVFileLoad(useCppFlag, fileNameFmt, arg_ptr);
	va_end(arg_ptr);
	return(res);
}


//////////////////////////////////////////////////////////////////////////////////////////////////
// callbacks

void jsVarCallBackClearHook(struct jsVarCallBackHook *h) {
    h->i = 0;
}

JSVAR_STATIC void jsVarCallBackFreeHook(struct jsVarCallBackHook *h) {
    h->i = 0;
    JSVAR_FREE(h->a);
    h->a = NULL;
    h->dim = 0;
}

int jsVarCallBackAddToHook(struct jsVarCallBackHook *h, void *ptr) {
    int i;

    i = h->i;
    if (i >= h->dim) {
        h->dim = h->dim * 2 + 1;
        JSVAR_REALLOCC(h->a, h->dim, jsVarCallBackHookFunType);
    }
    h->a[i] = (jsVarCallBackHookFunType) ptr;
    h->i ++;
    return(i);
}

static void jsVarCallBackRemoveIndexFromHook(struct jsVarCallBackHook *h, int i) {
    if (i < 0 || i >= h->i) return;
    for(i=i+1; i < h->i; i++) h->a[i-1] = h->a[i];
    h->i --;
}

void jsVarCallBackRemoveFromHook(struct jsVarCallBackHook *h, void *ptr) {
    int i;

    for(i=0; i<h->i && h->a[i] != ptr; i++) ;
    if (i < h->i) {
        jsVarCallBackRemoveIndexFromHook(h, i);
    }
}

// if src is NULL, allocate new copy of src itself
JSVAR_STATIC void jsVarCallBackCloneHook(struct jsVarCallBackHook *dst, struct jsVarCallBackHook *src) {
    jsVarCallBackHookFunType *a;

    if (src != NULL) *dst = *src;
    a = dst->a;
    JSVAR_ALLOCC(dst->a, dst->dim, jsVarCallBackHookFunType);
    memcpy(dst->a, a, dst->dim*sizeof(jsVarCallBackHookFunType));
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// popens

#if ! _WIN32
static void closeAllFdsFrom(int fd0) {
    int maxd, i;

    maxd = sysconf(_SC_OPEN_MAX);
    if (maxd > 1024) maxd = 1024;
    for(i=fd0; i<maxd; i++) close(i);
}

static int createPipesForPopens(int *in_fd, int *out_fd, int *pin, int *pout) {

    if (out_fd != NULL) {
        if (pipe(pin) != 0) {
            printf("%s: %s:%d: Can't create output pipe\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
            return(-1);
        }
        *out_fd = pin[1];
        // printf("pipe pin: %p %p: %d %d\n", pin, pin+1, pin[0], pin[1]);
    }
    if (in_fd != NULL) {
        if (pipe(pout) != 0) {
            printf("%s: %s:%d: Can't create input pipe\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
            if (out_fd != NULL) {
                close(pin[0]);
                close(pin[1]);
            }
            return(-1);
        }
        *in_fd = pout[0];
        // printf("pipe pout: %p %p: %d %d\n", pout, pout+1, pout[0], pout[1]);
    }
    return(0);
}

static void closePopenPipes(int *in_fd, int *out_fd, int pin[2], int pout[2]) {
    if (out_fd != NULL) {
        close(pin[0]);
        close(pin[1]);
    }
    if (in_fd != NULL) {
        close(pout[0]);
        close(pout[1]);
    }
}

JSVAR_STATIC pid_t popen2(char *command, int *in_fd, int *out_fd, int useBashFlag) {
    int                 pin[2], pout[2];
    pid_t               pid;
    int                 md;
    char                ccc[strlen(command)+10];

    if (createPipesForPopens(in_fd, out_fd, pin, pout) == -1) return(-1);

    pid = fork();

    if (pid < 0) {
        printf("%s:%s:%d: fork failed in popen2: %s\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, strerror(errno));
        closePopenPipes(in_fd, out_fd, pin, pout);
        return pid;
    }

    if (pid == 0) {

        if (out_fd != NULL) {
            close(pin[1]);
            dup2(pin[0], 0);
            close(pin[0]);
        } else {
            // do not inherit stdin, if no pipe is defined, close it.
            close(0);
        }

        // we do not want to loose completely stderr of the task. redirect it to a common file
        md = open("currentsubmsgs.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);
        dup2(md, 2);
        close(md);

        if (in_fd != NULL) {
            close(pout[0]);
            dup2(pout[1], 1);
            close(pin[1]);
        } else {
            // if there is no pipe for stdout, join stderr.
            dup2(2, 1);
        }

        // close all remaining fds. This is important because otherwise files and pipes may remain open
        // until the new process terminates.
        closeAllFdsFrom(3);

        // Exec is better, because otherwise this process may be unkillable (we would kill the shell not the process itself).
        if (useBashFlag) {
            execlp("bash", "bash", "-c", command, NULL);
        } else {
            sprintf(ccc, "exec %s", command);
            execlp("sh", "sh", "-c", ccc, NULL);
        }
        // execlp(command, command, NULL);
        printf("%s:%s:%d: Exec failed in popen2: %s\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, strerror(errno));
        exit(1);
    }

    if (in_fd != NULL) {
        close(pout[1]);
    }
    if (out_fd != NULL) {
        close(pin[0]);
    }

    return pid;
}

JSVAR_STATIC int popen2File(char *path, char *ioDirection) {
    char    command[strlen(path)+30];
    int     fd;
    int     r;

    if (strcmp(ioDirection, "r") == 0) {
        sprintf(command, "cat %s", path);
        r = popen2(command, &fd, NULL, 1);
        if (r < 0) return(-1);
    } else if (strcmp(ioDirection, "w") == 0) {
        sprintf(command, "cat > %s", path);
        r = popen2(command, NULL, &fd, 1);
        if (r < 0) return(-1);
    } else {
        printf("%s:%s:%d: Invalid ioDirection. Only r or w are acceopted values.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
        return(-1);
    }
    return(fd);
}
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// assynchronous socket connection functions

JSVAR_STATIC char *sprintfIpAddress_st(long unsigned ip) {
    static char res[JSVAR_TMP_STRING_SIZE];
    sprintf(res, "%ld.%ld.%ld.%ld", (ip >> 0) & 0xff, (ip >> 8) & 0xff, (ip >> 16) & 0xff, (ip >> 24) & 0xff);
    return(res);
}

JSVAR_STATIC int setFileNonBlocking(int fd) {
#if _WIN32
    return(-1);
#else
    unsigned    flg;
    // Set non-blocking 
    if((flg = fcntl(fd, F_GETFL, NULL)) < 0) { 
        printf("%s: %s:%d: Error on socket %d fcntl(..., F_GETFL) (%s)\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, fd, strerror(errno)); 
        return(-1);
    } 
    flg |= O_NONBLOCK; 
    if(fcntl(fd, F_SETFL, flg) < 0) { 
        printf("%s: %s:%d: Error on socket %d fcntl(..., F_SETFL) (%s)\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, fd, strerror(errno)); 
        return(-1);
    }
    return(0);
#endif
}

JSVAR_STATIC int setSocketNonBlocking(int fd) {
#if _WIN32
    int             r;
    unsigned long   mode;
    mode = 1;
    r = ioctlsocket(fd, FIONBIO, &mode);
    if (r) return(-1);
    return(0);
#else
    return(setFileNonBlocking(fd));
#endif
}

JSVAR_STATIC int setFileBlocking(int fd) {
#if _WIN32
    return(-1);
#else
    unsigned    flg;
    // Set blocking 
    if((flg = fcntl(fd, F_GETFL, NULL)) < 0) { 
        printf("%s: %s:%d: Error on socket %d fcntl(..., F_GETFL) (%s)\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, fd, strerror(errno)); 
        return(-1);
    } 
    flg &= ~(O_NONBLOCK); 
    if(fcntl(fd, F_SETFL, flg) < 0) { 
        printf("%s: %s:%d: Error on socket %d fcntl(..., F_SETFL) (%s)\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, fd, strerror(errno)); 
        return(-1);
    }
    return(0);
#endif
}

JSVAR_STATIC int setSocketBlocking(int fd) {
#if _WIN32
    int             r;
    unsigned long   mode;
    mode = 0; 
    r = ioctlsocket(fd, FIONBIO, &mode);
    if (r) return(-1);
    return(0);
#else
    return(setFileBlocking(fd));
#endif
}

JSVAR_STATIC void tuneSocketforLatency(int fd) {
#if _WIN32
  DWORD one;
  one = 1;
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof(one));
#else
  int one;
  one = 1;
  setsockopt(fd, SOL_TCP, TCP_NODELAY, &one, sizeof(one));
  setsockopt(fd, SOL_TCP, TCP_QUICKACK, &one, sizeof(one));
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
// Baio stands for Basic Assynchronous I/O library

/////////////////////////////////////////////////////////////////////////////////////

#define BAIO_STATUSES_CLEARED_PER_TICK              0x0ffff0

#define BAIO_BLOCKED_FOR_READ_IN_SPECIAL_STATUS_MASK (      \
        0                                       \
        | BAIO_BLOCKED_FOR_READ_IN_TCPIP_LISTEN \
        | BAIO_BLOCKED_FOR_READ_IN_SSL_CONNECT  \
        | BAIO_BLOCKED_FOR_READ_IN_SSL_ACCEPT   \
        )
#define BAIO_BLOCKED_FOR_READ_STATUS_MASK (     \
        0                                       \
        | BAIO_BLOCKED_FOR_READ_IN_READ         \
        | BAIO_BLOCKED_FOR_READ_IN_SSL_READ     \
        | BAIO_BLOCKED_FOR_READ_IN_SSL_WRITE    \
        )
#define BAIO_BLOCKED_FOR_WRITE_IN_SPECIAL_STATUS_MASK (     \
        0                                           \
        | BAIO_BLOCKED_FOR_WRITE_IN_TCPIP_CONNECT   \
        | BAIO_BLOCKED_FOR_WRITE_IN_SSL_CONNECT     \
        | BAIO_BLOCKED_FOR_WRITE_IN_SSL_ACCEPT      \
        )
#define BAIO_BLOCKED_FOR_WRITE_STATUS_MASK (        \
        0                                           \
        | BAIO_BLOCKED_FOR_WRITE_IN_WRITE           \
        | BAIO_BLOCKED_FOR_WRITE_IN_SSL_WRITE       \
        | BAIO_BLOCKED_FOR_WRITE_IN_SSL_READ        \
        )

#define BAIO_BLOCKED_FOR_READ_STATUSES              (BAIO_BLOCKED_FOR_READ_IN_SPECIAL_STATUS_MASK | BAIO_BLOCKED_FOR_READ_STATUS_MASK)
#define BAIO_BLOCKED_FOR_WRITE_STATUSES             (BAIO_BLOCKED_FOR_WRITE_IN_SPECIAL_STATUS_MASK | BAIO_BLOCKED_FOR_WRITE_STATUS_MASK)

#define BAIO_MSGS_BUFFER_RESIZE_INCREASE_OFFSET     4
#define BAIO_MSGS_PREVIOUS_INDEX(b, i)              ((i)==0?b->msize-1:(i)-1)
#define BAIO_MSGS_NEXT_INDEX(b, i)                  ((i)+1==b->msize?0:(i)+1)

#define BAIO_STATIC_STRINGS_RING_SIZE               64

#define BAIO_SEND_MAX_MESSAGE_LENGTH                1400    /* max length which can be sent within one send */
#define BAIO_SOCKET_FILE_BUFFER_SIZE                1024


#define BAIO_INIFINITY_INDEX                        (-1)

enum baioTypes {
    BAIO_TYPE_FD,                   // basic baio, fd is supplied from outside
    BAIO_TYPE_FD_SOCKET,            // basic baio, socket is supplied from outside
    BAIO_TYPE_FILE,                 // file i/o
    BAIO_TYPE_PIPED_FILE,           // piped file i/o
    BAIO_TYPE_PIPED_COMMAND,        // piped subprocess
    BAIO_TYPE_TCPIP_CLIENT,         // tcp ip client
    BAIO_TYPE_TCPIP_SERVER,         // tcp ip server
    BAIO_TYPE_TCPIP_SCLIENT,        // a client which has connected to tcp ip server
};

#if !( BAIO_USE_OPEN_SSL || WSAIO_USE_OPEN_SSL || JSVAR_USE_OPEN_SSL)

// Not using OpenSSL, redefined functions and data types provided by OpenSsl to dummy 
// or alternative implementations
#define SSL                                 void
#define SSL_CTX                             void
#define X509_STORE_CTX                      void
#define SSL_free(x)                         {}
#define SSL_load_error_strings()            {}
#define SSL_CTX_new(x)                      (NULL)
#define SSL_CTX_free(x)                     {}
#define SSL_CTX_use_certificate_file(...)   (-1)
#define SSL_CTX_use_PrivateKey_file(...)    (-1)
#define SSL_set_tlsext_host_name(...)       (-1)
#undef SSL_CTX_set_mode
#define SSL_CTX_set_mode(...)               {}
#define SSL_new(x)                          (NULL)
#define SSL_set_fd(...)                     (0)
#define SSL_get_error(...)                  (-1)
#define SSL_connect(...)                    (-1)
#define SSL_read(...)                       (-1)
#define SSL_write(...)                      (-1)
#define SSL_pending(...)                    (0)
#define SSL_library_init()                  {}
#define SSL_accept(...)                     (-1)
#define ERR_print_errors_fp(x)              {}
#define ERR_clear_error()                   {}
#undef SSL_ERROR_WANT_READ
#define SSL_ERROR_WANT_READ                 1
#undef SSL_ERROR_WANT_WRITE
#define SSL_ERROR_WANT_WRITE                2

// endif to openSsl
#endif


static SSL_CTX  *baioSslContext;
// ! If you use OpenSsl you have to copy server key and certificate to the following files !
// ! or change following macros to point to your key and certificate files.                !
static char     *baioSslRsaServerKeyFile = "server.key";
static char     *baioSslRsaServerCertFile = "server.crt";
//static char       *baioSslTrustedCertFile = "trusted.pem";


static char baioStaticStringsRing[BAIO_STATIC_STRINGS_RING_SIZE][JSVAR_TMP_STRING_SIZE];
static int  baioStaticStringsRingIndex = 0;

struct baio *baioTab[BAIO_MAX_CONNECTIONS];
int         baioTabMax;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

char *baioStaticRingGetTemporaryStringPtr() {
    char *res;
    res = baioStaticStringsRing[baioStaticStringsRingIndex];
    baioStaticStringsRingIndex  = (baioStaticStringsRingIndex+1) % BAIO_STATIC_STRINGS_RING_SIZE;
    return(res);
}

void baioCharBufferDump(char *prefix, char *s, int n) {
    int i;
    printf("%s", prefix);
    for(i=0; i<n; i++) printf(" %02x", ((unsigned char*)s)[i]);
    printf("\n");
    fflush(stdout);
}


void baioWriteBufferDump(struct baioWriteBuffer *b) {
#define BAIO_DUMP_LINE_SIZE 100
    int                     si, i;
    struct baioMsg          *m;
    char                    bb[BAIO_DUMP_LINE_SIZE+10];

    printf("baioWriteBufferDump Start:\n");
    printf("msgs %d-%d of %d:", b->mi, b->mj, b->msize);
    for(i=b->mi; i!=b->mj; i=BAIO_MSGS_NEXT_INDEX(b,i)) {
        m = &b->m[i];
        printf("[%d,%d] ", m->startIndex, m->endIndex);
    }
    printf("\n");
    printf("buffer %d-%d-%d-%d of %d: \n%d: ", b->i, b->ij, b->j, b->jj, b->size, b->i);
    if (b->size != 0) {
        assert(b->i >= 0 && b->i <= b->size);
        assert(b->ij >= 0 && b->ij <= b->size);
        assert(b->j >= 0 && b->j <= b->size);
        assert(b->jj >= 0 && b->jj <= b->size);
    }

    for(i=si=b->i; i!=b->j; i=(i+1)%b->size) {
        if (i%BAIO_DUMP_LINE_SIZE == 0) {bb[BAIO_DUMP_LINE_SIZE]=0; printf("%d: %s\n", si, bb); si=i;}
        if (isprint((unsigned char)b->b[i])) bb[i%BAIO_DUMP_LINE_SIZE] = b->b[i];
        else bb[i%BAIO_DUMP_LINE_SIZE]='.';
    }
    bb[i%BAIO_DUMP_LINE_SIZE]=0; 
    printf("%d: %s\n", si, bb);
    printf("\nbaioWriteBufferDump End.\n");
#undef BAIO_DUMP_LINE_SIZE
}

static int baioFdIsSocket(struct baio *bb) {
    if (bb == NULL) return(0);
    switch (bb->baioType) {
    case BAIO_TYPE_FD:
    case BAIO_TYPE_FILE:
    case BAIO_TYPE_PIPED_FILE:
    case BAIO_TYPE_PIPED_COMMAND:
        return(0);
    case BAIO_TYPE_FD_SOCKET:
    case BAIO_TYPE_TCPIP_CLIENT:
    case BAIO_TYPE_TCPIP_SERVER:
    case BAIO_TYPE_TCPIP_SCLIENT:
        return(1);
    default:
        printf("%s: %s:%d: Unknown baio type %d\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, bb->baioType);
        return(0);
    }
}

/////////

static void baioWriteMsgsInit(struct baioWriteBuffer *b) {
    b->mi = b->mj = 0;
}

static struct baioMsg *wsaioNextMsg(struct baioWriteBuffer *b) {
    if (b->mi == b->mj) return(NULL);
    return(&b->m[b->mi]);
}

static void baioMsgMaybeActivateNextMsg(struct baio *bb) {
    struct baioMsg          *m;
    struct baioWriteBuffer  *b;

    if ((bb->status & BAIO_STATUS_ACTIVE) == 0) return;

    b = &bb->writeBuffer;
    // skip a msg if any
    m = wsaioNextMsg(b);
    if (m == NULL) {
        // no message, check if shall wrap to the beginning of circular buffer
        if (b->i == b->ij && b->j < b->i) {
            b->i = 0;
            b->ij = b->j;
        }
    } else if (m->endIndex != BAIO_INIFINITY_INDEX) {
        // is the next message ready for uotput?
        // printf("checking for msg %d %d  <-> %d %d\n", b->i, b->ij, m->startIndex, m->endIndex); fflush(stdout);
        if (b->ij == m->startIndex) {
            b->ij = m->endIndex;
            b->mi = BAIO_MSGS_NEXT_INDEX(b, b->mi);
        } else if (b->i == b->ij) {
            b->i = m->startIndex;
            b->ij = m->endIndex;
            b->mi = BAIO_MSGS_NEXT_INDEX(b, b->mi);
        }
    } else {
        // we have only one message which is being created, if otherwise buffer is empty, skip the gap
        if (b->i == b->ij) {
            b->i = b->ij = m->startIndex;
        }
    }
}

void baioMsgsResizeToHoldSize(struct baio *bb, int newSize) {
    struct baioWriteBuffer  *b;
    int                     shift, usedlen;

    b = &bb->writeBuffer;
    if (b->msize == 0) {
        b->msize = newSize;
        if (jsVarDebugLevel > 20) printf("%s: %s:%d: Allocating msgs of write buffer of %p to %d\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, b, b->msize);
        JSVAR_REALLOCC(b->m, b->msize, struct baioMsg);
        b->mi = b->mj = 0;
    } else if (b->msize < newSize) {
        usedlen = b->msize - b->mi;
        shift = newSize - b->msize;
        b->msize += shift;
        if (jsVarDebugLevel > 20) printf("%s: %s:%d: Resizing msgs of write buffer of %p to %d\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, b, b->msize);
        JSVAR_REALLOCC(b->m, b->msize, struct baioMsg);
        memmove(b->m+b->mi+shift, b->m+b->mi, usedlen*sizeof(struct baioMsg));
        if (b->mj >= b->mi) b->mj += shift;
        b->mi += shift;
    }   
}

struct baioMsg *baioMsgPut(struct baio *bb, int startIndex, int endIndex) {
    int                     mjplus, mjminus;
    struct baioMsg          *m;
    struct baioWriteBuffer  *b;

    if ((bb->status & BAIO_STATUS_ACTIVE) == 0) return(NULL);

    // this is very important, otherwise we may be blocked
    // do not allow zero sized messages
    if (startIndex == endIndex && endIndex != BAIO_INIFINITY_INDEX) return(NULL);

    b = &bb->writeBuffer;

    if (b->mi == b->mj) baioWriteMsgsInit(b);

    // check that previous message was finished
    if (baioMsgInProgress(bb)) {
        printf("%s:%s:%d: Error: Putting a message while previous one is not finished!\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
    }

#if ! BAIO_EACH_WRITTEN_MSG_HAS_RECORD
    if (b->mi == b->mj) {
        // if there are no messages, just fiddle with i,j indexes
        // no message, maybe wrap to the beginning of circular buffer
        if (b->ij == startIndex && endIndex != BAIO_INIFINITY_INDEX) {
            if (startIndex > endIndex) {
                b->ij = b->jj;
            } else {
                b->ij = endIndex;
            }
            return(NULL);
        }
    }
#endif

    //  if there is no pending msg, reset msg "list"
    if (b->msize == 0) {
        baioMsgsResizeToHoldSize(bb, BAIO_MSGS_BUFFER_RESIZE_INCREASE_OFFSET);
    } else {
        mjplus = BAIO_MSGS_NEXT_INDEX(b,  b->mj);
        if (mjplus == b->mi) {
            // no (more) space in "msg" list resize
            baioMsgsResizeToHoldSize(bb, b->msize*2 + BAIO_MSGS_BUFFER_RESIZE_INCREASE_OFFSET);
        }
    }
    mjplus = BAIO_MSGS_NEXT_INDEX(b,  b->mj);

#if ! BAIO_EACH_WRITTEN_MSG_HAS_RECORD
    // if this is direct continuation of the previous msg, just join them
    // we can not do it if endIndex == BAIO_INIFINITY_INDEX, because such a message is in construction
    // and its startIndex can move
    if (b->mi != b->mj && endIndex != BAIO_INIFINITY_INDEX) {
        mjminus = BAIO_MSGS_PREVIOUS_INDEX(b, b->mj);
        m = &b->m[mjminus];
        if (m->endIndex == startIndex) {
            m->endIndex = endIndex;
            return(m);
        }
    }
#endif
    // add new message
    m = &b->m[b->mj];
    m->startIndex = startIndex;
    m->endIndex = endIndex;
    b->mj = mjplus;

#if BAIO_EACH_WRITTEN_MSG_HAS_RECORD
    // if each message has record, maybe we have to activate the one we have just added
    baioMsgMaybeActivateNextMsg(bb);
#endif
    return(m);
}

void baioMsgStartNewMessage(struct baio *bb) {
    baioMsgPut(bb, bb->writeBuffer.j, BAIO_INIFINITY_INDEX);
}

int baioMsgLastStartIndex(struct baio *bb) {
    struct baioWriteBuffer  *b;
    struct baioMsg          *m;

    b = &bb->writeBuffer;
    if (b->mi == b->mj) return(-1);
    m = &b->m[BAIO_MSGS_PREVIOUS_INDEX(b, b->mj)];
    return(m->startIndex);
}

int baioMsgInProgress(struct baio *bb) {
    struct baioWriteBuffer  *b;
    struct baioMsg          *m;

    b = &bb->writeBuffer;
    if (b->mi == b->mj) return(0);
    m = &b->m[BAIO_MSGS_PREVIOUS_INDEX(b, b->mj)];
    if (m->endIndex != BAIO_INIFINITY_INDEX) return(0);
    return(1);
}

void baioMsgRemoveLastMsg(struct baio *bb) {
    struct baioWriteBuffer  *b;
    struct baioMsg          *m, *mm;

    b = &bb->writeBuffer;
    if (b->mi == b->mj) return;
    b->mj = BAIO_MSGS_PREVIOUS_INDEX(b, b->mj);
    m = &b->m[b->mj];
    if (m->endIndex == BAIO_INIFINITY_INDEX || m->endIndex == b->j) {
        if (b->mi == b->mj) {
            b->j = b->ij;
        } else {
            mm = &b->m[BAIO_MSGS_PREVIOUS_INDEX(b, b->mj)];
            b->j = mm->endIndex;
            if (b->i < b->j && b->j < b->ij) {
                printf("%s: %s:%d: Internall error: baioMsgRemoveLastMsg: message was partially send out, can not be removed\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
            }
        }
    }
}

int baioMsgResetStartIndexForNewMsgSize(struct baio *bb, int newSize) {
    struct baioWriteBuffer  *b;
    struct baioMsg          *m;

    if ((bb->status & BAIO_STATUS_ACTIVE) == 0) return(-1);

    b = &bb->writeBuffer;
    if (newSize == 0) {
        // zero size message, simply remove it
        b->mj = BAIO_MSGS_PREVIOUS_INDEX(b, b->mj);
        return(-1);
    }

    assert(b->mi != b->mj);
    m = &b->m[BAIO_MSGS_PREVIOUS_INDEX(b, b->mj)];
    assert(m != NULL);
    assert(newSize <= bb->writeBuffer.j);
    m->startIndex = bb->writeBuffer.j - newSize;
    return(m->startIndex);
}

void baioMsgSend(struct baio *bb) {
    struct baioWriteBuffer  *b;
    struct baioMsg          *m;

    if ((bb->status & BAIO_STATUS_ACTIVE) == 0) return;

    b = &bb->writeBuffer;
    assert(b->mi != b->mj);
    m = &b->m[BAIO_MSGS_PREVIOUS_INDEX(b, b->mj)];
    assert(m != NULL);
    m->endIndex = bb->writeBuffer.j;
    baioMsgMaybeActivateNextMsg(bb);
}


void baioMsgLastSetSize(struct baio *bb, int newSize) {
    baioMsgResetStartIndexForNewMsgSize(bb, newSize);
    baioMsgSend(bb);
}

void baioLastMsgDump(struct baio *bb) {
    struct baioWriteBuffer  *b;
    struct baioMsg          *m;

    b = &bb->writeBuffer;
    if (b->mi == b->mj) {
        printf("baioLastMsgDump: Message was joined\n");
        return;
    }
    m = &b->m[BAIO_MSGS_PREVIOUS_INDEX(b, b->mj)];
    printf("baioLastMsgDump: Sending msg %d - %d\n", m->startIndex, m->endIndex);
    printf(": offset+3:  %.*s\n", m->endIndex-m->startIndex-3, b->b+m->startIndex+3);
    // baioCharBufferDump(": ", b->b+m->startIndex, m->endIndex-m->startIndex);
}

//////////////////////////////////////////////////////////////////////////////////////////////////


#if 0
static void baioReadBufferInit(struct baioReadBuffer *b, int initSize) {
    if (initSize == 0) {
        FREE(b->b);
        b->b = NULL;
    } else {
        REALLOCC(b->b, initSize, char);
    }
    b->i = b->j = 0;
    b->size = initSize;
}
#endif

static void baioReadBufferFree(struct baioReadBuffer *b) {
    JSVAR_FREE(b->b);
    b->b = NULL;
    b->i = b->j = 0;
    b->size = 0;
}

int baioReadBufferResize(struct baioReadBuffer *b, int minSize, int maxSize) {
    if (b->size >= maxSize) {
        printf("%s: %s:%d: Error: buffer %p too long, can't resize over %d.\n", 
               JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, b, maxSize);
        return(-1);
    }
    b->size = JSVAR_NEXT_RECOMMENDED_BUFFER_SIZE(b->size);
    if (b->size < minSize) b->size = minSize;
    if (b->size >= maxSize) b->size = maxSize;
    JSVAR_REALLOCC(b->b, b->size, char);
    // Write message after actual resizing. Otherwise, this message can reinvoke resizing and enter into a loop
    if (jsVarDebugLevel > 20) printf("%s: resizing read buffer %p to %d of %d\n", JSVAR_PRINT_PREFIX(), b, b->size, maxSize);
    return(0);
}

static int baioReadBufferShift(struct baio *bb, struct baioReadBuffer *b) {
    int         delta;

	assert(b->i <= b->j);
    delta = b->i;
    memmove(b->b, b->b+delta, (b->j - delta) * sizeof(b->b[0]));
    b->j -= delta;
    b->i -= delta;
    return(delta);
}

#if 0
static void baioWriteBufferInit(struct baioWriteBuffer *b, int initSize) {
    if (initSize == 0) {
        JSVAR_FREE(b->b);
        b->b = NULL;
    } else {
        JSVAR_REALLOCC(b->b, initSize, char);
    }
    b->i = b->ij = b->j = b->jj = 0;
    b->size = initSize;
    if (b->m != NULL) JSVAR_FREE(b->m);
    b->m = NULL;
    b->msize = 0;
    baioWriteMsgsInit(b);
}
#endif

static void baioWriteBufferFree(struct baioWriteBuffer *b) {
    if (jsVarDebugLevel > 20) printf("%s: freeing write buffer %p b->b == %p.\n", JSVAR_PRINT_PREFIX(), b, b->b);
    JSVAR_FREE(b->b);
    b->b = NULL;
    b->i = b->ij = b->j = b->jj = 0;
    b->size = 0;
    if (b->m != NULL) JSVAR_FREE(b->m);
    b->m = NULL;
    b->msize = 0;
    baioWriteMsgsInit(b);   
}

static int baioWriteBufferResize(struct baioWriteBuffer *b, int minSize, int maxSize) {
    if (b->size >= maxSize) {
        printf("%s: %s:%d: Error: write buffer %p too long, can't resize over %d.", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, b, maxSize);
        return(-1);
    }
    b->size = JSVAR_NEXT_RECOMMENDED_BUFFER_SIZE(b->size);
    if (b->size < minSize) b->size = minSize;
    if (b->size >= maxSize) b->size = maxSize;
    JSVAR_REALLOCC(b->b, b->size, char);
    // Write message after actual resizing. Otherwise, this message can reinvoke resizing and enter into a loop
    if (jsVarDebugLevel > 20) printf("%s: resizing write buffer %p to %d of %d. b->b == %p.\n", JSVAR_PRINT_PREFIX(), b, b->size, maxSize, b->b);
    return(0);
}

static void baioSslDisconnect(struct baio *bb) {
    // it can happen that during normal connect we are disconneted even before starting SSL connection
    if (bb->sslHandle != NULL) {
        // For some reason calling SSL_shutdown on a broken session made next connection wrong
        // so I prefer not to close such connections
        // SSL_shutdown(bb->sslHandle);
        SSL_free(bb->sslHandle);
        bb->sslHandle = NULL;
    }
}

static void baioFreeZombie(struct baio *bb) {
    int i;

    i = bb->index;
    assert(bb == baioTab[i]);

    if (bb->readBuffer.i != bb->readBuffer.j) {
        printf("%s:%s:%d: Warning: zombie read.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
    }
    if (bb->writeBuffer.i != bb->writeBuffer.j) {
        printf("%s:%s:%d: Warning: zombie write.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
    }
    baioReadBufferFree(&bb->readBuffer);
    baioWriteBufferFree(&bb->writeBuffer);

    // free all hooks
    jsVarCallBackFreeHook(&bb->callBackOnRead);
    jsVarCallBackFreeHook(&bb->callBackOnWrite);
    jsVarCallBackFreeHook(&bb->callBackOnError);
    jsVarCallBackFreeHook(&bb->callBackOnEof);
    jsVarCallBackFreeHook(&bb->callBackOnDelete);
    jsVarCallBackFreeHook(&bb->callBackOnReadBufferShift);
    jsVarCallBackFreeHook(&bb->callBackOnTcpIpConnect);
    jsVarCallBackFreeHook(&bb->callBackOnConnect);
    jsVarCallBackFreeHook(&bb->callBackOnAccept);
    jsVarCallBackFreeHook(&bb->callBackAcceptFilter);
    jsVarCallBackFreeHook(&bb->callBackOnTcpIpAccept);

	if (bb->sslSniHostName != NULL) JSVAR_FREE(bb->sslSniHostName);
	bb->sslSniHostName = NULL;
    JSVAR_FREE(bb); 
    baioTab[i] = NULL;
}

static void baioCloseFd(struct baio *bb) {
	if (baioFdIsSocket(bb)) closesocket(bb->fd); else close(bb->fd);
}

static int baioImmediateDeactivate(struct baio *bb) {
    // zombie will be cleared at the next cycle
    bb->status = BAIO_STATUS_ZOMBIE;
    bb->baioMagic = 0;
    // soft clean buffers
    bb->readBuffer.i = bb->readBuffer.j = 0;
    bb->writeBuffer.i = bb->writeBuffer.ij = bb->writeBuffer.j = bb->writeBuffer.jj = 0;
    bb->writeBuffer.mi = bb->writeBuffer.mj = 0;
    bb->sslPending = 0;

    if (bb->fd < 0) return(-1);
    if (bb->sslHandle != NULL) baioSslDisconnect(bb);
    if (bb->baioType != BAIO_TYPE_FD && bb->baioType != BAIO_TYPE_FD_SOCKET) {
        baioCloseFd(bb);
    }
    // we have to call callback befre reseting fd to -1, because delete call back may need it
    JSVAR_CALLBACK_CALL(bb->callBackOnDelete, callBack(bb));
    bb->fd = -1;

    return(0);
}

int baioCloseOnError(struct baio *bb) {
    JSVAR_CALLBACK_CALL(bb->callBackOnError, callBack(bb));
    baioImmediateDeactivate(bb);
    return(-1);
}

struct baio *baioFromMagic(int baioMagic) {
    int             i;
    struct baio     *bb;
    // baioMagic can never be 0
    if (baioMagic == 0) return(NULL);
    i = (baioMagic % BAIO_MAX_CONNECTIONS);
    bb = baioTab[i];
    if (bb == NULL) return(NULL);
    if (bb->baioMagic != baioMagic) return(NULL);
    if ((bb->status & BAIO_STATUS_ACTIVE) == 0) return(NULL);
    if ((bb->status & BAIO_STATUS_PENDING_CLOSE) != 0) return(NULL);
    return(bb);
}

/////////////////////////////////////////////////////////////////////////////////

static int baioTabFindUnusedEntryIndex(int baioStructType) {
    int i;

    for(i=0; i<baioTabMax && baioTab[i] != NULL; i++) ;

    if (i >= baioTabMax) {
        baioTabMax = i+1;
        if (i >= BAIO_MAX_CONNECTIONS) {
            printf("%s: %s:%d: Error: Can't allocate baio. Too many connections\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
            return(-1);
        }
    }
    return(i);
}

static void cloneCallBackHooks(struct baio *bb, struct baio *parent) {
    jsVarCallBackCloneHook(&bb->callBackOnRead, &parent->callBackOnRead);
    jsVarCallBackCloneHook(&bb->callBackOnWrite, &parent->callBackOnWrite);
    jsVarCallBackCloneHook(&bb->callBackOnError, &parent->callBackOnError);
    jsVarCallBackCloneHook(&bb->callBackOnEof, &parent->callBackOnEof);
    jsVarCallBackCloneHook(&bb->callBackOnDelete, &parent->callBackOnDelete);
    jsVarCallBackCloneHook(&bb->callBackOnReadBufferShift, &parent->callBackOnReadBufferShift);
    jsVarCallBackCloneHook(&bb->callBackOnTcpIpConnect, &parent->callBackOnTcpIpConnect);
    jsVarCallBackCloneHook(&bb->callBackOnConnect, &parent->callBackOnConnect);
    jsVarCallBackCloneHook(&bb->callBackOnAccept, &parent->callBackOnAccept);
    jsVarCallBackCloneHook(&bb->callBackAcceptFilter, &parent->callBackAcceptFilter);
    jsVarCallBackCloneHook(&bb->callBackOnTcpIpAccept, &parent->callBackOnTcpIpAccept);
}

static int baioLibraryInit(int deInitializationFlag) {
    static int      libraryInitialized = 0;
    int             r;
#if _WIN32
    static WSADATA  wsaData;
#endif

    if (libraryInitialized == 0 && deInitializationFlag == 0) {
#if _WIN32
        r = WSAStartup(MAKEWORD(2,2), &wsaData);
        if (r != 0) {
            printf("WSAStartup failed with error: %d\n", r);
            return(-1);
        }
#endif
        libraryInitialized = 1;
    } else if (libraryInitialized != 0 && deInitializationFlag != 0) {
#if _WIN32
        WSACleanup();
#endif
        libraryInitialized = 0;
    } else {
        return(1);
    }
    return(0);
}

static struct baio *baioInitBasicStructure(int i, int baioType, int ioDirections, int additionalSpaceAllocated, struct baio *parent) {
    struct baio *bb;
    int         parentsize;

    bb = baioTab[i];
    assert(bb != NULL);
    if (parent == NULL) {
        memset(bb, 0, sizeof(struct baio)+additionalSpaceAllocated);
    } else {
        parentsize = sizeof(struct baio) + parent->additionalSpaceAllocated;
        assert(parentsize <= (int)sizeof(struct baio) + additionalSpaceAllocated);
        memcpy(bb, parent, parentsize);
        // clone callbackHooks
        cloneCallBackHooks(bb, parent);
        if (additionalSpaceAllocated > parent->additionalSpaceAllocated) {
            memset((char *)bb+parentsize, 0, additionalSpaceAllocated - parent->additionalSpaceAllocated);
        }
    }
    if (parent == NULL) bb->initialReadBufferSize = (1<<10);
    if (parent == NULL) bb->readBufferRecommendedSize = (1<<16);
    if (parent == NULL) bb->initialWriteBufferSize = (1<<10);
    if (parent == NULL) bb->minFreeSizeBeforeRead = 1600;   /* to hold TCPIP packet */
    if (parent == NULL) bb->minFreeSizeAtEndOfReadBuffer = 1;
    if (parent == NULL) bb->maxReadBufferSize = (1<<24);
    if (parent == NULL) bb->maxWriteBufferSize = (1<<24);
    if (parent == NULL) bb->optimizeForSpeed = 0;

    bb->ioDirections = ioDirections;
    bb->status = BAIO_STATUS_ACTIVE;
    bb->fd = -1;
    bb->index = i;
    // baiomagic shall never be zero 
    // bb->baioMagic = (((magicCounter++) & 0x1ffffe) + 1) * BAIO_MAX_CONNECTIONS + i;
    bb->baioMagic = ((rand() & 0x1ffffe) + 1) * BAIO_MAX_CONNECTIONS + i;
	assert(bb->baioMagic != 0);
    bb->baioType = baioType;
    bb->additionalSpaceAllocated = additionalSpaceAllocated;

    // useSsl is inherited, but sslHandle and sslPending are not
    // bb->useSsl = 0;
    bb->sslHandle = NULL;
    bb->sslPending = 0;
	bb->sslSniHostName = NULL;

    memset(&bb->readBuffer, 0, sizeof(bb->readBuffer));
#if 0
    if (bb->ioDirections == BAIO_IO_DIRECTION_READ || bb->ioDirections == BAIO_IO_DIRECTION_RW) {
        // Hmm. I do not want to allocate buffer here, because then bb->initialReadBufferSize
        // set by user is unused
        baioBufferInit(&bb->readBuffer, bb->initialReadBufferSize);
    }
#endif
    // printf("reset writebuffer %p\n", &bb->writeBuffer);
    memset(&bb->writeBuffer, 0, sizeof(bb->writeBuffer));
#if 0
    if (bb->ioDirections == BAIO_IO_DIRECTION_WRITE || bb->ioDirections == BAIO_IO_DIRECTION_RW) {
        baioWriteBufferInit(&bb->writeBuffer, bb->initialWriteBufferSize);
    }
#endif
    return(bb);
}

struct baio *baioNewBasic(int baioType, int ioDirections, int additionalSpaceToAllocate) {
    int             i;
    struct baio     *bb;

    baioLibraryInit(0);
    i = baioTabFindUnusedEntryIndex(baioType);
    if (i < 0) return(NULL);
    assert(baioTab[i] == NULL);
    if (baioTab[i] == NULL) JSVAR_ALLOC_SIZE(baioTab[i], struct baio, sizeof(struct baio)+additionalSpaceToAllocate);
    bb = baioInitBasicStructure(i, baioType, ioDirections, additionalSpaceToAllocate, NULL);
    return(bb);
}

int baioClose(struct baio *bb) {
    if (bb->ioDirections == BAIO_IO_DIRECTION_READ || bb->ioDirections == BAIO_IO_DIRECTION_RW) {
        baioReadBufferFree(&bb->readBuffer);
    }
    if (bb->ioDirections == BAIO_IO_DIRECTION_WRITE || bb->ioDirections == BAIO_IO_DIRECTION_RW) {
        if (baioMsgInProgress(bb)) {
            // if there is an unfinished message in progress, remove it
            baioMsgRemoveLastMsg(bb);
        }
        if (bb->writeBuffer.i != bb->writeBuffer.j) {
            bb->status |= BAIO_STATUS_PENDING_CLOSE;
        }
    }
    if ((bb->status & BAIO_STATUS_PENDING_CLOSE) == 0) {
        baioImmediateDeactivate(bb);
        return(0);
    }
    return(1);
}

int baioCloseMagic(int baioMagic) {
    struct baio *bb;
    int         res;
    bb = baioFromMagic(baioMagic);
    if (bb == NULL) return(-1);
    res = baioClose(bb);
    return(res);
}

int baioCloseMagicOnError(int baioMagic) {
    struct baio *bb;
    int         res;
    bb = baioFromMagic(baioMagic);
    if (bb == NULL) return(-1);
    res = baioCloseOnError(bb);
    return(res);
}

int baioAttachFd(struct baio *bb, int fd) {
    JsVarInternalCheck(bb->fd == -1);
    JsVarInternalCheck(bb->status == BAIO_STATUS_ACTIVE);
    bb->fd = fd;
    return(0);
}

int baioDettachFd(struct baio *bb, int fd) {
    bb->fd = -1;
    bb->sslPending = 0;
    return(0);
}

// in fact, the real buffer size is b->size - 1, because if b->i == b->j we consider as empty buffer
// so the buffer is full if b->i == b->j-1, i.e. there is always 1 byte unused in our ring buffer
static int baioSizeOfContinuousFreeSpaceForWrite(struct baioWriteBuffer *b) {
    int i,j;
    i = b->i;
    j = b->j;
    if (i <= j) {
        return(b->size - j - 1);
    } else {
        return(i - j - 1);
    }
}

int baioWriteBufferUsedSpace(struct baio *b) {
    int     i,j;
    i = b->writeBuffer.i;
    j = b->writeBuffer.j;
    if (i <= j) {
        return(j - i);
    } else {
        return(b->writeBuffer.jj - i + b->writeBuffer.j);
    }
}

int baioPossibleSpaceForWrite(struct baio *bb) {
    struct baioWriteBuffer  *b;
    struct baioMsg          *m;

    b = &bb->writeBuffer;
    if (bb->optimizeForSpeed) {
        int     i,j;
        int     s1, s2;
        i = b->i;
        j = b->j;
        if (i <= j) {
            s1 = bb->maxWriteBufferSize - j - 1;
            s2 = i;
            if (b->mi != b->mj) {
                m = &b->m[BAIO_MSGS_PREVIOUS_INDEX(b, b->mj)];
                if (m->endIndex == BAIO_INIFINITY_INDEX) {JsVarInternalCheck(b->j-m->startIndex>=0); s2 -= b->j - m->startIndex;}
            }
            return(JSVAR_MAX(s1, s2));
        } else {
            return(i-j-1);
        }
    } else {
        return(bb->maxWriteBufferSize - baioWriteBufferUsedSpace(bb) - 1);
    }
}

static void baioMsgsReindexInterval(struct baioWriteBuffer  *b, int from, int to, int offset) {
    int             i;
    struct baioMsg  *m;
    for(i=b->mi; i!=b->mj; i=BAIO_MSGS_NEXT_INDEX(b, i)) {
        m = &b->m[i];
        if (m->startIndex != BAIO_INIFINITY_INDEX && m->startIndex >= from && m->startIndex <= to) m->startIndex += offset;
        if (m->endIndex != BAIO_INIFINITY_INDEX && m->endIndex >= from && m->endIndex <= to) m->endIndex += offset;
    }
}

static int baioGetSpaceForWriteInWrappedBuffer(struct baio *bb, int n) {
    int                     r, offset;
    struct baioWriteBuffer  *b;

    
    b = &bb->writeBuffer;
    assert(b->j < b->i);
    assert(b->jj > 0);

// printf("RESIZE: %p: baioGetSpaceForWriteInWrappedBuffer: 0\n", b);

    // we have space between j until i, if not enough allocate extra space and increase i
    if (b->i - b->j <= n && bb->optimizeForSpeed) return(-1);

    // printf("6");fflush(stdout);
    while (b->size - b->jj + b->i - b->j <= n) {
        r = baioWriteBufferResize(&bb->writeBuffer, bb->initialWriteBufferSize, bb->maxWriteBufferSize);
        if (r < 0) return(-1);
    }
// printf("RESIZE: %p: baioGetSpaceForWriteInWrappedBuffer: 1\n", b);
    if (b->i - b->j <= n) {
        // printf("RESIZE: %p: baioGetSpaceForWriteInWrappedBuffer: 2\n", b);
        offset = b->size - b->jj;
        // printf("offset == %d, b->i == %d, b->ij == %d, b->j == %d, b->jj == %d;  b->mi == %d, b->mj == %d\n", offset, b->i, b->ij, b->j, b->jj, b->mi, b->mj);
        memmove(b->b+b->i+offset, b->b+b->i, b->jj-b->i);
        // move all messages from j to the end of resized buffer
        baioMsgsReindexInterval(b, b->i, b->jj, offset);
        assert(b->ij >= b->i);
        if (b->ij >= b->i) b->ij += offset;
        b->i += offset;
        b->jj += offset;
    }
    return(0);
}

static int baioGetSpaceForWriteInEmptyBuffer(struct baio *bb, int n) {
    int                     r;
    struct baioWriteBuffer  *b;
    struct baioMsg          *m;

//printf("RESIZE: %p: baioGetSpaceForWriteInEmptyBuffer: 0\n", b);

    b = &bb->writeBuffer;
    // actually there can be message which is going to be constructed
    // assert(b->mi == b->mj);

    while (b->size <= n) {
        r = baioWriteBufferResize(&bb->writeBuffer, bb->initialWriteBufferSize, bb->maxWriteBufferSize);
        if (r < 0) return(-1);
    }
    // O.K. if we have enough of space
    // if (baioSizeOfContinuousFreeSpaceForWrite(b) > n) return(0);

    // no, reset buffer
    b->i = b->ij = b->j = b->jj = 0;
    if (b->mi != b->mj) {
//printf("RESIZE: %p: baioGetSpaceForWriteInEmptyBuffer: 1\n", b);
        assert(BAIO_MSGS_NEXT_INDEX(b, b->mi) == b->mj);
        m = &b->m[b->mi];
        assert(m->endIndex == BAIO_INIFINITY_INDEX);
        m->startIndex = 0;
    }
    return(0);
}

static int baioGetSpaceForWriteInNonEmptyLinearBufferAllMessagesReady(struct baio *bb, int n) {
    int                     r, offset;
    struct baioWriteBuffer  *b;

    b = &bb->writeBuffer;
    assert(b->i < b->j);

    // Unfortunately, all this branch is absolutely non-tested, because we always have a message being composed
    // TODO: Test this in some way
//printf("RESIZE: %p: baioGetSpaceForWriteInNonEmptyLinearBufferAllMessagesReadys: 0\n", b);

    // if we have enough of space at the end of buffer, do nothing
    if (b->size - b->j > n) return(0);
    // if not, if we have enough of space at the begginning of buffer, wrap buffer
    if (b->i > n) {
//printf("RESIZE: %p: baioGetSpaceForWriteInNonEmptyLinearBufferAllMessagesReadys: 1\n", b);
        // printf("1");fflush(stdout);
        b->jj = b->j;
        b->j = 0;
    } else {
// printf("RESIZE: %p: baioGetSpaceForWriteInNonEmptyLinearBufferAllMessagesReadys: 2\n", b);
        while (b->size - b->j <= n && b->size < bb->maxWriteBufferSize) {
            // if we still can resize, then resize
            r = baioWriteBufferResize(&bb->writeBuffer, bb->initialWriteBufferSize, bb->maxWriteBufferSize);
            if (r < 0) return(-1);
        }
        if (b->size - b->j <= n) {
// printf("RESIZE: %p: baioGetSpaceForWriteInNonEmptyLinearBufferAllMessagesReadys: 3\n", b);
            if (bb->optimizeForSpeed) return(-1);
            // TODO: Test this branch!
            if (b->size - b->j + b->i < n) return(-1);
// printf("RESIZE: %p: baioGetSpaceForWriteInNonEmptyLinearBufferAllMessagesReadys: 4\n", b);
            // it is better to shift the used buffer to the end, because new space will come from beginning
            // and in the future we will not need to shift it anymore
            offset = b->size - b->j;
            memmove(b->b+b->i+offset, b->b+b->i, b->j-b->i);
            baioMsgsReindexInterval(b, b->i, b->j, offset);
            b->jj = b->size;
            assert(b->ij >= b->i);
            if (b->ij >= b->i) b->ij += offset;
            b->i += offset;
            b->j = 0;
        }
    }
    return(0);
}

static void baioMemSwap(char *p1, char *p2, char *pe) {
    char    *t;
    int     l1, l2;
    // TODO: Do it without temporary allocation
    assert(p1 <= p2);
    assert(p2 <= pe);
    l1 = p2-p1;
    l2 = pe-p2;
    JSVAR_ALLOCC(t, l2, char);
    memmove(t, p2, l2);
    memmove(pe-l1, p1, l1);
    memmove(p1, t, l2);
    JSVAR_FREE(t);
}

static int baioGetSpaceForWriteInNonEmptyLinearBufferWithWaitingMessages(struct baio *bb, int n) {
    int                     r, msglen, offset;
    struct baioWriteBuffer  *b;
    struct baioMsg          *m;

    b = &bb->writeBuffer;
    assert(b->i < b->j);
    assert(b->mi != b->mj);

//printf("RESIZE: %p: baioGetSpaceForWriteInNonEmptyLinearBufferWithWaitingMessages: 0\n", b);

    // if we have enough of space at the end of buffer, do nothing
    if (b->size - b->j > n) return(0);
    // get last message
    m = &b->m[BAIO_MSGS_PREVIOUS_INDEX(b, b->mj)];

    if (m->endIndex != BAIO_INIFINITY_INDEX) {
        // printf("RESIZE: %p: baioGetSpaceForWriteInNonEmptyLinearBufferWithWaitingMessages: 1\n", b);
        // last message was finished we do not need to care about it
        r = baioGetSpaceForWriteInNonEmptyLinearBufferAllMessagesReady(bb, n);
        return(r);
    }

    // we are currently composing message and we need more space
    msglen = b->j - m->startIndex;
    if (b->i/*??? why there was -1*/ > msglen + n || (b->i == m->startIndex && b->size > msglen + n)) {
        // printf("RESIZE: %p: baioGetSpaceForWriteInNonEmptyLinearBufferWithWaitingMessages: 2. %d,%d,%d,%d  %d,%d,  %d,  %d,%d,   %d\n", b, b->i, b->ij, b->j, b->jj, b->mi, b->mj, m->startIndex, msglen, n, b->size);
        // we have enough of space, move the message to the beginning of buffer
        b->jj = m->startIndex;
        memmove(b->b, b->b+m->startIndex, msglen);
        if (b->i == m->startIndex) {
            // printf("RESIZE: %p: baioGetSpaceForWriteInNonEmptyLinearBufferWithWaitingMessages: 3\n", b);
            b->i = b->ij = 0;
        }
        m->startIndex = 0;
        b->j = msglen;
    } else {
        // not enough of space at the beginning of buffer, we have to allocate extra space at the end
        while (b->size - b->j <= n && b->size < bb->maxWriteBufferSize) {
            // if we still can resize, then resize
            r = baioWriteBufferResize(&bb->writeBuffer, bb->initialWriteBufferSize, bb->maxWriteBufferSize);
            if (r < 0) return(-1);
        }
        // printf("RESIZE: %p: baioGetSpaceForWriteInNonEmptyLinearBufferWithWaitingMessages: 4\n", b);
        if (b->size - b->j <= n) {
            if (bb->optimizeForSpeed) return(-1);
            if (b->size - b->j + b->i < n) return(-1);
            // printf("RESIZE: %p: baioGetSpaceForWriteInNonEmptyLinearBufferWithWaitingMessages: 5\n", b);
            offset = b->size - m->startIndex;
            // it is better to shift the used buffer to the end, because new space will come from beginning
            // and in the future we will not need to shift it anymore
            baioMemSwap(b->b+b->i, b->b+m->startIndex, b->b+b->size);
            // now the last message starts at b->i, move it to the beginning of buffer
            // TODO: Inline the swap and do not copy current message three times
            memmove(b->b, b->b+b->i, msglen);
            baioMsgsReindexInterval(b, b->i, b->j, offset);
            b->jj = b->size;
            assert(b->ij >= b->i);
            if (b->ij >= b->i) b->ij += offset;
            b->i += offset;
            b->j = msglen;
            m->startIndex = 0;
            assert(b->i - b->j >= n);
        }
    }
    return(0);
}

static int baioGetSpaceForWriteInLinearBuffer(struct baio *bb, int n) {
    int                     r;
    struct baioWriteBuffer  *b;

    b = &bb->writeBuffer;
    if (b->mi == b->mj) {
        r = baioGetSpaceForWriteInNonEmptyLinearBufferAllMessagesReady(bb, n);
    } else {
        r = baioGetSpaceForWriteInNonEmptyLinearBufferWithWaitingMessages(bb, n);
    }
    return(r);
}

static int baioGetSpaceForWrite(struct baio *bb, int n) {
    int                     r;
    struct baioWriteBuffer  *b;

    // We need space in a msg based ring buffer
    b = &bb->writeBuffer;
    if (b->i == b->j) {
        r = baioGetSpaceForWriteInEmptyBuffer(bb, n);
    } else if (b->i < b->j) {
        r = baioGetSpaceForWriteInLinearBuffer(bb, n);
    } else {
        r = baioGetSpaceForWriteInWrappedBuffer(bb, n);
    }
    return(r);
}

int baioMsgReserveSpace(struct baio *bb, int len) {
    int                     r;
    struct baioWriteBuffer  *b;

    if ((bb->status & BAIO_STATUS_ACTIVE) == 0) return(-1);

    b = &bb->writeBuffer;
    r = baioGetSpaceForWrite(bb, len);
    if (r < 0) return(-1);
    b->j += len;
    return(len);
}

int baioWriteToBuffer(struct baio *bb, char *s, int len) {
    int                     r;
    char                    *d;
    struct baioWriteBuffer  *b;

    if ((bb->status & BAIO_STATUS_ACTIVE) == 0) return(-1);

    r = baioGetSpaceForWrite(bb, len);
    if (r < 0) return(-1);
    b = &bb->writeBuffer;
    d = b->b + b->j;
    memmove(d, s, len);
    b->j += len;
    return(len);
}

int baioWriteMsg(struct baio *bb, char *s, int len) {
    int n;
    n = baioWriteToBuffer(bb, s, len);
    if (n >= 0) baioMsgPut(bb, bb->writeBuffer.j-n, bb->writeBuffer.j);
    return(n);
}

int baioVprintfToBuffer(struct baio *bb, char *fmt, va_list arg_ptr) {
    int                     n, r, dsize;
    struct baioWriteBuffer  *b;
    va_list                 arg_ptr_copy;

    if ((bb->status & BAIO_STATUS_ACTIVE) == 0) return(-1);

    b = &bb->writeBuffer;
    dsize = baioSizeOfContinuousFreeSpaceForWrite(b);
    if (dsize < 0) dsize = 0;
    va_copy(arg_ptr_copy, arg_ptr);
    n = vsnprintf(b->b + b->j, dsize, fmt, arg_ptr_copy);
#if _WIN32
    while (n < 0) {
        dsize = dsize * 2 + 1024;
        r = baioGetSpaceForWrite(bb, dsize+1);
        if (r < 0) return(-1);
        n = vsnprintf(b->b + b->j, dsize, fmt, arg_ptr_copy);
    }
#endif
    JsVarInternalCheck(n >= 0);
    va_copy_end(arg_ptr_copy);
    if (n >= dsize) {
        r = baioGetSpaceForWrite(bb, n+1);
        if (r < 0) return(-1);
        va_copy(arg_ptr_copy, arg_ptr);
        n = vsnprintf(b->b + b->j, n+1, fmt, arg_ptr_copy);
        JsVarInternalCheck(n>=0);
        va_copy_end(arg_ptr_copy);
    }

    b->j += n;
    return(n);
}

int baioPrintfToBuffer(struct baio *bb, char *fmt, ...) {
    int             res;
    va_list         arg_ptr;

    va_start(arg_ptr, fmt);
    res = baioVprintfToBuffer(bb, fmt, arg_ptr);
    va_end(arg_ptr);
    return(res);
}

int baioVprintfMsg(struct baio *bb, char *fmt, va_list arg_ptr) {
    int n;
    n = baioVprintfToBuffer(bb, fmt, arg_ptr);
    if (n >= 0) baioMsgPut(bb, bb->writeBuffer.j-n, bb->writeBuffer.j);
    return(n);
}

int baioPrintfMsg(struct baio *bb, char *fmt, ...) {
    int             res;
    va_list         arg_ptr;

    va_start(arg_ptr, fmt);
    res = baioVprintfMsg(bb, fmt, arg_ptr);
    va_end(arg_ptr);
    return(res);
}

//////////////////////////////////////////////////////////

static int baioSslVerifyCallback(int preverify, X509_STORE_CTX* x509_ctx) {
    if (! preverify) {
        printf("%s: %s:%d: Verification problem.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
    }
    return preverify;
}

static int baioSslLibraryInit() {
    static int libraryInitialized = 0;

    if (libraryInitialized) return(0);

    // Register the error strings for libcrypto & libssl
    SSL_load_error_strings();
    // Register the available ciphers and digests
    SSL_library_init();

    // We are probably sticked with version 2 because of some servers
    baioSslContext = SSL_CTX_new(SSLv23_method());
    // baioSslContext = SSL_CTX_new(SSLv3_method());

    if (baioSslContext == NULL) {
        ERR_print_errors_fp(stderr);
        return(-1);
    }

    /* Load server certificate */
    if (SSL_CTX_use_certificate_file(baioSslContext, baioSslRsaServerCertFile, SSL_FILETYPE_PEM) <= 0) {
        printf("%s:%s:%d: Can't load SSL server certificate file %s.", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, baioSslRsaServerCertFile);
        ERR_print_errors_fp(stdout);
        // Does not mind for client side, uncomment following if you need a Ssl server
        // SSL_CTX_free(baioSslContext);
        // baioSslContext = NULL;
        // return(-1);
    }
    /* Load private-key */
    if (SSL_CTX_use_PrivateKey_file(baioSslContext, baioSslRsaServerKeyFile, SSL_FILETYPE_PEM) <= 0) { 
        printf("%s:%s:%d: Can't load SSL server key file %s.", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, baioSslRsaServerKeyFile);
        ERR_print_errors_fp(stdout);
        // Does not mind for client side, uncomment following if you need a Ssl server
        //SSL_CTX_free(baioSslContext);
        //baioSslContext = NULL;
        //return(-1);
    }

#if 0
    /* unlock this code and define baioSslTrustedCertFile, if you want to verify certificates */
    SSL_CTX_set_verify(baioSslContext, SSL_VERIFY_PEER, baioSslVerifyCallback);

    if(! SSL_CTX_load_verify_locations(baioSslContext, baioSslTrustedCertFile, NULL)) {
        printf("%s:%s:%d: Can't load trusted certificates %s.", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, baioSslTrustedCertFile);
        ERR_print_errors_fp(stdout);
        SSL_CTX_free(baioSslContext);
        baioSslContext = NULL;
        return(-1);
    }
#endif

    SSL_CTX_set_mode(baioSslContext, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
    libraryInitialized = 1;
    return(0);
}

static int baioSslHandleInit(struct baio *bb) {
    int r;

    r = baioSslLibraryInit();
    if (r != 0) return(r);

    // Create an SSL struct for the connection
    bb->sslHandle = SSL_new(baioSslContext);
    if (bb->sslHandle == NULL) return(-1);

    // Connect the SSL struct to our connection
    if (! SSL_set_fd(bb->sslHandle, bb->fd)) return(-1);

    return(0);
}

static void baioSslConnect(struct baio *bb) {
    int err, r;

    if (bb->sslHandle == NULL) return;

    ERR_clear_error();
	// SNI support
	if (bb->sslSniHostName != NULL) SSL_set_tlsext_host_name(bb->sslHandle, bb->sslSniHostName);
    r = SSL_connect(bb->sslHandle);

    if (r == 0) {
        // First set status, then callbak, so that I can use the same callback as for copy
        bb->status |= BAIO_STATUS_EOF_READ;
        JSVAR_CALLBACK_CALL(bb->callBackOnEof, callBack(bb));
    }

    if (r < 0) {
        err = SSL_get_error(bb->sslHandle, r);
        if (err == SSL_ERROR_WANT_READ) {
            bb->status |= BAIO_BLOCKED_FOR_READ_IN_SSL_CONNECT;
        } else if (err==SSL_ERROR_WANT_WRITE) {
            bb->status |= BAIO_BLOCKED_FOR_WRITE_IN_SSL_CONNECT;
        } else {
            // printf("ssl connect r==%d err == %d\n", r, err);
            baioCloseOnError(bb);
            return;
        }
    } else {
        // Connection established
        JSVAR_CALLBACK_CALL(bb->callBackOnConnect, callBack(bb));
    }
}

static void baioOnTcpIpConnected(struct baio *bb) {
    int     r;

    JSVAR_CALLBACK_CALL(bb->callBackOnTcpIpConnect, callBack(bb));
    // It seems that I should getsockopt to read the SO_ERROR to check if connection was successful, however is it really necessary?
    {
        int err;
        socklen_t len = sizeof(err);
        getsockopt(bb->fd, SOL_SOCKET, SO_ERROR, (char*)&err, &len);
        if (err != 0) {baioCloseOnError(bb); return;}
    }

    if (bb->useSsl == 0) {
        // nothing more to do if clean connection
        JSVAR_CALLBACK_CALL(bb->callBackOnConnect, callBack(bb));
        return;
    }
    r = baioSslHandleInit(bb);
    if (r != 0) {
        baioCloseOnError(bb);
        return;
    }
    baioSslConnect(bb);
}

static void baioSslAccept(struct baio *bb) {
    int r, err;

    if (bb->sslHandle == NULL) return;

    ERR_clear_error();
    r = SSL_accept(bb->sslHandle);

    // printf("%s: ssl_accept returned %d\n", JSVAR_PRINT_PREFIX(), r); fflush(stdout);
    if (r == 1) {
        // accepted
        JSVAR_CALLBACK_CALL(bb->callBackOnAccept, callBack(bb));
        if (jsVarDebugLevel > 0) printf("%s: %s:%d: SSL connection accepted on socket %d\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, bb->fd);
        return;
    }

    // not accepted
    err = SSL_get_error(bb->sslHandle, r);
    if (err == SSL_ERROR_WANT_READ) {
        bb->status |= BAIO_BLOCKED_FOR_READ_IN_SSL_ACCEPT;
    } else if (err == SSL_ERROR_WANT_WRITE) {
        bb->status |= BAIO_BLOCKED_FOR_WRITE_IN_SSL_ACCEPT;
    } else {
        // printf("r == %d, err == %d errno %s\n", r, err, JSVAR_STR_ERRNO());
        if (jsVarDebugLevel > 0) printf("%s: %s:%d: SSL accept on socket %d failed\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, bb->fd);
        baioImmediateDeactivate(bb);
    }
}

static void baioOnTcpIpListenActivated(struct baio *bb) {
    struct sockaddr_in      clientaddr;
    int                     fd, i, r;
    unsigned                ip, port;
    struct baio             *cc;
    socklen_t               len;

    // anyway I'll continue listening
    bb->status |= BAIO_BLOCKED_FOR_READ_IN_TCPIP_LISTEN;

    len = sizeof(clientaddr);
    fd = accept(bb->fd, (struct sockaddr *)&clientaddr, &len);
    if (fd < 0) {
        if (jsVarDebugLevel > 0) printf("A new client connection failed.\n");
        return;
    }
    r = setSocketNonBlocking(fd);
    if (r < 0) {
        printf("%s: Can't set new tcp/ip client connection to non-blocking state, closing it.\n", JSVAR_PRINT_PREFIX());
        closesocket(fd);
        return;
    }
    ip = clientaddr.sin_addr.s_addr;
    port = clientaddr.sin_port;

    if (jsVarDebugLevel > 0) printf("%s: connection accepted from %s:%d on socket %d.\n", JSVAR_PRINT_PREFIX(), sprintfIpAddress_st(ip), port, bb->fd);

    r = 0;
    JSVAR_CALLBACK_CALL(bb->callBackAcceptFilter, (r = callBack(bb, ip, port)));

    // reject connection if filter rejected
    if (r < 0) {
        closesocket(fd);
        return;
    }

    // continue with creating new baio
    i = baioTabFindUnusedEntryIndex(BAIO_TYPE_TCPIP_SCLIENT);
    if (i < 0) {
        printf("%s: Can't allocate new baio for new client %s:%d on socket %d\n", JSVAR_PRINT_PREFIX(), sprintfIpAddress_st(ip), port, bb->fd);
        closesocket(fd);
        return;
    }
    assert(baioTab[i] == NULL);
    if (baioTab[i] == NULL) JSVAR_ALLOC_SIZE(baioTab[i], struct baio, sizeof(struct baio)+bb->additionalSpaceAllocated);
    cc = baioInitBasicStructure(i, BAIO_TYPE_TCPIP_SCLIENT, BAIO_IO_DIRECTION_RW, bb->additionalSpaceAllocated, bb);
    cc->fd = fd;
    cc->remoteComputerIp = ip;
    cc->parentBaioMagic = bb->baioMagic;
    JSVAR_CALLBACK_CALL(cc->callBackOnTcpIpAccept, callBack(cc));
    if (cc->useSsl == 0) {
        JSVAR_CALLBACK_CALL(cc->callBackOnAccept, callBack(cc));
    } else {
        r = baioSslHandleInit(cc);
        if (r != 0) {
            baioCloseOnError(cc);
            return;
        }
        baioSslAccept(cc);
    }
}

static int baioItIsPreferrableToResizeReadBufferOverMovingData(struct baioReadBuffer *b, struct baio *bb) {
    // if there is room for resizeing and data occupy too much space, prefer resizing
    // buffer is of maximal size, move
    if (b->size >= bb->maxReadBufferSize / 2) return(0);
    // More than half of space occupied in the buffer, prefer resize
    if (b->j - b->i >= b->size / 2) return(1);
    return(0);
}

static void baioNormalizeBufferBeforeRead(struct baioReadBuffer *b, struct baio *bb) {
    int delta; 

    if (b->i == b->j) {
        // we have previously processed all the buffer, reset indexes to start from the very beginning
        b->i = b->j = 0;
    } 
    if (b->b != NULL
        && b->size - b->j < bb->minFreeSizeBeforeRead + bb->minFreeSizeAtEndOfReadBuffer   // not enough space for read
        && baioItIsPreferrableToResizeReadBufferOverMovingData(b, bb) == 0
        ) {
        // we rolled to the end of the buffer, move the trail to get enough of space
        delta = baioReadBufferShift(bb, b);
        JSVAR_CALLBACK_CALL(bb->callBackOnReadBufferShift, callBack(bb, delta));
    }
    while (b->size - b->j < bb->minFreeSizeBeforeRead + bb->minFreeSizeAtEndOfReadBuffer && b->size < bb->maxReadBufferSize) {
        // buffer too small, resize it until it fits the size or max value
        baioReadBufferResize(b, bb->initialReadBufferSize, bb->maxReadBufferSize);
    }
}

static void baioHandleSuccesfullRead(struct baio *bb, int n) {
    struct baioReadBuffer   *b;
    int                     sj;

    b = &bb->readBuffer;
    if (n == 0) {
        // end of file
        bb->status |= BAIO_STATUS_EOF_READ;
        // Hmm. maybe I shall call eof callback only when readbuffer is empty
        // Unfortunately this is not possible, because we can miss it (if read buffer is emptied out of baio).
        JSVAR_CALLBACK_CALL(bb->callBackOnEof, callBack(bb));
    } else if (n > 0) {
        bb->totalBytesRead += n;
        sj = b->j;
        b->j += n;
        if (bb->minFreeSizeAtEndOfReadBuffer > 0) b->b[b->j] = 0;
        JSVAR_CALLBACK_CALL(bb->callBackOnRead, callBack(bb, sj, n));
        // If we have read half of the read buffer at once, maybe it is time to resize it
        if (n > b->size / 2 && b->size < bb->readBufferRecommendedSize) {
            baioReadBufferResize(b, bb->initialReadBufferSize, bb->maxReadBufferSize);
        }
    }
}

static int baioOnCanRead(struct baio *bb) {
    struct baioReadBuffer   *b;
    int                     n;
    int                     minFreeSizeAtEndOfReadBuffer;

    b = &bb->readBuffer;
    minFreeSizeAtEndOfReadBuffer = bb->minFreeSizeAtEndOfReadBuffer;
    baioNormalizeBufferBeforeRead(b, bb);
    if (baioFdIsSocket(bb)) {
        n = recv(bb->fd, b->b+b->j, b->size-b->j-1-minFreeSizeAtEndOfReadBuffer, 0);
    } else {
        n = read(bb->fd, b->b+b->j, b->size-b->j-1-minFreeSizeAtEndOfReadBuffer);
    }
    if (jsVarDebugLevel > 20) {printf("read returned %d\n", n); fflush(stdout);}
    if (n < 0) return(baioCloseOnError(bb));
    baioHandleSuccesfullRead(bb, n);
    return(n);
}

static int baioOnCanSslRead(struct baio *bb) {
    struct baioReadBuffer   *b;
    int                     n, err;
    int                     minFreeSizeAtEndOfReadBuffer;

    assert(bb->useSsl);
    if (bb->sslHandle == NULL) return(0);

    b = &bb->readBuffer;
    minFreeSizeAtEndOfReadBuffer = bb->minFreeSizeAtEndOfReadBuffer;
    baioNormalizeBufferBeforeRead(b, bb);
    bb->sslPending = 0;
    ERR_clear_error();
    n = SSL_read(bb->sslHandle, b->b + b->j, b->size - b->j - minFreeSizeAtEndOfReadBuffer);
    if (jsVarDebugLevel > 20) {printf("SSL_read(,,%d) returned %d\n", b->size - b->j - minFreeSizeAtEndOfReadBuffer, n); fflush(stdout);}
    if (n < 0) {
        err = SSL_get_error(bb->sslHandle, n);
        if (err == SSL_ERROR_WANT_WRITE) {
            bb->status |= BAIO_BLOCKED_FOR_WRITE_IN_SSL_READ;
            return(0);
        }
        if (err == SSL_ERROR_WANT_READ) {
            bb->status |= BAIO_BLOCKED_FOR_READ_IN_SSL_READ;
            return(0);
        }
        return(baioCloseOnError(bb));
    }
    baioHandleSuccesfullRead(bb, n);
    if (bb->sslHandle != NULL) bb->sslPending = SSL_pending(bb->sslHandle); 
    return(n);
}

static int baioWriteOrSslWrite(struct baio *bb) {
    struct baioWriteBuffer  *b;
    int                     n, err, len;

    b = &bb->writeBuffer;
    if (bb->useSsl == 0) {
        len = b->ij - b->i;
        if (baioFdIsSocket(bb)) {
            if (len > BAIO_SEND_MAX_MESSAGE_LENGTH) len = BAIO_SEND_MAX_MESSAGE_LENGTH;
            n = send(bb->fd, b->b+b->i, len, 0);
        } else {
            n = write(bb->fd, b->b+b->i, len);
        }
        if (jsVarDebugLevel > 20) {printf("write(,,%d) returned %d\n", b->ij - b->i, n); fflush(stdout);}
        if (n < 0 && (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR)) n = 0;
    } else {
        if (bb->sslHandle == NULL) return(-1);
        n = SSL_write(bb->sslHandle, b->b+b->i, b->ij - b->i);
        if (jsVarDebugLevel > 20) {printf("SSL_write(,,%d) returned %d\n", b->ij - b->i, n); fflush(stdout);}
        if (n < 0) {
            err = SSL_get_error(bb->sslHandle, n);
            if (err == SSL_ERROR_WANT_WRITE) {
                bb->status |= BAIO_BLOCKED_FOR_WRITE_IN_SSL_WRITE;
                n = 0;
            } else if (err == SSL_ERROR_WANT_READ) {
                bb->status |= BAIO_BLOCKED_FOR_READ_IN_SSL_WRITE;
                n = 0;
            }
        }
    }
    if (n > 0) bb->totalBytesWritten += n;
    // printf("write: %d bytes, b->i == %d: Bytes: '%.20s...%.20s'\n", n, b->i, b->b+b->i, b->b+b->i+n-20);
    // printf("write: b->i == %d: %.*s\n", b->i, n, b->b+b->i);
    // printf("write: b->i == %d: %.*s\n", b->i, n, b->b+b->i);
    // printf("b->i == %d: ", b->i);baioCharBufferDump("baioWritten: ", b->b+b->i, n);
    // printf("b->j == %d: ", b->j);
    // printf("%s: writen %d bytes to fd %d\n", JSVAR_PRINT_PREFIX(), n, bb->fd);
    return(n);
}

int baioOnCanWrite(struct baio *bb) {
    struct baioWriteBuffer  *b;
    int                     n;
    int                     si;

    n = 0;
    b = &bb->writeBuffer;
    si = b->i;

    // only one write is allowed by select, go back to the main loop after a single write
    if (bb->fd >= 0 && b->i < b->ij) {
        n = baioWriteOrSslWrite(bb);
        if (n < 0) return(baioCloseOnError(bb));
        b->i += n;
    }
    baioMsgMaybeActivateNextMsg(bb);

    if (n != 0) JSVAR_CALLBACK_CALL(bb->callBackOnWrite, callBack(bb, si, n));
    // Attention has been taken here, because onWrite callback can itself write something
    // On the other hand we shall not call it on closed bb and we shall not shift it before
    // calling onwrite callback, because it would not receive correct indexes
    if (BAIO_WRITE_BUFFER_DO_FULL_SIZE_RING_CYCLES == 0 && b->i == b->j) {
        // with this optimization you can not use write buffer to retrieving historical data sent out
        // if buffer is empty after a write it is reinitialized to zero indexes
        b->i = b->ij = b->j = b->jj = 0;
        assert(bb->writeBuffer.mi == bb->writeBuffer.mj || bb->status == BAIO_STATUS_ZOMBIE);
        baioWriteMsgsInit(b);
    }
    if (b->i == b->j) {
        if (bb->status & BAIO_STATUS_PENDING_CLOSE) baioImmediateDeactivate(bb);
    }
    return(n);
}

/////////////////////


static int baioAddSelectFdToSet(int maxfd, int fd, fd_set *ss) {
    if (ss != NULL) {
        FD_SET(fd, ss);
        if (fd > maxfd) return(fd);
    }
    return(maxfd);
}

int baioSetSelectParams(int maxfd, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *t) {
    int             i, fd;
    struct baio     *bb;
    unsigned        status;
    int             flagWaitingForReadFd;
    int             flagWaitingForWriteFd;
    int             flagForZeroTimeoutAsBufferedDataAvailableWithoutFd;

    flagForZeroTimeoutAsBufferedDataAvailableWithoutFd = 0;
    for(i=0; i<baioTabMax; i++) {
        bb = baioTab[i];
        if (bb == NULL) goto nextfd;
        status = bb->status;
        fd = bb->fd;
        flagWaitingForReadFd = flagWaitingForWriteFd = 0;
        // free zombies
        if (1 && bb->status == BAIO_STATUS_ZOMBIE) {
            baioFreeZombie(bb);
        } else if (bb->baioType == BAIO_TYPE_TCPIP_SCLIENT && (status & BAIO_STATUS_EOF_READ)) {
            // I am allocating server clients, so I am going to delete them as well
            baioImmediateDeactivate(bb);
        } else if ((status & BAIO_STATUS_ACTIVE) &&  fd >= 0) {
            // special statuses (like connect, ssl_accept, ...
            if (status & BAIO_BLOCKED_FOR_READ_IN_SPECIAL_STATUS_MASK) {
                flagWaitingForReadFd = 1;
            } else if (status & BAIO_BLOCKED_FOR_WRITE_IN_SPECIAL_STATUS_MASK) {
                flagWaitingForWriteFd = 1;
            } else {
                // check for reading
                if (status & BAIO_BLOCKED_FOR_READ_STATUS_MASK) {
                    flagWaitingForReadFd = 1;
                } else if (
                    (bb->ioDirections == BAIO_IO_DIRECTION_READ || bb->ioDirections == BAIO_IO_DIRECTION_RW)
                    && (status & BAIO_STATUS_EOF_READ) == 0
                    && (status & BAIO_STATUS_PENDING_CLOSE) == 0
                    && bb->maxReadBufferSize - (bb->readBuffer.j - bb->readBuffer.i) > bb->minFreeSizeBeforeRead
                    ) {
                    if (bb->useSsl) {
                        if (bb->sslHandle != NULL && bb->sslPending > 0) {
                            bb->status |= BAIO_BUFFERED_DATA_AVAILABLE_FOR_SSL_READ;
                            if (fd > maxfd) maxfd = fd;
                            flagForZeroTimeoutAsBufferedDataAvailableWithoutFd = 1;
                        } else {
                            bb->status |= BAIO_BLOCKED_FOR_READ_IN_SSL_READ;
                            flagWaitingForReadFd = 1;
                        }
                    } else {
                        bb->status |= BAIO_BLOCKED_FOR_READ_IN_READ;
                        flagWaitingForReadFd = 1;
                    }
                }
                // check for writing
                if (status & BAIO_BLOCKED_FOR_WRITE_STATUS_MASK) {
                    flagWaitingForWriteFd = 1;
                } else if (
                    (bb->ioDirections == BAIO_IO_DIRECTION_WRITE || bb->ioDirections == BAIO_IO_DIRECTION_RW)
                    // previously there was the !=, but it may be safer to use <
                    // && bb->writeBuffer.i != bb->writeBuffer.ij
                    && bb->writeBuffer.i < bb->writeBuffer.ij
                    ) {
                    if (bb->useSsl) bb->status |= BAIO_BLOCKED_FOR_WRITE_IN_SSL_WRITE;
                    else bb->status |= BAIO_BLOCKED_FOR_WRITE_IN_WRITE;
                    flagWaitingForWriteFd = 1;
                }
            }
            if (flagWaitingForReadFd) {
                maxfd = baioAddSelectFdToSet(maxfd, fd, readfds);
            }
            if (flagWaitingForWriteFd) {
                maxfd = baioAddSelectFdToSet(maxfd, fd, writefds);
            }
            if (flagWaitingForReadFd || flagWaitingForWriteFd) {
                maxfd = baioAddSelectFdToSet(maxfd, fd, exceptfds);
            }
            if (jsVarDebugLevel > 20) {printf("%s: Set      %d: fd %d: status: %6x -> %6x; rwe : %d%d%d\n", JSVAR_PRINT_PREFIX(), i, fd, status, bb->status, (readfds != NULL&&FD_ISSET(fd, readfds)), (writefds != NULL && FD_ISSET(fd, writefds)), (exceptfds != NULL && FD_ISSET(fd, exceptfds))); fflush(stdout);}
        }
    nextfd:;
    }
    if (flagForZeroTimeoutAsBufferedDataAvailableWithoutFd) {
        if (t != NULL) {
            t->tv_sec = 0;
            t->tv_usec = 0;
        } else {
            printf("%s:%s:%d: Warning: Timeout parameter zero when read buffer full. Possible latency problem!\n", 
                   JSVAR_PRINT_PREFIX(), __FILE__, __LINE__
                );
        }
    }
    return(maxfd+1);
}

int baioOnSelectEvent(int maxfd, fd_set *readfds, fd_set *writefds, fd_set *exceptfds) {
    int             i, fd, max, res;
    unsigned        status;
    struct baio     *bb;

    res = 0;
    // loop until current baioTabMax, actions in the loop may add new descriptors, but those
    // were not waited at this moment. Do not check them
    max = baioTabMax;
    for(i=0; i<max; i++) {
        bb = baioTab[i];
        if (bb == NULL) goto nextfd;
#if EDEBUG
        assert(bb->writeBuffer.ij >= bb->writeBuffer.i);
#endif
        status = bb->status;
        bb->status &= ~(BAIO_STATUSES_CLEARED_PER_TICK);
        fd = bb->fd;
        // printf("fd == %d, maxfd == %d\n", fd, maxfd);
        if (0 && bb->status == BAIO_STATUS_ZOMBIE) {
            baioFreeZombie(bb);
        } else if (fd >= 0 && fd < maxfd) {
            if (jsVarDebugLevel > 20) {printf("%s: Event on %d: fd %d: status: %6x    %6s; rwep: %d%d%d%d\n", JSVAR_PRINT_PREFIX(), i, fd, status, "", (readfds != NULL&&FD_ISSET(fd, readfds)), (writefds != NULL && FD_ISSET(fd, writefds)), (exceptfds != NULL && FD_ISSET(fd, exceptfds)), (status & BAIO_BUFFERED_DATA_AVAILABLE_FOR_SSL_READ)); fflush(stdout);}
            // first check for ssl_pending
            if (exceptfds != NULL && FD_ISSET(fd, exceptfds)) {
                res ++;
                baioImmediateDeactivate(bb);
                goto nextfd;
            }
            if (status & BAIO_BUFFERED_DATA_AVAILABLE_FOR_SSL_READ) {
                baioOnCanSslRead(bb);
                res ++;
            } else if ((readfds == NULL || FD_ISSET(fd, readfds) == 0) && (writefds == NULL || FD_ISSET(fd, writefds) == 0)) {
                // no activity on this fd, status remains unchanged
                bb->status = status;
            } else {
                if (readfds != NULL && FD_ISSET(fd, readfds)) {
                    res ++;
                    if (status & BAIO_BLOCKED_FOR_READ_IN_TCPIP_LISTEN) {
                        baioOnTcpIpListenActivated(bb);
                        if (jsVarDebugLevel > 30) baioWriteBufferDump(&bb->writeBuffer);
                        goto nextfd;
                    } 
                    if (status & BAIO_BLOCKED_FOR_READ_IN_SSL_CONNECT) {
                        baioSslConnect(bb);
                        if (jsVarDebugLevel > 30) baioWriteBufferDump(&bb->writeBuffer);
                        goto nextfd;
                    } 
                    if (status & BAIO_BLOCKED_FOR_READ_IN_SSL_ACCEPT) {
                        baioSslAccept(bb);
                        if (jsVarDebugLevel > 30) baioWriteBufferDump(&bb->writeBuffer);
                        goto nextfd;
                    } 
                    if (status & BAIO_BLOCKED_FOR_READ_IN_READ) {
                        baioOnCanRead(bb);
                    } 
                    if (status & BAIO_BLOCKED_FOR_READ_IN_SSL_READ) {
                        baioOnCanSslRead(bb);
                    } 
                    if (status & BAIO_BLOCKED_FOR_READ_IN_SSL_WRITE) {
                        baioOnCanWrite(bb);
                    } 
                }
                if (writefds != NULL && FD_ISSET(fd, writefds)) {
                    res ++;
                    if (status & BAIO_BLOCKED_FOR_WRITE_IN_TCPIP_CONNECT) {
                        baioOnTcpIpConnected(bb);
                        if (jsVarDebugLevel > 30) baioWriteBufferDump(&bb->writeBuffer);
                        goto nextfd;
                    } 
                    if (status & BAIO_BLOCKED_FOR_WRITE_IN_SSL_CONNECT) {
                        baioSslConnect(bb);
                        if (jsVarDebugLevel > 30) baioWriteBufferDump(&bb->writeBuffer);
                        goto nextfd;
                    } 
                    if (status & BAIO_BLOCKED_FOR_WRITE_IN_SSL_ACCEPT) {
                        baioSslAccept(bb);
                        if (jsVarDebugLevel > 30) baioWriteBufferDump(&bb->writeBuffer);
                        goto nextfd;
                    } 
                    if (status & BAIO_BLOCKED_FOR_WRITE_IN_WRITE) {
                        baioOnCanWrite(bb);
                    }
                    if (status & BAIO_BLOCKED_FOR_WRITE_IN_SSL_READ) {
                        baioOnCanSslRead(bb);
                    } 
                    if (status & BAIO_BLOCKED_FOR_WRITE_IN_SSL_WRITE) {
                        baioOnCanWrite(bb);
                    } 
                }
            }
        } else {
            // fd was not watched at this tick, do not change status
            bb->status = status;
        }
        if (jsVarDebugLevel > 40) baioWriteBufferDump(&bb->writeBuffer);
#if EDEBUG
        assert(bb->writeBuffer.ij >= bb->writeBuffer.i);
#endif
    nextfd:;
    }
    return(res);
}

//////////////////////////////////////////////////////////

int baioSelect(int maxfd, fd_set *r, fd_set *w, fd_set *e, struct timeval *t) {
    int             res;
#if _WIN32
    int             i, fd;
    struct baio     *bb;

    // Remove all file related fds from select as Windows do not manage this.
    // Only sockets can be sent to select. Files are considered as ready to read/write.
    uint8_t         imask[BAIO_MAX_CONNECTIONS];
    for(i=0; i<baioTabMax; i++) {
        imask[i] = 0;
        bb = baioTab[i];
        if (bb != NULL && ! baioFdIsSocket(bb)) {
            fd = bb->fd;
            if (FD_ISSET(fd, r)) {imask[i] |= 0x1; FD_CLR(fd, r);}
            if (FD_ISSET(fd, w)) {imask[i] |= 0x2; FD_CLR(fd, w);}
            if (FD_ISSET(fd, e)) {imask[i] |= 0x4; FD_CLR(fd, e);}
        }
    }
    if (maxfd == 1) {
        Sleep(t->tv_sec*1000 + t->tv_usec/1000);
        res = 0;
    } else {
        res = select(maxfd, r, w, e, t);
    }
    for(i=0; i<baioTabMax; i++) {
        bb = baioTab[i];
        if (bb != NULL) {
            fd = bb->fd;
            if (imask[i] & 0x1) {FD_SET(fd, r);}
            if (imask[i] & 0x2) {FD_SET(fd, w);}
        }
    }
#else
    res = select(maxfd, r, w, e, t);
#endif  

    return(res);
}

int baioPoll2(int timeOutUsec, int (*addUserFds)(int maxfd, fd_set *r, fd_set *w, fd_set *e), void (*processUserFds)(int maxfd, fd_set *r, fd_set *w, fd_set *e)) {
    fd_set          r, w, e;
    struct timeval  t;
    int             maxfd, res;

    FD_ZERO(&r);
    FD_ZERO(&w);
    FD_ZERO(&e);

    t.tv_sec = timeOutUsec / 1000000LL;
    t.tv_usec = timeOutUsec % 1000000LL; 
    maxfd = baioSetSelectParams(0, &r, &w, &e, &t);
    if (addUserFds != NULL) maxfd = addUserFds(maxfd, &r, &w, &e);
    // printf("calling select, maxfd == %d\n", maxfd);fflush(stdout);
    res = baioSelect(maxfd, &r, &w, &e, &t);
    if (res < 0) {
        if (errno != EINTR) {
            printf("%s:%s:%d: ERROR: select returned %d and errno == %d (%s)!\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, res, errno, strerror(errno));
        }
        FD_ZERO(&r); FD_ZERO(&w); FD_ZERO(&e); 
    }
    // printf("returning from select, maxfd == %d\n", maxfd);fflush(stdout);
    if (processUserFds != NULL) processUserFds(maxfd, &r, &w, &e);
    baioOnSelectEvent(maxfd, &r, &w, &e);
    return(res);
}

int baioPoll(int timeOutUsec) {
    int res;
    res =  baioPoll2(timeOutUsec, NULL, NULL);
    return(res);
}

//////////////////////////////////////////////////////////
// baio file

struct baio *baioNewFile(char *path, int ioDirection, int additionalSpaceToAllocate) {
    struct baio     *bb;
    int             r, fd;
    unsigned        flags;

    if (path == NULL) return(NULL);

    if (ioDirection == BAIO_IO_DIRECTION_READ) flags = (O_RDONLY);
    else if (ioDirection == BAIO_IO_DIRECTION_WRITE) flags = (O_WRONLY | O_CREAT);
    else if (ioDirection == BAIO_IO_DIRECTION_RW) flags = (O_RDWR | O_CREAT);
    else {
        printf("%s:%s:%d: Invalid ioDirection\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
        return(NULL);
    }

    // if not windows
    // flags |= O_NONBLOCK;

    // printf("%s: Opening file %s:%o\n", JSVAR_PRINT_PREFIX(), path, flags);

    fd = open(path, flags, 00644);
#if ! _WIN32
    r = setFileNonBlocking(fd);
    if (r < 0) {
        printf("%s: Error: Can't set file descriptor to non-blocking state, closing it.", JSVAR_PRINT_PREFIX());
        close(fd);
        return(NULL);
    }
#endif

    bb = baioNewBasic(BAIO_TYPE_FILE, ioDirection, additionalSpaceToAllocate);
    bb->fd = fd;
    return(bb);
}

//////////////////////////////////////////////////////////
// pseudo read file
// This is used to provide a single string in form of buffered input (for assynchronous copy, for example)
// It is used mainly for sending precalculated html pages in web server.

struct baio *baioNewPseudoFile(char *string, int stringLength, int additionalSpaceToAllocate) {
    struct baio     *bb;
    int             r;

    if (string == NULL) return(NULL);

    // printf("%s: Opening pseudo file\n", JSVAR_PRINT_PREFIX());

    bb = baioNewBasic(BAIO_TYPE_FILE, BAIO_IO_DIRECTION_READ, additionalSpaceToAllocate);
    bb->status |= BAIO_STATUS_EOF_READ;
    r = baioReadBufferResize(&bb->readBuffer, stringLength, stringLength);
    if (r < 0) {
        baioImmediateDeactivate(bb);
        return(NULL);
    }
    memmove(bb->readBuffer.b, string, stringLength);
    bb->readBuffer.i = 0;
    bb->readBuffer.j = stringLength;

    return(bb);
}

//////////////////////////////////////////////////////////
// baio piped file

struct baio *baioNewPipedFile(char *path, int ioDirection, int additionalSpaceToAllocate) {
    struct baio     *bb;
    int             r, fd;
    char            *iod;

#if _WIN32
    printf("%s:%s:%d: Piped files are not available for Windows system. Use socket file instaed.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
    return(NULL);
#else

    if (path == NULL) return(NULL);

    if (ioDirection == BAIO_IO_DIRECTION_READ) iod = "r";
    else if (ioDirection == BAIO_IO_DIRECTION_WRITE) iod = "w";
    else {
        printf("%s:%s:%d: Invalid ioDirection: piped file can be open for read only or write only.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
        return(NULL);
    }


    fd = popen2File(path, iod);
    if (fd < 0) return(NULL);
    r = setFileNonBlocking(fd);
    if (r < 0) {
        printf("%s: Error: Can't set file descriptor to non-blocking state, closing it.\n", JSVAR_PRINT_PREFIX());
        close(fd);
        return(NULL);
    }

    // printf("%s: Opening piped file %s\n", JSVAR_PRINT_PREFIX(), path);

    bb = baioNewBasic(BAIO_TYPE_PIPED_FILE, ioDirection, additionalSpaceToAllocate);
    bb->fd = fd;
    return(bb);
#endif
}

//////////////////////////////////////////////////////////
// baio piped command

struct baio *baioNewPipedCommand(char *command, int ioDirection, int additionalSpaceToAllocate) {
    struct baio     *bb;
    int             r, fd;
    char            *iod;

#if _WIN32
    printf("%s:%s:%d: Piped commandss are not available for Windows system. \n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
    return(NULL);
#else

    if (command == NULL) return(NULL);
    if (ioDirection == BAIO_IO_DIRECTION_READ) {
        r = popen2(command, &fd, NULL, 1);
    } else if (ioDirection == BAIO_IO_DIRECTION_WRITE) {
        r = popen2(command, NULL, &fd, 1);
    } else {
        printf("%s:%s:%d: Invalid ioDirection: piped command can be open for read only or write only.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
        r = -1;
    }
	if (r < 0) return(NULL);
    if (fd < 0) return(NULL);
    r = setFileNonBlocking(fd);
    if (r < 0) {
        printf("%s: Error: Can't set file descriptor to non-blocking state, closing it.\n", JSVAR_PRINT_PREFIX());
        close(fd);
        return(NULL);
    }
    bb = baioNewBasic(BAIO_TYPE_PIPED_COMMAND, ioDirection, additionalSpaceToAllocate);
    bb->fd = fd;
    return(bb);
#endif
}

//////////////////////////////////////////////////////////
// baio TCP/IP network

static int baioSetUseSslFromSslFlag(struct baio *bb, enum baioSslFlags sslFlag) {
    if (sslFlag == BAIO_SSL_NO) {
        bb->useSsl = 0;
        return(0);
    } else if (sslFlag == BAIO_SSL_YES) {
#if BAIO_USE_OPEN_SSL || WSAIO_USE_OPEN_SSL || JSVAR_USE_OPEN_SSL
        bb->useSsl = 1;
        return(0);
#else
        return(-1);
#endif
    } else {
        return(1);
    }
}

int baioGetSockAddr(char *hostName, int port, struct sockaddr *out_saddr, int *out_saddrlen) {
#if 1 || _WIN32
    int                     r;
    struct addrinfo         *server, hints;
    char                    ps[JSVAR_TMP_STRING_SIZE];

    server = NULL;
    memset(&hints, 0, sizeof(hints) );
    // hints.ai_family = AF_UNSPEC;
	hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    sprintf(ps, "%d", port);
    // Resolve the server address and port
    r = getaddrinfo(hostName, ps, &hints, &server);
    if ( r != 0 ) {
        printf("%s: getaddrinfo %s:%d failed with error: %d\n", JSVAR_PRINT_PREFIX(), hostName, port, r);
        return(-1);
    }
    if (server == NULL) {
        printf("%s: getaddrinfo gives no result\n", JSVAR_PRINT_PREFIX());
        return(-1);
    }
    if (out_saddr == NULL) return(-1);
    if (*out_saddrlen < (int)server->ai_addrlen) return(-1);
    *out_saddrlen = (int)server->ai_addrlen;
    memcpy(out_saddr, server->ai_addr, *out_saddrlen);
#else
    struct hostent      *server;
    int                 n;

    server = gethostbyname(hostName);
    if (server == NULL) return(-1);
    if (out_saddr == NULL) return(-1);
    if (*out_saddrlen < sizeof(struct sockaddr_in)) return(-1);
    *out_saddrlen = sizeof(struct sockaddr_in);
    memset(out_saddr, 0, sizeof(struct sockaddr_in));
    memcpy(&((struct sockaddr_in *)out_saddr)->sin_addr.s_addr, server->h_addr, server->h_length);
    ((struct sockaddr_in *)out_saddr)->sin_port = htons(port);
    ((struct sockaddr_in *)out_saddr)->sin_family = AF_INET;
#endif
    return(0);
}

struct baio *baioNewTcpipClient(char *hostName, int port, enum baioSslFlags sslFlag, int additionalSpaceToAllocate) {
    struct baio             *bb;
    int                     r, fd;
    struct sockaddr         saddr;
    int                     saddrlen;


    saddrlen = sizeof(saddr);
    r = baioGetSockAddr(hostName, port, &saddr, &saddrlen);

    if (r != 0) return(NULL);
    if (saddrlen < 0) return(NULL);

    bb = baioNewBasic(BAIO_TYPE_TCPIP_CLIENT, BAIO_IO_DIRECTION_RW, additionalSpaceToAllocate);

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        printf("%s: %s:%d: Can't create socket\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
        baioImmediateDeactivate(bb);
        return(NULL);
    }

    r = setSocketNonBlocking(fd);
    if (r < 0) {
        printf("%s: %s:%d: Can't set socket non blocking\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
        goto failreturn;
    }

    bb->fd = fd;
    r = baioSetUseSslFromSslFlag(bb, sslFlag);
    if (r < 0) {
        printf("%s: %s:%d: Can't set ssl. Baio is compiled without ssl support\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
        goto failreturn;
    } else if (r > 0) {
        printf("%s: %s:%d: Warning: wrong value for sslFlag\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
    }
	// SNI support?
	bb->sslSniHostName = strDuplicate(hostName);

    if (jsVarDebugLevel > 5) printf("%s: Connecting to %s:%d\n", JSVAR_PRINT_PREFIX(), sprintfIpAddress_st(((struct sockaddr_in*)&saddr)->sin_addr.s_addr), ntohs(((struct sockaddr_in*)&saddr)->sin_port));

    // connect
    r = connect(fd, &saddr, saddrlen);
#if _WIN32
    if (r == 0 || (r == SOCKET_ERROR && (WSAGetLastError() == EINPROGRESS || WSAGetLastError() == WSAEWOULDBLOCK))) 
#else
    if (r == 0 || (r < 0 && errno == EINPROGRESS)) 
#endif
    {
        // connected or pending connect
        // do not immediately call sslconnect even if connected here, rather return bb let the user to setup
        // callbacks go through select and continue there
        bb->status |= BAIO_BLOCKED_FOR_WRITE_IN_TCPIP_CONNECT;
    } else {
        printf("%s: %s:%d: Connect returned %d: %s!\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, r, JSVAR_STR_ERRNO());
        printf("%s: %s", JSVAR_PRINT_PREFIX(), JSVAR_STR_ERRNO());
        goto failreturn;
    }
    return(bb);

failreturn:
    closesocket(fd);
    baioImmediateDeactivate(bb);
    return(NULL);
}

struct baio *baioNewTcpipServer(int port, enum baioSslFlags sslFlag, int additionalSpaceToAllocate) {
    struct baio             *bb;
    struct sockaddr_in      sockaddr;
    int                     r, fd;
#if _WIN32
    DWORD                   one;
#else
    int                     one;
#endif

    if (port < 0) return(NULL);

    bb = baioNewBasic(BAIO_TYPE_TCPIP_SERVER, BAIO_IO_DIRECTION_RW, additionalSpaceToAllocate);

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        printf("%s: %s:%d: Can't create socket\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
        baioImmediateDeactivate(bb);
        return(NULL);
    }

    one = 1;
    r = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *) &one, sizeof(one));
    if (r < 0) {
        printf("%s: %s:%d: Can't set socket reusable\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
        goto failreturn;
    }

    r = setSocketNonBlocking(fd);
    if (r < 0) {
        printf("%s: %s:%d: Can't set socket non blocking\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
        goto failreturn;
    }

    r = baioSetUseSslFromSslFlag(bb, sslFlag);
    if (r < 0) {
        printf("%s: %s:%d: Can't set ssl. Baio is compiled without ssl support\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
        goto failreturn;
    } else if (r > 0) {
        printf("%s: %s:%d: Warning: wrong value for sslFlag\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
    }

    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_addr.s_addr = INADDR_ANY;
    // next line shall restrict connections on loppback device only
    // sockaddr.sin_addr.s_addr = htonl(2130706433L)
    sockaddr.sin_port = htons(port);

    if (jsVarDebugLevel > 0) printf("%s: Listening on %d\n", JSVAR_PRINT_PREFIX(), port);

    r = bind(fd, (struct sockaddr *)&sockaddr, sizeof(sockaddr));
    if (r < 0) {
        printf("%s: %s:%d: Bind to %d failed: %s\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, port, JSVAR_STR_ERRNO());
        goto failreturn;
    }

    r = listen(fd, 128);
    if (r < 0) {
        printf("%s: %s:%d: Listen for %d failed: %s\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, port, JSVAR_STR_ERRNO());
        goto failreturn;
    }

    bb->status |= BAIO_BLOCKED_FOR_READ_IN_TCPIP_LISTEN;
    bb->fd = fd;
    return(bb);

failreturn:
    closesocket(fd);
    baioImmediateDeactivate(bb);
    return(NULL);
}


//////////////////////////////////////////////////////////
// baio socket file

struct baioOpenSocketFileData {
    int     ioDirection;
    int     sockFd;
    char    path[1];        // maybe MAX_PATH, anyway the structure will be allocated long enough to keep whole path
};

// This is a simple TCP/IP server, accepting connection and sending/reading a file content.
#if _WIN32
DWORD WINAPI baioSocketFileServerThreadStartRoutine(LPVOID arg)
#else
void *baioSocketFileServerThreadStartRoutine(void *arg) 
#endif
{
    struct baioOpenSocketFileData   *targ;
    int                             newsockfd;
    socklen_t                       clilen;
    struct sockaddr_in              cli_addr;
    int                             r, n, nn;
    FILE                            *ff;
    char                            buffer[BAIO_SOCKET_FILE_BUFFER_SIZE];
    fd_set                          rset;
    struct timeval                  tout;

    newsockfd = -1;

    // signal(SIGCHLD, zombieHandler);              // avoid system of keeping child zombies
    targ = (struct baioOpenSocketFileData *) arg;

    // This is a kind of timeout, if the main thread does not connect within 10 seconds, exit
    FD_ZERO(&rset);
    FD_SET(targ->sockFd, &rset);
    tout.tv_sec = 10;
    tout.tv_usec = 0;
    r = select(targ->sockFd+1, &rset, NULL, NULL, &tout);

    // If timeout, abort thread
    if (r == 0) goto finito;

    clilen = sizeof(cli_addr);
    newsockfd = accept(targ->sockFd, (struct sockaddr *) &cli_addr, &clilen);
    if (newsockfd < 0) {
        printf("%s:%s:%d: baioOpenSocketFileStartRoutine: Error on accept: %s", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, strerror(errno));
        goto finito;
    }
    //printf("%s: baioOpenSocketFileServerThread: Connection Accepted for %s\n", JSVAR_PRINT_PREFIX(), targ->path);

    if (targ->ioDirection == BAIO_IO_DIRECTION_READ) {
        ff = fopen(targ->path, "rb");
        if (ff == NULL) {
            printf("%s:%s:%d: baioOpenSocketFileStartRoutine: Cant' open %s for read: %s", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, targ->path, strerror(errno));
            goto finito;
        }
        do {
            n = fread(buffer, 1, BAIO_SOCKET_FILE_BUFFER_SIZE, ff);
            //printf("%s: baioOpenSocketFileServerThread: Read %d bytes from %s\n", JSVAR_PRINT_PREFIX(), n, targ->path);
            if (n > 0) {
                nn = send(newsockfd, buffer, n, 0);
                if (n != nn) printf("%s: baioOpenSocketFileServerThread: Sent %d bytes from %s while read %d\n", JSVAR_PRINT_PREFIX(), nn, targ->path, n);              
            }
        } while (n == BAIO_SOCKET_FILE_BUFFER_SIZE);
        fclose(ff);
    } else if (targ->ioDirection == BAIO_IO_DIRECTION_WRITE) {
        ff = fopen(targ->path, "wb");
        if (ff == NULL) {
            printf("%s:%s:%d: baioOpenSocketFileStartRoutine: Cant' open %s for write: %s", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, targ->path, strerror(errno));
            goto finito;
        }
        do {
            n = recv(newsockfd, buffer, BAIO_SOCKET_FILE_BUFFER_SIZE, 0);
            if (n > 0) n = fwrite(buffer, n, 1, ff);
        } while (n > 0);
        fclose(ff);
    }

finito:
    if (newsockfd >= 0) closesocket(newsockfd);
    closesocket(targ->sockFd);
    free(targ);
#if _WIN32
    return(0);
#else
    return(NULL);
#endif
}

// returns port number
static int baioSocketFileLaunchServerThread(char *path, int ioDirection) {
    int                             r, pathlen;
    struct baioOpenSocketFileData   *targ;
    int                             sockfd, portno;
    socklen_t                       slen;
    struct sockaddr_in              serv_addr;

    static int                      baioOpenSocketFileDisabled = 0;

    if (baioOpenSocketFileDisabled) return(-1);

    // Start a simple TCP/IP server, which will listen in another thread
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("%s:%s:%d: baioOpenSocketFileStartRoutine: Can't get socket: %s", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, strerror(errno));
        return(-1);
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = 0;     // port number 0 informs the system that any port can be taken
    r = bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    if (r < 0) {
        printf("%s:%s:%d: baioOpenSocketFileStartRoutine: Can't bind: %s", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, strerror(errno));
        closesocket(sockfd);
        return(-1);
    }
    slen = sizeof(serv_addr);
    r = getsockname(sockfd, (struct sockaddr *) &serv_addr, &slen);
    if (r < 0) {
        printf("%s:%s:%d: baioOpenSocketFileStartRoutine: Getsockname failed: %s", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, strerror(errno));
        closesocket(sockfd);
        return(-1);
    }
    portno = ntohs(serv_addr.sin_port);
    r = listen(sockfd, 1);
    if (r < 0) {
        printf("%s:%s:%d: baioOpenSocketFileStartRoutine: listen failed: %s", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, strerror(errno));
        closesocket(sockfd);
        return(-1);
    }

    pathlen = strlen(path);
    targ = (struct baioOpenSocketFileData *) malloc(sizeof(struct baioOpenSocketFileData) + pathlen + 1);
    targ->ioDirection = ioDirection;
    strcpy(targ->path, path);
    targ->sockFd = sockfd;

#if _WIN32

    {
        HANDLE ttt;
        ttt = CreateThread(NULL, 0, baioSocketFileServerThreadStartRoutine, targ, 0, NULL);
        if (ttt == NULL) {
            printf("%s:%s:%d: CreateThread failed in baioSocketFileLaunchServerThread\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
            closesocket(sockfd);
            return(-1);
        }
        // detach the thread
        r = CloseHandle(ttt);
        if (r == 0) {
            printf("%s: %s:%d: Error: baioOpenSocketFile can't CloseHandle. Preventing any other socket files!\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
            // Be severe here. Otherwise there is a risk to exhaust thread limit in which case
            // the whole task would be terminated by the system.
            baioOpenSocketFileDisabled = 1;
        }
    }

#else

#if JSVAR_PTHREADS
    {
        pthread_t ttt;
        r = pthread_create(&ttt, NULL, baioSocketFileServerThreadStartRoutine, targ);
        if (r) {
            printf("%s:%s:%d: pthread_create failed in baioSocketFileLaunchServerThread: %s\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, strerror(errno));
            closesocket(sockfd);
            return(-1);
        }
        r = pthread_detach(ttt);
        if (r) {
            printf("%s: %s:%d: Error: baioOpenSocketFile can't detach thread: %s. Preventing any other socket files!\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, strerror(errno));
            // Be severe here. Otherwise there is a risk to exhaust thread limit in which case
            // the whole task would be terminated by the system.
            baioOpenSocketFileDisabled = 1;
        }
    }
#else
    printf("%s: %s:%d: Error: JSVAR must be linked with pthreads and macro JSVAR_PTHREADS defined to use socket files on Unix systems!\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
#endif
#endif

    return(portno);
}

struct baio *baioNewSocketFile(char *path, int ioDirection, int additionalSpaceToAllocate) {
    struct baio         *bb;
    int                 portno;
    struct sockaddr_in  serv_addr;
    struct hostent      *server;

    if (path == NULL) return(NULL);

    if (ioDirection != BAIO_IO_DIRECTION_READ && ioDirection != BAIO_IO_DIRECTION_WRITE) {
        printf("%s:%s:%d: Invalid ioDirection: socket file can be open for read only or write only.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
        return(NULL);
    }
    portno = baioSocketFileLaunchServerThread(path, ioDirection);
    if (portno < 0) return(NULL);

    // printf("Socket file: using port %d\n", portno);
    // printf("%s: Opening socket file %s\n", JSVAR_PRINT_PREFIX(), path);

    // TODO: I need some elementary synchronization, not to connect before the thread is accepting connections
    // usleep(1);
    bb = baioNewTcpipClient("127.0.0.1", portno, BAIO_SSL_NO, additionalSpaceToAllocate);
    return(bb);
}


















/////////////////////////////////////////////////////////////////////////////////////////
// Wsaio stands for Web and Websocket Server based on baio Assynchronous I/O


#define WSAIO_CHUNKED_ANSWER_LENGTH_SPACE_RESERVE   10

#define WSAIO_WEBSOCKET_GUID            "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

#define WSAIO_WEBSOCKET_FIN_MSK         0x80
#define WSAIO_WEBSOCKET_RSV1_MSK        0x40
#define WSAIO_WEBSOCKET_RSV2_MSK        0x20
#define WSAIO_WEBSOCKET_RSV3_MSK        0x10

#define WSAIO_WEBSOCKET_OP_CODE_MSK             0x0f
#define WSAIO_WEBSOCKET_OP_CODE_CONTINUATION    0x00
#define WSAIO_WEBSOCKET_OP_CODE_TEXT            0x01
#define WSAIO_WEBSOCKET_OP_CODE_BIN             0x02
#define WSAIO_WEBSOCKET_OP_CODE_CLOSED          0x08
#define WSAIO_WEBSOCKET_OP_CODE_PING            0x09
#define WSAIO_WEBSOCKET_OP_CODE_PONG            0x0a

#define WSAIO_WEBSOCKET_MASK_MSK        0x80
#define WSAIO_WEBSOCKET_PLEN_MSK        0x7f

#define WSAIO_SEND_FILE_BUFFER_SIZE     1400 /* a value around a typical MTU (1500) minus IP, TCP and TLS header and some reserve */

enum wsaioStates {
    WSAIO_STATE_WAITING_FOR_WWW_REQUEST,
    WSAIO_STATE_WAITING_PROCESSING_WWW_REQUEST,
    WSAIO_STATE_WEBSOCKET_ACTIVE,
};

enum wsaioHttpHeaders {
    WSAIO_HTTP_HEADER_NONE,
    WSAIO_HTTP_HEADER_Connection,
    WSAIO_HTTP_HEADER_Host,
    WSAIO_HTTP_HEADER_Origin,
    WSAIO_HTTP_HEADER_Upgrade,

    WSAIO_HTTP_HEADER_Accept_Encoding,
    WSAIO_HTTP_HEADER_Content_Length,

    WSAIO_HTTP_HEADER_Sec_WebSocket_Key,
    WSAIO_HTTP_HEADER_Sec_WebSocket_Protocol,
    WSAIO_HTTP_HEADER_Sec_WebSocket_Version,
    WSAIO_HTTP_HEADER_MAX,
};

static char *wsaioHttpHeader[WSAIO_HTTP_HEADER_MAX];
static int  wsaioHttpHeaderLen[WSAIO_HTTP_HEADER_MAX];


//////////////////////////////////////////////////////////////////////////////////

#if 0
static char *wsaioStrnchr(char *s, int len, int c) {
    char *send;

    if (s == NULL) return(NULL);
    send = s+len;
    while (s<send && *s != c) s++;
    if (s < send) return(s);
    return(NULL);
}
#endif

static int wsaioHexDigitToInt(int hx) {
    if (hx >= '0' && hx <= '9') return(hx - '0');
    if (hx >= 'A' && hx <= 'F') return(hx - 'A' + 10);
    if (hx >= 'a' && hx <= 'f') return(hx - 'a' + 10);
    return(-1);
}

/* 
If OpenSSL is not linked with jsvar, we need another implementation of SHA1.
The SHA1 code originates from https://github.com/clibs/sha1, all credits
for it belongs to Steve Reid.
*/
#if BAIO_USE_OPEN_SSL || WSAIO_USE_OPEN_SSL || JSVAR_USE_OPEN_SSL

// Using OpenSSL, use SHA1 from openSsl
#define SHA1_DIGEST_LENGTH                  SHA_DIGEST_LENGTH 
#define JSVAR_SHA1(s, r)                    SHA1(s, strlen((const char *)s), r);


#else


// Not using OpenSSL, redefined functions and data types provided by OpenSsl to dummy 
// or alternative implementations
#define SHA1_DIGEST_LENGTH                  20
#define JSVAR_SHA1(s, r)                    SHA1((char *)r, (char *)s, strlen((const char *)s));

/////////////////////////////////////////////////////////
// start of included sha1.[hc] code
/*
SHA-1 in C
By Steve Reid <steve@edmweb.com>
100% Public Domain
*/
/* #define LITTLE_ENDIAN * This should be #define'd already, if true. */
/* #define SHA1HANDSOFF * Copies data before messing with it. */

#define SHA1HANDSOFF

typedef struct
{
    uint32_t state[5];
    uint32_t count[2];
    unsigned char buffer[64];
} SHA1_CTX;

void SHA1Transform(
    uint32_t state[5],
    const unsigned char buffer[64]
    );

void SHA1Init(
    SHA1_CTX * context
    );

void SHA1Update(
    SHA1_CTX * context,
    const unsigned char *data,
    uint32_t len
    );

void SHA1Final(
    unsigned char digest[20],
    SHA1_CTX * context
    );

void SHA1(
    char *hash_out,
    const char *str,
    int len);

#define rol(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))
#if BYTE_ORDER == LITTLE_ENDIAN
#define blk0(i) (block->l[i] = (rol(block->l[i],24)&0xFF00FF00) \
    |(rol(block->l[i],8)&0x00FF00FF))
#elif BYTE_ORDER == BIG_ENDIAN
#define blk0(i) block->l[i]
#else
#error "Endianness not defined!"
#endif
#define blk(i) (block->l[i&15] = rol(block->l[(i+13)&15]^block->l[(i+8)&15] \
    ^block->l[(i+2)&15]^block->l[i&15],1))

/* (R0+R1), R2, R3, R4 are the different operations used in SHA1 */
#define R0(v,w,x,y,z,i) z+=((w&(x^y))^y)+blk0(i)+0x5A827999+rol(v,5);w=rol(w,30);
#define R1(v,w,x,y,z,i) z+=((w&(x^y))^y)+blk(i)+0x5A827999+rol(v,5);w=rol(w,30);
#define R2(v,w,x,y,z,i) z+=(w^x^y)+blk(i)+0x6ED9EBA1+rol(v,5);w=rol(w,30);
#define R3(v,w,x,y,z,i) z+=(((w|x)&y)|(w&x))+blk(i)+0x8F1BBCDC+rol(v,5);w=rol(w,30);
#define R4(v,w,x,y,z,i) z+=(w^x^y)+blk(i)+0xCA62C1D6+rol(v,5);w=rol(w,30);

/* Hash a single 512-bit block. This is the core of the algorithm. */

void SHA1Transform(
    uint32_t state[5],
    const unsigned char buffer[64]
)
{
    uint32_t a, b, c, d, e;

    typedef union
    {
        unsigned char c[64];
        uint32_t l[16];
    } CHAR64LONG16;

#ifdef SHA1HANDSOFF
    CHAR64LONG16 block[1];      /* use array to appear as a pointer */

    memcpy(block, buffer, 64);
#else
    /* The following had better never be used because it causes the
     * pointer-to-const buffer to be cast into a pointer to non-const.
     * And the result is written through.  I threw a "const" in, hoping
     * this will cause a diagnostic.
     */
    CHAR64LONG16 *block = (const CHAR64LONG16 *) buffer;
#endif
    /* Copy context->state[] to working vars */
    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];
    /* 4 rounds of 20 operations each. Loop unrolled. */
    R0(a, b, c, d, e, 0);
    R0(e, a, b, c, d, 1);
    R0(d, e, a, b, c, 2);
    R0(c, d, e, a, b, 3);
    R0(b, c, d, e, a, 4);
    R0(a, b, c, d, e, 5);
    R0(e, a, b, c, d, 6);
    R0(d, e, a, b, c, 7);
    R0(c, d, e, a, b, 8);
    R0(b, c, d, e, a, 9);
    R0(a, b, c, d, e, 10);
    R0(e, a, b, c, d, 11);
    R0(d, e, a, b, c, 12);
    R0(c, d, e, a, b, 13);
    R0(b, c, d, e, a, 14);
    R0(a, b, c, d, e, 15);
    R1(e, a, b, c, d, 16);
    R1(d, e, a, b, c, 17);
    R1(c, d, e, a, b, 18);
    R1(b, c, d, e, a, 19);
    R2(a, b, c, d, e, 20);
    R2(e, a, b, c, d, 21);
    R2(d, e, a, b, c, 22);
    R2(c, d, e, a, b, 23);
    R2(b, c, d, e, a, 24);
    R2(a, b, c, d, e, 25);
    R2(e, a, b, c, d, 26);
    R2(d, e, a, b, c, 27);
    R2(c, d, e, a, b, 28);
    R2(b, c, d, e, a, 29);
    R2(a, b, c, d, e, 30);
    R2(e, a, b, c, d, 31);
    R2(d, e, a, b, c, 32);
    R2(c, d, e, a, b, 33);
    R2(b, c, d, e, a, 34);
    R2(a, b, c, d, e, 35);
    R2(e, a, b, c, d, 36);
    R2(d, e, a, b, c, 37);
    R2(c, d, e, a, b, 38);
    R2(b, c, d, e, a, 39);
    R3(a, b, c, d, e, 40);
    R3(e, a, b, c, d, 41);
    R3(d, e, a, b, c, 42);
    R3(c, d, e, a, b, 43);
    R3(b, c, d, e, a, 44);
    R3(a, b, c, d, e, 45);
    R3(e, a, b, c, d, 46);
    R3(d, e, a, b, c, 47);
    R3(c, d, e, a, b, 48);
    R3(b, c, d, e, a, 49);
    R3(a, b, c, d, e, 50);
    R3(e, a, b, c, d, 51);
    R3(d, e, a, b, c, 52);
    R3(c, d, e, a, b, 53);
    R3(b, c, d, e, a, 54);
    R3(a, b, c, d, e, 55);
    R3(e, a, b, c, d, 56);
    R3(d, e, a, b, c, 57);
    R3(c, d, e, a, b, 58);
    R3(b, c, d, e, a, 59);
    R4(a, b, c, d, e, 60);
    R4(e, a, b, c, d, 61);
    R4(d, e, a, b, c, 62);
    R4(c, d, e, a, b, 63);
    R4(b, c, d, e, a, 64);
    R4(a, b, c, d, e, 65);
    R4(e, a, b, c, d, 66);
    R4(d, e, a, b, c, 67);
    R4(c, d, e, a, b, 68);
    R4(b, c, d, e, a, 69);
    R4(a, b, c, d, e, 70);
    R4(e, a, b, c, d, 71);
    R4(d, e, a, b, c, 72);
    R4(c, d, e, a, b, 73);
    R4(b, c, d, e, a, 74);
    R4(a, b, c, d, e, 75);
    R4(e, a, b, c, d, 76);
    R4(d, e, a, b, c, 77);
    R4(c, d, e, a, b, 78);
    R4(b, c, d, e, a, 79);
    /* Add the working vars back into context.state[] */
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    /* Wipe variables */
    a = b = c = d = e = 0;
#ifdef SHA1HANDSOFF
    memset(block, '\0', sizeof(block));
#endif
}


/* SHA1Init - Initialize new context */

void SHA1Init(
    SHA1_CTX * context
)
{
    /* SHA1 initialization constants */
    context->state[0] = 0x67452301;
    context->state[1] = 0xEFCDAB89;
    context->state[2] = 0x98BADCFE;
    context->state[3] = 0x10325476;
    context->state[4] = 0xC3D2E1F0;
    context->count[0] = context->count[1] = 0;
}


/* Run your data through this. */

void SHA1Update(
    SHA1_CTX * context,
    const unsigned char *data,
    uint32_t len
)
{
    uint32_t i;

    uint32_t j;

    j = context->count[0];
    if ((context->count[0] += len << 3) < j)
        context->count[1]++;
    context->count[1] += (len >> 29);
    j = (j >> 3) & 63;
    if ((j + len) > 63)
    {
        memcpy(&context->buffer[j], data, (i = 64 - j));
        SHA1Transform(context->state, context->buffer);
        for (; i + 63 < len; i += 64)
        {
            SHA1Transform(context->state, &data[i]);
        }
        j = 0;
    }
    else
        i = 0;
    memcpy(&context->buffer[j], &data[i], len - i);
}


/* Add padding and return the message digest. */

void SHA1Final(
    unsigned char digest[20],
    SHA1_CTX * context
)
{
    unsigned i;

    unsigned char finalcount[8];

    unsigned char c;

#if 0    /* untested "improvement" by DHR */
    /* Convert context->count to a sequence of bytes
     * in finalcount.  Second element first, but
     * big-endian order within element.
     * But we do it all backwards.
     */
    unsigned char *fcp = &finalcount[8];

    for (i = 0; i < 2; i++)
    {
        uint32_t t = context->count[i];

        int j;

        for (j = 0; j < 4; t >>= 8, j++)
            *--fcp = (unsigned char) t}
#else
    for (i = 0; i < 8; i++)
    {
        finalcount[i] = (unsigned char) ((context->count[(i >= 4 ? 0 : 1)] >> ((3 - (i & 3)) * 8)) & 255);      /* Endian independent */
    }
#endif
    c = 0200;
    SHA1Update(context, &c, 1);
    while ((context->count[0] & 504) != 448)
    {
        c = 0000;
        SHA1Update(context, &c, 1);
    }
    SHA1Update(context, finalcount, 8); /* Should cause a SHA1Transform() */
    for (i = 0; i < 20; i++)
    {
        digest[i] = (unsigned char)
            ((context->state[i >> 2] >> ((3 - (i & 3)) * 8)) & 255);
    }
    /* Wipe variables */
    memset(context, '\0', sizeof(*context));
    memset(&finalcount, '\0', sizeof(finalcount));
}

void SHA1(
    char *hash_out,
    const char *str,
    int len)
{
    SHA1_CTX ctx;
    unsigned int ii;

    SHA1Init(&ctx);
    for (ii=0; ii<len; ii+=1)
        SHA1Update(&ctx, (const unsigned char*)str + ii, 1);
    SHA1Final((unsigned char *)hash_out, &ctx);
    hash_out[20] = '\0';
}

// end of SHA1 implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif









void wsaioWebsocketStartNewMessage(struct wsaio *ww) {
    // put there a gap of inifinite length (will be resize when the msg is finished)
    baioMsgPut(&ww->b, ww->b.writeBuffer.j, BAIO_INIFINITY_INDEX);
    baioMsgReserveSpace(&ww->b, 10);
}

// This function overwrites original parameter (decoded URL is never longer than original)
static char *wsaioUriDecode(char *uri) {
    char *s, *d;
    for(d=s=uri; *s; s++, d++) {
        if (*s == '+') *d = ' ';
        else if (*s == '%') {
            if (s[1] != ' ' && s[1] != 0 && s[2] != ' ' && s[2] != 0) {
                *d = wsaioHexDigitToInt(s[1]) * 16 + wsaioHexDigitToInt(s[2]);
                s += 2;
            }
        } else {
            *d = *s;
        }
    }
    *d = 0;
    return(d);
}

static char *wsaioStrFindTwoNewlines(char *s) {
    char *res;
    res = strstr(s, "\r\n\r\n");
    if (res != NULL) return(res);
    res = strstr(s, "\n\n");
    if (res != NULL) return(res);
    return(NULL);
}

#define WSAIO_HTTP_HEADER_SET(name, namelen, val, vallen, header) { \
        if (strncasecmp(name, #header, sizeof(#header)-1) == 0) {       \
            wsaioHttpHeader[WSAIO_HTTP_HEADER_##header] = val;          \
            wsaioHttpHeaderLen[WSAIO_HTTP_HEADER_##header] = vallen;        \
        }                                                               \
    }
#define WSAIO_HTTP_HEADER_SET2(name, namelen, val, vallen, header1, header2) { \
        if (strncasecmp(name, #header1 "-" #header2, sizeof(#header1)+sizeof(#header2)-1) == 0) {       \
            wsaioHttpHeader[WSAIO_HTTP_HEADER_##header1##_##header2] = val;         \
            wsaioHttpHeaderLen[WSAIO_HTTP_HEADER_##header1##_##header2] = vallen;       \
        }                                                               \
    }
#define WSAIO_HTTP_HEADER_SET3(name, namelen, val, vallen, header1, header2, header3) { \
        if (strncasecmp(name, #header1 "-" #header2 "-" #header3, sizeof(#header1)+sizeof(#header2)+sizeof(#header3)-1) == 0) {     \
            wsaioHttpHeader[WSAIO_HTTP_HEADER_##header1##_##header2##_##header3] = val;         \
            wsaioHttpHeaderLen[WSAIO_HTTP_HEADER_##header1##_##header2##_##header3] = vallen;       \
        }                                                               \
    }
#define WSAIO_HTTP_HEADER_EQUALS(headerIndex, headerValue) (            \
        wsaioHttpHeader[headerIndex] != NULL                            \
        && strlen(headerValue) == wsaioHttpHeaderLen[headerIndex]       \
        && strncasecmp(wsaioHttpHeader[headerIndex], headerValue, wsaioHttpHeaderLen[headerIndex]) == 0 \
        )

static int wsaioHttpHeaderContains(int headerIndex, char *str) {
    char    *s;
    int     len;

    len = strlen(str);
    s = wsaioHttpHeader[headerIndex];
    while (s != NULL && *s != 0) {
        if (strncasecmp(s, str, len) == 0) return(1);
        while (*s != '\n' && *s != ',' && *s != 0) s ++;
        if (*s != ',') return(0);
        s++;
        while (*s != 0 && isblank(*s)) s++ ;
    }
    return(0);
}

// parse http header. Only those header fields that we are interested in are tested
static void wsaioAddHttpHeaderField(char *name, int namelen, char *val, int vallen) {
    // check if this field is interesting for us and save it if yes
    if (name == NULL || namelen == 0) return;
    // printf("Adding header pair %.*s : %.*s\n", namelen, name, vallen, val);
    switch (name[0]) {
    case 'a':
    case 'A':
        WSAIO_HTTP_HEADER_SET2(name, namelen, val, vallen, Accept,Encoding);
        break;
    case 'c':
    case 'C':
        WSAIO_HTTP_HEADER_SET(name, namelen, val, vallen, Connection);
        WSAIO_HTTP_HEADER_SET2(name, namelen, val, vallen, Content,Length);
        break;
    case 'h':
    case 'H':
        WSAIO_HTTP_HEADER_SET(name, namelen, val, vallen, Host);
        break;
    case 'o':
    case 'O':
        WSAIO_HTTP_HEADER_SET(name, namelen, val, vallen, Origin);
        break;
    case 'u':
    case 'U':
        WSAIO_HTTP_HEADER_SET(name, namelen, val, vallen, Upgrade);
        break;
    case 's':
    case 'S':
        WSAIO_HTTP_HEADER_SET3(name, namelen, val, vallen, Sec,WebSocket,Key);
        WSAIO_HTTP_HEADER_SET3(name, namelen, val, vallen, Sec,WebSocket,Protocol);
        WSAIO_HTTP_HEADER_SET3(name, namelen, val, vallen, Sec,WebSocket,Version);
        break;
    }
}

static void wsaioOnWwwToWebsocketProtocolSwitchRequest(struct wsaio *ww, struct baio *bb, int requestHeaderLen, int contentLength, char *uri) {
    int             acceptKeyLen, keylen;
    char            acceptKey[JSVAR_TMP_STRING_SIZE];
    unsigned char   sha1hash[SHA1_DIGEST_LENGTH];
    char            ttt[JSVAR_TMP_STRING_SIZE];

    if (wsaioHttpHeader[WSAIO_HTTP_HEADER_Sec_WebSocket_Key] == NULL) goto wrongRequest;
    keylen = wsaioHttpHeaderLen[WSAIO_HTTP_HEADER_Sec_WebSocket_Key];
    if (keylen + sizeof(WSAIO_WEBSOCKET_GUID) >= JSVAR_TMP_STRING_SIZE-1) {
        printf("%s: %s:%d: Websocket key is too large.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
        goto wrongRequest;
    }
    strncpy(ttt, wsaioHttpHeader[WSAIO_HTTP_HEADER_Sec_WebSocket_Key], keylen);
    strcpy(ttt+keylen, WSAIO_WEBSOCKET_GUID);
	//printf("GOING TO HASH: %s\n", ttt);
    JSVAR_SHA1((unsigned char*)ttt, sha1hash);
    acceptKeyLen = base64_encode((char*)sha1hash, SHA1_DIGEST_LENGTH, acceptKey, sizeof(acceptKey));
	//printf("accept key ssl:   %s\n", acceptKey);

    // Generate websocket accept header
    baioPrintfMsg(bb, 
                  "HTTP/1.1 101 Switching Protocols\r\n"
                  "Upgrade: websocket\r\n"
                  "Connection: Upgrade\r\n"
                  "Sec-WebSocket-Accept: %s\r\n"
                  "Sec-WebSocket-Protocol: %.*s\r\n"
                  "Sec-WebSocket-Origin: %.*s\r\n"
                  "Sec-WebSocket-Location: %s://%.*s%s\r\n"
                  // "Sec-WebSocket-Extensions: permessage-deflate\r\n"
                  "\r\n"
                  ,
                  acceptKey,
                  wsaioHttpHeaderLen[WSAIO_HTTP_HEADER_Sec_WebSocket_Protocol],
                  wsaioHttpHeader[WSAIO_HTTP_HEADER_Sec_WebSocket_Protocol],
				  wsaioHttpHeaderLen[WSAIO_HTTP_HEADER_Origin],wsaioHttpHeader[WSAIO_HTTP_HEADER_Origin],
                  (ww->b.useSsl?"wss":"ws"), wsaioHttpHeaderLen[WSAIO_HTTP_HEADER_Host],wsaioHttpHeader[WSAIO_HTTP_HEADER_Host],uri
        );

    bb->readBuffer.i += requestHeaderLen+contentLength;
    ww->state = WSAIO_STATE_WEBSOCKET_ACTIVE;
    ww->previouslySeenWebsocketFragmentsOffset = 0;
    wsaioWebsocketStartNewMessage(ww);

	if (jsVarDebugLevel > 0) printf("%s: %s:%d: Switching protocol to websocket.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
    JSVAR_CALLBACK_CALL(ww->callBackOnWebsocketAccept, callBack(ww, uri));
    return;

wrongRequest:
    printf("%s: %s:%d: Closing connection: Wrong websocket protocol switch request.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
    baioClose(bb);
}

void wsaioHttpStartNewAnswer(struct wsaio *ww) {
    baioMsgStartNewMessage(&ww->b);
    baioMsgReserveSpace(&ww->b, ww->answerHeaderSpaceReserve);
    ww->fixedSizeAnswerHeaderLen = baioPrintfToBuffer(
        &ww->b,
        "Server: %s\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n"
        ,
        ww->serverName
        );
}

static void wsaioHttpFinishAnswerHeaderAndSendBuffer(struct wsaio *ww, char *statusCodeAndDescription, char *contentType, char *additionalHeaders, int chunkedFlag) {
    int         startIndex, contentLen, variableSizeAnswerHeaderLen;
    char        *ss, *dd;

    if ((ww->b.status & BAIO_STATUS_ACTIVE) == 0) return;

    startIndex = baioMsgLastStartIndex(&ww->b);
    ss = ww->b.writeBuffer.b + startIndex;
    contentLen = ww->b.writeBuffer.j - startIndex - ww->answerHeaderSpaceReserve - ww->fixedSizeAnswerHeaderLen;
    assert(contentLen >= 0);

    variableSizeAnswerHeaderLen = sprintf(
        ss,
        "HTTP/1.1 %s\r\n"
        "Content-type: %s\r\n"
        "%s"
        ,
        statusCodeAndDescription,
        contentType,
        (additionalHeaders==NULL?"":additionalHeaders)
        );

    if (chunkedFlag) {
        variableSizeAnswerHeaderLen += sprintf(ss + variableSizeAnswerHeaderLen, "Transfer-Encoding: chunked\r\n");
    } else {
        variableSizeAnswerHeaderLen += sprintf(ss + variableSizeAnswerHeaderLen, "Content-Length: %d\r\n", contentLen);
    }

    assert(variableSizeAnswerHeaderLen < ww->answerHeaderSpaceReserve);
    // move it to the right place
    dd = ss + ww->answerHeaderSpaceReserve - variableSizeAnswerHeaderLen;
    memmove(dd, ss, variableSizeAnswerHeaderLen);

    baioMsgResetStartIndexForNewMsgSize(&ww->b, variableSizeAnswerHeaderLen+ww->fixedSizeAnswerHeaderLen+contentLen);
    baioMsgSend(&ww->b);
}

void wsaioHttpFinishAnswer(struct wsaio *ww, char *statusCodeAndDescription, char *contentType, char *additionalHeaders) {
    wsaioHttpFinishAnswerHeaderAndSendBuffer(ww, statusCodeAndDescription, contentType, additionalHeaders, 0);
    // only when request was answered, we can start to process next request
    ww->b.readBuffer.i += ww->requestSize;
    ww->state = WSAIO_STATE_WAITING_FOR_WWW_REQUEST;
}

void wsaioHttpStartChunkInChunkedAnswer(struct wsaio *ww) {
    int     r;

    baioMsgStartNewMessage(&ww->b);
#if EDEBUG
    r = baioPrintfToBuffer(&ww->b, "1234567890");
#else
    r = baioMsgReserveSpace(&ww->b, WSAIO_CHUNKED_ANSWER_LENGTH_SPACE_RESERVE);
#endif
    JsVarInternalCheck(r == WSAIO_CHUNKED_ANSWER_LENGTH_SPACE_RESERVE);
}

void wsaioHttpFinishAnswerHeaderAndStartChunkedAnswer(struct wsaio *ww, char *statusCodeAndDescription, char *contentType, char *additionalHeaders) {
    wsaioHttpFinishAnswerHeaderAndSendBuffer(ww, statusCodeAndDescription, contentType, additionalHeaders, 1);
    wsaioHttpStartChunkInChunkedAnswer(ww);
}

void wsaioHttpFinalizeAndSendCurrentChunk(struct wsaio *ww, int finalChunkFlag) {
    int         chunkSize;
    int         startIndex, n, r;
    char        *ss, *dd;

    if ((ww->b.status & BAIO_STATUS_ACTIVE) == 0) return;

    startIndex = baioMsgLastStartIndex(&ww->b);
    chunkSize = ww->b.writeBuffer.j - startIndex - WSAIO_CHUNKED_ANSWER_LENGTH_SPACE_RESERVE;
    assert(chunkSize >= 0);

    // printf("chunkSize == %d && finalChunkFlag == %d\n",chunkSize, finalChunkFlag);

    if (chunkSize == 0 && finalChunkFlag == 0) return;

    // chunk terminates by CRLF
    r = baioWriteToBuffer(&ww->b, "\r\n", 2);
    // TODO: It may happen that there is not enough of space in write buffer for the CRLF
    JsVarInternalCheck(r == 2);

    // get start index one more time, because previous write could wrap the buffer
    startIndex = baioMsgLastStartIndex(&ww->b);
    ss = ww->b.writeBuffer.b + startIndex;

    // TODO: Simply print chunk size in reversed order up to (ss + WSAIO_CHUNKED_ANSWER_LENGTH_SPACE_RESERVE) 
    n = sprintf(ss, "%x\r\n", chunkSize);
    dd = ss + WSAIO_CHUNKED_ANSWER_LENGTH_SPACE_RESERVE - n;
    memmove(dd, ss, n);

    baioMsgResetStartIndexForNewMsgSize(&ww->b, chunkSize+n+2);
    baioMsgSend(&ww->b);

    if (chunkSize != 0) {
        if (finalChunkFlag) {
            r = baioPrintfMsg(&ww->b, "0\r\n\r\n");
            JsVarInternalCheck(r == 5);         
        } else {
            wsaioHttpStartChunkInChunkedAnswer(ww);
        }
    }
    if (finalChunkFlag) {
        // final one, we can start to process next request
        ww->b.readBuffer.i += ww->requestSize;  
        ww->state = WSAIO_STATE_WAITING_FOR_WWW_REQUEST;
    }
}

char *wsaioGetFileMimeType(char *fname) {
    char        *suffix;
    char        *mimetype;
	char		sss[JSVAR_TMP_STRING_SIZE];
	int			i;

    mimetype = "text/html";
    if (1 || fname != NULL) {
		suffix = strrchr(fname, '.');
		if (suffix != NULL) {
			// convert to lower cases
			for(i=0; suffix[i] != 0 && i<JSVAR_TMP_STRING_SIZE-1; i++) sss[i] = tolower(suffix[i]);
			sss[i] = 0;
			if (strcmp(sss, ".js") == 0) mimetype = "text/javascript";
			else if (strcmp(sss, ".css") == 0) mimetype = "text/css";
			else if (strcmp(sss, ".json") == 0) mimetype = "application/json";
			else if (strcmp(sss, ".pdf") == 0) mimetype = "application/pdf";
			else if (strcmp(sss, ".gif") == 0) mimetype = "image/gif";
			else if (strcmp(sss, ".jpeg") == 0) mimetype = "image/jpeg";
			else if (strcmp(sss, ".jpg") == 0) mimetype = "image/jpeg";
			else if (strcmp(sss, ".png") == 0) mimetype = "image/png";
			else if (strcmp(sss, ".svg") == 0) mimetype = "image/svg+xml";
			else if (strcmp(sss, ".tif") == 0) mimetype = "image/tiff";
			else if (strcmp(sss, ".tiff") == 0) mimetype = "image/tiff";
			else if (strcmp(sss, ".webp") == 0) mimetype = "image/webp";
		}
    }
    return(mimetype);
}

int wsaioHttpSendFile(struct wsaio *ww, char *fname) {
    FILE    *ff;
    char    b[WSAIO_SEND_FILE_BUFFER_SIZE];
    int     n, res;

    ff = fopen(fname, "r");
    if (ff == NULL) {
        wsaioHttpFinishAnswer(ww, "404 Not Found", "text/html", NULL);
        return(-1);
    }
    res = 0;
    while ((n = fread(b, 1, WSAIO_SEND_FILE_BUFFER_SIZE, ff)) > 0) res += baioWriteToBuffer(&ww->b, b, n);
    fclose(ff);
    wsaioHttpFinishAnswer(ww, "200 OK", wsaioGetFileMimeType(fname), NULL);
    return(res);
}

//////////
// assynchronous  file send

static int wsaioHttpSendFileAsyncCallBackOnError(struct baio *b) ;
static int wsaioHttpSendFileAsyncCallBackOnReadWrite(struct baio *b, int fromj, int n) ;

static void wsaioHttpSendFileAsyncRemoveCallbacks(struct baio *dest) {
    if (dest == NULL) return;
    jsVarCallBackRemoveFromHook(&dest->callBackOnWrite, (void *) wsaioHttpSendFileAsyncCallBackOnReadWrite);
    jsVarCallBackRemoveFromHook(&dest->callBackOnError, (void *) wsaioHttpSendFileAsyncCallBackOnError);
}

static int wsaioHttpSendFileAsyncCallBackOnError(struct baio *b) {
    struct baio *src, *dest;
    // printf("CALLBACK ERROR\n"); fflush(stdout);
    src = baioFromMagic(b->u[0].i);
    dest = baioFromMagic(b->u[1].i);

    if (src != NULL && src->fd >= 0) baioCloseFd(src);
    wsaioHttpSendFileAsyncRemoveCallbacks(dest);

    baioCloseMagic(b->u[0].i);
    baioCloseMagic(b->u[1].i);
    return(0);
}

static int wsaioHttpSendFileAsyncCallBackOnReadWrite(struct baio *b, int fromj, int num) {
    struct baio *src, *dest;
    int         n, srcsize, destsize, csize;

    // printf("%s: CALLBACK COPY: fd == %d\n", JSVAR_PRINT_PREFIX(), b->fd); fflush(stdout);

    src = baioFromMagic(b->u[0].i);
    dest = baioFromMagic(b->u[1].i);

    if (dest == NULL) return(wsaioHttpSendFileAsyncCallBackOnError(b));
    // if src == NULL, we are just pumping rest of write buffer, do nothing
    if (src == NULL) return(0);
    srcsize = src->readBuffer.j - src->readBuffer.i;
    destsize = baioPossibleSpaceForWrite(dest)-WSAIO_CHUNKED_ANSWER_LENGTH_SPACE_RESERVE-8;
    csize = JSVAR_MIN(srcsize, destsize);

    // printf("srcsize, destsize, csize == %d, %d, %d\n", srcsize, destsize, csize);

    if (csize > 0) {
        // copy csize bytes from src to dest
        n = baioWriteToBuffer(dest, src->readBuffer.b+src->readBuffer.i, csize);
        JsVarInternalCheck(n > 0);
        if (n > 0) {
            src->readBuffer.i += n;
            wsaioHttpFinalizeAndSendCurrentChunk((struct wsaio*)dest, 0);
        }
    }
    // test also destsize as we need to start the next chunk of a chunked answer
    if ((src->status & BAIO_STATUS_EOF_READ) && src->readBuffer.i == src->readBuffer.j) {
        // printf("%s: FINAL: fd == %d\n\n", JSVAR_PRINT_PREFIX(), b->fd);
		// Hmm. Why I am closing it twice here ?
        if (src->fd >= 0) {
			baioCloseFd(src);
			src->fd = -1;
		}
        baioCloseMagic(b->u[0].i);
        wsaioHttpSendFileAsyncRemoveCallbacks(dest);
        wsaioHttpFinalizeAndSendCurrentChunk((struct wsaio*)dest, 1);
    }
    return(0);
}

static int wsaioHttpForwardFromBaio(struct wsaio *ww, struct baio *bb, char *mimeType, char *additionalHeaders) {
    int                 cb1, cb2;

    baioReadBufferResize(&bb->readBuffer, WSAIO_SEND_FILE_BUFFER_SIZE, WSAIO_SEND_FILE_BUFFER_SIZE);

    jsVarCallBackAddToHook(&bb->callBackOnRead, (void *) wsaioHttpSendFileAsyncCallBackOnReadWrite);
    jsVarCallBackAddToHook(&bb->callBackOnEof, (void *) wsaioHttpSendFileAsyncCallBackOnReadWrite);
    jsVarCallBackAddToHook(&bb->callBackOnError, (void *) wsaioHttpSendFileAsyncCallBackOnError);
    cb1 = jsVarCallBackAddToHook(&ww->b.callBackOnWrite, (void *) wsaioHttpSendFileAsyncCallBackOnReadWrite);
    cb2 = jsVarCallBackAddToHook(&ww->b.callBackOnError, (void *) wsaioHttpSendFileAsyncCallBackOnError);
    bb->u[0].i = ww->b.u[0].i = bb->baioMagic;
    bb->u[1].i = ww->b.u[1].i = ww->b.baioMagic;

    wsaioHttpFinishAnswerHeaderAndStartChunkedAnswer(ww, "200 OK", mimeType, additionalHeaders);

    return(0);
}

int wsaioHttpForwardFd(struct wsaio *ww, int fd, char *mimeType, char *additionalHeaders) {
    struct baio         *bb;
    int                 r;

    bb = baioNewBasic(BAIO_TYPE_FD, BAIO_IO_DIRECTION_READ, 0);
    if (bb == NULL) return(-1);
    bb->fd = fd;
    r = wsaioHttpForwardFromBaio(ww, bb, mimeType, additionalHeaders);
    return(r);
}

int wsaioHttpForwardString(struct wsaio *ww, char *str, int strlen, char *mimeType) {
    struct baio         *bb;
    int                 cb1, cb2;

    bb = baioNewPseudoFile(str, strlen, 0);
    if (bb == NULL) return(-1);

    cb1 = jsVarCallBackAddToHook(&ww->b.callBackOnWrite, (void *) wsaioHttpSendFileAsyncCallBackOnReadWrite);
    cb2 = jsVarCallBackAddToHook(&ww->b.callBackOnError, (void *) wsaioHttpSendFileAsyncCallBackOnError);

    bb->u[0].i = ww->b.u[0].i = bb->baioMagic;
    bb->u[1].i = ww->b.u[1].i = ww->b.baioMagic;

    wsaioHttpFinishAnswerHeaderAndStartChunkedAnswer(ww, "200 OK", mimeType, NULL);
    return(0);
}

int wsaioHttpSendFileAsync(struct wsaio *ww, char *fname, char *additionalHeaders) {
    int             r;
    struct baio     *bb;

    bb = baioNewFile(fname, BAIO_IO_DIRECTION_READ, 0);
    // bb = baioNewPipedFile(fname, BAIO_IO_DIRECTION_READ, 0);
    // bb = baioNewSocketFile(fname, BAIO_IO_DIRECTION_READ, 0);
    if (bb == NULL) return(-1);
    // printf("%s: SendFileAsync %s: forwarding fd %d --> %d\n", JSVAR_PRINT_PREFIX(), fname, bb->fd, ww->b.fd);
    r = wsaioHttpForwardFromBaio(ww, bb, wsaioGetFileMimeType(fname), additionalHeaders);

    return(r);
}

////////////////////////////////////////////////////

void wsaioWebsocketCompleteFrame(struct wsaio *ww, int opcode) {
    char                    *hh;
    int                     msgIndex, msgSize;
    int                     headbyte;
    long long               payloadlen;
    struct baio             *bb;
    struct baioWriteBuffer  *b;

    bb = &ww->b;
    if ((bb->status & BAIO_STATUS_ACTIVE) == 0) return;
    
    b = &bb->writeBuffer;
    msgIndex = baioMsgLastStartIndex(bb);
    assert(msgIndex >= 0);
    hh = b->b + msgIndex;
    payloadlen = b->j - msgIndex - 10;

    // do not send empty messages
    if (payloadlen <= 0) return;

    // TODO: Maybe implement fragmented messages
    headbyte = (opcode | WSAIO_WEBSOCKET_FIN_MSK);

	// printf("msgIndex, b->j == %d, %d, payloadlen == %lld\n", msgIndex, b->j, payloadlen); fflush(stdout);
    if (payloadlen < 126) {
        msgSize = payloadlen+2;
        hh += 8;
        *(hh++) = headbyte;
        *(hh++) = 0x00 | payloadlen;            
    } else if (payloadlen < (1<<16)) {
        msgSize = payloadlen+4;
        hh += 6;
        *(hh++) = headbyte;
        *(hh++) = 0x00 | 126;
        *(hh++) = (payloadlen >> 8) & 0xff;
        *(hh++) = (payloadlen) & 0xff;
    } else {
        msgSize = payloadlen+10;
        *(hh++) = headbyte;
        *(hh++) = 0x00 | 127;
        *(hh++) = (payloadlen >> 56) & 0xff;
        *(hh++) = (payloadlen >> 48) & 0xff;
        *(hh++) = (payloadlen >> 40) & 0xff;
        *(hh++) = (payloadlen >> 32) & 0xff;
        *(hh++) = (payloadlen >> 24) & 0xff;
        *(hh++) = (payloadlen >> 16) & 0xff;
        *(hh++) = (payloadlen >> 8) & 0xff;
        *(hh++) = (payloadlen) & 0xff;
    }
    baioMsgResetStartIndexForNewMsgSize(bb, msgSize);
    baioMsgSend(bb);
    // baioLastMsgDump(&ww->b);
    wsaioWebsocketStartNewMessage(ww);
}

void wsaioWebsocketCompleteMessage(struct wsaio *ww) {
    wsaioWebsocketCompleteFrame(ww, WSAIO_WEBSOCKET_OP_CODE_TEXT);
}

int wsaioWebsocketVprintf(struct wsaio *ww, char *fmt, va_list arg_ptr) {
    int res;
    res = baioVprintfToBuffer(&ww->b, fmt, arg_ptr);
    wsaioWebsocketCompleteMessage(ww);
    return(res);
}

int wsaioWebsocketPrintf(struct wsaio *ww, char *fmt, ...) {
    int             res;
    va_list         arg_ptr;

    va_start(arg_ptr, fmt);
    res = wsaioWebsocketVprintf(ww, fmt, arg_ptr);
    va_end(arg_ptr);
    return(res);
}

//////////
enum wsaioRequestTypeEnum {
	WSAIO_RQT_NONE,
	WSAIO_RQT_GET,
	WSAIO_RQT_POST,
	WSAIO_RQT_MAX,
};


static void wsaioOnWwwRequest(struct wsaio *ww, struct baio *bb, int requestHeaderLen, int contentLength, char *uri, int requestType) {
    int     r;
    int     maximalAcceptableContentLength;

    ww->currentRequest.postQuery = bb->readBuffer.b + bb->readBuffer.i + requestHeaderLen;
    ww->requestSize = requestHeaderLen + contentLength;
    ww->state = WSAIO_STATE_WAITING_FOR_WWW_REQUEST;
    wsaioHttpStartNewAnswer(ww);

    r = 0;
	if (requestType == WSAIO_RQT_GET) {
		JSVAR_CALLBACK_CALL(ww->callBackOnWwwGetRequest, (r = callBack(ww, uri)));
	} else if (requestType == WSAIO_RQT_POST) {
		JSVAR_CALLBACK_CALL(ww->callBackOnWwwPostRequest, (r = callBack(ww, uri)));
	} else {
		printf("%s: %s:%d: Web: Wrong requestType %d.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, requestType);
		goto wrongRequest;	
	}

    // finalize answer
    return;

wrongRequest:
    printf("%s: %s:%d: Closing connection.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
    baioClose(bb);
}

static void wsaioOnWwwRead(struct wsaio *ww) {
    struct baio     *bb;
    int             requestHeaderLen, contentLength, maximalAcceptableContentLength;
    char            *s, *hend, *hstart, *method, *httpVersion;
    char            *fieldName, *fieldValue;
    char            *uristr, *query;
    int             i, uriLen, fieldNameLen, fieldValueLen;

    bb = &ww->b;

    assert(bb->readBuffer.j < bb->readBuffer.size);
    bb->readBuffer.b[bb->readBuffer.j] = 0;

    hstart = bb->readBuffer.b + bb->readBuffer.i;
    hend = wsaioStrFindTwoNewlines(hstart);

	// printf("READ %.*s\n",  bb->readBuffer.j - bb->readBuffer.i, hstart); fflush(stdout);

    // if I do not have whole header, wait until I have
    if (hend == NULL) return;

    if (ww->state != WSAIO_STATE_WAITING_FOR_WWW_REQUEST) {
        assert(0 && "Delayed answer to requests are not yet implemented");
    }
    // printf("GOT HEADER\n"); fflush(stdout);

    // parsing of HTTP header 
    // clean previous http header values
    for(i=0; i<WSAIO_HTTP_HEADER_MAX; i++) {
        wsaioHttpHeader[i] = NULL;
        wsaioHttpHeaderLen[i] = 0;
    }
    // skip any initial newlines (I allow lines terminated by single \n instead of \r\n)
    s = hstart;
    while (*s == '\r' || *s == '\n') s ++;
    method = s;
    while (s < hend && *s != ' ' && *s != '\n') s++;
    if (*s != ' ') goto wrongRequest;
    s++;
    uristr = s;
    while (s < hend && *s != ' '&& *s != '\n') s++;
    if (*s != ' ') goto wrongRequest;
    uriLen = s - uristr;
    s++;
    httpVersion = s;
    while (s < hend && *s != '\n') s++;
    s++;
    // parse HTTP header fields
    while (! ((s[0] == '\r' && s[1] == '\n') || (s[0] == '\n'))) {
        // skip blanks
        while (*s == '\r' || *s == '\n' || *s == ' ' || *s == '\t') s++;
        fieldName = s;
        while (*s != ':' && *s != '\n') s++;
        if (*s == ':') {
            fieldNameLen = s - fieldName;
            while (fieldNameLen>0 && isblank(fieldName[fieldNameLen-1])) fieldNameLen--;
            s ++;
            // skip blank, maybe I shall relax from the (obsolete part of) standard allowing multiline header values?
            while (s[0] == ' ' 
                   || s[0] == '\t' 
                   || (s[0] == '\r' && s[1] == '\n' && (s[2] == ' ' || s[2] == '\t'))
                   || (s[0] == '\n' && (s[1] == ' ' || s[1] == '\t'))
                ) {
                s ++;
            }
            fieldValue = s;
            while (! ((s[0] == '\r' && s[1] == '\n' && s[2] != ' ' && s[2] != '\t') || (s[0] == '\n' && s[1] != ' ' && s[1] != '\t'))) s ++;
            fieldValueLen = s - fieldValue;
            while (fieldValueLen>0 && isblank(fieldValue[fieldValueLen-1])) fieldValueLen--;
            s += 2;
            wsaioAddHttpHeaderField(fieldName, fieldNameLen, fieldValue, fieldValueLen);
        }
    }
    while (*s != '\n') s++;
    s++;
    requestHeaderLen = s - hstart;

    // for the moment no chunked connection, content length required
    contentLength = 0;
    if (wsaioHttpHeader[WSAIO_HTTP_HEADER_Content_Length] != NULL) {
        contentLength = atoi(wsaioHttpHeader[WSAIO_HTTP_HEADER_Content_Length]);
        maximalAcceptableContentLength = bb->maxReadBufferSize - requestHeaderLen - bb->minFreeSizeAtEndOfReadBuffer - bb->minFreeSizeBeforeRead-1;
		// printf("MAX %d\n", maximalAcceptableContentLength);
        if (contentLength < 0 || contentLength > maximalAcceptableContentLength) {
            printf("%s: %s:%d: Web: Wrong or too large content length %d.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, contentLength);
            goto wrongRequest;
        }
    }

	// if we do not have whole request, do nothing
	if (bb->readBuffer.i + requestHeaderLen + contentLength > bb->readBuffer.j) return;

    uristr[uriLen] = 0;
    query = strchr(uristr, '?');
    if (query == NULL) {
        query = "";
    } else {
        *query = 0;
        query ++;
    }
    ww->currentRequest.getQuery = query;
    ww->currentRequest.postQuery = "";      // will be set later
    wsaioUriDecode(uristr);

    // only GET and POST method is accepted for the moment
    if (strncmp(method, "GET", 3) == 0) {
		if (wsaioHttpHeaderContains(WSAIO_HTTP_HEADER_Upgrade, "websocket") 
			&& wsaioHttpHeaderContains(WSAIO_HTTP_HEADER_Connection, "Upgrade")) {
			wsaioOnWwwToWebsocketProtocolSwitchRequest(ww, bb, requestHeaderLen, contentLength, uristr);
		} else {
			wsaioOnWwwRequest(ww, bb, requestHeaderLen, contentLength, uristr, WSAIO_RQT_GET);
		}
	} else if (strncmp(method, "POST", 4) == 0) {
		wsaioOnWwwRequest(ww, bb, requestHeaderLen, contentLength, uristr, WSAIO_RQT_POST);
	} else {
		goto wrongRequest;
	}

    return;

wrongRequest:
    printf("%s: %s:%d: Closing connection: Wrong request: %.*s\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, (int)(hend-hstart), hstart);
    baioClose(bb);
}

static int wsaioCompactCompletedFragmentedWebsocketMessage(struct wsaio *ww, int *opcodeOut, unsigned char **payloadOut, long long *payloadLenOut) {
    struct baio         *bb;
    unsigned char       *s, *msgStart, *payload;
    int                 fin, masked, opcode, firstFrameFlag;
    unsigned            b;
    long long           payloadLen, payloadLenTotal;

    bb = &ww->b;
    firstFrameFlag = 1;
    fin = 0;
    s = (unsigned char *)bb->readBuffer.b + bb->readBuffer.i;
    while(fin == 0) {
        b = *s++;
        fin = b & WSAIO_WEBSOCKET_FIN_MSK;
        opcode = b & WSAIO_WEBSOCKET_OP_CODE_MSK;

        b = *s++;
        masked = b & WSAIO_WEBSOCKET_MASK_MSK;
        payloadLen = b & WSAIO_WEBSOCKET_PLEN_MSK;
        if (payloadLen == 126) {
            payloadLen = (s[0] << 8) + s[1];
            s += 2;
        } else if (payloadLen == 127) {
            payloadLen = (((((((((((((((long long)s[0]<<8)+s[1])<<8)+s[2])<<8)+s[3])<<8)+s[4])<<8)+s[5])<<8)+s[6])<<8)+s[7]);
            s += 8;
        }

        if (masked) s += 4;

        if (firstFrameFlag) {
            if (opcode !=  WSAIO_WEBSOCKET_OP_CODE_TEXT) return(-1);
            if (opcodeOut != NULL) *opcodeOut = opcode;
            payload = s;
            payloadLenTotal = payloadLen;
        } else {
            if (opcode ==  WSAIO_WEBSOCKET_OP_CODE_CONTINUATION) {
                memmove(payload+payloadLenTotal, s, payloadLen);
                payloadLenTotal += payloadLen;
            }
        }
        s += payloadLen;
        firstFrameFlag = 0;
    };
    if (payloadOut != NULL) *payloadOut = payload;
    if (payloadLenOut != NULL) *payloadLenOut = payloadLenTotal;
    return(0);
}

static void wsaioOnWsRead(struct wsaio *ww) {
    struct baio         *bb;
    unsigned            b;
    long long           payloadLen;
    unsigned char       *mask;
    int                 availableSize;
    int                 maximalPayloadLen;
    int                 r, masked, opcode, fin, i;
    unsigned char       *s, *msgStart, *payload;

    bb = &ww->b;
    // printf("WSREAD %.*s\n",  bb->readBuffer.j - bb->readBuffer.i, bb->readBuffer.b + bb->readBuffer.i); fflush(stdout);
    for(;;) {
    continueWithNextFrame:
        // we need whole message in our buffer, so there is a maximum for payload length
        maximalPayloadLen = bb->maxReadBufferSize - bb->minFreeSizeAtEndOfReadBuffer - bb->minFreeSizeBeforeRead - ww->previouslySeenWebsocketFragmentsOffset - 11;
        // parse websockets frames
        // we need complete frame header
        availableSize = bb->readBuffer.j - bb->readBuffer.i - ww->previouslySeenWebsocketFragmentsOffset;
        if (availableSize < 2) return;

        s = msgStart = (unsigned char *)bb->readBuffer.b + bb->readBuffer.i + ww->previouslySeenWebsocketFragmentsOffset;
        b = *s++;
        fin = b & WSAIO_WEBSOCKET_FIN_MSK;
        opcode = b & WSAIO_WEBSOCKET_OP_CODE_MSK;

        b = *s++;
        masked = b & WSAIO_WEBSOCKET_MASK_MSK;
        payloadLen = b & WSAIO_WEBSOCKET_PLEN_MSK;
        if (payloadLen == 126) {
            if (availableSize < 4) return;
            payloadLen = (s[0] << 8) + s[1];
            s += 2;
        } else if (payloadLen == 127) {
            if (availableSize < 10) return;
            payloadLen = (((((((((((((((long long)s[0]<<8)+s[1])<<8)+s[2])<<8)+s[3])<<8)+s[4])<<8)+s[5])<<8)+s[6])<<8)+s[7]);
            s += 8;
        }
        if (payloadLen >= maximalPayloadLen) {
            printf("%s: %s:%d: Websockets message too long (try to increase maxReadBufferSize). Closing connection\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
            // This may mean we are lost in websockets frames, close the connection
            baioCloseOnError(bb);
            return;
        }

        mask = NULL;
        if (masked) {
            // According to the standard, client shall always mask its data!
            mask = s;
            s += 4;
        }
        // we are waiting for the whole frame before demasking it
        if (availableSize < s - msgStart + payloadLen) return;

        payload = s;
        if (masked) {
            // demask payload
            for(i=0; i<payloadLen; i++) payload[i] ^= mask[i%4];
        }
        s += payloadLen;

        // printf("%s: %s:%d: Info: opcode fin == %d, %d.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, opcode, fin);
        if (fin == 0) {
            // printf("%s: %s:%d: Info: received a fragmented websocket msg.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
            ww->previouslySeenWebsocketFragmentsOffset += s - msgStart;
            goto continueWithNextFrame;
        } else if (ww->previouslySeenWebsocketFragmentsOffset != 0) {
            // this may be a control frame within fragmented message or a final frame of a fragmented message
            ww->previouslySeenWebsocketFragmentsOffset += s - msgStart;
            if (opcode == WSAIO_WEBSOCKET_OP_CODE_CONTINUATION) {
                // printf("%s: %s:%d: Info: received last frame of a fragmented websocket msg.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
                wsaioCompactCompletedFragmentedWebsocketMessage(ww, &opcode, &payload, &payloadLen);
            }
        }

        if (opcode ==  WSAIO_WEBSOCKET_OP_CODE_TEXT) {
            r = 0;
            // JSVAR_CALLBACK_CALL(ww->callBackOnWebsocketGetMessage, (r = callBack(ww, bb->readBuffer.i+(payload-msgStart), payloadLen)));
            JSVAR_CALLBACK_CALL(ww->callBackOnWebsocketGetMessage, (r = callBack(ww, (char*)payload - bb->readBuffer.b, payloadLen)));
        } else if (opcode == WSAIO_WEBSOCKET_OP_CODE_CLOSED) {
            if (jsVarDebugLevel > 0) printf("%s: Websocket connection closed by remote host.\n", JSVAR_PRINT_PREFIX());
            baioClose(bb);
            return;
        } else if (opcode == WSAIO_WEBSOCKET_OP_CODE_PING) {
            // ping, answer with pong frame with the same payload as ping
            // printf("%s: %s:%d: Warning: a websocket ping received. Not yet implemented\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
            baioWriteToBuffer(&ww->b, (char*)payload, payloadLen);
            wsaioWebsocketCompleteFrame(ww, WSAIO_WEBSOCKET_OP_CODE_PONG);
            // a ping request within fragmented message
            if (ww->previouslySeenWebsocketFragmentsOffset != 0) goto continueWithNextFrame;
        } else if (opcode == WSAIO_WEBSOCKET_OP_CODE_PONG) {
            // pong, ignore
            printf("%s: %s:%d: Warning: a websocket pong received. Not yet implemented\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
            // a pong within fragmented message
            if (ww->previouslySeenWebsocketFragmentsOffset != 0) goto continueWithNextFrame;
        } else {
            printf("%s: %s:%d: Warning a non textual frame with opcode == %d received. Not yet implemented\n", 
                   JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, opcode);
            // This may mean we are lost in websockets frames, close the connection
            baioCloseOnError(bb);
            return;
        }

        // the whole message is processed whether it was fragmented or not
        bb->readBuffer.i = ((char*)s - bb->readBuffer.b);
        ww->previouslySeenWebsocketFragmentsOffset = 0;
    }
}

///////////////////////////////////////////////////////////////

static int wsaioOnBaioRead(struct baio *bb, int fromj, int n) {
    struct wsaio *ww;

    ww = (struct wsaio *) bb;

    // We do not process new read requests unless having some space available in write buffer (to write headers, etc).
    if (baioPossibleSpaceForWrite(bb) < ww->minSpaceInWriteBufferToProcessRequest) {
        if (bb->maxWriteBufferSize < ww->minSpaceInWriteBufferToProcessRequest * 2) {
            static uint8_t warningIssued = 0;
            if (warningIssued == 0) {
                printf("%s: %s:%d: Warning: maxWriteBufferSize is less than 2*minSpaceInWriteBufferToProcessReques. The web server may have trouble answering requests.\n",
                       JSVAR_PRINT_PREFIX(), __FILE__, __LINE__
                    );
                warningIssued = 1;
            }
        }
        return(0);
    }

    switch (ww->state) {
    case WSAIO_STATE_WAITING_FOR_WWW_REQUEST:
        wsaioOnWwwRead(ww);
        break;
    case WSAIO_STATE_WEBSOCKET_ACTIVE:
        wsaioOnWsRead(ww);
        break;
    default:
        printf("%s: %s:%d: Internal error. Unknown wsaio state %d\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, ww->state);
    }
    return(0);
}

static int wsaioOnBaioWrite(struct baio *bb, int fromi, int n) {
    struct wsaio *ww;
    ww = (struct wsaio *) bb;
    // Hmm. check that it is actually websocket send
    // if (ww->callBackOnWebsocketWrite != NULL) return(ww->callBackOnWebsocketWrite(ww, fromi, n));
    // if there is unprocessed read, maybe it was waiting for some space for write, we shall process it
    if (bb->readBuffer.i != bb->readBuffer.j) wsaioOnBaioRead(bb, bb->readBuffer.j, 0);
    return(0);
}

static int wsaioOnBaioDelete(struct baio *bb) {
    struct wsaio *ww;

    // printf("wsaioOnBaioDelete: bb==%p, bb->writeBuffer.b == %p\n", bb, bb->writeBuffer.b); fflush(stdout);

    ww = (struct wsaio *) bb;
    JSVAR_CALLBACK_CALL(ww->callBackOnDelete, callBack(ww));

    // free my hooks
    jsVarCallBackFreeHook(&ww->callBackAcceptFilter);
    jsVarCallBackFreeHook(&ww->callBackOnAccept);
    jsVarCallBackFreeHook(&ww->callBackOnWwwGetRequest);
    jsVarCallBackFreeHook(&ww->callBackOnWwwPostRequest);
    jsVarCallBackFreeHook(&ww->callBackOnWebsocketAccept);
    jsVarCallBackFreeHook(&ww->callBackOnWebsocketGetMessage);
    jsVarCallBackFreeHook(&ww->callBackOnWebsocketSend);
    jsVarCallBackFreeHook(&ww->callBackOnDelete);

    return(0);
}

static int wsaioBaioAcceptFilter(struct baio *bb, unsigned ip, unsigned port) {
    struct wsaio    *ww;
    int             r;

    ww = (struct wsaio *) bb;
    r = 0;
    JSVAR_CALLBACK_CALL(ww->callBackAcceptFilter, (r = callBack(ww, ip, port)));
    return(r);
}

static int wsaioOnBaioAccept(struct baio *bb) {
    struct wsaio *ww;
    ww = (struct wsaio *) bb;

    JSVAR_CALLBACK_CALL(ww->callBackOnAccept, callBack(ww));
    return(0);
}

static int wsaioOnBaioTcpIpAccept(struct baio *bb) {
    struct wsaio *ww;
    ww = (struct wsaio *) bb;

    jsVarCallBackCloneHook(&ww->callBackAcceptFilter, NULL);
    jsVarCallBackCloneHook(&ww->callBackOnTcpIpAccept, NULL);
    jsVarCallBackCloneHook(&ww->callBackOnAccept, NULL);
    jsVarCallBackCloneHook(&ww->callBackOnWwwGetRequest, NULL);
    jsVarCallBackCloneHook(&ww->callBackOnWwwPostRequest, NULL);
    jsVarCallBackCloneHook(&ww->callBackOnWebsocketAccept, NULL);
    jsVarCallBackCloneHook(&ww->callBackOnWebsocketGetMessage, NULL);
    jsVarCallBackCloneHook(&ww->callBackOnWebsocketSend, NULL);
    jsVarCallBackCloneHook(&ww->callBackOnDelete, NULL);

    JSVAR_CALLBACK_CALL(ww->callBackOnTcpIpAccept, callBack(ww));

    return(0);
}

struct wsaio *wsaioNewServer(int port, enum baioSslFlags sslFlag, int additionalSpaceToAllocate) {
    struct wsaio    *ww;
    struct baio     *bb;

    // casting pointer to the first element of struct is granted in ANSI C.
    bb = baioNewTcpipServer(port, sslFlag, sizeof(struct wsaio) - sizeof(struct baio) + additionalSpaceToAllocate);
    if (bb == NULL) return(NULL);

    ww = (struct wsaio *) bb;
    ww->wsaioSecurityCheckCode = 0xcacebaf;
    ww->state = WSAIO_STATE_WAITING_FOR_WWW_REQUEST;
    ww->serverName = "myserver.com";
    ww->answerHeaderSpaceReserve = 256;
    ww->minSpaceInWriteBufferToProcessRequest = ww->answerHeaderSpaceReserve + 512;

    bb->minFreeSizeAtEndOfReadBuffer = 1;
    jsVarCallBackAddToHook(&bb->callBackAcceptFilter, (void *) wsaioBaioAcceptFilter);
    jsVarCallBackAddToHook(&bb->callBackOnTcpIpAccept, (void *) wsaioOnBaioTcpIpAccept);
    jsVarCallBackAddToHook(&bb->callBackOnAccept, (void *) wsaioOnBaioAccept);
    jsVarCallBackAddToHook(&bb->callBackOnRead, (void *) wsaioOnBaioRead);
    jsVarCallBackAddToHook(&bb->callBackOnWrite, (void *) wsaioOnBaioWrite);
    jsVarCallBackAddToHook(&bb->callBackOnDelete, (void *) wsaioOnBaioDelete);
    return(ww);
}

// This is supposed to retrieve a few short values. Would be slow for large environments
char *jsVarGetEnvPtr(char *env, char *key) {
    int mn, n;
    char *s, kk[JSVAR_TMP_STRING_SIZE];
    mn = JSVAR_TMP_STRING_SIZE-5;
    n = strlen(key);
    if (n >= mn) n = mn;
    kk[0] = '&';
    strncpy(kk+1, key, n);
    kk[n+1] = '=';
    kk[n+2] = 0;

    // printf("looking for '%s' in '%s'\n", kk, env);

    if (strncmp(env, kk+1, n+1) == 0) {
        s = env+n+1;
    } else {
        s = strstr(env, kk);
        if (s == NULL) return(NULL);
        s += n+2;
    }
    return(s);
}

char *jsVarGetEnv(char *env, char *key, char *dst, int dstlen) {
    char            *s, *d;
    int             i;

    if (env == NULL || key == NULL || dst == NULL) return(NULL);

	dst[0] = 0;
    s = jsVarGetEnvPtr(env, key);
    if (s==NULL) return(NULL);

    for(d=dst,i=0; *s && *s!='&' && i<dstlen-1; d++,s++,i++) *d = *s;
    *d = 0;

    wsaioUriDecode(dst);
    return(dst);
}

struct jsVarDynamicString *jsVarGetEnvDstr(char *env, char *key) {
    struct jsVarDynamicString   *res;
    char                        *s, *d;

    if (env == NULL || key == NULL) return(NULL);
    s = jsVarGetEnvPtr(env, key);
    if (s==NULL) return(NULL);
    res = jsVarDstrCreate();
    for(; *s && *s!='&'; s++) {
        jsVarDstrAddCharacter(res, *s);
    }
    jsVarDstrAddCharacter(res, 0);
    d = wsaioUriDecode(res->s);
    jsVarDstrTruncateToSize(res, d - res->s);
    return(res);
}

char *jsVarGetEnv_st(char *env, char *key) {
    char            *res;

    if (env == NULL || key == NULL) return(NULL);
    res = baioStaticRingGetTemporaryStringPtr();
    res = jsVarGetEnv(env, key, res, JSVAR_TMP_STRING_SIZE);
    return(res);
}

int jsVarGetEnvInt(char *env, char *key, int defaultValue) {
    char *v;
    v = jsVarGetEnv_st(env, key);
    if (v == NULL) return(defaultValue);
    return(atoi(v));
}

double jsVarGetEnvDouble(char *env, char *key, double defaultValue) {
    char *v;
    v = jsVarGetEnv_st(env, key);
    if (v == NULL) return(defaultValue);
    return(strtod(v, NULL));
}


















/////////////////////////////////////////////////////////////////////////////////////////
// jsvar stands for javascript variables. 
// synchronization between C and Javascript variables

#define JSVAR_AVERAGE_VARS_PER_ADDRESS_HASH_TABLE_CELL  4
#define JSVAR_ADDRESS_HASH_FUN(p)                       ((uintptr_t)(p) / 8)

// an index value used for all indexes (must be less than zero)
#define JSVAR_INDEX_ALL                     -1

#define JSVAR_BAIO_MAGIC_ALL                0

// following is getting value of user's variable which is an array element 
// it basically takes pointer first element and increases it by index * arrayElementSize
// for this the pointer has to be converted to char * and then back to the original type

// Message type. This is only for C -> javascript messages
#define JSVAR_MSG_V_TYPE_CODE_DOUBLE        'f'
#define JSVAR_MSG_V_TYPE_CODE_NUMERIC       'i'
#define JSVAR_MSG_V_TYPE_CODE_STRING        's'
#define JSVAR_MSG_V_TYPE_CODE_EVAL          '^'

#define JSVAR_INDEX_FILE_NAME               "index.html"

//////////////////

static int jsVarComparator(struct jsVar *o1, struct jsVar *o2) {
    return(strcmp(o1->jsname, o2->jsname));
}

static int jsVarWatchSubcomparator(struct jsVar *o1, struct jsVar *o2) {
    int n, res;
    n = o1->jsnameLength;
    JsVarInternalCheck(n == strlen(o1->jsname));
    res = strncmp(o1->jsname, o2->jsname, n-1);
    // printf("subcomp %s %s --> %d\n", o1->jsname, o2->jsname, res);
    return(res);
}

static int jsVarPrefixSubcomparator(struct jsVar *o1, struct jsVar *o2) {
    int n, res;
    n = o1->jsnameLength;
    JsVarInternalCheck(n == strlen(o1->jsname));
    res = strncmp(o1->jsname, o2->jsname, n);
    return(res);
}


// rbtree (or other fully ordered) data structure is important
// It ensures that shorter strings are updated before longer,
// for example obj.x is updated before obj.x.i
typedef struct jsVar jsVar;
SGLIB_DEFINE_RBTREE_PROTOTYPES(jsVar, leftSubtree, rightSubtree, color, jsVarComparator);
SGLIB_DEFINE_RBTREE_FUNCTIONS(jsVar, leftSubtree, rightSubtree, color, jsVarComparator);

static int jsVarAddressComparator(struct jsVar *o1, struct jsVar *o2) {
    if ((char*)o1->valptr < (char*)o2->valptr) return(-1);
    if ((char*)o1->valptr > (char*)o2->valptr) return(1);
    return(0);
}

typedef struct jsVar jsVarl;
SGLIB_DEFINE_DL_LIST_PROTOTYPES(jsVarl, jsVarAddressComparator, previousInAddressHash, nextInAddressHash);
SGLIB_DEFINE_DL_LIST_FUNCTIONS(jsVarl, jsVarAddressComparator, previousInAddressHash, nextInAddressHash);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// We have some global static data

static struct jsVarCompressionData  *jsVarCompressionGlobal;

// redblack tree indexed by name
// struct jsVar *jsVars = NULL;
// static struct jsVar jsVarsUpdatedListFirstElementSentinel = {0, };
// static struct jsVar *jsVarsUpdatedList = &jsVarsUpdatedListFirstElementSentinel;


// hash table indexed by address (initialize to 1 element table to avoid module zero is find is called on empty table)
// Hmm. Maybe all this stuff with address binding is useless in our case.
static struct jsVar *jsVarAddressTableInitTable = NULL;
static struct jsVarAddressTable jsVarsAddr = {&jsVarAddressTableInitTable, 0, 1};

// This is initialization for the global VariableSpace, review it if changing "struct jsVarSpace" or "struct jsVar"!!!
static int jsVarSpaceTabIndex = 1;
#if 0
static struct jsVarSpace jsVarSpaceTab[JSVAR_MAX_VARSPACES] = {
    {
        .jsVarSpaceMagic = 17*JSVAR_MAX_VARSPACES+0, 
        .jsVars = NULL, 
        .jsVarsUpdatedListFirstElementSentinel = {
            .varSpace = &jsVarSpaceTab[0],
        }, 
        .jsVarsUpdatedList = &jsVarSpaceTab[0].jsVarsUpdatedListFirstElementSentinel,
    },
    {0,},
};
#else
static struct jsVarSpace jsVarSpaceTab[JSVAR_MAX_VARSPACES] = {
    {
        17*JSVAR_MAX_VARSPACES+0, 
        NULL, 
        { 0, 0, &jsVarSpaceTab[0], "defaultVarSpaceSentinel", sizeof("defaultVarSpaceSentinel")-1, }, 
        &jsVarSpaceTab[0].jsVarsUpdatedListFirstElementSentinel
    },
    {0,},
};
#endif

// exported global data space (used for dictionaries and as default input space)
struct jsVarSpace *jsVarSpaceGlobal = &jsVarSpaceTab[0];
// exported debug level
int jsVarDebugLevel = 1;


///////////////////////////////////////////////////////////////////////////////////////////////////////////
// forward declaration
static void jsVarAddressTableAdd(struct jsVarAddressTable *tt, struct jsVar *v);
static int jsVarIsValidWatchType(int varType) ;

///////////////////////////

#if 0
static char *jsStrnDuplicate(char *s, int len) {
    char    *res;

    if (s == NULL) return(NULL);
    ALLOCC(res, len+1, char);
    strncpy(res, s, len);
    res[len]=0;
    return(res);
}
#endif

static char *jsStrDuplicateAlwaysAlloc(char *s) {
    char    *res;
    int     len;

    if (s == NULL) s = "";
    len = strlen(s);
    JSVAR_ALLOCC(res, len+1, char);
    strcpy(res, s);
    return(res);
}

static wchar_t *jsWstrDuplicateAlwaysAlloc(wchar_t *s) {
    wchar_t     *res;
    int         len;

    if (s == NULL) s = L"";
    len = wcslen(s);
    JSVAR_ALLOCC(res, len+1, wchar_t);
    wcscpy(res, s);
    return(res);
}

static int jsVarEncodeNumber(uint64_t n, int negativeFlag, char *dst) {
    char        *res, *s, *send;
    int         i, len, reslen;
    uint64_t    a;

    if (negativeFlag) {
        JsVarInternalCheck((n & (1ULL<<63)) != 0);
#if EDEBUG
        assert((n & (1ULL<<63)) != 0);
#endif
        n = (~n) + 1; // n = -n;
        JsVarInternalCheck((n & (1ULL<<63)) == 0);
    }

    res = baioStaticRingGetTemporaryStringPtr();
    s = send = res + JSVAR_TMP_STRING_SIZE/2;
    *s = 0;
    a = n;
    do {
        *--s = a % 128;
        a = a / 128;
    } while (a != 0);
    len = send - s - 1;
    // case len == 0  && sign > 0 is a special case (short encoding), because it is the usual case
    if (n < 64 && negativeFlag == 0) {
        // 1 byte encoding, do nothing
        *s += 0;
    } else if (n < 128 && negativeFlag == 0) {
        // 2 bytes encoding (only one byte written so far), write the second one
        *--s = 64;
    } else if (n < 4096 && negativeFlag == 0) {
        // 2 bytes encoding both bytes written, adjust leading byte to indicate 2 bytes encoding
        *s += 64;
    } else {
        // long encoding, the number starts by a leading byte storing length and sign only. The absolute
        // value of the number starts by the second byte.
        if (negativeFlag == 0) {
            *--s = 96 + len;
        } else {
            *--s = 96 + 16 + len;
        }
    }
    // if the string starts with 127 this can be a problem ,because 127 is considered as end of message
    JsVarInternalCheck(*s != 127);
    reslen = send - s;
    for(i=0; i<reslen; i++) dst[i] = s[i];
    // printf("Encoding %c %"PRIu64" to '%s' --> ", (negativeFlag?'-':'+'), n, s); for(i=0; i<reslen; i++) printf("%d ", ((unsigned char*)s)[i]); printf("\n");
    return(reslen);
}

char *jsVarDecodeNumber(char *ss, int msglen, int64_t *res) {
    unsigned char *msg;
    int c, c2, i, sign, len, imax, n;

    msg = (unsigned char *) ss;
    i = 0;
    c = msg[i];
    if (c < 64) {
        // 1 byte encoding
        *res = c;
        return(ss+1);
    } else if (c < 96) {
        // 2 bytes encoding
        i ++;
        if (i >= msglen) return(NULL);
        c2 = msg[i];
        *res = (c - 64) * 128 + c2;
        return(ss+2);
    }
    // long encoding
    sign = 1;
    len = c - 96;
    if (len >= 16) {
        // negative number
        sign = -1;
        len -= 16;
    }
    i++;
    imax = i + len + 1;
    if (imax >= msglen) return(NULL);
    n = 0;
    while (i<imax) {
        c = msg[i];
        n = n * 128 + c;
        i++;
    }
    *res = n;
    return(ss+i);
}

static void *jsVarGetValueAddress(struct jsVar *v) {
    return(v->valptr);
}

static void jsVarSetValueAddress(struct jsVar *v, int varType, void *p) {
    v->varType = varType;
    v->valptr = p;
}

static void jsVarAddressTableResize(struct jsVarAddressTable *tt) {
    int             i, odim;
    struct jsVar    **ot, *tll, *ll;

    odim = tt->dim;
    ot = tt->t;
    tt->dim = tt->dim * 2 + 256;
    JSVAR_ALLOCC(tt->t, tt->dim, struct jsVar *);
    for(i=0; i<tt->dim; i++) tt->t[i] = NULL;
    tt->elems = 0;
    for(i=0; i<odim; i++) {
        ll=ot[i]; 
        while (ll!=NULL) {
            tll = ll;
            ll=ll->nextInAddressHash;
            jsVarAddressTableAdd(tt, tll);
        }
    }
    // logPrintfLine("%s:%d: resizing jsVarAddressTable from %d to %d.", __FILE__, __LINE__, odim, tt->dim);
    if (odim > 1) JSVAR_FREE(ot);
}
static void jsVarAddressTableAdd(struct jsVarAddressTable *tt, struct jsVar *v) {
    unsigned    h;
    void        *addr;

    addr = jsVarGetValueAddress(v);
    if (JSVAR_AVERAGE_VARS_PER_ADDRESS_HASH_TABLE_CELL*tt->elems >= tt->dim) jsVarAddressTableResize(tt);
    h = JSVAR_ADDRESS_HASH_FUN(addr) % tt->dim;
    sglib_jsVarl_add(&tt->t[h], v);
    tt->elems ++;
}
static void jsVarAddressTableDelete(struct jsVarAddressTable *tt, struct jsVar *v) {
    unsigned    h;
    void        *a;

    a = jsVarGetValueAddress(v);
    h = JSVAR_ADDRESS_HASH_FUN(a) % tt->dim;
    assert(sglib_jsVarl_is_member(tt->t[h], v));
    sglib_jsVarl_delete(&tt->t[h], v);  
    tt->elems --;
}


static void jsVarDumpTree(char *msg, struct jsVar *tree) {
    struct sglib_jsVar_iterator     iii;
    struct jsVar                    *ss;
    int                             i;

    i = 0;
    printf("[jsVarDumpVariables] start: %s\n", msg);
    for(ss = sglib_jsVar_it_init_inorder(&iii, tree); ss != NULL; ss = sglib_jsVar_it_next(&iii)) {
        printf("%d: %s\n", i++, ss->jsname);
    }
    printf("[jsVarDumpVariables] end\n");
}

void jsVarDumpVariables(char *msg, struct jsVarSpace *jss) {
    jsVarDumpTree(msg, jss->jsVars);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////
// our compression

static void jsVarCtrieDump(struct jsVarCompressionTrie *tt) {
    if (tt == NULL) return;
    printf("[%d,", tt->ccode);
    jsVarCtrieDump(tt->left);
    printf(",");
    jsVarCtrieDump(tt->right);
    printf("]");
}

static void jsVarCtrieResetCcodeRec(struct jsVarCompressionTrie *tt, int fromCcode, int toCcode) {
    if (tt == NULL) return;
    if (tt->ccode == fromCcode) {
        tt->ccode = toCcode;
        jsVarCtrieResetCcodeRec(tt->left, fromCcode, toCcode);
        jsVarCtrieResetCcodeRec(tt->right, fromCcode, toCcode);
    }
}

static void jsVarCtrieAddStringRec(struct jsVarCompressionTrie **trieAddr, char *str, int ccode, int previousStrCode, int index) {
    struct jsVarCompressionTrie *tt;
    int                         i,j,c,bit;
    tt = *trieAddr;
    if (tt == NULL) {
        JSVAR_ALLOC(tt, struct jsVarCompressionTrie);
        tt->ccode = previousStrCode;
        tt->left = tt->right = NULL;
        *trieAddr = tt;
    }
    i = index / 8;
    j = index % 8;
    c = ((unsigned char *)str)[i];
    if (c == 0) {
        // whole string added, create final node
        jsVarCtrieResetCcodeRec(tt, tt->ccode, ccode);
        return;
    }
    assert(c != 0);
    bit = (c >> j) & 0x01;
    if (bit == 0) {
        jsVarCtrieAddStringRec(&tt->left, str, ccode, tt->ccode, index + 1);
    } else {
        jsVarCtrieAddStringRec(&tt->right, str, ccode, tt->ccode, index + 1);
    }
}

static int jsVarCtrieFindStringPrefixRec(struct jsVarCompressionTrie *tt, char *str, int index) {
    int i,j,c,bit;
    i = index / 8;
    j = index % 8;
    c = ((unsigned char *)str)[i];
    if (c == 0) return(tt->ccode);
    bit = (c >> j) & 0x01;
    if (bit == 0 && tt->left != NULL) {
        return(jsVarCtrieFindStringPrefixRec(tt->left, str, index+1));
    } if (bit != 0 && tt->right != NULL) {
        return(jsVarCtrieFindStringPrefixRec(tt->right, str, index+1));
    } else {
        return(tt->ccode);
    }
}

static void jsVarCtrieAddString(struct jsVarCompressionTrie **trieAddr, char *str, int ccode) {
    jsVarCtrieAddStringRec(trieAddr, str, ccode, -1, 0);
}

static int jsVarCtrieFindStringPrefix(struct jsVarCompressionTrie *tt, char *str) {
    if (tt == NULL) return(-1);
    return(jsVarCtrieFindStringPrefixRec(tt, str, 0));
}

static void jsVarCtrieFree(struct jsVarCompressionTrie *tt) {
    if (tt == NULL) return;
    jsVarCtrieFree(tt->left);
    jsVarCtrieFree(tt->right);
    JSVAR_FREE(tt);
}

void jsVarCompressionDataDump(struct jsVarCompressionData *dd) {
    int i;
    printf("[jsVarCompressionDataDump] start:\n");
    printf("[jsVarCompressionDataDump] dictionary: ");
    for(i=0; i<dd->dictionarySize; i++) {
        if (dd->dictionary[i] != NULL) printf("%d:%s ", i, dd->dictionary[i]);
    }
    printf("\n");
    printf("[jsVarCompressionDataDump] tree: ");
    jsVarCtrieDump(dd->tree);
    printf("\n");
    printf("[jsVarCompressionDataDump] start:\n");
}

void jsVarCompressionDataFree(struct jsVarCompressionData *dd) {
    int     i;

    for(i=0; i<dd->dictionarySize; i++) {
        JSVAR_FREE(dd->dictionary[i]);
    }
    jsVarCtrieFree(dd->tree);
    JSVAR_FREE(dd->dictionary);
    JSVAR_FREE(dd->dictionarylengths);
    JSVAR_FREE(dd);
}

struct jsVarCompressionData *jsVarCompressionDataCreate(char **dictionary, int dictionarySize) {
    int                             i;
    struct jsVarCompressionData     *dd;
    
    JSVAR_ALLOC(dd, struct jsVarCompressionData);
    JSVAR_ALLOCC(dd->dictionary, dictionarySize, char *);
    JSVAR_ALLOCC(dd->dictionarylengths, dictionarySize, int);
    dd->dictionarySize = dictionarySize;
    dd->tree = NULL;
    for(i=0; i<dictionarySize; i++) {
        if (dictionary[i] == NULL) {
            dd->dictionary[i] = NULL;
            dd->dictionarylengths[i] = 0;
        } else {
            dd->dictionary[i] = strDuplicate(dictionary[i]);
            dd->dictionarylengths[i] = strlen(dd->dictionary[i]);
            jsVarCtrieAddString(&dd->tree, dd->dictionary[i], i);
        } 
    }
    return(dd);
}

static int jsVarMessageAndVariableTypeCode(struct jsVar *v) {
    int vTypeCode;
    if (v->varType == JSVT_STRING 
        || v->varType == JSVT_STRING_ARRAY 
        || v->varType == JSVT_STRING_VECTOR
        || v->varType == JSVT_STRING_LIST
        || v->varType == JSVT_WSTRING 
        || v->varType == JSVT_WSTRING_ARRAY 
        || v->varType == JSVT_WSTRING_VECTOR
        || v->varType == JSVT_WSTRING_LIST

        ) {
        vTypeCode = JSVAR_MSG_V_TYPE_CODE_STRING;
    } else if (
        v->varType == JSVT_FLOAT 
        || v->varType == JSVT_FLOAT_ARRAY 
        || v->varType == JSVT_FLOAT_VECTOR
        || v->varType == JSVT_FLOAT_LIST
        || v->varType == JSVT_DOUBLE 
        || v->varType == JSVT_DOUBLE_ARRAY 
        || v->varType == JSVT_DOUBLE_VECTOR
        || v->varType == JSVT_DOUBLE_LIST
        ) {
        vTypeCode = JSVAR_MSG_V_TYPE_CODE_DOUBLE;
    } else {
        vTypeCode = JSVAR_MSG_V_TYPE_CODE_NUMERIC;
    }
    return(vTypeCode);
}

void jsVarCompressMystr(struct jsVarDynamicString *mm, struct jsVarCompressionData *dd) {
    int i, j, r;

    if (dd == NULL || dd->tree == NULL) return;

    i = j = 0;
    while (i<mm->size) {
        r = jsVarCtrieFindStringPrefix(dd->tree, mm->s+i);
        if (r < 0) {
            mm->s[j++] = mm->s[i++];
        } else if (r < JSVAR_DICTIONARY_ONE_BYTE_ENCODING_INDEX) {
            mm->s[j++] = r;
            i += dd->dictionarylengths[r];
        } else {
            assert(r >= JSVAR_DICTIONARY_ONE_BYTE_ENCODING_INDEX && r < JSVAR_DICTIONARY_SIZE);
            if (dd->dictionarylengths[r] >= 2) {
                mm->s[j++] = r/96 + JSVAR_DICTIONARY_ONE_BYTE_ENCODING_INDEX;
                mm->s[j++] = r%96;
                i += dd->dictionarylengths[r];
            } else {
                // do not "compress" to words shorter than 2 bytes
                mm->s[j++] = mm->s[i++];                
            }
        }
    }
    mm->s[j] = 0;
    mm->size = j;
}

static void jsVarResetCompressedCodeLenAndName(struct jsVar *v) {
    char                        ttt[JSVAR_TMP_STRING_SIZE];
    int                         rlen, tttlen;
    struct jsVarDynamicString   *mm;

    mm = jsVarDstrCreateFromCharPtr(v->jsname, v->jsnameLength);
    // only variable name is compressed
    jsVarCompressMystr(mm, jsVarCompressionGlobal);
    if (v->compressedCodeLenAndJsname != NULL) JSVAR_FREE(v->compressedCodeLenAndJsname);
    ttt[0] = jsVarMessageAndVariableTypeCode(v);
    tttlen = 1 + jsVarEncodeNumber(mm->size, 0, ttt+1);
    rlen = tttlen+mm->size;
    JSVAR_ALLOCC(v->compressedCodeLenAndJsname, rlen, char);
    memcpy(v->compressedCodeLenAndJsname, ttt, tttlen);
    memcpy(v->compressedCodeLenAndJsname+tttlen, mm->s, mm->size);
    v->compressedCodeLenAndJsnameLength = rlen;
    jsVarDstrFree(&mm);
}

void jsVarCompressionDataResetGlobal(struct jsVarCompressionData *dd) {
    int                             i;
    struct sglib_jsVar_iterator     iii;
    struct jsVar                    *ss;
    struct jsVarSpace               *jss;

    jsVarCompressionGlobal = dd;

    for(i=0; i<jsVarSpaceTabIndex; i++) {
        if (jsVarSpaceTab[i].jsVarSpaceMagic != 0) {
            jss = &jsVarSpaceTab[i];
            for(ss = sglib_jsVar_it_init(&iii, jss->jsVars); ss != NULL; ss = sglib_jsVar_it_next(&iii)) {
                jsVarResetCompressedCodeLenAndName(ss);
            }
        }
    }
}

void jsVarCompressionSetGlobalDictionary(char *dictionary[JSVAR_DICTIONARY_SIZE]) {
    struct jsVarCompressionData *dd;

    dd = jsVarCompressionDataCreate(dictionary, JSVAR_DICTIONARY_SIZE);
    // jsVarCompressionDataDump(dd);
    jsVarCompressionDataResetGlobal(dd);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////

int jsVarBindVariableToConnection(struct jsVar *v, struct jsVaraio *jj) {
    int             i,j,c;
    int             baioMagic;

    baioMagic = jj->w.b.baioMagic;
    if (jsVarDebugLevel > 30) printf("%s: Binding %s to connection with magic number %d\n", JSVAR_PRINT_PREFIX(), v->jsname, baioMagic);

    for(c=i=0; i<v->baioMagicsSize && c<v->baioMagicsElemCount; i++) {
        if (v->baioMagics[i] == baioMagic) {
            if (jsVarDebugLevel > 90) printf("%s:%s:%d: baioMagic %d already in jsVar %s\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, baioMagic, v->jsname);
            return(1);
        }
        if (v->baioMagics[i]) c ++;
    }
    for(i=0; i<v->baioMagicsSize; i++) {
        if (v->baioMagics[i] == 0) break;
    }
    if (i == v->baioMagicsSize) {
        j = v->baioMagicsSize;
        v->baioMagicsSize = v->baioMagicsSize * 2 + 2;
        JSVAR_REALLOCC(v->baioMagics, v->baioMagicsSize, int);
        for( ; j<v->baioMagicsSize; j++) v->baioMagics[j] = 0;
    }
    v->baioMagics[i] = baioMagic;
    v->baioMagicsElemCount ++;

    // If this is an input variable and the connection does not have inputVarSpace set, set it by default to this one
    if (v->syncDirection == JSVSD_JAVASCRIPT_TO_C || v->syncDirection == JSVSD_BOTH) {
        if (jj->inputVarSpaceMagic == 0) jj->inputVarSpaceMagic = v->varSpace->jsVarSpaceMagic;
        if (v->varSpace->jsVarSpaceMagic != jj->inputVarSpaceMagic && v->varSpace->jsVarSpaceMagic != jsVarSpaceGlobal->jsVarSpaceMagic) {
            printf("%s: %s:%d: Warning: varSpace mismatch. Input variable %s is bind to a connection storing values into different varSpace.",
                   JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, v->jsname
                );
        }
    }

    // update / initialize the variable to the current value
    // use firstBindingFlag to avoid possible double update for the first binding
    jsVarSendVariableUpdate(v, baioMagic, (v->baioMagicsElemCount == 1));
    return(0);
}

static int jsVarFindBoundConnection(struct jsVar *v, int baioMagic) {
    int     i, c, *mm;

    mm = v->baioMagics;
    for(c=i=0; i<v->baioMagicsSize && v->baioMagics[i] != baioMagic && c<v->baioMagicsElemCount; i++) {
        if (v->baioMagics[i]) c ++;
    }
    if (i<v->baioMagicsSize && v->baioMagics[i] == baioMagic) return(i);
    return(-1);
}

static void jsVarUnBindConnection(struct jsVar *v, int baioMagic) {
    int     i;

    if (jsVarDebugLevel > 30) printf("%s: Remove binding %s from connection with magic number %d\n", JSVAR_PRINT_PREFIX(), v->jsname, baioMagic);
    i = jsVarFindBoundConnection(v, baioMagic);
    if (i >= 0) {
        assert(v->baioMagics[i] == baioMagic);
        v->baioMagics[i] = 0;
        v->baioMagicsElemCount --;
    } else {
        printf("%s:%s:%d: Error: Deleting baioMagic which is not in jsVar\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
    }
}

#if JSVAR_USE_REGEXP
void jsVarBindMatchingVariables(struct jsVaraio *jj, struct jsVarSpace *jss, char *regexpfmt, ...) {
    int                             regretval;
    regex_t                         regex;
    struct sglib_jsVar_iterator     iii;
    struct jsVar                    *ss;
    va_list                         arg_ptr;
    struct jsVarDynamicString       *regexpstr;
    
    va_start(arg_ptr, regexpfmt);
    regexpstr = jsVarDstrCreateByVPrintf(regexpfmt, arg_ptr);
    va_end(arg_ptr);

    regretval = regcomp(&regex, regexpstr->s, REG_EXTENDED);
    if (regretval) goto finish;

    for(ss = sglib_jsVar_it_init_inorder(&iii, jss->jsVars); ss != NULL; ss = sglib_jsVar_it_next(&iii)) {
        if (regexec(&regex, ss->jsname, 0, NULL, 0) == 0) {
            jsVarBindVariableToConnection(ss, jj);
        }       
    }
    
    regfree(&regex);
finish:
    jsVarDstrFree(&regexpstr);
}
#endif

void jsVarBindPrefixVariables(struct jsVaraio *jj, struct jsVarSpace *jss, char *prefixfmt, ...) {
    struct sglib_jsVar_iterator     iii;
    struct jsVar                    *ss, memb;
    va_list                         arg_ptr;
    struct jsVarDynamicString       *prefixstr;

    va_start(arg_ptr, prefixfmt);
    prefixstr = jsVarDstrCreateByVPrintf(prefixfmt, arg_ptr);
    va_end(arg_ptr);

    memset(&memb, 0, sizeof(memb));
    memb.jsname = prefixstr->s;
    memb.jsnameLength = strlen(memb.jsname);

    for(ss = sglib_jsVar_it_init_on_equal(&iii, jss->jsVars, jsVarPrefixSubcomparator, &memb); ss != NULL; ss = sglib_jsVar_it_next(&iii)) {
        // printf("Refresh prefix %s\n", ss->jsname);
        jsVarBindVariableToConnection(ss, jj);
    }

    jsVarDstrFree(&prefixstr);
}

#if JSVAR_USE_REGEXP
void jsVarUnbindMatchingVariables(struct jsVaraio *jj, struct jsVarSpace *jss, char *regexpfmt, ...) {
    int                             regretval;
    regex_t                         regex;
    struct sglib_jsVar_iterator     iii;
    struct jsVar                    *ss;
    va_list                         arg_ptr;
    struct jsVarDynamicString       *regexpstr;
    int                             baioMagic;

    baioMagic = jj->w.b.baioMagic;

    va_start(arg_ptr, regexpfmt);
    regexpstr = jsVarDstrCreateByVPrintf(regexpfmt, arg_ptr);
    va_end(arg_ptr);

    regretval = regcomp(&regex, regexpstr->s, REG_EXTENDED);
    if (regretval) goto finish;

    for(ss = sglib_jsVar_it_init_inorder(&iii, jss->jsVars); ss != NULL; ss = sglib_jsVar_it_next(&iii)) {
        if (regexec(&regex, ss->jsname, 0, NULL, 0) == 0) {
            jsVarUnBindConnection(ss, baioMagic);
        }       
    }
    
    regfree(&regex);
finish:
    jsVarDstrFree(&regexpstr);
}
#endif

void jsVarUnbindPrefixVariables(struct jsVarSpace *jss, struct jsVaraio *jj, char *prefixfmt, ...) {
    struct sglib_jsVar_iterator     iii;
    struct jsVar                    *ss, memb;
    va_list                         arg_ptr;
    struct jsVarDynamicString       *prefixstr;
    int                             baioMagic;

    baioMagic = jj->w.b.baioMagic;

    va_start(arg_ptr, prefixfmt);
    prefixstr = jsVarDstrCreateByVPrintf(prefixfmt, arg_ptr);
    va_end(arg_ptr);

    memset(&memb, 0, sizeof(memb));
    memb.jsname = prefixstr->s;
    memb.jsnameLength = strlen(memb.jsname);

    for(ss = sglib_jsVar_it_init_on_equal(&iii, jss->jsVars, jsVarPrefixSubcomparator, &memb); ss != NULL; ss = sglib_jsVar_it_next(&iii)) {
        // printf("Refresh prefix %s\n", ss->jsname);
        jsVarUnBindConnection(ss, baioMagic);
    }

    jsVarDstrFree(&prefixstr);
}

#if JSVAR_USE_REGEXP
void jsVarDeleteMatchingVariables(struct jsVarSpace *jss, char *regexpfmt, ...) {
    int                             regretval;
    regex_t                         regex;
    struct sglib_jsVar_iterator     iii;
    struct jsVar                    *ss;
    va_list                         arg_ptr;
    struct jsVarDynamicString       *regexpstr;
    struct jsVar                    *delist;

    va_start(arg_ptr, regexpfmt);
    regexpstr = jsVarDstrCreateByVPrintf(regexpfmt, arg_ptr);
    va_end(arg_ptr);

    // first get them into a list, because we can not delete while within iterator
    delist = NULL;

    regretval = regcomp(&regex, regexpstr->s, REG_EXTENDED);
    if (regretval) goto finish;

    for(ss = sglib_jsVar_it_init_inorder(&iii, jss->jsVars); ss != NULL; ss = sglib_jsVar_it_next(&iii)) {
        if (regexec(&regex, ss->jsname, 0, NULL, 0) == 0) {
            ss->nextInDeletionList = delist;
            delist = ss;
        }       
    }

    regfree(&regex);

    while (delist != NULL) {
        ss = delist;
        delist = delist->nextInDeletionList;
        // printf("deleting variable %s\n", ss->jsname);fflush(stdout);
        jsVarDeleteVariable(ss);
    }

finish:
    jsVarDstrFree(&regexpstr);
}
#endif

void jsVarDeletePrefixVariables(struct jsVarSpace *jss, char *prefixfmt, ...) {
    struct sglib_jsVar_iterator     iii;
    struct jsVar                    *ss, memb;
    va_list                         arg_ptr;
    struct jsVarDynamicString       *prefixstr;
    struct jsVar                    *delist;

    va_start(arg_ptr, prefixfmt);
    prefixstr = jsVarDstrCreateByVPrintf(prefixfmt, arg_ptr);
    va_end(arg_ptr);

    // first get them into a list, because we can not delete while within iterator
    delist = NULL;

    memset(&memb, 0, sizeof(memb));
    memb.jsname = prefixstr->s;
    memb.jsnameLength = strlen(memb.jsname);
    
    for(ss = sglib_jsVar_it_init_on_equal(&iii, jss->jsVars, jsVarPrefixSubcomparator, &memb); ss != NULL; ss = sglib_jsVar_it_next(&iii)) {
        ss->nextInDeletionList = delist;
        delist = ss;
    }
    
    while (delist != NULL) {
        ss = delist;
        delist = delist->nextInDeletionList;
        // printf("deleting variable %s\n", ss->jsname);fflush(stdout);
        jsVarDeleteVariable(ss);
    }

    jsVarDstrFree(&prefixstr);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////

static struct jsVar *jsVarGetVariableByName(struct jsVarSpace *jss, char *javaScriptName) {
    struct jsVar *memb, jj;
    jj.jsname = javaScriptName;
    memb = sglib_jsVar_find_member(jss->jsVars, &jj);
    return(memb);
}

struct jsVar *jsVarGetVariable(struct jsVarSpace *jss, char *javaScriptNameFmt, ...) {
    struct jsVarDynamicString   *ss;
    struct jsVar                *memb;
    va_list                     arg_ptr;

    va_start(arg_ptr, javaScriptNameFmt);
    ss = jsVarDstrCreateByVPrintf(javaScriptNameFmt, arg_ptr);
    va_end(arg_ptr);
    memb = jsVarGetVariableByName(jss, ss->s);
    jsVarDstrFree(&ss);
    return(memb);
}

JSVAR_STATIC struct jsVar *jsVarBindNameToConnection(struct jsVarSpace *jss, struct jsVaraio *jj, char *javaScriptName) {
    struct jsVar    *memb;

    memb = jsVarGetVariableByName(jss, javaScriptName);
    if (memb != NULL) jsVarBindVariableToConnection(memb, jj);
    return(memb);
}

struct jsVar *jsVarBindAddressToConnection(struct jsVaraio *jj, int varType, void *addr) {
    unsigned                        h;
    struct jsVar                    *v, vv;
    struct jsVarAddressTable        *tt;
    struct sglib_jsVarl_iterator    ii;
    // get jsVar

    tt = &jsVarsAddr;
    vv.varType = varType;
    jsVarSetValueAddress(&vv, varType, addr);
    h = JSVAR_ADDRESS_HASH_FUN(addr) % tt->dim;
    // there can be several jsvar variables bind to the same physical address
    for (v = sglib_jsVarl_it_init_on_equal(&ii, tt->t[h], jsVarAddressComparator, &vv);
         v != NULL;
         v = sglib_jsVarl_it_next(&ii)
        ) {
        jsVarBindVariableToConnection(v, jj);
    }
    return(0);

}

static void jsvarSetWatchForNewlyAllocatedVariable(struct jsVar *v) {
    char            ttt[JSVAR_LONG_LONG_TMP_STRING_SIZE];
    int             n;
    struct jsVar    *vv;

    strncpy(ttt, v->jsname, sizeof(ttt));
    ttt[sizeof(ttt)-1] = 0;
    n = strlen(ttt);
    vv = NULL;
    while (vv == NULL) {
        for(n-=2; n>-0 && ttt[n] != ']' && ttt[n] != '[' && ttt[n] != '.'; n--) ;
        if (n < 0) return;  // no watch found for this variable
        if (ttt[n] == ']') ttt[++n] = 0;
        else ttt[n] = 0;
        assert(v->varSpace != NULL);
        vv = jsVarGetVariableByName(v->varSpace, ttt);
    }
    // vv is watch for our variable
    // printf("%s: setting watch for %s to %s\n", JSVAR_PRINT_PREFIX(), v->jsname, vv->jsname);
    v->watchVariable = vv;
}

static int jsvarIsListType(int varType) {
    switch (varType) {
    case JSVT_CHAR_LIST:
    case JSVT_SHORT_LIST:
    case JSVT_INT_LIST:
    case JSVT_LONG_LIST:
    case JSVT_LLONG_LIST:
    case JSVT_FLOAT_LIST:
    case JSVT_DOUBLE_LIST:
    case JSVT_STRING_LIST:
    case JSVT_WSTRING_LIST:
    case JSVT_U_CHAR_LIST:
    case JSVT_U_SHORT_LIST:
    case JSVT_U_INT_LIST:
    case JSVT_U_LONG_LIST:
    case JSVT_U_LLONG_LIST:
        return(1);
    }
    return(0);
}

static int jsvarIsArrayType(int varType) {
    switch (varType) {
    case JSVT_CHAR_ARRAY:
    case JSVT_SHORT_ARRAY:
    case JSVT_INT_ARRAY:
    case JSVT_LONG_ARRAY:
    case JSVT_LLONG_ARRAY:
    case JSVT_FLOAT_ARRAY:
    case JSVT_DOUBLE_ARRAY:
    case JSVT_STRING_ARRAY:
    case JSVT_WSTRING_ARRAY:
    case JSVT_U_CHAR_ARRAY:
    case JSVT_U_SHORT_ARRAY:
    case JSVT_U_INT_ARRAY:
    case JSVT_U_LONG_ARRAY:
    case JSVT_U_LLONG_ARRAY:
        return(1);
    }
    return(0);
}

static int jsvarIsVectorType(int varType) {
    switch (varType) {
    case JSVT_CHAR_VECTOR:
    case JSVT_SHORT_VECTOR:
    case JSVT_INT_VECTOR:
    case JSVT_LONG_VECTOR:
    case JSVT_LLONG_VECTOR:
    case JSVT_FLOAT_VECTOR:
    case JSVT_DOUBLE_VECTOR:
    case JSVT_STRING_VECTOR:
    case JSVT_WSTRING_VECTOR:
    case JSVT_U_CHAR_VECTOR:
    case JSVT_U_SHORT_VECTOR:
    case JSVT_U_INT_VECTOR:
    case JSVT_U_LONG_VECTOR:
    case JSVT_U_LLONG_VECTOR:
        return(1);
    }
    return(0);
}

static int jsvarIsIndexedType(int varType) {
    return(jsvarIsArrayType(varType) || jsvarIsListType(varType) || jsvarIsVectorType(varType));
}

static int isWatchVariableName(char *name) {
    int                             nlen;

    nlen = strlen(name);
    // if not a watch variable, do nothing
    if (nlen > 1 && name[nlen-1] == '~') return(1);
    return(0);
}

static int jsvarComputeListLength(struct jsVar *v) {
    void    *ll;
    int     i, res;

    if (! jsvarIsListType(v->varType)) {
        printf("%s: %s:%d: Internal error. List length requested from a non list data structure.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
        return(0);
    }
    res = 0;
    for(i=0,ll=*(void**)v->valptr; ll!=NULL; i++,ll=JSVAR_LIST_NEXT_VALUE(v, ll, i)) res ++;
    return(res);
}

#define JSVAR_INITIALIZE_ARRAY_VAR(javaScriptName, memb, elementSize, tcode) { \
        if (elementSize < 0) memb->arrayVectorElementSize = sizeof(JSVAR_TCODE_TYPE(tcode)); \
        /* the following ir probably useles, as memb->lastSentArrayDimension is always zero here */ \
        JSVAR_ALLOCC(JSVAR_LAST_SENT_VAL(memb,tcode,a), memb->lastSentArrayDimension, JSVAR_TCODE_TYPE(tcode)); \
    }


static struct jsVar *jsVarAllocate(
    struct jsVarSpace *jss, char *javaScriptName, int varType, int elementSize, int vectorElementOffset, 
    void *(*valueFromElemFunction)(void *), void *(*nextElementFunction)(void *), int dimension, int *dimensionAddr
    ) {
    struct jsVar    *memb;
    int             nlen;

    JSVAR_ALLOC(memb, struct jsVar);
    memset(memb, 0, sizeof(*memb));
    memb->varType = JSVT_NONE;
    memb->syncDirection = JSVSD_C_TO_JAVASCRIPT;
    memb->varSpace = jss;
    nlen = strlen(javaScriptName);
    JSVAR_ALLOCC(memb->jsname, nlen+1, char);
    strcpy(memb->jsname, javaScriptName);
    memb->jsnameLength = nlen;
    memb->compressedCodeLenAndJsname = NULL;
    memb->compressedCodeLenAndJsnameLength = 0;
    memb->arrayDimension = dimension;
    if (dimensionAddr == NULL || jsvarIsArrayType(varType) || isWatchVariableName(javaScriptName)) {
        memb->vectorDimensionAddr = &memb->arrayDimension;
    } else {
        memb->vectorDimensionAddr = dimensionAddr;
    }
    memb->arrayVectorElementSize = elementSize;
    memb->vectorElementOffset = vectorElementOffset;
    memb->listNextElementFunction = nextElementFunction;
    memb->listValueFromElementFunction = valueFromElemFunction;
    memb->lastSentArrayDimension = 0;
    // if (dimension != NULL) memb->lastSentArrayDimension = *dimension;
    memb->varType = varType;
    memb->baioMagics = NULL;
    memb->baioMagicsElemCount = memb->baioMagicsSize = 0;
    memb->nextInDeletionList = NULL;
    memb->nextInUpdateList = NULL;
    memb->previousInUpdateList = NULL;
    memset(&memb->callBackOnChange, 0, sizeof(memb->callBackOnChange));
    memb->modifiedIndexes.a = NULL;

    // Maybe we can remove all this stuff if we simply set lastSentDimension to 0
    switch (varType) {
    case JSVT_INT_ARRAY:
    case JSVT_INT_VECTOR:
    case JSVT_INT_LIST:
        JSVAR_INITIALIZE_ARRAY_VAR(javaScriptName, memb, elementSize, JSVT_INT_TCODE);
        break;
    case JSVT_U_INT_ARRAY:
    case JSVT_U_INT_VECTOR:
    case JSVT_U_INT_LIST:
        JSVAR_INITIALIZE_ARRAY_VAR(javaScriptName, memb, elementSize, JSVT_U_INT_TCODE);
        break;
    case JSVT_SHORT_ARRAY:
    case JSVT_SHORT_VECTOR:
    case JSVT_SHORT_LIST:
        JSVAR_INITIALIZE_ARRAY_VAR(javaScriptName, memb, elementSize, JSVT_SHORT_TCODE);
        break;
    case JSVT_U_SHORT_ARRAY:
    case JSVT_U_SHORT_VECTOR:
    case JSVT_U_SHORT_LIST:
        JSVAR_INITIALIZE_ARRAY_VAR(javaScriptName, memb, elementSize, JSVT_U_SHORT_TCODE);
        break;
    case JSVT_CHAR_ARRAY:
    case JSVT_CHAR_VECTOR:
    case JSVT_CHAR_LIST:
        JSVAR_INITIALIZE_ARRAY_VAR(javaScriptName, memb, elementSize, JSVT_CHAR_TCODE);
        break;
    case JSVT_U_CHAR_ARRAY:
    case JSVT_U_CHAR_VECTOR:
    case JSVT_U_CHAR_LIST:
        JSVAR_INITIALIZE_ARRAY_VAR(javaScriptName, memb, elementSize, JSVT_U_CHAR_TCODE);
        break;
    case JSVT_LONG_ARRAY:
    case JSVT_LONG_VECTOR:
    case JSVT_LONG_LIST:
        JSVAR_INITIALIZE_ARRAY_VAR(javaScriptName, memb, elementSize, JSVT_LONG_TCODE);
        break;
    case JSVT_U_LONG_ARRAY:
    case JSVT_U_LONG_VECTOR:
    case JSVT_U_LONG_LIST:
        JSVAR_INITIALIZE_ARRAY_VAR(javaScriptName, memb, elementSize, JSVT_U_LONG_TCODE);
        break;
    case JSVT_LLONG_ARRAY:
    case JSVT_LLONG_VECTOR:
    case JSVT_LLONG_LIST:
        JSVAR_INITIALIZE_ARRAY_VAR(javaScriptName, memb, elementSize, JSVT_LLONG_TCODE);
        break;
    case JSVT_U_LLONG_ARRAY:
    case JSVT_U_LLONG_VECTOR:
    case JSVT_U_LLONG_LIST:
        JSVAR_INITIALIZE_ARRAY_VAR(javaScriptName, memb, elementSize, JSVT_U_LLONG_TCODE);
        break;
    case JSVT_FLOAT_ARRAY:
    case JSVT_FLOAT_VECTOR:
    case JSVT_FLOAT_LIST:
        JSVAR_INITIALIZE_ARRAY_VAR(javaScriptName, memb, elementSize, JSVT_FLOAT_TCODE);
        break;
    case JSVT_DOUBLE_ARRAY:
    case JSVT_DOUBLE_VECTOR:
    case JSVT_DOUBLE_LIST:
        JSVAR_INITIALIZE_ARRAY_VAR(javaScriptName, memb, elementSize, JSVT_DOUBLE_TCODE);
        break;
    case JSVT_STRING_ARRAY:
    case JSVT_STRING_VECTOR:
    case JSVT_STRING_LIST:
        JSVAR_INITIALIZE_ARRAY_VAR(javaScriptName, memb, elementSize, JSVT_STRING_TCODE);
        break;
    case JSVT_WSTRING_ARRAY:
    case JSVT_WSTRING_VECTOR:
    case JSVT_WSTRING_LIST:
        JSVAR_INITIALIZE_ARRAY_VAR(javaScriptName, memb, elementSize, JSVT_WSTRING_TCODE);
        break;
    }   
    jsVarResetCompressedCodeLenAndName(memb);
    jsvarSetWatchForNewlyAllocatedVariable(memb);
    sglib_jsVar_add(&jss->jsVars, memb);
    return(memb);
}

static int jsVarCheckNewVariablePreConditions(struct jsVarSpace *jss, char *javaScriptName, int varType, struct jsVar **membOut) {
    struct jsVar    *memb;
    char            *s;

    *membOut = NULL;

    // also check varspace consistency
    if (jss == NULL) {
        printf("%s: %s:%d: Error: jsVarSpace can't be null.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
        return(-1);
    }

    if (jss != jsVarSpaceFromMagic(jss->jsVarSpaceMagic)) {
        printf("%s: %s:%d: Error: Wrong jsVarSpace. Maybe it was freed in between.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
        return(-1);
    }

    if (javaScriptName == NULL) {
        printf("%s: %s:%d: Error: Variable name can't be null.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
#if EDEBUG
        assert(0);
#endif
        return(-1);
    }

    memb = jsVarGetVariableByName(jss, javaScriptName);
    if (memb != NULL) {
        printf("%s: %s:%d: Error: Variable %s exists\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, javaScriptName);
        *membOut = memb;
        return(-1);
    }

    s = strstr(javaScriptName, "[?]");
    if (jsvarIsIndexedType(varType)) {
        if (s == NULL) {
            printf("%s: %s:%d: Error: Indexation string [?] is missing in %s.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, javaScriptName);
        }
    } else {
        if (s != NULL) {
            printf("%s: %s:%d: Error: Basic variable %s contains indexation string [?].\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, javaScriptName);
        }
    }

    if (isWatchVariableName(javaScriptName) && ! jsVarIsValidWatchType(varType)) {
        printf("%s: %s:%d: Error: Watch variable %s is not of a valid type. Ignored.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, javaScriptName);
        return(-1);
    }

    return(0);
}

static struct jsVar *jsVarSynchronize(
    struct jsVarSpace *jss, char *javaScriptName, int varType, void *addr, int arrayVectorElementSize, 
    int vectorElementOffset, void *(*valueFromElemFunction)(void *), void *(*nextElementFunction)(void *), int dimension, int *dimensionAddr
    ) {
    struct jsVar                    *memb;
    void                            *ll;
    int                             i;

    // check if the name, addr, etc is O.K. for creation of a new variable, if not return immediately
    if (jsVarCheckNewVariablePreConditions(jss, javaScriptName, varType, &memb) < 0) return(memb);

    if (addr == NULL) {
        printf("%s: %s:%d: Wrong address for %s\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, javaScriptName);
        return(NULL);
    }

    if (isWatchVariableName(javaScriptName)) {
        printf("%s: %s:%d: Error: %s is a name for watch variable!\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, javaScriptName);
        return(NULL);
    }

    if (dimensionAddr != NULL) dimension = *dimensionAddr;
    if (jsvarIsListType(varType)) {
        dimension = 0;
        // calculate initial list dimension
        for(i=0,ll=*(void**)addr; ll!=NULL; i++,ll=nextElementFunction(ll/*, i*/)) dimension ++;
        dimensionAddr = NULL;
    }
    memb = jsVarAllocate(jss, javaScriptName, varType, arrayVectorElementSize, vectorElementOffset, valueFromElemFunction, nextElementFunction, dimension, dimensionAddr);
    memb->valptr = addr;
    jsVarAddressTableAdd(&jsVarsAddr, memb);
    return(memb);
}

static struct jsVar *jsVarSynchronizeInput(
    struct jsVarSpace *jss, char *javaScriptName, int varType, void *addr, int arrayVectorElementSize, 
    int vectorElementOffset, void *(*valueFromElemFunction)(void *), void *(*nextElementFunction)(void *), int dimension, 
    int *dimensionAddr, void (*callBackFunction)(struct jsVar *v, int index, struct jsVaraio *jj)
    ) {
    struct jsVar    *v;
    v = jsVarSynchronize(jss, javaScriptName, varType, addr, arrayVectorElementSize, vectorElementOffset, valueFromElemFunction, nextElementFunction, dimension, dimensionAddr);
    v->syncDirection = JSVSD_JAVASCRIPT_TO_C;
    if (callBackFunction != NULL) {
        jsVarCallBackClearHook(&v->callBackOnChange);
        jsVarCallBackAddToHook(&v->callBackOnChange, (void *) callBackFunction);
    }
    return(v);
}

#define JSVAR_ALLOCATE_VALUE_AND_CREATE_NEW_VAR(dim, tcode, valptr, arrayElementSize) { \
        if (0 && (dim) == 0) {                                              \
            valptr = NULL;                                              \
        } else {                                                        \
            JSVAR_ALLOCC(valptr, (dim), JSVAR_TCODE_TYPE(tcode));               \
        }                                                               \
        for(i=0; i<(dim); i++) ((JSVAR_TCODE_TYPE(tcode)*)valptr)[i] = 0; \
    }

static struct jsVar *jsVarCreateNewSynchronized(struct jsVarSpace *jss, char *javaScriptName, int varType, int dimension) {
    int                             i;
    void                            *valptr;
    struct jsVar                    *memb;
    struct sglib_jsVar_iterator     iii;
    struct jsVar                    *ss;

    // check if the name, addr, etc is O.K. for creation of a new variable, if not return immediately
    if (jsVarCheckNewVariablePreConditions(jss, javaScriptName, varType, &memb) < 0) return(memb);

    memb = jsVarAllocate(jss, javaScriptName, varType, -1, 0, NULL, 0, dimension, NULL);

    switch (varType) {
    case JSVT_INT:  
    case JSVT_U_INT:    
    case JSVT_INT_ARRAY:
    case JSVT_U_INT_ARRAY:
        JSVAR_ALLOCATE_VALUE_AND_CREATE_NEW_VAR(dimension, i, valptr, arrayElementSize);
        break;
    case JSVT_INT_VECTOR:
    case JSVT_U_INT_VECTOR:
        JSVAR_ALLOCATE_VALUE_AND_CREATE_NEW_VAR(dimension, i, memb->selfCreatedVectorAddr, arrayElementSize);
        valptr = & memb->selfCreatedVectorAddr;
        break;
    case JSVT_SHORT:    
    case JSVT_U_SHORT:  
    case JSVT_SHORT_ARRAY:
    case JSVT_U_SHORT_ARRAY:
        JSVAR_ALLOCATE_VALUE_AND_CREATE_NEW_VAR(dimension, w, valptr, arrayElementSize);
        break;
    case JSVT_SHORT_VECTOR:
    case JSVT_U_SHORT_VECTOR:
        JSVAR_ALLOCATE_VALUE_AND_CREATE_NEW_VAR(dimension, w, memb->selfCreatedVectorAddr, arrayElementSize);
        valptr = & memb->selfCreatedVectorAddr;
        break;
    case JSVT_CHAR: 
    case JSVT_U_CHAR:   
    case JSVT_CHAR_ARRAY:
    case JSVT_U_CHAR_ARRAY:
        JSVAR_ALLOCATE_VALUE_AND_CREATE_NEW_VAR(dimension, b, valptr, arrayElementSize);
        break;
    case JSVT_CHAR_VECTOR:
    case JSVT_U_CHAR_VECTOR:
        JSVAR_ALLOCATE_VALUE_AND_CREATE_NEW_VAR(dimension, b, memb->selfCreatedVectorAddr, arrayElementSize);
        valptr = & memb->selfCreatedVectorAddr;
        break;
    case JSVT_LONG: 
    case JSVT_U_LONG:   
    case JSVT_LONG_ARRAY:
    case JSVT_U_LONG_ARRAY:
        JSVAR_ALLOCATE_VALUE_AND_CREATE_NEW_VAR(dimension, l, valptr, arrayElementSize);
        break;
    case JSVT_LONG_VECTOR:
    case JSVT_U_LONG_VECTOR:
        JSVAR_ALLOCATE_VALUE_AND_CREATE_NEW_VAR(dimension, l, memb->selfCreatedVectorAddr, arrayElementSize);
        valptr = & memb->selfCreatedVectorAddr;
        break;
    case JSVT_LLONG:    
    case JSVT_U_LLONG:  
    case JSVT_LLONG_ARRAY:
    case JSVT_U_LLONG_ARRAY:
        JSVAR_ALLOCATE_VALUE_AND_CREATE_NEW_VAR(dimension, ll, valptr, arrayElementSize);
        break;
    case JSVT_LLONG_VECTOR:
    case JSVT_U_LLONG_VECTOR:
        JSVAR_ALLOCATE_VALUE_AND_CREATE_NEW_VAR(dimension, ll, memb->selfCreatedVectorAddr, arrayElementSize);
        valptr = & memb->selfCreatedVectorAddr;
        break;
    case JSVT_FLOAT:    
    case JSVT_FLOAT_ARRAY:
        JSVAR_ALLOCATE_VALUE_AND_CREATE_NEW_VAR(dimension, f, valptr, arrayElementSize);
        break;
    case JSVT_FLOAT_VECTOR:
        JSVAR_ALLOCATE_VALUE_AND_CREATE_NEW_VAR(dimension, f, memb->selfCreatedVectorAddr, arrayElementSize);
        valptr = & memb->selfCreatedVectorAddr;
        break;
    case JSVT_DOUBLE:   
    case JSVT_DOUBLE_ARRAY:
        JSVAR_ALLOCATE_VALUE_AND_CREATE_NEW_VAR(dimension, d, valptr, arrayElementSize);
        break;
    case JSVT_DOUBLE_VECTOR:
        JSVAR_ALLOCATE_VALUE_AND_CREATE_NEW_VAR(dimension, d, memb->selfCreatedVectorAddr, arrayElementSize);
        valptr = & memb->selfCreatedVectorAddr;
        break;
    case JSVT_STRING:   
    case JSVT_STRING_ARRAY:
        JSVAR_ALLOCATE_VALUE_AND_CREATE_NEW_VAR(dimension, s, valptr, arrayElementSize);
        break;
    case JSVT_STRING_VECTOR:
        JSVAR_ALLOCATE_VALUE_AND_CREATE_NEW_VAR(dimension, s, memb->selfCreatedVectorAddr, arrayElementSize);
        valptr = & memb->selfCreatedVectorAddr;
        break;
    case JSVT_WSTRING:  
    case JSVT_WSTRING_ARRAY:
        JSVAR_ALLOCATE_VALUE_AND_CREATE_NEW_VAR(dimension, ws, valptr, arrayElementSize);
        break;
    case JSVT_WSTRING_VECTOR:
        JSVAR_ALLOCATE_VALUE_AND_CREATE_NEW_VAR(dimension, ws, memb->selfCreatedVectorAddr, arrayElementSize);
        valptr = & memb->selfCreatedVectorAddr;
        break;
    default:
        printf("%s: %s:%d: Can not create variable of type %d\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, varType);
        return(NULL);
    }   

    memb->valptr = valptr;
    jsVarAddressTableAdd(&jsVarsAddr, memb);

    if (isWatchVariableName(memb->jsname)) {
        for(ss = sglib_jsVar_it_init_on_equal(&iii, jss->jsVars, jsVarWatchSubcomparator, memb); ss != NULL; ss = sglib_jsVar_it_next(&iii)) {
            if (ss->watchVariable != NULL) {
                if (ss->watchVariable == memb->watchVariable) {
                    // I am becoming a watch of this variable
                    // printf("%s: variable %s becomes a watch for %s\n", JSVAR_PRINT_PREFIX(), memb->jsname, ss->jsname);
                    ss->watchVariable = memb;
                }
            } else if (ss != memb) {
                // printf("%s: assigning watch %s to variable %s.\n", JSVAR_PRINT_PREFIX(), memb->jsname, ss->jsname);
                ss->watchVariable = memb;
            }
        }
    }

    return(memb);
}

static struct jsVar *jsVarCreateNewSynchronizedInput(struct jsVarSpace *jss, char *javaScriptName, int varType, int dimension, void (*callBackFunction)(struct jsVar *v, int index, struct jsVaraio *jj)) {
    struct jsVar *v;
    v = jsVarCreateNewSynchronized(jss, javaScriptName, varType, dimension);
    v->syncDirection = JSVSD_JAVASCRIPT_TO_C;
    if (callBackFunction != NULL) {
        jsVarCallBackClearHook(&v->callBackOnChange);
        jsVarCallBackAddToHook(&v->callBackOnChange, (void *) callBackFunction);
    }
    return(v);
}

static void jsVarInsertToVarSpaceUpdateList(struct jsVar *v) {
	if (v->previousInUpdateList == NULL) {
		assert(v->varSpace != NULL);
		SGLIB_DL_LIST_ADD_AFTER(struct jsVar, v->varSpace->jsVarsUpdatedList, v, previousInUpdateList, nextInUpdateList);
	}
}

void jsVarResizeVector(struct jsVar *vv, int newDimension) {
	if (vv == NULL) return;
    if (! jsvarIsVectorType(vv->varType)) {
        printf("%s: %s:%d: Error: Resizing non-vector variable %s.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, vv->jsname);
        return;
    }
    if (vv->selfCreatedVectorAddr == NULL && vv->arrayDimension != 0) {
        printf("%s: %s:%d: Warning: Resizing vector linked to existing C array ignored for %s.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, vv->jsname);
        return;     
    }
    vv->arrayDimension = newDimension;
    switch (vv->varType) {
    case JSVT_INT_VECTOR:
    case JSVT_U_INT_VECTOR:
        JSVAR_REALLOCC(vv->selfCreatedVectorAddr, newDimension, JSVAR_TCODE_TYPE(i));
        break;
    case JSVT_SHORT_VECTOR:
    case JSVT_U_SHORT_VECTOR:
        JSVAR_REALLOCC(vv->selfCreatedVectorAddr, newDimension, JSVAR_TCODE_TYPE(w));
        break;
    case JSVT_CHAR_VECTOR:
    case JSVT_U_CHAR_VECTOR:
        JSVAR_REALLOCC(vv->selfCreatedVectorAddr, newDimension, JSVAR_TCODE_TYPE(b));
        break;
    case JSVT_LONG_VECTOR:
    case JSVT_U_LONG_VECTOR:
        JSVAR_REALLOCC(vv->selfCreatedVectorAddr, newDimension, JSVAR_TCODE_TYPE(l));
        break;
    case JSVT_LLONG_VECTOR:
    case JSVT_U_LLONG_VECTOR:
        JSVAR_REALLOCC(vv->selfCreatedVectorAddr, newDimension, JSVAR_TCODE_TYPE(ll));
        break;
    case JSVT_FLOAT_VECTOR:
        JSVAR_REALLOCC(vv->selfCreatedVectorAddr, newDimension, JSVAR_TCODE_TYPE(f));
        break;
    case JSVT_DOUBLE_VECTOR:
        JSVAR_REALLOCC(vv->selfCreatedVectorAddr, newDimension, JSVAR_TCODE_TYPE(d));
        break;
    case JSVT_STRING_VECTOR:
        JSVAR_REALLOCC(vv->selfCreatedVectorAddr, newDimension, JSVAR_TCODE_TYPE(s));
        break;
    case JSVT_WSTRING_VECTOR:
        JSVAR_REALLOCC(vv->selfCreatedVectorAddr, newDimension, JSVAR_TCODE_TYPE(ws));
        break;
    default:
        printf("%s: %s:%d: Internal error: unexpected variable type %d\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, vv->varType);
    }
	jsVarInsertToVarSpaceUpdateList(vv);
}

int jsVarGetDimension(struct jsVar *vv) {
	if (vv == NULL) return(0);
	if (jsvarIsArrayType(vv->varType) || jsvarIsVectorType(vv->varType)) return(*vv->vectorDimensionAddr);
	if (jsvarIsListType(vv->varType)) return(jsvarComputeListLength(vv));
	return(1);
}

#define JSVARS_UPDATE_WATCH(v, tcode, funtid) {                         \
        JSVAR_ELEM_VALUE(v, tcode) ++;                                  \
        if (JSVAR_ELEM_VALUE(v, tcode) == 0) JSVAR_ELEM_VALUE(v, tcode) ++; \
        if (notifyForFastUpdate) jsVarNoteUp##funtid(v);                \
    }

#define JSVARS_UPDATE_WATCH_VECTOR(v, index, tcode, funtid) {                   \
        JSVAR_VECTOR_ELEM_VALUE(v, tcode, index) ++;                    \
        if (JSVAR_VECTOR_ELEM_VALUE(v, tcode, index) == 0) JSVAR_VECTOR_ELEM_VALUE(v, tcode, index) ++; \
        if (notifyForFastUpdate) jsVarNoteUp##funtid##Vector(v, index);     \
    }

#define JSVARS_UPDATE_WATCH_VECTOR_DIMENSION(v, dim, newDim, tcode) {   \
        void *val;                                                      \
        assert(v->valptr != NULL) ;                                     \
        val = *((void **)v->valptr);                                    \
        JSVAR_REALLOCC(val, newDim, JSVAR_TCODE_TYPE(tcode));                   \
        *((void **)v->valptr) = val;                                    \
        for(i=dim; i<newDim; i++) JSVAR_VECTOR_ELEM_VALUE(v, tcode, i) = 0; \
        *(v)->vectorDimensionAddr = newDim;                             \
    }

static int jsVarIsValidWatchType(int varType) {
    switch (varType) {
    case JSVT_CHAR:
    case JSVT_U_CHAR:
    case JSVT_SHORT:
    case JSVT_U_SHORT:
    case JSVT_INT:
    case JSVT_U_INT:
    case JSVT_LONG:
    case JSVT_U_LONG:
    case JSVT_LLONG:
    case JSVT_U_LLONG:
    case JSVT_CHAR_VECTOR:
    case JSVT_U_CHAR_VECTOR:
    case JSVT_SHORT_VECTOR:
    case JSVT_U_SHORT_VECTOR:
    case JSVT_INT_VECTOR:
    case JSVT_U_INT_VECTOR:
    case JSVT_LONG_VECTOR:
    case JSVT_U_LONG_VECTOR:
    case JSVT_LLONG_VECTOR:
    case JSVT_U_LLONG_VECTOR:
        return(1);
    }
    return(0);
}

static void jsVarUpdateWatch(struct jsVar *v, int index, int notifyForFastUpdate) {
    if (v == NULL) return;

    switch (v->varType) {
    case JSVT_CHAR:
    case JSVT_U_CHAR:
        JSVARS_UPDATE_WATCH(v, JSVT_U_CHAR_TCODE, Char);
        break;
    case JSVT_SHORT:
    case JSVT_U_SHORT:
        JSVARS_UPDATE_WATCH(v, JSVT_U_SHORT_TCODE, Short);
        break;
    case JSVT_INT:
    case JSVT_U_INT:
        JSVARS_UPDATE_WATCH(v, JSVT_U_INT_TCODE, Int);
        break;
    case JSVT_LONG:
    case JSVT_U_LONG:
        JSVARS_UPDATE_WATCH(v, JSVT_U_LONG_TCODE, Long);
        break;
    case JSVT_LLONG:
    case JSVT_U_LLONG:
        JSVARS_UPDATE_WATCH(v, JSVT_U_LLONG_TCODE, Llong);
        break;
    case JSVT_CHAR_VECTOR:
    case JSVT_U_CHAR_VECTOR:
        JSVARS_UPDATE_WATCH_VECTOR(v, index, JSVT_U_CHAR_TCODE, Char);
        break;
    case JSVT_SHORT_VECTOR:
    case JSVT_U_SHORT_VECTOR:
        JSVARS_UPDATE_WATCH_VECTOR(v, index, JSVT_U_SHORT_TCODE, Short);
        break;
    case JSVT_INT_VECTOR:
    case JSVT_U_INT_VECTOR:
        JSVARS_UPDATE_WATCH_VECTOR(v, index, JSVT_U_INT_TCODE, Int);
        break;
    case JSVT_LONG_VECTOR:
    case JSVT_U_LONG_VECTOR:
        JSVARS_UPDATE_WATCH_VECTOR(v, index, JSVT_U_LONG_TCODE, Long);
        break;
    case JSVT_LLONG_VECTOR:
    case JSVT_U_LLONG_VECTOR:
        JSVARS_UPDATE_WATCH_VECTOR(v, index, JSVT_U_LLONG_TCODE, Llong);
        break;
    default:
        printf("%s:%s:%d: Jsvar %s: watch type %d is not implemented yet\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, v->jsname, v->varType);
        return;
    }   
}

static void jsVarMaybeUpdateWatchDimension(struct jsVar *v, int newDim) {
    int i, dim;

    if (v == NULL) return;
    dim = *v->vectorDimensionAddr;

    if (dim == newDim) return;

    switch (v->varType) {
    case JSVT_CHAR:
    case JSVT_U_CHAR:
    case JSVT_SHORT:
    case JSVT_U_SHORT:
    case JSVT_INT:
    case JSVT_U_INT:
    case JSVT_LONG:
    case JSVT_U_LONG:
    case JSVT_LLONG:
    case JSVT_U_LLONG:
        break;
    case JSVT_CHAR_VECTOR:
    case JSVT_U_CHAR_VECTOR:
        JSVARS_UPDATE_WATCH_VECTOR_DIMENSION(v, dim, newDim, JSVT_U_CHAR_TCODE);
        break;
    case JSVT_SHORT_VECTOR:
    case JSVT_U_SHORT_VECTOR:
        JSVARS_UPDATE_WATCH_VECTOR_DIMENSION(v, dim, newDim, JSVT_U_SHORT_TCODE);
        break;
    case JSVT_INT_VECTOR:
    case JSVT_U_INT_VECTOR:
        JSVARS_UPDATE_WATCH_VECTOR_DIMENSION(v, dim, newDim, JSVT_U_INT_TCODE);
        break;
    case JSVT_LONG_VECTOR:
    case JSVT_U_LONG_VECTOR:
        JSVARS_UPDATE_WATCH_VECTOR_DIMENSION(v, dim, newDim, JSVT_U_LONG_TCODE);
        break;
    case JSVT_LLONG_VECTOR:
    case JSVT_U_LLONG_VECTOR:
        JSVARS_UPDATE_WATCH_VECTOR_DIMENSION(v, dim, newDim, JSVT_U_LLONG_TCODE);
        break;
    default:
        printf("%s:%s:%d: Jsvar %s: wrong watch type %d.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, v->jsname, v->varType);
        return;
    }
    // if we are changing dimension, also update the variable itself
    if (newDim != 0) jsVarUpdateWatch(v, 0, 0);
}

#define JSVARS_DEFINE_SET_VAR_FUNCTIONS(tcode, funtid)                  \
    struct jsVar *jsVarCreateNewSynchronized##funtid(struct jsVarSpace *jss, char *javaScriptName) { \
        return(jsVarCreateNewSynchronized(jss, javaScriptName, JSVAR_TCODE_TYPE_ENUM(tcode), 1)); \
    }                                                                   \
    struct jsVar *jsVarCreateNewSynchronized##funtid##Input(struct jsVarSpace *jss, char *javaScriptName, void (*callBackFunction)(struct jsVar *v, int index, struct jsVaraio *jj)) { \
        return(jsVarCreateNewSynchronizedInput(jss, javaScriptName, JSVAR_TCODE_TYPE_ENUM(tcode), 1, callBackFunction)); \
    }                                                                   \
    struct jsVar *jsVarSynchronize##funtid(struct jsVarSpace *jss, char *javaScriptName, JSVAR_TCODE_TYPE(tcode) *v) { \
        return(jsVarSynchronize(jss, javaScriptName, JSVAR_TCODE_TYPE_ENUM(tcode), (void*)v, sizeof(int), 0, NULL, NULL, 1, NULL)); \
    }                                                                   \
    struct jsVar *jsVarSynchronize##funtid##Input(struct jsVarSpace *jss, char *javaScriptName, JSVAR_TCODE_TYPE(tcode) *v, void (*callBackFunction)(struct jsVar *v, int index, struct jsVaraio *jj)) { \
        return(jsVarSynchronizeInput(jss, javaScriptName, JSVAR_TCODE_TYPE_ENUM(tcode), (void*)v, sizeof(int), 0, NULL, NULL, 1, NULL, callBackFunction)); \
    }                                                                   \
    struct jsVar *jsVarCreateNewSynchronized##funtid##Array(struct jsVarSpace *jss, char *javaScriptName, int dimension) { \
        return(jsVarCreateNewSynchronized(jss, javaScriptName, JSVAR_TCODE_TYPE_ENUM(tcode##a), dimension)); \
    }                                                                   \
    struct jsVar *jsVarCreateNewSynchronized##funtid##ArrayInput(struct jsVarSpace *jss, char *javaScriptName, int dimension, void (*callBackFunction)(struct jsVar *v, int index, struct jsVaraio *jj)) { \
        return(jsVarCreateNewSynchronizedInput(jss, javaScriptName, JSVAR_TCODE_TYPE_ENUM(tcode##a), dimension, callBackFunction)); \
    }                                                                   \
    struct jsVar *jsVarSynchronize##funtid##Array(struct jsVarSpace *jss, char *javaScriptName, JSVAR_TCODE_TYPE(tcode) *v, int arrayElementSize, int dimension) { \
        return(jsVarSynchronize(jss, javaScriptName, JSVAR_TCODE_TYPE_ENUM(tcode##a), (void*)v, arrayElementSize, 0, NULL, NULL, dimension, NULL)); \
    }                                                                   \
    struct jsVar *jsVarSynchronize##funtid##ArrayInput(struct jsVarSpace *jss, char *javaScriptName, JSVAR_TCODE_TYPE(tcode) *v, int arrayElementSize, int dimension, void (*callBackFunction)(struct jsVar *v, int index, struct jsVaraio *jj)) { \
        return(jsVarSynchronizeInput(jss, javaScriptName, JSVAR_TCODE_TYPE_ENUM(tcode##a), (void*)v, arrayElementSize, 0, NULL, NULL, dimension, NULL, callBackFunction)); \
    }                                                                   \
    struct jsVar *jsVarCreateNewSynchronized##funtid##Vector(struct jsVarSpace *jss, char *javaScriptName) { \
        return(jsVarCreateNewSynchronized(jss, javaScriptName, JSVAR_TCODE_TYPE_ENUM(tcode##v), 0)); \
    }                                                                   \
    struct jsVar *jsVarCreateNewSynchronized##funtid##VectorInput(struct jsVarSpace *jss, char *javaScriptName, void (*callBackFunction)(struct jsVar *v, int index, struct jsVaraio *jj)) { \
        return(jsVarCreateNewSynchronizedInput(jss, javaScriptName, JSVAR_TCODE_TYPE_ENUM(tcode##v), 0, callBackFunction)); \
    }                                                                   \
    struct jsVar *jsVarSynchronize##funtid##Vector(struct jsVarSpace *jss, char *javaScriptName, void *v, int vectorElementSize, int vectorElementOffset, int *dimension) { \
        return(jsVarSynchronize(jss, javaScriptName, JSVAR_TCODE_TYPE_ENUM(tcode##v), (void*)v, vectorElementSize, vectorElementOffset, NULL, NULL, *dimension, dimension)); \
    }                                                                   \
    struct jsVar *jsVarSynchronize##funtid##VectorInput(struct jsVarSpace *jss, char *javaScriptName, void *v, int vectorElementSize, int vectorElementOffset, int *dimension, void (*callBackFunction)(struct jsVar *v, int index, struct jsVaraio *jj)) { \
        return(jsVarSynchronizeInput(jss, javaScriptName, JSVAR_TCODE_TYPE_ENUM(tcode##v), (void*)v, vectorElementSize, vectorElementOffset, NULL, NULL, *dimension, dimension, callBackFunction)); \
    }                                                                   \
    struct jsVar *jsVarSynchronize##funtid##List(struct jsVarSpace *jss, char *javaScriptName, void *v, JSVAR_TCODE_TYPE(tcode) *(*valueFromElemFunction)(void *), void *(*nextElementFunction)(void *)) { \
        return(jsVarSynchronize(jss, javaScriptName, JSVAR_TCODE_TYPE_ENUM(tcode##li), (void*)v, 0, 0, (void * (*)(void *))valueFromElemFunction, nextElementFunction, 0, NULL)); \
    }                                                                   \
    void jsVarNoteUp##funtid(struct jsVar *v) {                         \
        /* following line of code was thinked as an optimization, unfortunately it is wrong for strings */ \
        /* which can have the same pointer, but the string inside changed */ \
        /* if (v->lastSentVal.tcode == JSVAR_ELEM_VALUE(v, tcode)) return; */ \
        if (v == NULL) return;                                          \
        if (v->varType != JSVAR_TCODE_TYPE_ENUM(tcode)) {               \
            printf("%s: Variable type mismatch: %s\n", JSVAR_PRINT_PREFIX(), v->jsname); \
            return;                                                     \
        }                                                               \
        if (v->previousInUpdateList == NULL) {                          \
            assert(v->varSpace != NULL);                                \
            SGLIB_DL_LIST_ADD_AFTER(struct jsVar, v->varSpace->jsVarsUpdatedList, v, previousInUpdateList, nextInUpdateList); \
            if (v->watchVariable != NULL) jsVarUpdateWatch(v->watchVariable, 0, 1); \
        }                                                               \
    }                                                                   \
    void jsVarNoteUp##funtid##Array(struct jsVar *v, int index) {       \
        /* TODO: SImplify this for array, it was taken from vector */   \
        if (v == NULL) return;                                          \
        if (v->varType != JSVAR_TCODE_TYPE_ENUM(tcode##a)) {            \
            printf("%s: Variable type mismatch: %s\n", JSVAR_PRINT_PREFIX(), v->jsname); \
            return;                                                     \
        }                                                               \
        if (*v->vectorDimensionAddr <= index) {                         \
            printf("%s: jsVarNoteUp%sArray: Index %d larger than dimension %d of %s\n", JSVAR_PRINT_PREFIX(), #funtid, index, *v->vectorDimensionAddr, v->jsname); \
        }                                                               \
        if (index < v->lastSentArrayDimension && v->lastSentVal.tcode##a[index] == JSVAR_ARRAY_ELEM_VALUE(v, tcode, index)) return; \
        jsVarInsertToVarSpaceUpdateList(v);								\
        if (v->modifiedIndexes.a == NULL) jsVarIndexSetInit(&v->modifiedIndexes, v->lastSentArrayDimension); \
        if (*v->vectorDimensionAddr > v->modifiedIndexes.bitsDim) jsVarIndexSetResize(&v->modifiedIndexes, *v->vectorDimensionAddr); \
        jsVarIndexSetAdd(&v->modifiedIndexes, index);                   \
        if (v->watchVariable != NULL) {                                 \
            jsVarMaybeUpdateWatchDimension(v->watchVariable, *v->vectorDimensionAddr); \
            jsVarUpdateWatch(v->watchVariable, index, 1);               \
        }                                                               \
    }                                                                   \
    void jsVarNoteUp##funtid##Vector(struct jsVar *v, int index) {      \
        if (v == NULL) return;                                          \
        if (v->varType != JSVAR_TCODE_TYPE_ENUM(tcode##v)) {            \
            printf("jsVarGet: Variable type mismatch: %s\n", v->jsname); \
            return;                                                     \
        }                                                               \
        if (*v->vectorDimensionAddr <= index) {                         \
            printf("%s: jsVarNoteUp%sArray: Index %d larger than dimension %d of %s\n", JSVAR_PRINT_PREFIX(), #funtid, index, *v->vectorDimensionAddr, v->jsname); \
        }                                                               \
        if (index < v->lastSentArrayDimension && v->lastSentVal.tcode##a[index] == JSVAR_VECTOR_ELEM_VALUE(v, tcode, index)) return; \
        jsVarInsertToVarSpaceUpdateList(v);								\
        if (v->modifiedIndexes.a == NULL) jsVarIndexSetInit(&v->modifiedIndexes, v->lastSentArrayDimension); \
        if (*v->vectorDimensionAddr > v->modifiedIndexes.bitsDim) jsVarIndexSetResize(&v->modifiedIndexes, *v->vectorDimensionAddr); \
        jsVarIndexSetAdd(&v->modifiedIndexes, index);                   \
        if (v->watchVariable != NULL) {                                 \
            jsVarMaybeUpdateWatchDimension(v->watchVariable, *v->vectorDimensionAddr); \
            jsVarUpdateWatch(v->watchVariable, index, 1);               \
        }                                                               \
    }                                                                   \
    void jsVarSet##funtid(struct jsVar *v, JSVAR_TCODE_TYPE(tcode) val) { \
        if (v == NULL) return;                                          \
        if (v->varType != JSVAR_TCODE_TYPE_ENUM(tcode)) {				\
            printf("%s: Error: jsVarSet"#funtid": variable type mismatch: %s\n", JSVAR_PRINT_PREFIX(), v->jsname); \
            return;                                                     \
        }                                                               \
        *((JSVAR_TCODE_TYPE(tcode) *) (v->valptr)) = val;               \
        jsVarNoteUp##funtid(v);                                         \
    }                                                                   \
    void jsVarSet##funtid##Array(struct jsVar *v, int index, JSVAR_TCODE_TYPE(tcode) val) { \
        if (v == NULL) return;                                          \
        if (v->varType != JSVAR_TCODE_TYPE_ENUM(tcode##a)) {            \
            printf("%s: Error: jsVarSet"#funtid"Array: variable type mismatch: %s\n", JSVAR_PRINT_PREFIX(), v->jsname); \
            return;                                                     \
        }                                                               \
        JSVAR_ARRAY_ELEM_VALUE(v, tcode, index) = val;                  \
        jsVarNoteUp##funtid##Array(v, index);                           \
    }                                                                   \
    void jsVarSet##funtid##Vector(struct jsVar *v, int index, JSVAR_TCODE_TYPE(tcode) val) { \
        if (v == NULL) return;                                          \
        if (v->varType != JSVAR_TCODE_TYPE_ENUM(tcode##v)) {            \
            printf("%s: Error: jsVarSet"#funtid"Vector: variable type mismatch: %s\n", JSVAR_PRINT_PREFIX(), v->jsname); \
            return;                                                     \
        }                                                               \
        JSVAR_VECTOR_ELEM_VALUE(v, tcode, index) = val;                 \
        jsVarNoteUp##funtid##Vector(v, index);                          \
    }                                                                   \
    JSVAR_TCODE_TYPE(tcode) jsVarGet##funtid(struct jsVar *v) {         \
        if (v == NULL) return((JSVAR_TCODE_TYPE(tcode))0);              \
        if (v->varType != JSVAR_TCODE_TYPE_ENUM(tcode)) {               \
            printf("%s: Error: jsVarGet"#funtid": variable type mismatch: %s\n", JSVAR_PRINT_PREFIX(), v->jsname); \
            return((JSVAR_TCODE_TYPE(tcode))0);                         \
        }                                                               \
        if (v->valptr == NULL) return((JSVAR_TCODE_TYPE(tcode))0);      \
        return(JSVAR_ELEM_VALUE(v, tcode));                             \
    }                                                                   \
    JSVAR_TCODE_TYPE(tcode) jsVarGet##funtid##Array(struct jsVar *v, int index) { \
        if (v == NULL) return((JSVAR_TCODE_TYPE(tcode))0);              \
        if (v->varType != JSVAR_TCODE_TYPE_ENUM(tcode##a)) {            \
            printf("%s: Error: jsVarGet"#funtid"Array: variable type mismatch: %s\n", JSVAR_PRINT_PREFIX(), v->jsname); \
            return((JSVAR_TCODE_TYPE(tcode))0);                         \
        }                                                               \
        if (v->valptr == NULL) return((JSVAR_TCODE_TYPE(tcode))0);      \
        return(JSVAR_ARRAY_ELEM_VALUE(v, tcode, index));                \
    }                                                                   \
    JSVAR_TCODE_TYPE(tcode) jsVarGet##funtid##Vector(struct jsVar *v, int index) { \
        if (v == NULL) return((JSVAR_TCODE_TYPE(tcode))0);              \
        if (v->varType != JSVAR_TCODE_TYPE_ENUM(tcode##v)) {            \
            printf("%s: Error: jsVarGet"#funtid"Vector: variable type mismatch: %s\n", JSVAR_PRINT_PREFIX(), v->jsname); \
            return((JSVAR_TCODE_TYPE(tcode))0);                         \
        }                                                               \
        if (v->valptr == NULL) return((JSVAR_TCODE_TYPE(tcode))0);      \
        return(JSVAR_VECTOR_ELEM_VALUE(v, tcode, index));               \
    }                                                                   \
    int jsVarSet##funtid##Addr(JSVAR_TCODE_TYPE(tcode) *a, JSVAR_TCODE_TYPE(tcode) val) { \
        unsigned                        h;                              \
        struct jsVar                    *v, vv;                         \
        struct jsVarAddressTable        *tt;                            \
        struct sglib_jsVarl_iterator    ii;                             \
                                                                        \
        tt = &jsVarsAddr;                                               \
        vv.varType = JSVAR_TCODE_TYPE_ENUM(tcode);                      \
        vv.valptr = a;                                                  \
        h = JSVAR_ADDRESS_HASH_FUN(a) % tt->dim;                        \
        for (v = sglib_jsVarl_it_init_on_equal(&ii, tt->t[h], jsVarAddressComparator, &vv); \
             v != NULL;                                                 \
             v = sglib_jsVarl_it_next(&ii)                              \
            ) {                                                         \
            jsVarSet##funtid(v, val);                                   \
        }                                                               \
        return(0);                                                      \
    }                                                                   \
    int jsVarUpdate##funtid##Addr(JSVAR_TCODE_TYPE(tcode) *a) {         \
        unsigned                        h;                              \
        struct jsVar                    *v, vv;                         \
        struct jsVarAddressTable        *tt;                            \
        struct sglib_jsVarl_iterator    ii;                             \
                                                                        \
        tt = &jsVarsAddr;                                               \
        vv.varType = JSVAR_TCODE_TYPE_ENUM(tcode);                      \
        vv.valptr = a;                                                  \
        h = JSVAR_ADDRESS_HASH_FUN(a) % tt->dim;                        \
        for (v = sglib_jsVarl_it_init_on_equal(&ii, tt->t[h], jsVarAddressComparator, &vv); \
             v != NULL;                                                 \
             v = sglib_jsVarl_it_next(&ii)                              \
            ) {                                                         \
            jsVarSendVariableUpdate(v, 0, 0);                           \
        }                                                               \
        return(0);                                                      \
    }

// TODO: Make special care for Strings, create a copy of string at jsVarSetString, free it if it was set before and do not copy when sending update
JSVARS_DEFINE_SET_VAR_FUNCTIONS(i,Int);
JSVARS_DEFINE_SET_VAR_FUNCTIONS(w,Short);
JSVARS_DEFINE_SET_VAR_FUNCTIONS(b,Char);
JSVARS_DEFINE_SET_VAR_FUNCTIONS(l,Long);
JSVARS_DEFINE_SET_VAR_FUNCTIONS(ll,Llong);
JSVARS_DEFINE_SET_VAR_FUNCTIONS(f,Float);
JSVARS_DEFINE_SET_VAR_FUNCTIONS(d,Double);
JSVARS_DEFINE_SET_VAR_FUNCTIONS(s,String);
JSVARS_DEFINE_SET_VAR_FUNCTIONS(ws,Wstring);
JSVARS_DEFINE_SET_VAR_FUNCTIONS(ui,Uint);
JSVARS_DEFINE_SET_VAR_FUNCTIONS(uw,Ushort);
JSVARS_DEFINE_SET_VAR_FUNCTIONS(ub,Uchar);
JSVARS_DEFINE_SET_VAR_FUNCTIONS(ul,Ulong);
JSVARS_DEFINE_SET_VAR_FUNCTIONS(ull,Ullong);

void jsVarNoteUpdate(struct jsVar *v) {

    if (v == NULL) return;

    switch (v->varType) {
    case JSVT_CHAR:
        jsVarNoteUpChar(v);
        break;
    case JSVT_U_CHAR:
        jsVarNoteUpUchar(v);
        break;
    case JSVT_SHORT:
        jsVarNoteUpShort(v);
        break;
    case JSVT_U_SHORT:
        jsVarNoteUpUshort(v);
        break;
    case JSVT_INT:
        jsVarNoteUpInt(v);
        break;
    case JSVT_U_INT:
        jsVarNoteUpUint(v);
        break;
    case JSVT_LONG:
        jsVarNoteUpLong(v);
        break;
    case JSVT_U_LONG:
        jsVarNoteUpUlong(v);
        break;
    case JSVT_LLONG:
        jsVarNoteUpLlong(v);
        break;
    case JSVT_U_LLONG:
        jsVarNoteUpUllong(v);
        break;
    case JSVT_FLOAT:
        jsVarNoteUpFloat(v);
        break;
    case JSVT_DOUBLE:
        jsVarNoteUpDouble(v);
        break;
    case JSVT_STRING:
        jsVarNoteUpString(v);
        break;
    case JSVT_WSTRING:
        jsVarNoteUpWstring(v);
        break;
    default:
        if (jsvarIsArrayType(v->varType)) {
            printf("%s:%s:%d: jsVarNoteUp: Error: %s wrong type. Use jsVarUpArray to update array variable\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, v->jsname);
        } else if (jsvarIsVectorType(v->varType)) {
            printf("%s:%s:%d: jsVarNoteUp: Error: %s wrong type. Use jsVarUpVector to update vector variable.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, v->jsname);
        } else {
            printf("%s:%s:%d: Jsvar %s: type %d does not support incremental updates\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, v->jsname, v->varType);
        }
        return;
    }
}


static void jsVarNoteUpIndexedType(struct jsVar *v, int index) {

    if (v == NULL) return;

    switch (v->varType) {
    case JSVT_CHAR_ARRAY:
        jsVarNoteUpCharArray(v, index);
        break;
    case JSVT_CHAR_VECTOR:
        jsVarNoteUpCharVector(v, index);
        break;
    case JSVT_U_CHAR_ARRAY:
        jsVarNoteUpUcharArray(v, index);
        break;
    case JSVT_U_CHAR_VECTOR:
        jsVarNoteUpUcharVector(v, index);
        break;
    case JSVT_SHORT_ARRAY:
        jsVarNoteUpShortArray(v, index);
        break;
    case JSVT_SHORT_VECTOR:
        jsVarNoteUpShortVector(v, index);
        break;
    case JSVT_U_SHORT_ARRAY:
        jsVarNoteUpUshortArray(v, index);
        break;
    case JSVT_U_SHORT_VECTOR:
        jsVarNoteUpUshortVector(v, index);
        break;
    case JSVT_INT_ARRAY:
        jsVarNoteUpIntArray(v, index);
        break;
    case JSVT_INT_VECTOR:
        jsVarNoteUpIntVector(v, index);
        break;
    case JSVT_U_INT_ARRAY:
        jsVarNoteUpUintArray(v, index);
        break;
    case JSVT_U_INT_VECTOR:
        jsVarNoteUpUintVector(v, index);
        break;
    case JSVT_LONG_ARRAY:
        jsVarNoteUpLongArray(v, index);
        break;
    case JSVT_LONG_VECTOR:
        jsVarNoteUpLongVector(v, index);
        break;
    case JSVT_U_LONG_ARRAY:
        jsVarNoteUpUlongArray(v, index);
        break;
    case JSVT_U_LONG_VECTOR:
        jsVarNoteUpUlongVector(v, index);
        break;
    case JSVT_LLONG_ARRAY:
        jsVarNoteUpLlongArray(v, index);
        break;
    case JSVT_LLONG_VECTOR:
        jsVarNoteUpLlongVector(v, index);
        break;
    case JSVT_U_LLONG_ARRAY:
        jsVarNoteUpUllongArray(v, index);
        break;
    case JSVT_U_LLONG_VECTOR:
        jsVarNoteUpUllongVector(v, index);
        break;
    case JSVT_FLOAT_ARRAY:
        jsVarNoteUpFloatArray(v, index);
        break;
    case JSVT_FLOAT_VECTOR:
        jsVarNoteUpFloatVector(v, index);
        break;
    case JSVT_DOUBLE_ARRAY:
        jsVarNoteUpDoubleArray(v, index);
        break;
    case JSVT_DOUBLE_VECTOR:
        jsVarNoteUpDoubleVector(v, index);
        break;
    case JSVT_STRING_ARRAY:
        jsVarNoteUpStringArray(v, index);
        break;
    case JSVT_STRING_VECTOR:
        jsVarNoteUpStringVector(v, index);
        break;
    case JSVT_WSTRING_ARRAY:
        jsVarNoteUpWstringArray(v, index);
        break;
    case JSVT_WSTRING_VECTOR:
        jsVarNoteUpWstringVector(v, index);
        break;
    default:
        printf("%s:%s:%d: Jsvar %s: type %d does not support indexed incremental updates.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, v->jsname, v->varType);
        return;
    }
}


void jsVarNoteUpdateArray(struct jsVar *v, int index) {

    if (v == NULL) return;
    if (! jsvarIsArrayType(v->varType)) {
        printf("%s:%s:%d: jsVarNoteUpArray: Error: %s is not of array type.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, v->jsname);
        return;
    }
    jsVarNoteUpIndexedType(v, index);
}

void jsVarNoteUpdateVector(struct jsVar *v, int index) {

    if (v == NULL) return;
    if (! jsvarIsVectorType(v->varType)) {
        printf("%s:%s:%d: jsVarNoteUpArray: Error: %s is not of array type.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, v->jsname);
        return;
    }
    jsVarNoteUpIndexedType(v, index);
}

/////////////////////////////////////////////////////////////////////////////
// updating

static struct jsVarDynamicString *jsVarsCreateEncodedString(char *val) {
    struct jsVarDynamicString   *res;
    unsigned char               *s;
    char                        ttt[JSVAR_TMP_STRING_SIZE];
    int                         tttlen;

    res = jsVarDstrCreate();
    for(s=(unsigned char*)val; *s; s++) {
        // TODO: Do this without double copying
        tttlen = jsVarEncodeNumber(*s, 0, ttt);
        jsVarDstrAppendData(res, ttt, tttlen);
    }
    return(res);
}

static struct jsVarDynamicString *jsVarsCreateEncodedWstring(wchar_t *val) {
    struct jsVarDynamicString   *res;
    wchar_t                     *s;
    int                         i, n, n2, d;
    char                        ttt[JSVAR_TMP_STRING_SIZE];
    int                         tttlen;

    res = jsVarDstrCreate();
    for(s=val; *s; s++) {
        // I am not sure about signedness of wchar_t when converted to int
        n = *s;
#if _WIN32
#if JSVAR_WCHAR_T_UTF16
        // UTF-16 encoding
        if (n >= 0xD800 && n <= 0xDBFF) {
            n2 = *(s+1);
            if (n2 >= 0xDC00 && n2 <= 0xDFFF) {
                n = (n << 10) + n2 - 0x35FDC00; 
                s ++;
            }
        }
#elif JSVAR_WCHAR_T_UTF8
        // UTF-8 encoding
        d = 0;
        if (n < 0x80) {
            d = 0; 
            n = n & 0x7F; 
        } else if ((n & 0xE0) == 0xC0) {
            d = 1;
            n = n & 0x1F;
        } else if ((n & 0xF0) == 0xE0) {
            d = 2;
            n = n & 0x0F;
        } else if ((n & 0xF8) == 0xF0) {
            d = 3;
            n = n & 0x07;
        } else if ((n & 0xFC) == 0xF8) {
            d = 4;  
            n = n & 0x03;
        } else if ((n & 0xFE) == 0xFC) {
            d = 5;
            n = n & 0x01;
        }
        for(i=0; i<d; i++, s++) {
            if ((s[1] & 0xC0) != 0x80) break;
            n = (n << 6) + (s[1] & 0x3F);
        }
#endif
#endif
        tttlen = jsVarEncodeNumber(labs(n), (n<0), ttt);
        jsVarDstrAppendData(res, ttt, tttlen);
    }
    return(res);
}

int jsVarSendVEval(struct jsVaraio *jj, char *fmt, va_list arg_ptr) {
    int                         len, tttlen;
    char                        ttt[JSVAR_TMP_STRING_SIZE];
    struct jsVarDynamicString   *ss, *sse;

    ss = jsVarDstrCreateByVPrintf(fmt, arg_ptr);
    sse = jsVarsCreateEncodedString(ss->s);

    ttt[0] = JSVAR_MSG_V_TYPE_CODE_EVAL;
    tttlen = 1 + jsVarEncodeNumber(sse->size, 0, ttt+1);
    baioWriteToBuffer(&jj->w.b, ttt, tttlen);
    len = baioWriteToBuffer(&jj->w.b, sse->s, sse->size);
    wsaioWebsocketCompleteMessage(&jj->w);
    jsVarDstrFree(&sse);
    jsVarDstrFree(&ss);
	return(len);
}

int jsVarSendEval(struct jsVaraio *jj, char *fmt, ...) {
	int				res;
    va_list         arg_ptr;

    if (jj == NULL) return(-1);
    va_start(arg_ptr, fmt);
    res = jsVarSendVEval(jj, fmt, arg_ptr);
    va_end(arg_ptr);
	return(res);
}

static int jsVarWriteVariableNameIfNotWritten(struct jsVaraio *jj, struct jsVar *v, char *arrayResizeFlag) {
    int     vTypeCode, r, n;
    n = 0;
    if (jj->variableNameWrittenFlag == 0) {
        vTypeCode = jsVarMessageAndVariableTypeCode(v);
        r = baioWriteToBuffer(&jj->w.b, v->compressedCodeLenAndJsname, v->compressedCodeLenAndJsnameLength);
        if (r < 0) return(r);
        n += r;
        r = baioWriteToBuffer(&jj->w.b, arrayResizeFlag, 1);
        // r = baioPrintfToBuffer(&jj->w.b, "%c%s%s%c", vTypeCode, v->jsnameLenCode, v->jsname, (arrayResizeFlag?'<':'='));
        if (r < 0) return(r);
        n += r;
        jj->variableNameWrittenFlag = 1;
    }
    return(n);
}


static struct jsVaraio *jsVarsApplyOperationOnBaio(int baioMagic, struct jsVar *v, int index, int operation, va_list arg_ptr) {
#define JSVARSAPPLYOPERATIONONBAIO_RETURN_AFTER_FAIL() {failline = __LINE__; goto returnAfterFail;}
    struct jsVarIndexSet        *ee;
    struct jsVaraio             *jj;
    int                         r, msgLen;
    char                        *msg;
    int                         (*jsvarFilterPerConnection)(struct jsVaraio *ww, struct jsVar *v, int index);
    char                        ttt[JSVAR_TMP_STRING_SIZE];
    int                         tttlen;
    uint64_t                    val, dimension;
    int                         valsign, failline;

    jj = (struct jsVaraio *) baioFromMagic(baioMagic);
    if (jj != NULL) {
        jsvarFilterPerConnection = jj->filterFunction;
        if (jsvarFilterPerConnection == NULL || index < 0 || jsvarFilterPerConnection(jj, v, index) == 0) {
            switch (operation) {
            case JSVMOFO_INIT_MESSAGE:
                jj->variableNameWrittenFlag = 0;
                break;
            case JSVMOFO_WRITE_INDEX_LEN_AND_STRING_TO_BUFFER:
                r = jsVarWriteVariableNameIfNotWritten(jj, v, "=");
                if (r < 0) JSVARSAPPLYOPERATIONONBAIO_RETURN_AFTER_FAIL();

                tttlen = jsVarEncodeNumber(index, 0, ttt);
                r = baioWriteToBuffer(&jj->w.b, ttt, tttlen);
                if (r < 0) JSVARSAPPLYOPERATIONONBAIO_RETURN_AFTER_FAIL();

                msgLen = va_arg(arg_ptr, int);
                tttlen = jsVarEncodeNumber(msgLen, 0, ttt);
                r = baioWriteToBuffer(&jj->w.b, ttt, tttlen);
                if (r < 0) JSVARSAPPLYOPERATIONONBAIO_RETURN_AFTER_FAIL();

                msg = va_arg(arg_ptr, char *);
                r = baioWriteToBuffer(&jj->w.b, msg, msgLen);
                if (r < 0) JSVARSAPPLYOPERATIONONBAIO_RETURN_AFTER_FAIL();
                break;
            case JSVMOFO_WRITE_INDEX_AND_LLONG_VALUE:
                r = jsVarWriteVariableNameIfNotWritten(jj, v, "=");
                if (r < 0) JSVARSAPPLYOPERATIONONBAIO_RETURN_AFTER_FAIL();

                tttlen = jsVarEncodeNumber(index, 0, ttt);
                r = baioWriteToBuffer(&jj->w.b, ttt, tttlen);
                if (r < 0) JSVARSAPPLYOPERATIONONBAIO_RETURN_AFTER_FAIL();

                val = va_arg(arg_ptr, unsigned long long);
                valsign = va_arg(arg_ptr, int);
                tttlen = jsVarEncodeNumber(val, valsign, ttt);
                r = baioWriteToBuffer(&jj->w.b, ttt, tttlen);
                if (r < 0) JSVARSAPPLYOPERATIONONBAIO_RETURN_AFTER_FAIL();
                // if (jsVarDebugLevel > 40) printf("%s: Written update of %s: new value %"PRIu64"\n", JSVAR_PRINT_PREFIX(), v->jsname, (valsign?-val:val));
                break;
            case JSVMOFO_WRITE_ARRAY_RESIZE:
                r = jsVarWriteVariableNameIfNotWritten(jj, v, "<");
                if (r < 0) JSVARSAPPLYOPERATIONONBAIO_RETURN_AFTER_FAIL();

                dimension = va_arg(arg_ptr, int);
                tttlen = jsVarEncodeNumber(dimension, 0, ttt);
                r = baioWriteToBuffer(&jj->w.b, ttt, tttlen);
                if (r < 0) JSVARSAPPLYOPERATIONONBAIO_RETURN_AFTER_FAIL();
                break;
            case JSVMOFO_WRITE_VALUES_TERMINAL_CHAR:
                if (jj->variableNameWrittenFlag) {
                    r = baioPrintfToBuffer(&jj->w.b, "%c", 127);
                    if (r < 0) JSVARSAPPLYOPERATIONONBAIO_RETURN_AFTER_FAIL();
                }
                break;
            case JSVMOFO_COMPLETE_MSG:
                if (jj->variableNameWrittenFlag) {
                    wsaioWebsocketCompleteMessage(&jj->w);
                }
                break;
            case JSVMOFO_ADD_TO_INDEX_SET:
                if (jj->variableNameWrittenFlag) {
                    ee = va_arg(arg_ptr, struct jsVarIndexSet *);
                    assert(ee != NULL);
                    jsVarIndexSetAdd(ee, jj->w.b.index);
                }
                break;
            default:
                printf("%s: %s:%d: Invalid operation code %d\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, operation);
            }
        }
    }
    return(jj);

returnAfterFail:
    // probably write buffer full
    printf("%s: %s:%d: Jsvar Closing connection (magic %d). Write buffer full.\n", JSVAR_PRINT_PREFIX(), __FILE__, failline, baioMagic);
	fflush(stdout); assert(0);
    baioCloseMagicOnError(baioMagic);
    return(NULL);
    
}

// if baioMagic > 0 send to the baioMagic only
// if baioMagic == 0, send to all followers
// if baioMagic < 0 sent to all followers except -baioMagic
static void jsVarsMapOnFollowers(struct jsVar *v, int filterIndex, int baioMagic, int operation, ...) {
    va_list             arg_ptr, arg_ptr_copy;
    int                 i, c, bm;
    struct jsVaraio     *ww;

    va_start(arg_ptr, operation);
    if (jsVarDebugLevel > 20) printf("%s: Update of %s to %d requested.\n", JSVAR_PRINT_PREFIX(), v->jsname, baioMagic);
    if (baioMagic > 0) {
        ww = jsVarsApplyOperationOnBaio(baioMagic, v, filterIndex, operation, arg_ptr);
    } else {
        for(c=i=0; i<v->baioMagicsSize && c<v->baioMagicsElemCount; i++) {
            bm = v->baioMagics[i];
            if (jsVarDebugLevel > 20) printf("%s: Updating %s to v->baioMagics[%d] == %d.\n", JSVAR_PRINT_PREFIX(), v->jsname, i, bm);
            if (bm != 0) {
                if (bm != -baioMagic) {
                    va_copy(arg_ptr_copy, arg_ptr);
                    ww = jsVarsApplyOperationOnBaio(bm, v, filterIndex, operation, arg_ptr_copy);
                    va_copy_end(arg_ptr_copy);
                    // no longer connected to this stream, remove from followers
                    if (ww == NULL) jsVarUnBindConnection(v, bm);
                }
                // I have to recheck the original value v->baioMagics[i] (instead of bm), 
                // because connection could be closed in between
                if (v->baioMagics[i] != 0) c ++;
            }
        }
    }
    va_end(arg_ptr);
}

static void jsVarsFinishFollowersMsg(struct jsVar *v, int baioMagic, ...) {
    va_list         arg_ptr;

    va_start(arg_ptr, baioMagic);
    jsVarsMapOnFollowers(v, -1, baioMagic, JSVMOFO_COMPLETE_MSG);
    va_end(arg_ptr);
}
static void jsVarsAddFollowersBaioToIndexSet(struct jsVar *v, int baioMagic, struct jsVarIndexSet *ee) {
    jsVarsMapOnFollowers(v, -1, baioMagic, JSVMOFO_ADD_TO_INDEX_SET, ee);
}

#if 0
static int jsVarsPrintfToFollowersMaybeFirstValue(struct jsVar *v, int index, int baioMagic, int arrayResizeFlag, char *fmt, ...) {
    va_list                     arg_ptr;
    int                         vTypeCode;
    struct jsVarDynamicString   *mm;

    // This is an optimization, if there is no attached connection, do not bother with formatting, compressing, etc.
    if (baioMagic <= 0 && v->baioMagicsElemCount == 0) return(1);

    va_start(arg_ptr, fmt);
    //mm = jsVarDstrCreateByVPrintf(fmt, arg_ptr);
    //jsVarCompressMystr(mm, jsVarCompressionGlobal);
    //jsVarsMapOnFollowers(v, index, baioMagic, JSVMOFO_WRITE_TO_BUFFER, arrayResizeFlag, mm->size, mm->s, arg_ptr);
    jsVarsMapOnFollowers(v, index, baioMagic, arrayResizeFlag, JSVMOFO_PRINTF_TO_BUFFER, iiindex, 0, fmt, arg_ptr);
    //jsVarDstrFree(&mm);
    return(1);
}

static int jsVarsWriteToFollowersMaybeFirstValue(struct jsVar *v, int index, int baioMagic, int arrayResizeFlag, int index, int slen, char *s) {
    va_list                     arg_ptr;
    int                         vTypeCode;
    struct jsVarDynamicString   *mm;

    if (baioMagic > 0 || v->baioMagicsElemCount != 0) jsVarsMapOnFollowers(v, index, baioMagic, arrayResizeFlag, JSVMOFO_WRITE_INDEX_LEN_AND_STRING_TO_BUFFER, index, slen, s);
    return(1);
}

static int jsVarsWriteToFollowersIndexAndValue(struct jsVar *v, int index, int baioMagic, int arrayResizeFlag, int index, uint64 val, int valIsNegative) {
    va_list                     arg_ptr;
    int                         vTypeCode;
    struct jsVarDynamicString   *mm;

    if (baioMagic > 0 || v->baioMagicsElemCount != 0) jsVarsMapOnFollowers(v, index, baioMagic, arrayResizeFlag, JSVMOFO_WRITE_INDEX_LEN_AND_STRING_TO_BUFFER, index, slen, s);
    return(1);
}
#endif

static void jsVarMaybeSendArraySizeBeforeResize(struct jsVar *v, int dimension, int baioMagic) {
    if (baioMagic || dimension != v->lastSentArrayDimension) {
        // ttlen = sprintf(tt, "%d", dimension);
        JsVarInternalCheck(dimension >= 0);
        if (baioMagic > 0 || v->baioMagicsElemCount != 0) jsVarsMapOnFollowers(v, -1, baioMagic, JSVMOFO_WRITE_ARRAY_RESIZE, dimension);
        v->lastSentArrayDimension = dimension;
    }
}

#define JSVARS_MAYBE_REALLOCATE_ARRAY_AND_SEND_DIMENSION(tcode, dim, nullvalue) { \
        if (dim != v->lastSentArrayDimension) {                         \
            JSVAR_REALLOCC(v->lastSentVal.tcode##a, dim, JSVAR_TCODE_TYPE(tcode)); \
            for(i=v->lastSentArrayDimension; i<dim; i++) v->lastSentVal.tcode##a[i] = nullvalue; \
            if (v->modifiedIndexes.a != NULL) jsVarIndexSetResize(&v->modifiedIndexes, dim); \
            if (v->watchVariable != NULL) jsVarMaybeUpdateWatchDimension(v->watchVariable, dim); \
        }                                                               \
        jsVarMaybeSendArraySizeBeforeResize(v, dim, baioMagic);			\
    }

#define JSVARS_MAYBE_SEND_ARRAY_OR_VECTOR_DIMENSION_UPDATE(tcode, dim) {    \
        dim = *v->vectorDimensionAddr;                                  \
        JSVARS_MAYBE_REALLOCATE_ARRAY_AND_SEND_DIMENSION(tcode, dim, 0); \
    }


#define JSVARS_MAYBE_SEND_INTEGER_ELEM(acessString, tcode, i) {         \
        if (baioMagic || v->lastSentVal.acessString != val) {           \
            if (baioMagic <= 0 || firstBindingFlag) {                   \
                if (v->watchVariable != NULL) jsVarUpdateWatch(v->watchVariable, i, 0); \
                v->lastSentVal.acessString = val;                       \
            }                                                           \
            if (baioMagic > 0 || v->baioMagicsElemCount != 0) jsVarsMapOnFollowers(v, i, baioMagic, JSVMOFO_WRITE_INDEX_AND_LLONG_VALUE, (long long)val, (val<0)); \
        }                                                               \
    }

#define JSVARS_MAYBE_SEND_FLOAT_ELEM(accessString, tcode, i) {          \
        if (baioMagic || v->lastSentVal.accessString != val) {          \
            char tt[JSVAR_TMP_STRING_SIZE];                                 \
            int  ttlen;                                                 \
            if (baioMagic <= 0 || firstBindingFlag) {                   \
                if (v->watchVariable != NULL) jsVarUpdateWatch(v->watchVariable, i, 0); \
                v->lastSentVal.accessString = val;                      \
            }                                                           \
            ttlen = sprintf(tt, "%f", val);                             \
            if (baioMagic > 0 || v->baioMagicsElemCount != 0) jsVarsMapOnFollowers(v, i, baioMagic, JSVMOFO_WRITE_INDEX_LEN_AND_STRING_TO_BUFFER, ttlen, tt);\
        }                                                               \
    }

#define JSVARS_MAYBE_SEND_DOUBLE_ELEM(accessString, tcode, i) {         \
        if (baioMagic || v->lastSentVal.accessString != val) {          \
            char tt[JSVAR_TMP_STRING_SIZE];                                 \
            int  ttlen;                                                 \
            if (baioMagic <= 0 || firstBindingFlag) {                   \
                if (v->watchVariable != NULL) jsVarUpdateWatch(v->watchVariable, i, 0); \
                v->lastSentVal.accessString = val;                      \
            }                                                           \
            ttlen = sprintf(tt, "%f", val);                             \
            if (baioMagic > 0 || v->baioMagicsElemCount != 0) jsVarsMapOnFollowers(v, i, baioMagic, JSVMOFO_WRITE_INDEX_LEN_AND_STRING_TO_BUFFER, ttlen, tt);\
        }                                                               \
    }

#define JSVARS_MAYBE_SEND_STRING_ELEM(accessString, tcode, i) {         \
        if (val == NULL) val = "";                                      \
        if (baioMagic || v->lastSentVal.accessString == NULL || strcmp(v->lastSentVal.accessString, val) != 0) { \
            struct jsVarDynamicString *encodedStr;                      \
            if (baioMagic <= 0 || firstBindingFlag) {                   \
                if (val != v->lastSentVal.accessString) {               \
                    if (v->watchVariable != NULL) jsVarUpdateWatch(v->watchVariable, i, 0); \
                    JSVAR_FREE(v->lastSentVal.accessString);            \
                    v->lastSentVal.accessString = jsStrDuplicateAlwaysAlloc(val); \
                }                                                       \
            }                                                           \
            encodedStr = jsVarsCreateEncodedString(val);                \
            if (baioMagic > 0 || v->baioMagicsElemCount != 0) jsVarsMapOnFollowers(v, i, baioMagic, JSVMOFO_WRITE_INDEX_LEN_AND_STRING_TO_BUFFER, strlen(encodedStr->s), encodedStr->s); \
            jsVarDstrFree(&encodedStr);                                     \
        }                                                               \
    }

#define JSVARS_MAYBE_SEND_WSTRING_ELEM(accessString, tcode, i) {        \
        if (val == NULL) val = L"";                                     \
        if (baioMagic || v->lastSentVal.accessString == NULL || wcscmp(v->lastSentVal.accessString, val) != 0) { \
            struct jsVarDynamicString *encodedStr;                      \
            if (baioMagic <= 0 || firstBindingFlag) {                   \
                if (val != v->lastSentVal.accessString) {               \
                    if (v->watchVariable != NULL) jsVarUpdateWatch(v->watchVariable, i, 0); \
                    JSVAR_FREE(v->lastSentVal.accessString);            \
                    v->lastSentVal.accessString = jsWstrDuplicateAlwaysAlloc(val); \
                }                                                       \
            }                                                           \
            encodedStr = jsVarsCreateEncodedWstring(val);               \
            if (baioMagic > 0 || v->baioMagicsElemCount != 0) jsVarsMapOnFollowers(v, i, baioMagic, JSVMOFO_WRITE_INDEX_LEN_AND_STRING_TO_BUFFER, strlen(encodedStr->s), encodedStr->s); \
            jsVarDstrFree(&encodedStr);                                     \
        }                                                               \
    }


#define JSVARS_SEND_VARIABLE_UPDATE(tcode, msgVType) {      \
        JSVAR_TCODE_TYPE(tcode) val;                            \
        val = *((JSVAR_TCODE_TYPE(tcode) *)v->valptr);          \
        JSVARS_MAYBE_SEND_##msgVType##_ELEM(tcode, tcode, 0);   \
    }

#define JSVARS_SEND_ARRAY_VARIABLE_UPDATE(tcode, msgVType, ForLoopParameters) {         \
        int   i, j, dim;                                                    \
        JSVARS_MAYBE_SEND_ARRAY_OR_VECTOR_DIMENSION_UPDATE(tcode, dim);     \
        for ForLoopParameters {                                     \
            JSVAR_TCODE_TYPE(tcode) val = JSVAR_ARRAY_ELEM_VALUE(v, tcode, i); \
            JSVARS_MAYBE_SEND_##msgVType##_ELEM(tcode##a[i], tcode, i); \
        }                                                               \
    }

#define JSVARS_SEND_VECTOR_VARIABLE_UPDATE(tcode, msgVType, ForLoopParameters) {        \
        int   i, j, dim;                                                    \
        JSVARS_MAYBE_SEND_ARRAY_OR_VECTOR_DIMENSION_UPDATE(tcode, dim);     \
        for ForLoopParameters {                                     \
            JSVAR_TCODE_TYPE(tcode) val = JSVAR_VECTOR_ELEM_VALUE(v, tcode, i); \
            JSVARS_MAYBE_SEND_##msgVType##_ELEM(tcode##a[i], tcode, i); \
        }                                                               \
    }

#define JSVARS_SEND_LIST_VARIABLE_UPDATE(tcode, msgVType, ForLoopParameters) {      \
        void  *ll;                                                      \
        int   i, dim;                                                   \
        dim = jsvarComputeListLength(v);                                \
        JSVARS_MAYBE_REALLOCATE_ARRAY_AND_SEND_DIMENSION(tcode, dim, 0); \
        for ForLoopParameters { \
            JSVAR_TCODE_TYPE(tcode) val = JSVAR_LIST_ELEM_VALUE(v, tcode, ll); \
            JSVARS_MAYBE_SEND_##msgVType##_ELEM(tcode##a[i], tcode, i); \
        }                                                               \
    }

// full refresh macros
#define JSVARS_SEND_VARIABLE_FULL_UPDATE(tcode, msgVType) {     \
        JSVARS_SEND_VARIABLE_UPDATE(tcode, msgVType);\
    }

#define JSVARS_SEND_ARRAY_VARIABLE_FULL_UPDATE(tcode, msgVType) {       \
        JSVARS_SEND_ARRAY_VARIABLE_UPDATE(tcode, msgVType, (i=0; i < dim; i++)); \
    }

#define JSVARS_SEND_VECTOR_VARIABLE_FULL_UPDATE(tcode, msgVType) {      \
        JSVARS_SEND_VECTOR_VARIABLE_UPDATE(tcode, msgVType, (i=0; i < dim; i++)); \
    }

#define JSVARS_SEND_LIST_VARIABLE_FULL_UPDATE(tcode, msgVType) {        \
        JSVARS_SEND_LIST_VARIABLE_UPDATE(tcode, msgVType, (i=0,ll=*(void**)v->valptr; ll!=NULL; i++,ll=JSVAR_LIST_NEXT_VALUE(v, ll, i))); \
    }

// only modified indexes refresh macros
#define JSVARS_SEND_VARIABLE_MODIFIED_INDEXES_UPDATE(tcode, msgVType) {     \
        JSVARS_SEND_VARIABLE_UPDATE(tcode, msgVType);\
    }

#define JSVARS_SEND_ARRAY_VARIABLE_MODIFIED_INDEXES_UPDATE(tcode, msgVType) { \
        JSVARS_SEND_ARRAY_VARIABLE_UPDATE(tcode, msgVType, (j=0; j<v->modifiedIndexes.ai && (i=v->modifiedIndexes.a[j],1); j++)); \
    }

#define JSVARS_SEND_VECTOR_VARIABLE_MODIFIED_INDEXES_UPDATE(tcode, msgVType) { \
        JSVARS_SEND_VECTOR_VARIABLE_UPDATE(tcode, msgVType, (j=0; j<v->modifiedIndexes.ai && (i=v->modifiedIndexes.a[j],1); j++)); \
    }

#define JSVARS_SEND_LIST_VARIABLE_MODIFIED_INDEXES_UPDATE(tcode, msgVType) {        \
        printf("%s: %s:%d: %s: Fast update on lists is not available. Lists have to be updated with full refresh.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, v->jsname); \
    }

// immediate refresh macros
#define JSVARS_SEND_VARIABLE_ELEMENT_UPDATE(tcode, msgVType) {      \
        JSVARS_SEND_VARIABLE_UPDATE(tcode, msgVType);\
    }

#define JSVARS_SEND_ARRAY_VARIABLE_ELEMENT_UPDATE(tcode, msgVType) {        \
        JSVARS_SEND_ARRAY_VARIABLE_UPDATE(tcode, msgVType, (i=index; i < index+1; i++)); \
    }

#define JSVARS_SEND_VECTOR_VARIABLE_ELEMENT_UPDATE(tcode, msgVType) {       \
        JSVARS_SEND_VECTOR_VARIABLE_UPDATE(tcode, msgVType, (i=index; i < index+1; i++)); \
    }

#define JSVARS_SEND_LIST_VARIABLE_ELEMENT_UPDATE(tcode, msgVType) { \
        printf("%s: %s:%d: %s: Immediate update on lists is not implemented yet\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, v->jsname); \
    }

// TODO: Shorten the macro by splitting it or something
#define JSVARS_MAP_UPDATE_MACRO_ON_V(UpdateType) {                      \
        switch (v->varType) {                                           \
        case JSVT_INT:                                                  \
            JSVARS_SEND_VARIABLE_##UpdateType(i, INTEGER);              \
            break;                                                      \
        case JSVT_SHORT:                                                \
            JSVARS_SEND_VARIABLE_##UpdateType(w, INTEGER);              \
            break;                                                      \
        case JSVT_CHAR:                                                 \
            JSVARS_SEND_VARIABLE_##UpdateType(b, INTEGER);              \
            break;                                                      \
        case JSVT_LONG:                                                 \
            JSVARS_SEND_VARIABLE_##UpdateType(l, INTEGER);              \
            break;                                                      \
        case JSVT_LLONG:                                                \
            JSVARS_SEND_VARIABLE_##UpdateType(ll, INTEGER);             \
            break;                                                      \
        case JSVT_U_INT:                                                \
            JSVARS_SEND_VARIABLE_##UpdateType(ui, INTEGER);             \
            break;                                                      \
        case JSVT_U_SHORT:                                              \
            JSVARS_SEND_VARIABLE_##UpdateType(uw, INTEGER);             \
            break;                                                      \
        case JSVT_U_CHAR:                                               \
            JSVARS_SEND_VARIABLE_##UpdateType(ub, INTEGER);             \
            break;                                                      \
        case JSVT_U_LONG:                                               \
            JSVARS_SEND_VARIABLE_##UpdateType(ul, INTEGER);             \
            break;                                                      \
        case JSVT_U_LLONG:                                              \
            JSVARS_SEND_VARIABLE_##UpdateType(ull, INTEGER);            \
            break;                                                      \
        case JSVT_FLOAT:                                                \
            JSVARS_SEND_VARIABLE_##UpdateType(d, FLOAT);                \
            break;                                                      \
        case JSVT_DOUBLE:                                               \
            JSVARS_SEND_VARIABLE_##UpdateType(d, DOUBLE);               \
            break;                                                      \
        case JSVT_STRING:                                               \
            JSVARS_SEND_VARIABLE_##UpdateType(s, STRING);               \
            break;                                                      \
        case JSVT_WSTRING:                                              \
            JSVARS_SEND_VARIABLE_##UpdateType(ws, WSTRING);             \
            break;                                                      \
        case JSVT_INT_ARRAY:                                            \
            JSVARS_SEND_ARRAY_VARIABLE_##UpdateType(i, INTEGER);        \
            break;                                                      \
        case JSVT_SHORT_ARRAY:                                          \
            JSVARS_SEND_ARRAY_VARIABLE_##UpdateType(w, INTEGER);        \
            break;                                                      \
        case JSVT_CHAR_ARRAY:                                           \
            JSVARS_SEND_ARRAY_VARIABLE_##UpdateType(b, INTEGER);        \
            break;                                                      \
        case JSVT_LONG_ARRAY:                                           \
            JSVARS_SEND_ARRAY_VARIABLE_##UpdateType(l, INTEGER);        \
            break;                                                      \
        case JSVT_LLONG_ARRAY:                                          \
            JSVARS_SEND_ARRAY_VARIABLE_##UpdateType(ll, INTEGER);       \
            break;                                                      \
        case JSVT_U_INT_ARRAY:                                          \
            JSVARS_SEND_ARRAY_VARIABLE_##UpdateType(ui, INTEGER);       \
            break;                                                      \
        case JSVT_U_SHORT_ARRAY:                                        \
            JSVARS_SEND_ARRAY_VARIABLE_##UpdateType(uw, INTEGER);       \
            break;                                                      \
        case JSVT_U_CHAR_ARRAY:                                         \
            JSVARS_SEND_ARRAY_VARIABLE_##UpdateType(ub, INTEGER);       \
            break;                                                      \
        case JSVT_U_LONG_ARRAY:                                         \
            JSVARS_SEND_ARRAY_VARIABLE_##UpdateType(ul, INTEGER);       \
            break;                                                      \
        case JSVT_U_LLONG_ARRAY:                                        \
            JSVARS_SEND_ARRAY_VARIABLE_##UpdateType(ull, INTEGER);      \
            break;                                                      \
        case JSVT_FLOAT_ARRAY:                                          \
            JSVARS_SEND_ARRAY_VARIABLE_##UpdateType(d, FLOAT);          \
            break;                                                      \
        case JSVT_DOUBLE_ARRAY:                                         \
            JSVARS_SEND_ARRAY_VARIABLE_##UpdateType(d, DOUBLE);         \
            break;                                                      \
        case JSVT_STRING_ARRAY:                                         \
            JSVARS_SEND_ARRAY_VARIABLE_##UpdateType(s, STRING);         \
            break;                                                      \
        case JSVT_WSTRING_ARRAY:                                        \
            JSVARS_SEND_ARRAY_VARIABLE_##UpdateType(ws, WSTRING);       \
            break;                                                      \
        case JSVT_INT_VECTOR:                                           \
            JSVARS_SEND_VECTOR_VARIABLE_##UpdateType(i, INTEGER);       \
            break;                                                      \
        case JSVT_SHORT_VECTOR:                                         \
            JSVARS_SEND_VECTOR_VARIABLE_##UpdateType(w, INTEGER);       \
            break;                                                      \
        case JSVT_CHAR_VECTOR:                                          \
            JSVARS_SEND_VECTOR_VARIABLE_##UpdateType(b, INTEGER);       \
            break;                                                      \
        case JSVT_LONG_VECTOR:                                          \
            JSVARS_SEND_VECTOR_VARIABLE_##UpdateType(l, INTEGER);       \
            break;                                                      \
        case JSVT_LLONG_VECTOR:                                         \
            JSVARS_SEND_VECTOR_VARIABLE_##UpdateType(ll, INTEGER);      \
            break;                                                      \
        case JSVT_U_INT_VECTOR:                                         \
            JSVARS_SEND_VECTOR_VARIABLE_##UpdateType(ui, INTEGER);      \
            break;                                                      \
        case JSVT_U_SHORT_VECTOR:                                       \
            JSVARS_SEND_VECTOR_VARIABLE_##UpdateType(uw, INTEGER);      \
            break;                                                      \
        case JSVT_U_CHAR_VECTOR:                                        \
            JSVARS_SEND_VECTOR_VARIABLE_##UpdateType(ub, INTEGER);      \
            break;                                                      \
        case JSVT_U_LONG_VECTOR:                                        \
            JSVARS_SEND_VECTOR_VARIABLE_##UpdateType(ul, INTEGER);      \
            break;                                                      \
        case JSVT_U_LLONG_VECTOR:                                       \
            JSVARS_SEND_VECTOR_VARIABLE_##UpdateType(ull, INTEGER);     \
            break;                                                      \
        case JSVT_FLOAT_VECTOR:                                     \
            JSVARS_SEND_VECTOR_VARIABLE_##UpdateType(d, FLOAT);     \
            break;                                                      \
        case JSVT_DOUBLE_VECTOR:                                        \
            JSVARS_SEND_VECTOR_VARIABLE_##UpdateType(d, DOUBLE);        \
            break;                                                      \
        case JSVT_STRING_VECTOR:                                        \
            JSVARS_SEND_VECTOR_VARIABLE_##UpdateType(s, STRING);        \
            break;                                                      \
        case JSVT_WSTRING_VECTOR:                                       \
            JSVARS_SEND_VECTOR_VARIABLE_##UpdateType(ws, WSTRING);      \
            break;                                                      \
        case JSVT_INT_LIST:                                             \
            JSVARS_SEND_LIST_VARIABLE_##UpdateType(i, INTEGER);         \
            break;                                                      \
        case JSVT_SHORT_LIST:                                           \
            JSVARS_SEND_LIST_VARIABLE_##UpdateType(w, INTEGER);         \
            break;                                                      \
        case JSVT_CHAR_LIST:                                            \
            JSVARS_SEND_LIST_VARIABLE_##UpdateType(b, INTEGER);         \
            break;                                                      \
        case JSVT_LONG_LIST:                                            \
            JSVARS_SEND_LIST_VARIABLE_##UpdateType(l, INTEGER);         \
            break;                                                      \
        case JSVT_LLONG_LIST:                                           \
            JSVARS_SEND_LIST_VARIABLE_##UpdateType(ll, INTEGER);        \
            break;                                                      \
        case JSVT_U_INT_LIST:                                           \
            JSVARS_SEND_LIST_VARIABLE_##UpdateType(ui, INTEGER);        \
            break;                                                      \
        case JSVT_U_SHORT_LIST:                                         \
            JSVARS_SEND_LIST_VARIABLE_##UpdateType(uw, INTEGER);        \
            break;                                                      \
        case JSVT_U_CHAR_LIST:                                          \
            JSVARS_SEND_LIST_VARIABLE_##UpdateType(ub, INTEGER);        \
            break;                                                      \
        case JSVT_U_LONG_LIST:                                          \
            JSVARS_SEND_LIST_VARIABLE_##UpdateType(ul, INTEGER);        \
            break;                                                      \
        case JSVT_U_LLONG_LIST:                                         \
            JSVARS_SEND_LIST_VARIABLE_##UpdateType(ull, INTEGER);       \
            break;                                                      \
        case JSVT_FLOAT_LIST:                                           \
            JSVARS_SEND_LIST_VARIABLE_##UpdateType(d, FLOAT);           \
            break;                                                      \
        case JSVT_DOUBLE_LIST:                                          \
            JSVARS_SEND_LIST_VARIABLE_##UpdateType(d, DOUBLE);          \
            break;                                                      \
        case JSVT_STRING_LIST:                                          \
            JSVARS_SEND_LIST_VARIABLE_##UpdateType(s, STRING);          \
            break;                                                      \
        case JSVT_WSTRING_LIST:                                         \
            JSVARS_SEND_LIST_VARIABLE_##UpdateType(ws, WSTRING);        \
            break;                                                      \
        default:                                                        \
            printf("%s: %s:%d: Unknown variable type %d\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, v->varType);  \
            return;                                                 \
        }                                                               \
    }

// The "firstBindingFlag" is an optimization. If set, it causes that the lastSentValue is updated.
// If we do not use it, we would update this variable twice. Once right now and once on the next update.
static void jsVarsPrintfVariableUpdates(struct jsVar *v, int baioMagic, int firstBindingFlag) {
    jsVarsMapOnFollowers(v, -1, baioMagic, JSVMOFO_INIT_MESSAGE);
    JSVARS_MAP_UPDATE_MACRO_ON_V(FULL_UPDATE);
    jsVarsMapOnFollowers(v, -1, baioMagic, JSVMOFO_WRITE_VALUES_TERMINAL_CHAR);
}

static void jsVarsPrintfVariableModifiedIndexesUpdate(struct jsVar *v, int baioMagic, int firstBindingFlag) {
    jsVarsMapOnFollowers(v, -1, baioMagic, JSVMOFO_INIT_MESSAGE);
    JSVARS_MAP_UPDATE_MACRO_ON_V(MODIFIED_INDEXES_UPDATE);
    jsVarsMapOnFollowers(v, -1, baioMagic, JSVMOFO_WRITE_VALUES_TERMINAL_CHAR);
}

static void jsVarsPrintfVariableElementUpdate(struct jsVar *v, int index, int baioMagic, int firstBindingFlag) {
    jsVarsMapOnFollowers(v, -1, baioMagic, JSVMOFO_INIT_MESSAGE);
    JSVARS_MAP_UPDATE_MACRO_ON_V(ELEMENT_UPDATE);
    jsVarsMapOnFollowers(v, -1, baioMagic, JSVMOFO_WRITE_VALUES_TERMINAL_CHAR);
}

void jsVarSendVariableUpdate(struct jsVar *v, int baioMagic, int firstBindingFlag) {
	// Test for baioMagic allows to initialize input variables on the Javascript side to setup
	// auto-defined update for "<button>, <input>" etc. DOM elements.
	if (v->syncDirection == JSVSD_C_TO_JAVASCRIPT || v->syncDirection == JSVSD_BOTH || baioMagic > 0) {
		jsVarsPrintfVariableUpdates(v, baioMagic, firstBindingFlag);
		jsVarsFinishFollowersMsg(v, baioMagic);
	}
}

void jsVarSendVariableElementUpdate(struct jsVar *v, int index, int baioMagic, int firstBindingFlag) {
	if (v->syncDirection == JSVSD_C_TO_JAVASCRIPT || v->syncDirection == JSVSD_BOTH) {
		jsVarsPrintfVariableElementUpdate(v, index, baioMagic, firstBindingFlag);
		jsVarsFinishFollowersMsg(v, baioMagic);
	}
}

struct jsVar *jsVarGetParsedVariable(struct jsVaraio *jj, char *javaScriptName) {
    struct jsVar        *memb;
    struct jsVarSpace   *jss;

    memb = NULL;

    // printf("GETTING inputspace of %p -> %d\n", jj, jj->inputVarSpaceMagic);
    jss = jsVarSpaceFromMagic(jj->inputVarSpaceMagic);
    if (jss != NULL) memb = jsVarGetVariableByName(jss, javaScriptName);
    if (memb == NULL) {
        jss = jsVarSpaceGlobal;
        if (jss != NULL) memb = jsVarGetVariableByName(jss, javaScriptName);
    }

    // For security reasons, do not allow javascript to create new variables. If the variable does not exists, return NULL
    return(memb);
}

static int jsVarInputVariablePreCheck(struct jsVar *v, int index, char *name, struct jsVaraio *jj) {
    if (v == NULL) {
        printf("%s: %s:%d: jsVarParseVariableUpdate variable %s updated from javascript does not exist!\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, name);
        return(-1);
    }
    if (v->syncDirection != JSVSD_JAVASCRIPT_TO_C && v->syncDirection != JSVSD_BOTH) {
        printf("%s: %s:%d: jsVarParseVariableUpdate variable %s is read-only. It can not be modified from javascript!\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, name);
        return(-1);
    }
    if (jsVarFindBoundConnection(v, jj->w.b.baioMagic) < 0) {
        printf("%s: %s:%d: jsVarParseVariableUpdate variable %s is not bound to connection from where it is updated!\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, name);
        return(-1);         
    }
    if (index < 0 || (index != 0 && (v->vectorDimensionAddr == NULL || index >= *v->vectorDimensionAddr))) {
        printf("%s: %s:%d: jsVarParseVariableUpdate index %d out of range of %s\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, index, name);
        return(-1);
    }
    if (v->valptr == NULL) {
        printf("%s: %s:%d: Internal error: v->valptr == NULL.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
        return(-1);
    }
    return(0);
}

static char *jsVarCreateStringFromJsMessage(char *val, int vlen, int clen, char **res) {
    char        *rr, *ss, *ssend;
    int64_t     c;
    int         i;

    assert(vlen >= 0);
    JSVAR_ALLOCC(rr, clen+1, char);
    ss = val;
    ssend = ss+vlen;
    for(i=0; i<clen; i++) {
        ss = jsVarDecodeNumber(ss, ssend-ss, &c);
        if (ss == NULL) {
            printf("%s: %s:%d: Error while receiving string constant\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
            break;
        }
        rr[i] = c;
    }
    rr[i] = 0;
    *res = rr;
    return(ss);
}

static char *jsVarCreateWstringFromJsMessage(char *val, int vlen, int clen, wchar_t **res) {
    char        *ss, *ssend;
    wchar_t     *rr;
    int64_t     c;
    int         i;

    assert(vlen >= 0);
    JSVAR_ALLOCC(rr, clen+1, wchar_t);
    ss = val;
    ssend = ss+vlen;
    for(i=0; i<clen; i++) {
        ss = jsVarDecodeNumber(ss, ssend-ss, &c);
        if (ss == NULL) {
            printf("%s: %s:%d: Error while receiving wstring constant\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
            break;
        }
        rr[i] = c;
    }
    rr[i] = 0;
    *res = rr;
    return(ss);
}

static char *jsVarParseVariableUpdate(struct jsVaraio *jj, char *ss0, int n) {
    struct jsVar    *v;
    double          dval;
    char            *ss, *val, *sval, *name, *nameend;
    wchar_t         *wsval;
    int             vTypeCode, index, nameendchar;
    int64_t         nlen, vlen, ind;

    // TODO: There shall be some system of permissions or what guaranteeing that only "owner" of variable
    // can overwrite it, or something like this !!!
    // Otherwise this is a huge security hole.

    nameend = NULL;
    ss = ss0;
// printf("Got message %.*s\n", n, ss); fflush(stdout);
    if (n < 6) goto fail;
    vTypeCode = *ss;
    ss ++;
    ss = jsVarDecodeNumber(ss, ss0+n-ss, &nlen);
    if (ss == NULL) goto fail;
    name = ss;
    ss += nlen;
    if (ss - ss0 > n) goto fail;
    nameend = ss; nameendchar = *ss;
    ss = jsVarDecodeNumber(ss, ss0+n-ss, &ind);
    if (ss == NULL) goto fail;
    index = ind;
    if (*ss != '=') goto fail;
    ss ++;
    *nameend = 0;   // because there is at least ':' I can overwrite end of name with 0 to get C string
    ss = jsVarDecodeNumber(ss, ss0+n-ss, &vlen);
    if (ss == NULL) goto fail;
    val = ss;
    // Hmm. Maybe we shall split 'd' case into two cases (int, double) as it is in messages C->Js
    switch (vTypeCode) {
    case 'd':
        ss += vlen;
        dval = atof(val);
        v = jsVarGetParsedVariable(jj, name);
        if (jsVarInputVariablePreCheck(v, index, name, jj) != 0) goto fail;
        if (v->varType == JSVT_INT || v->varType == JSVT_INT_ARRAY) {
            JSVAR_ARRAY_ELEM_VALUE(v,i,index) = dval;
        } else if (v->varType == JSVT_U_INT || v->varType == JSVT_U_INT_ARRAY) {
            JSVAR_ARRAY_ELEM_VALUE(v,ui,index) = dval;
        } else if (v->varType == JSVT_LONG || v->varType == JSVT_LONG_ARRAY) {
            JSVAR_ARRAY_ELEM_VALUE(v,l,index) = dval;
        } else if (v->varType == JSVT_U_LONG || v->varType == JSVT_U_LONG_ARRAY) {
            JSVAR_ARRAY_ELEM_VALUE(v,ul,index) = dval;
        } else if (v->varType == JSVT_LLONG || v->varType == JSVT_LLONG_ARRAY) {
            JSVAR_ARRAY_ELEM_VALUE(v,ll,index) = dval;
        } else if (v->varType == JSVT_U_LLONG || v->varType == JSVT_U_LLONG_ARRAY) {
            JSVAR_ARRAY_ELEM_VALUE(v,ull,index) = dval;
        } else if (v->varType == JSVT_SHORT || v->varType == JSVT_SHORT_ARRAY) {
            JSVAR_ARRAY_ELEM_VALUE(v,w,index) = dval;
        } else if (v->varType == JSVT_U_SHORT || v->varType == JSVT_U_SHORT_ARRAY) {
            JSVAR_ARRAY_ELEM_VALUE(v,uw,index) = dval;
        } else if (v->varType == JSVT_CHAR || v->varType == JSVT_CHAR_ARRAY) {
            JSVAR_ARRAY_ELEM_VALUE(v,b,index) = dval;
        } else if (v->varType == JSVT_U_CHAR || v->varType == JSVT_U_CHAR_ARRAY) {
            JSVAR_ARRAY_ELEM_VALUE(v,ub,index) = dval;
        } else if (v->varType == JSVT_FLOAT || v->varType == JSVT_FLOAT_ARRAY) {
            JSVAR_ARRAY_ELEM_VALUE(v,f,index) = dval;
        } else if (v->varType == JSVT_DOUBLE || v->varType == JSVT_DOUBLE_ARRAY) {
            JSVAR_ARRAY_ELEM_VALUE(v,d,index) = dval;
        } else if (v->varType == JSVT_INT_VECTOR) {
            JSVAR_VECTOR_ELEM_VALUE(v,i,index) = dval;
        } else if (v->varType == JSVT_U_INT_VECTOR) {
            JSVAR_VECTOR_ELEM_VALUE(v,ui,index) = dval;
        } else if (v->varType == JSVT_LONG_VECTOR) {
            JSVAR_VECTOR_ELEM_VALUE(v,l,index) = dval;
        } else if (v->varType == JSVT_U_LONG_VECTOR) {
            JSVAR_VECTOR_ELEM_VALUE(v,ul,index) = dval;
        } else if (v->varType == JSVT_LLONG_VECTOR) {
            JSVAR_VECTOR_ELEM_VALUE(v,ll,index) = dval;
        } else if (v->varType == JSVT_U_LLONG_VECTOR) {
            JSVAR_VECTOR_ELEM_VALUE(v,ull,index) = dval;
        } else if (v->varType == JSVT_SHORT_VECTOR) {
            JSVAR_VECTOR_ELEM_VALUE(v,w,index) = dval;
        } else if (v->varType == JSVT_U_SHORT_VECTOR) {
            JSVAR_VECTOR_ELEM_VALUE(v,uw,index) = dval;
        } else if (v->varType == JSVT_CHAR_VECTOR) {
            JSVAR_VECTOR_ELEM_VALUE(v,b,index) = dval;
        } else if (v->varType == JSVT_U_CHAR_VECTOR) {
            JSVAR_VECTOR_ELEM_VALUE(v,ub,index) = dval;
        } else if (v->varType ==  JSVT_FLOAT_VECTOR) {
            JSVAR_VECTOR_ELEM_VALUE(v,f,index) = dval;
        } else if (v->varType ==  JSVT_DOUBLE_VECTOR) {
            JSVAR_VECTOR_ELEM_VALUE(v,d,index) = dval;
        } else {
            goto fail;
        }
        JSVAR_CALLBACK_CALL(v->callBackOnChange, callBack(v, index, jj));
        jsVarSendVariableElementUpdate(v, index, - jj->w.b.baioMagic, 0);
        break;
    case 's':
        // val[vlen] = 0;
        v = jsVarGetParsedVariable(jj, name);
        if (jsVarInputVariablePreCheck(v, index, name, jj) != 0) goto fail;
        if (v->varType == JSVT_STRING || v->varType == JSVT_STRING_ARRAY) {
            ss = jsVarCreateStringFromJsMessage(val, ss0+n-val, vlen, &sval);
            if (ss == NULL) goto fail;
            // printf("GOT STRING '%s'\n", sval);
            JSVAR_ARRAY_ELEM_VALUE(v,s,index) = sval;
        } else if (v->varType == JSVT_STRING_VECTOR) {
            ss = jsVarCreateStringFromJsMessage(val, ss0+n-val, vlen, &sval);
            if (ss == NULL) goto fail;
            JSVAR_VECTOR_ELEM_VALUE(v,s,index) = sval;
        } else if (v->varType == JSVT_WSTRING || v->varType == JSVT_WSTRING_ARRAY) {
            ss = jsVarCreateWstringFromJsMessage(val, ss0+n-val, vlen, &wsval);
            if (ss == NULL) goto fail;
            // printf("GOT STRING '%s'\n", sval);
            JSVAR_ARRAY_ELEM_VALUE(v,ws,index) = wsval;
        } else if (v->varType == JSVT_WSTRING_VECTOR) {
            ss = jsVarCreateWstringFromJsMessage(val, ss0+n-val, vlen, &wsval);
            if (ss == NULL) goto fail;
            JSVAR_VECTOR_ELEM_VALUE(v,ws,index) = wsval;
        } else {
            printf("%s: %s:%d: jsVarParseVariableUpdate: string update received for non string variable %s.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, name);
            goto fail;
        }
        JSVAR_CALLBACK_CALL(v->callBackOnChange, callBack(v, index, jj));
        jsVarSendVariableElementUpdate(v, index, - jj->w.b.baioMagic, 0);
        if (v->varType == JSVT_STRING) {
            // this is a trick, so that *v->vp.s points to the lastsentvalue, which will be freed automatically on next update
            JSVAR_FREE(sval);
            JSVAR_ELEM_VALUE(v,s) = v->lastSentVal.s;
        } else if (v->varType == JSVT_STRING_ARRAY) {
            JSVAR_FREE(sval);
            JSVAR_ARRAY_ELEM_VALUE(v,s,index) = v->lastSentVal.sa[index];
        } else if (v->varType == JSVT_STRING_VECTOR) {
            JSVAR_FREE(sval);
            JSVAR_VECTOR_ELEM_VALUE(v,s,index) = v->lastSentVal.sa[index];
        } else if (v->varType == JSVT_WSTRING) {
            JSVAR_FREE(wsval);
            JSVAR_ELEM_VALUE(v,ws) = v->lastSentVal.ws;
        } else if (v->varType == JSVT_WSTRING_ARRAY) {
            JSVAR_FREE(wsval);
            JSVAR_ARRAY_ELEM_VALUE(v,ws,index) = v->lastSentVal.wsa[index];
        } else if (v->varType == JSVT_WSTRING_VECTOR) {
            JSVAR_FREE(wsval);
            JSVAR_VECTOR_ELEM_VALUE(v,ws,index) = v->lastSentVal.wsa[index];

        }
        break;
    default:
        goto fail;
    }
    if (ss == NULL) goto fail;
    if (*ss != ';') printf("%s: %s:%d: jsVarParseVariableUpdate message does not terminate with ;\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
    ss ++;
    if (ss - ss0 > n) goto fail;
    if (nameend != NULL) *nameend = nameendchar;
    return(ss);
fail:
    if (nameend != NULL) *nameend = nameendchar;
    return(NULL);
}

static void jsVarIfWatchVariableClearItFromArrayVariables(struct jsVar *v) {
    struct sglib_jsVar_iterator     iii;
    struct jsVar                    *ss;

    if (! isWatchVariableName(v->jsname)) return;
    // watch variable, clear it from members
    assert(v->varSpace != NULL);
    for(ss = sglib_jsVar_it_init_on_equal(&iii, v->varSpace->jsVars, jsVarWatchSubcomparator, v); ss != NULL; ss = sglib_jsVar_it_next(&iii)) {
        if (ss->watchVariable == v) {
            // printf("%s: removing watch %s from %s, new watch is %s \n", JSVAR_PRINT_PREFIX(), v->jsname, ss->jsname, v->watchVariable==NULL?"NULL":v->watchVariable->jsname);
            ss->watchVariable = v->watchVariable;
        }
    }
}

void jsVarDeleteVariable(struct jsVar *v) {
    int i;

    if (v == NULL) return;

    if (v->previousInUpdateList != NULL) {
        if (jsVarDebugLevel > 10) printf("%s: Warning: Deleted variable %s is noted for update. Removing it from update list.\n", JSVAR_PRINT_PREFIX(), v->jsname);
        assert(v->varSpace != NULL);
        SGLIB_DL_LIST_DELETE(struct jsVar, v->varSpace->jsVarsUpdatedList, v, previousInUpdateList, nextInUpdateList);
    }

    // TODO: If this is an array allocated by jsVar (createNewVariable), then free also value array !!!
    jsVarIfWatchVariableClearItFromArrayVariables(v);
    assert(v->varSpace != NULL);
    sglib_jsVar_delete(&v->varSpace->jsVars, v);
    jsVarAddressTableDelete(&jsVarsAddr, v);
    switch (v->varType) {
    case JSVT_STRING:
        JSVAR_FREE(v->lastSentVal.s);
        break;
    case JSVT_WSTRING:
        JSVAR_FREE(v->lastSentVal.ws);
        break;
    case JSVT_INT_ARRAY:
    case JSVT_INT_VECTOR:
    case JSVT_INT_LIST:
        JSVAR_FREE(v->lastSentVal.ia);
        jsVarIndexSetFree(&v->modifiedIndexes);
        break;
    case JSVT_U_INT_ARRAY:
    case JSVT_U_INT_VECTOR:
    case JSVT_U_INT_LIST:
        JSVAR_FREE(v->lastSentVal.uia);
        jsVarIndexSetFree(&v->modifiedIndexes);
        break;
    case JSVT_SHORT_ARRAY:
    case JSVT_SHORT_VECTOR:
    case JSVT_SHORT_LIST:
        JSVAR_FREE(v->lastSentVal.wa);
        jsVarIndexSetFree(&v->modifiedIndexes);
        break;
    case JSVT_U_SHORT_ARRAY:
    case JSVT_U_SHORT_VECTOR:
    case JSVT_U_SHORT_LIST:
        JSVAR_FREE(v->lastSentVal.uwa);
        jsVarIndexSetFree(&v->modifiedIndexes);
        break;
    case JSVT_CHAR_ARRAY:
    case JSVT_CHAR_VECTOR:
    case JSVT_CHAR_LIST:
        JSVAR_FREE(v->lastSentVal.ba);
        jsVarIndexSetFree(&v->modifiedIndexes);
        break;
    case JSVT_U_CHAR_ARRAY:
    case JSVT_U_CHAR_VECTOR:
    case JSVT_U_CHAR_LIST:
        JSVAR_FREE(v->lastSentVal.uba);
        jsVarIndexSetFree(&v->modifiedIndexes);
        break;
    case JSVT_LONG_ARRAY:
    case JSVT_LONG_VECTOR:
    case JSVT_LONG_LIST:
        JSVAR_FREE(v->lastSentVal.la);
        jsVarIndexSetFree(&v->modifiedIndexes);
        break;
    case JSVT_U_LONG_ARRAY:
    case JSVT_U_LONG_VECTOR:
    case JSVT_U_LONG_LIST:
        JSVAR_FREE(v->lastSentVal.ula);
        jsVarIndexSetFree(&v->modifiedIndexes);
        break;
    case JSVT_LLONG_ARRAY:
    case JSVT_LLONG_VECTOR:
    case JSVT_LLONG_LIST:
        JSVAR_FREE(v->lastSentVal.lla);
        jsVarIndexSetFree(&v->modifiedIndexes);
        break;
    case JSVT_U_LLONG_ARRAY:
    case JSVT_U_LLONG_VECTOR:
    case JSVT_U_LLONG_LIST:
        JSVAR_FREE(v->lastSentVal.ulla);
        jsVarIndexSetFree(&v->modifiedIndexes);
        break;
    case JSVT_FLOAT_ARRAY:
    case JSVT_FLOAT_VECTOR:
    case JSVT_FLOAT_LIST:
        JSVAR_FREE(v->lastSentVal.fa);
        jsVarIndexSetFree(&v->modifiedIndexes);
        break;
    case JSVT_DOUBLE_ARRAY:
    case JSVT_DOUBLE_VECTOR:
    case JSVT_DOUBLE_LIST:
        JSVAR_FREE(v->lastSentVal.da);
        jsVarIndexSetFree(&v->modifiedIndexes);
        break;
    case JSVT_STRING_ARRAY:
    case JSVT_STRING_VECTOR:
    case JSVT_STRING_LIST:
        for(i = v->lastSentArrayDimension-1; i>=0 ; i--) JSVAR_FREE(v->lastSentVal.sa[i]);
        JSVAR_FREE(v->lastSentVal.sa);
        jsVarIndexSetFree(&v->modifiedIndexes);
        break;
    case JSVT_WSTRING_ARRAY:
    case JSVT_WSTRING_VECTOR:
    case JSVT_WSTRING_LIST:
        for(i = v->lastSentArrayDimension-1; i>=0 ; i--) JSVAR_FREE(v->lastSentVal.wsa[i]);
        JSVAR_FREE(v->lastSentVal.wsa);
        jsVarIndexSetFree(&v->modifiedIndexes);
        break;
    }
    assert(v->modifiedIndexes.a == NULL && v->modifiedIndexes.bits == NULL);
    jsVarCallBackFreeHook(&v->callBackOnChange);
    // FREE(v->jsname);
    JSVAR_FREE(v->baioMagics);
    JSVAR_FREE(v);
}

#if 0
static void jsVarDeleteDisconnectedVariables() {
    struct jsVar    *tdd;
    int             i;
    // delete disconnected variables
    while (deletionList != &uniqueListSentinel) {
        assert(deletionList!=NULL);
        tdd = deletionList;
        deletionList = tdd->nextInDeletionList;
        assert(tdd->previousInUpdateList == NULL);
        assert(tdd->baioMagicsElemCount == 0);
        tdd->nextInDeletionList = NULL;
        jsVarDeleteVariable(tdd);
    }
}
#endif

static void jsVarsCompleteMessagesFromUpdatedBaios(struct jsVarIndexSet *baiosToUpdate) {
    int i;
    struct wsaio *ww;
    for(i=0; i<baiosToUpdate->ai; i++) {
        ww = (struct wsaio *) baioTab[baiosToUpdate->a[i]];
        wsaioWebsocketCompleteMessage(ww);
    }
}

static int jsVarUpdateListComparator(struct jsVar *o1, struct jsVar *o2) {
    assert(o1->varSpace != NULL);
    assert(o2->varSpace != NULL);
    assert(o1->varSpace == o2->varSpace);
    if (o1 == &o1->varSpace->jsVarsUpdatedListFirstElementSentinel) return(-1);
    if (o2 == &o2->varSpace->jsVarsUpdatedListFirstElementSentinel) return(1);
    return(jsVarComparator(o1, o2));
}

void jsVarCleanNotedUpdateList(struct jsVarSpace *jss) {
    struct jsVar                    *v, *vv;

    assert(jss->jsVarsUpdatedList == &jss->jsVarsUpdatedListFirstElementSentinel);
    v=jss->jsVarsUpdatedList->nextInUpdateList; 
    while (v != NULL) {
        vv = v->nextInUpdateList;
        v->nextInUpdateList = v->previousInUpdateList = NULL;
        if (v->modifiedIndexes.a != NULL) jsVarIndexSetClear(&v->modifiedIndexes);
        v = vv;
    }
    jss->jsVarsUpdatedList->nextInUpdateList = NULL;
    assert(jss->jsVarsUpdatedList->previousInUpdateList == NULL);
}

static void jsVarRefreshVariable(struct jsVar *ss, struct jsVarIndexSet *baiosToUpdate) {
	// Updating JS->C variables was problem when submitting large file from JS->C. In this case 
	// I do not want the file to be sent back (it overflows writebuffer).
	// Why the direction restriction was put into comment previously? Because we wanted DOM elems 
	// related to input variables to be auto-initialized to JS-C sending function.
	if (ss->syncDirection == JSVSD_C_TO_JAVASCRIPT || ss->syncDirection == JSVSD_BOTH) {
		if (ss->baioMagicsElemCount != 0) {
			jsVarsPrintfVariableUpdates(ss, JSVAR_BAIO_MAGIC_ALL, 0);
			jsVarsAddFollowersBaioToIndexSet(ss, JSVAR_BAIO_MAGIC_ALL, baiosToUpdate);
		}
	}
}

void jsVarRefreshNotes(struct jsVarSpace *jss) {
    static struct jsVarIndexSet     baiosToUpdate = {0,};
    struct jsVar                    *ss;

    // first, sort the list of updated variables
    SGLIB_DL_LIST_SORT(struct jsVar, jss->jsVarsUpdatedList, jsVarUpdateListComparator, previousInUpdateList, nextInUpdateList);

    jsVarIndexSetReinit(&baiosToUpdate, BAIO_MAX_CONNECTIONS);

    assert(jss->jsVarsUpdatedList == &jss->jsVarsUpdatedListFirstElementSentinel);
    // update noted variables
    for(ss=jss->jsVarsUpdatedList->nextInUpdateList; ss != NULL; ss = ss->nextInUpdateList) {
        if (ss->baioMagicsElemCount != 0 /* && ss->syncDirection != JSVSD_JAVASCRIPT_TO_C */) {
            if (jsVarDebugLevel > 10) printf("Updating %s\n", ss->jsname);
            jsVarsPrintfVariableModifiedIndexesUpdate(ss, JSVAR_BAIO_MAGIC_ALL, 0);
            jsVarsAddFollowersBaioToIndexSet(ss, JSVAR_BAIO_MAGIC_ALL, &baiosToUpdate);
        }
    }

    jsVarsCompleteMessagesFromUpdatedBaios(&baiosToUpdate);
    jsVarCleanNotedUpdateList(jss);
}

// TODO: rename current jsVarsRefreshNotes to jsVarsRefreshVarSpaceNotes
// TODO: jsVarsRefreshNotes() will iterate over all varspaces

void jsVarRefreshPrefix(struct jsVarSpace *jss, char *jsNamePrefix) {
    static struct jsVarIndexSet     baiosToUpdate = {0,};
    struct sglib_jsVar_iterator     iii;
    struct jsVar                    *ss, memb;

    memset(&memb, 0, sizeof(memb));
    memb.jsname = jsNamePrefix;
    memb.jsnameLength = strlen(jsNamePrefix);

    jsVarIndexSetReinit(&baiosToUpdate, BAIO_MAX_CONNECTIONS);

    for(ss = sglib_jsVar_it_init_on_equal(&iii, jss->jsVars, jsVarPrefixSubcomparator, &memb); ss != NULL; ss = sglib_jsVar_it_next(&iii)) {
        // printf("Refresh prefix %s\n", ss->jsname);
        jsVarRefreshVariable(ss, &baiosToUpdate);
    }

    jsVarsCompleteMessagesFromUpdatedBaios(&baiosToUpdate);
}

#if JSVAR_USE_REGEXP
void jsVarRefreshRegexp(struct jsVarSpace *jss, char *regexp) {
    static struct jsVarIndexSet     baiosToUpdate = {0,};
    struct sglib_jsVar_iterator     iii;
    struct jsVar                    *ss;
    int                             regretval;
    regex_t                         regex;

    jsVarIndexSetReinit(&baiosToUpdate, BAIO_MAX_CONNECTIONS);

    regretval = regcomp(&regex, regexp, REG_EXTENDED);
    if (regretval) {
        printf("%s: jsVarsRefreshRegexp: Invalid regular expression %s\n", JSVAR_PRINT_PREFIX(), regexp);
        return;
    }

    for(ss = sglib_jsVar_it_init_inorder(&iii, jss->jsVars); ss != NULL; ss = sglib_jsVar_it_next(&iii)) {
        if (regexec(&regex, ss->jsname, 0, NULL, 0) == 0) {
            jsVarRefreshVariable(ss, &baiosToUpdate);
        }
    }
    regfree(&regex);

    jsVarsCompleteMessagesFromUpdatedBaios(&baiosToUpdate);
}
#endif

void jsVarRefreshAll(struct jsVarSpace *jss) {
    static struct jsVarIndexSet         baiosToUpdate = {0,};
    struct sglib_jsVar_iterator     iii;
    struct jsVar                    *ss;

    jsVarIndexSetReinit(&baiosToUpdate, BAIO_MAX_CONNECTIONS);

    for(ss = sglib_jsVar_it_init_inorder(&iii, jss->jsVars); ss != NULL; ss = sglib_jsVar_it_next(&iii)) {
        jsVarRefreshVariable(ss, &baiosToUpdate);
    }

    jsVarsCompleteMessagesFromUpdatedBaios(&baiosToUpdate);
    jsVarCleanNotedUpdateList(jss);
}

int jsVarPoll2(int timeOutUsec, int (*addUserFds)(int maxfd, fd_set *r, fd_set *w, fd_set *e), void (*processUserFds)(int maxfd, fd_set *r, fd_set *w, fd_set *e)) {
    int i, res;
    for(i=0; i<jsVarSpaceTabIndex; i++) {
        if (jsVarSpaceTab[i].jsVarSpaceMagic != 0) jsVarRefreshNotes(&jsVarSpaceTab[i]);
    }
    res = baioPoll2(timeOutUsec, addUserFds, processUserFds);
    return(res);
}

int jsVarPoll(int timeOutUsec) {
    int res;
    res = jsVarPoll2(timeOutUsec, NULL, NULL);
    return(res);
}

/////////////////////////////////////////

// If changing this function, change also default initialization of jsVarSpaceGlobal !!!
struct jsVarSpace *jsVarSpaceCreate() {
    int                 i;
    struct jsVarSpace   *jss;

    for(i=0; i<jsVarSpaceTabIndex; i++) {
        if (jsVarSpaceTab[i].jsVarSpaceMagic == 0) break;
    }
    if (i >= jsVarSpaceTabIndex) {
        if (jsVarSpaceTabIndex + 1 >= JSVAR_MAX_VARSPACES) {
            printf("%s: %s:%d: Error: Too many varSpaces. Increase JSVAR_MAX_VARSPACES.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
            return(NULL);
        }
        jsVarSpaceTabIndex ++;
    }
    jss = &jsVarSpaceTab[i];
    memset(jss, 0, sizeof(*jss));
    // valid magic can never be 0
    jss->jsVarSpaceMagic = (rand() % (1<<16) + 1) * JSVAR_MAX_VARSPACES + i;
    jss->jsVars = NULL;
    jss->jsVarsUpdatedListFirstElementSentinel.varSpace = jss;
    jss->jsVarsUpdatedList = &jss->jsVarsUpdatedListFirstElementSentinel;
    return(jss);
}

void jsVarSpaceDelete(struct jsVarSpace *jss) {
    jsVarDeletePrefixVariables(jss, "");
    memset(jss, 0, sizeof(*jss));
    jss->jsVarSpaceMagic = 0;
}

struct jsVarSpace *jsVarSpaceFromMagic(int magic) {
    int                 i;
    struct jsVarSpace   *jss;

    if (magic == 0) return(NULL);

    i = magic % JSVAR_MAX_VARSPACES;
    jss = &jsVarSpaceTab[i];
    if (jss->jsVarSpaceMagic == magic) return(jss);
    return(NULL);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////

static char jsvarJavascriptText[] = JSVAR_STRINGIFY(
    if (typeof jsvar === "undefined") {
        jsvar = {variableUpdate:{}, variableArrayResize:{}};
        jsvar.connected = false;
    } else if (typeof jsvar !== 'object') {
        throw new Error("%s: Variable 'jsvar' is not an object!", JSVAR_PRINT_PREFIX());
    }
    \n
    jsvar.initialize = function() {
        // if (window.attachEvent) {
        //  window.attachEvent('onload', function(evt) {jsvar.start();} );
        //} else {
        var oldOnload = window.onload;
        window.onload = function(evt) {
            if (oldOnload) oldOnload(evt);
            // if starting websocket immediately, then some pending "loading" icon remains
            setTimeout(function(){ 
                    if (typeof jsvarOnReady === "function") jsvarOnReady();
                    startJsvar();
                }, 100);
        }\n;
        //}

        // functions to shorten the description "document.querySelector()"
        jsvar.s = function (selector) {return(document.querySelector(selector));};
        jsvar.sa = function (selector) {return(document.querySelectorAll(selector));};
        // dummy parameter function. Allows to map different names jsvar.vd(XXX).y to jsvar.y
        jsvar.vd = function (dummyparameter) {return jsvar;};
        // initialization of jsvar connection and message exchanging
        jsvar.start = function () {startJsvar();};

        // explicit sending of a variable value from javascript to C
        jsvar.noteUpdateNumber =  function (varname) {sendMsg(varname, "d", String(eval(varname)), 0);};
        jsvar.noteUpdateString =  function (varname) {sendMsg(varname, "s", String(eval(varname)), 0);};
        jsvar.noteUpdateObject =  function (varname) {sendMsg(varname, "s", JSON.stringify(eval(varname)), 0);};
        // explicit assignement of a value to synchronized var in C
        jsvar.setNumber = function(varname, value) {sendMsg(varname, "d", String(value), 0);};
        jsvar.setString = function(varname, value) {sendMsg(varname, "s", String(value), 0);};
        // explicit assignement of a value to synchronized array element var in C
        jsvar.setNumberArray = function(varname, value, index) {sendMsg(varname, "d", String(value), index);};
        jsvar.setStringArray = function(varname, value, index) {sendMsg(varname, "s", String(value), index);};

        // function called to assign a value to a variable, can be redefined by the user
        jsvar.assign = function (varname, index, value) {defaultAssignFunction(varname, index, value);};
        // function called to resize an array, can be redefined by the user
        jsvar.resize = function (varname, newdim) {defaultResizeFunction(varname, newdim);};
        // objects to store updating and resizing functions. User will define fields for its variables
        if (jsvar.variableUpdate == undefined) jsvar.variableUpdate = {};
        if (jsvar.variableArrayResize == undefined) jsvar.variableArrayResize = {};
		// configuration option whether to allow assignement to variable by replacing string "[value]" by actual value
		if (jsvar.allowFunctionalNames == undefined) jsvar.allowFunctionalNames = true;
        // compression dictionary, obsolete now.
        jsvar.dictionary = [];

        // common variables
        var jsVarWebSocket = null;
        var charCodeEquality = "=".charCodeAt(0);
        var charCodeCircumflex = "^".charCodeAt(0);
        var charCodeDollar = "$".charCodeAt(0);
        var charCodeStar = "*".charCodeAt(0);
        var charCodeDot = ".".charCodeAt(0);
        var charCodeColon = ":".charCodeAt(0);
        var charCodeSemicolon = ";".charCodeAt(0);
        var charCodeZero = "0".charCodeAt(0);
        var charCodeNine = "9".charCodeAt(0);
        var charCodeLess = "<".charCodeAt(0);
        var charCodeGreater = ">".charCodeAt(0);
        var charCodeBrakeOpen = "[".charCodeAt(0);
        var charCodeBrakeClose = "]".charCodeAt(0);
        var stringFromCharCode64 = String.fromCharCode(64);

        var problemCounter = 0;

        function identity(x) {return(x);}
		function removeAppendixAfter(s, delimiter) {
			var res = s;
			var i = res.indexOf(delimiter);
			if (i >= 0) res = res.substring(0, i);
			return(res);
		}
        function jsvarOpenWebSocket() {
            if (jsVarWebSocket != null) {
                jsVarWebSocket.close();
                jsVarWebSocket = null;
                return;
            }
            var wsurl = document.URL.replace("http", "ws");
			wsurl = removeAppendixAfter(wsurl, '?');
			wsurl = removeAppendixAfter(wsurl, '#');
            jsVarWebSocket = new WebSocket(wsurl, "jsync");
            jsVarWebSocket.onopen = function() {
                if (jsvar.debuglevel > 10) console.log("JsVar:", (new Date()) + ": Websocket to "+wsurl+" is opened");
                if (jsvar.onConnected != null) jsvar.onConnected();
                jsvar.connected = true;
            };
            jsVarWebSocket.onmessage = function (evt) {
                var received_msg = evt.data;
                if (jsvar.debuglevel > 100) console.log("JsVar:", (new Date()) + ": Websocket Got Message: ", received_msg);
                if (jsvar.onMessageReceived != null) jsvar.onMessageReceived(received_msg);
                parseMsg(received_msg);
            };
            jsVarWebSocket.onclose = function() { 
                if (jsvar.debuglevel > 10) console.log("JsVar:", (new Date()) + ": Websocket connection to "+wsurl+" is closed."); 
                jsvar.connected = false;
                if (jsvar.onDisconnected != null) jsvar.onDisconnected();
                jsVarWebSocket = null;
                // try to reconnect
                setTimeout(jsvarOpenWebSocket, 5000);
            };
        }\n;

        function problemHappened() {
            problemCounter ++;
            if (problemCounter > 10 && jsVarWebSocket != null) {
                // problemCounter = 0;
                console.log("JsVar: Too many problems, closing websocket", jsVarWebSocket);
                // for some reason websocket continues receiving message after having called close, so void onmessage function
                jsVarWebSocket.onmessage = function () {};
                jsVarWebSocket.close();
                // and it does not call onclose, so call it manually
                jsVarWebSocket.onclose();
            }
        }\n;

        function jsVarsDecodeNumber(msg, i, msglen) {
            var c = msg.charCodeAt(i);
            if (c < 64) {
                // 1 byte encoding
                // console.log("DECODE 1byte", msg, i, msglen, c);
                return({i:i+1, n:c});
            } else if (c < 96) {
                // 2 bytes encoding
                i ++;
                if (i > msglen) return(null);
                var c2 = msg.charCodeAt(i);
                var res = (c - 64) * 128 + c2;
                // console.log("DECODE 2byte", msg, i, msglen, res);
                return({i:i+1, n:res});
            }
            // long encoding
            var sign = 1;
            var len = c - 96;
            if (len >= 16) {
                // negative number
                sign = -1;
                len -= 16;
            }
            i++;
            var imax = i + len + 1;
            if (imax > msglen) return(null);
            var res = 0;
            while (i<imax) {
                var c = msg.charCodeAt(i);
                res = res * 128 + c;
                i++;
            }
            // console.log("DECODE LONG", msg, i, msglen, res);
            return({i:i, n:res*sign});
        }\n;

        function jsVarEncodeNumber(n) {
            var negativeFlag = false;
            if (n<0) {
                n = -n;
                negativeFlag = true;
            }
            // case len == 0  && sign > 0 is a special case (short encoding), because it is the usual case
            if (n < 64 && negativeFlag == false) {
                // 1 byte encoding, do nothing
                return(String.fromCharCode(n));
            } else if (n < 128 && negativeFlag == false) {
                // 2 bytes encoding 
                return(stringFromCharCode64 + String.fromCharCode(n));
            } else if (n < 4096 && negativeFlag == false) {
                // 2 bytes encoding 
                return(String.fromCharCode(64 + Math.floor(n / 128)) + String.fromCharCode(n % 128));
            } else {
                // long encoding, the number starts by a leading byte storing length and sign only. The absolute
                // value of the number starts by the second byte.
                var rcc = [];
                var a = n;
                var i = 0;
                do {
                    rcc[i] = String.fromCharCode(a % 128);
                    a = Math.floor(a / 128);
                    i ++;
                } while (a != 0);
                len = i-1;
                if (negativeFlag == false) {
                    rcc[i] = String.fromCharCode(96 + len);
                } else {
                    rcc[i] = String.fromCharCode(96 + 16 + len);
                }
                var res = "";
                for(;i>=0; i--) res += rcc[i];
                return(res);
            }
        }\n;

        // javascript to C messages coding
        function sendMsg(varname, valueType, value, index) {
            if (typeof varname !== 'string' && ! varname instanceof String) {
                console.log("JsVar: sendMsg: varname is not a string", varname);
                return;
            }
            if (typeof value !== 'string' && ! varname instanceof String) {
                console.log("JsVar: sendMsg: value is not a string", varname);
                return;
            }
            var msg;
            var nlen = varname.length;
            var vlen = value.length;
            msg = "";
            msg += valueType;
            msg += jsVarEncodeNumber(nlen);
            msg += varname;
            msg += jsVarEncodeNumber(index);
            msg += "=";
            msg += jsVarEncodeNumber(vlen);
            if (valueType == 's') {
                for(var i=0; i<vlen; i++) {
                    msg += jsVarEncodeNumber(value.charCodeAt(i));
                }
            } else {
                msg += value;
            }
            msg += ";";
            // console.log("sending message", msg, nlen, index, vlen);
            if (jsVarWebSocket != null) jsVarWebSocket.send(msg);
        }\n;

        function decompressString(msg) {
            if (jsvar.dictionary == null) return(msg);
            var res = "";
            var i = 0;
            var msglen = msg.length;
            while (i < msglen) {
                var c = msg.charCodeAt(i);
                var cc = c;
                var oneCharEncodingIndex = 30; /* TODO: This shall be JSVAR_DICTIONARY_ONE_BYTE_ENCODING_INDEX, manage to send it from C */
                if (c < 32) {
                    if (c >= oneCharEncodingIndex) {
                        i ++;
                        if (i < msglen) {
                            var cc = (c-oneCharEncodingIndex)*96+msg.charCodeAt(i);
                        } else {
                            console.log("jsVar error: decompressString: uncomplete 2 bytes dictionary word ");
                        }
                    }
                    if (jsvar.dictionary[cc] == null) {
                        console.log("jsVar error: decompressString: unknown dictionary word " + cc);
                    } else {
                        res += jsvar.dictionary[cc];
                    }
                } else {
                    res += String.fromCharCode(c);
                }
                i ++;
            }
            return(res);
        }\n;

        // I am creating special function fort this, because google chrome seems not to optimize functions containing eval
        function executeEval(jsvarevalmsg) {
            if (jsvar.debuglevel > 10) console.log("JsVar: Executing Eval:", jsvarevalmsg);
            eval(jsvarevalmsg);
        }\n;

        function jsVarDecodeString(msg) {
            var res = "";
            var i=0;
            var msglen = msg.length;
            while (i<msglen) {
                var pp = jsVarsDecodeNumber(msg, i, msglen);
                if (pp == null) return(res);
                //if (pp.n < 0) return(res);
                //if (pp.i <= i) return(res);
                i = pp.i;
                res += String.fromCharCode(pp.n);
                // res += String.fromCodePoint(pp.n);
            }
            return(res);
        }\n;

        // C to Javascript messages decoding
        function parseMsg(msg) {
			// TODO: variable names from here may clash with suer variable names, make them unique
            try {
                var msglen = msg.length;
                var i = 0;
                // console.log("PARSEMSG:", msg, msglen, i); 
                while (i+6 < msglen) {
                    // console.log("PARSE SUBMSG:", msg.substring(i));
                    var vTypeCode =  msg.substring(i, i+1);
                    i ++;
                    var pp = jsVarsDecodeNumber(msg, i, msglen);
                    if (pp == null) return(-1);
                    if (pp.n <= 0) return(-1);
                    i = pp.i+pp.n;
                    if (vTypeCode === "^") {
                        // message to execute an eval command, no variable updates
                        var evalstr = jsVarDecodeString(msg.substring(pp.i, pp.i+pp.n));
                        try {
                            executeEval(evalstr);
                        } catch (e) {
                            console.log("Exception in eval: ", evalstr, e, e.stack);
                            problemHappened();
                        }
                    } else {
                        var varname = decompressString(msg.substring(pp.i, pp.i+pp.n));
                        var previousIndex = 0;
                        // TODO: Put together vTypeCode and resizeFlag into one char
                        var resizeFlag = msg.charCodeAt(i);
                        // console.log("RECORDTYPE", resizeFlag, i, msg);
                        i += 1;
                        if (resizeFlag == charCodeLess) {
                            // array resizing
                            var pp = jsVarsDecodeNumber(msg, i, msglen);
                            if (pp == null) return(-1);
                            i = pp.i;
                            try {
                                jsvar.resize(varname, pp.n);
                            } catch (e) {
                                console.log("Exception in resizing ", varname, "to", pp.n, e, e.stack);
                                problemHappened();                              
                            }
                        } else if (resizeFlag != charCodeEquality) {
                            console.log("JsVar: Wrong resizeFlag "+String.fromCharCode(resizeFlag)+"("+resizeFlag+") in ", msg);
                            jsVarWebSocket.close();
                            return(-1);
                        }
                        // theoretically, if there is 15 chars long negative number it can give the starting value equal 127, but in practice this
                        // will never happen
                        while (i<msglen && msg.charCodeAt(i) != 127) {
                            var pp1 = jsVarsDecodeNumber(msg, i, msglen);
                            if (pp1 == null) return(-1);
                            // TODO: Incremental indexes
                            var indexOffset = pp1.n;
                            var index = previousIndex + indexOffset;
                            var pp2 = jsVarsDecodeNumber(msg, pp1.i, msglen);
                            if (pp2 == null) return(-1);
                            i = pp2.i;
                            if (vTypeCode == 'i') {
                                var value = pp2.n;
                            } else {
                                var vlen = pp2.n;
                                if (vlen < 0) return(-1);
                                if (msglen < pp2.i + vlen) return(-1);
                                var value = msg.substring(pp2.i, pp2.i+vlen);
                                if (vTypeCode == 's') value = jsVarDecodeString(value); // string
                                else value = Number(value);                             // floating point constant
                                i = pp2.i + vlen;
                            }
                            try {
                                jsvar.assign(varname, index, value);
                            } catch (e) {
                                console.log("Exception in assignement ", varname, "([", index, "]) = ", value, e, e.stack);
                                problemHappened();                              
                            }
                            previousIndex = index;
                            previousIndex = 0; /* unfortunately, incremental indexes must solve problem with filtration first */
                        }
                    }
                    i++;
                }
                return(i+1);
            } catch (e) {
                // console.log("Exception in message:", msg, e, e.stack);
                console.log("Exception in message:", e, e.stack);
                problemHappened();
                return(msg.length);
            }
        }\n;

        function createVariableIntermediateObjectsOrArrays(varname) {
            var len = varname.length;
            var v = '';
            for(var i=0; i<len; i++) {
                var c = varname.charCodeAt(i);
                // console.log("v == ", v);
                if (c == charCodeDot) {
                    // console.log("checking ", v);
                    var o = eval("typeof "+v);
                    // console.log("o == ", o);
                    if (o == 'undefined') {
                        // console.log("executing ", v + ' = {}');
                        eval(v + ' = {}');
                    }
                } else if (c == charCodeBrakeOpen) {
                    // console.log("checking2 ", v);
                    var o = eval("typeof "+v);
                    // console.log("o == ", o);
                    if (o == 'undefined') {
                        // console.log("executing ", v + ' = []');
                        eval(v + ' = []');
                    }
                    while ((c=varname.charCodeAt(i)) != charCodeBrakeClose) {
                        v += String.fromCharCode(c);
                        i++;
                    }
                }
                v += String.fromCharCode(c);
            }
        }\n;

        function evalAssign(jsvarvarname, jsvarCurrentValue) {
            // if (jsvar.debuglevel > 10) console.log("JsVar: Assign by eval: ", jsvarvarname, "([", jsvarindex, "])", "=", jsvarCurrentValue);
            if (jsvar.debuglevel > 10) console.log("JsVar: Assign by eval: ", jsvarvarname, "=", jsvarCurrentValue);
            // This is tricky, it is using 'jsvarCurrentValue' from eval scope which is bind to the value paramater. 
            // If I put eval(jsvarvarname + ' = ' + jsvarCurrentValue) it will not work (at least for string values).
            // TODO: check all functions calling eval and put there unique argument names not to clash with evaluated string
			var assignementcommand = jsvarvarname + ' = jsvarCurrentValue';
            try {
                eval(assignementcommand);
            } catch (e) {
                console.log("JsVar: Warning: Problem while assigning to variable ", jsvarvarname, "! Trying to fix it by creating intermediate objects/arrays.");
                createVariableIntermediateObjectsOrArrays(jsvarvarname);
                eval(assignementcommand);
            }
        }\n;

		function evalSplitCommand(command, jsvarCurrentValue) {
            // The same trick as above, it is using 'value' from eval scope which is bind to the value paramater. 
			eval(command);
		}\n;

        function multiAssignRecursion(ss, sslen, ssi, a, index, value) {
			// TODO: remove this function
            // console.log("MultiAssignRecursion", ss, sslen, ssi, a, index, value);
            var ssi1 = ssi+1;
            var alen = a.length;
            if (alen !== undefined) {
                for(var j=0; j<alen; j++) {
                    var _elem_ = a[j];
                    var evale = '_elem_ '+ss[ssi];
                    if (ssi1 >= sslen) {
                        evalAssign(evale, value);
                    } else {
                        var b = eval(evale);
                        defaultMultiAssignRecursion(ss, sslen, ssi1, b, index, value);
                    }
                }
            }
        }\n;

        function defaultAssignFunction(varname, index, value) {
            // console.log("ASSIGN", varname, index, value);
            if (jsvar.variableUpdate[varname] === null) return;
            if (jsvar.variableUpdate[varname] == undefined) setVariableUpdateFunctionToDefault(varname);
            if (jsvar.variableUpdate[varname] !== null) jsvar.variableUpdate[varname](index, value);
        }\n;

        function setVariableUpdateFunctionToDefault(varname) {
			// TODO: variable names from here may clash with suer variable names, make them unique
            // TODO: In some ultra safe mode we can check syntax of varname (not to contain 
            // parenthesis and invoking unsafe function for example)
            if (varname.match(/\\[\\*\\]/)) {
                // multi assign
                var ss = varname.split(/\\[\\*\\]/g);
                var sslen = ss.length;
                jsvar.variableUpdate[varname] = function(index, value) {
                    var _a_ = eval(ss[0]);
                    multiAssignRecursion(ss, sslen, 1, _a_, index, value);
                }
            } else if (varname.match(/\\~$/)) {
                // array watch assignement, this is unusual if the user defined such variable, he shall manage it
                console.log("JsVar: Warning: Unmanaged watch variable", varname, ". Did you forget to set jsvar.variableUpdate[...]?");
                jsvar.variableUpdate[varname] = null;
            } else if (varname.match(/\\[\\!\\]/)) {
                // array resize information
                console.log("JsVar: Error: Update on array dimension: Not yet implemented: ", varname);
                jsvar.variableUpdate[varname] = function (index, value) {
                    // console.log("UPDATE ON ARRAY DIMENSION", varname, index, value);
                }
            } else if (varname.match(/\\[\\?\\]/)) {
                // array element assignement
				// TODO: remove those splits and simply replace [?] by [index]
                var split = varname.split(/\\[\\?\\]/);
                var aname = split[0] + '[';
                var bname = ']' + split[1];
				if (jsvar.allowFunctionalNames && bname.slice(-1) == ";") {
					jsvar.variableUpdate[varname] = function(index, value) {
						var command = aname + index + bname;
						evalSplitCommand(command, value);
					}					
				} else {
					jsvar.variableUpdate[varname] = function(index, value) {
						var varname = aname + index + bname;
						evalAssign(varname, value);
					}
				}
            } else {
                // single variable assignemt
                var allowCtoJscriptUpdatesFlag = maybeAddDefaultInitJavascriptToCupdateFunction(varname);
                if (allowCtoJscriptUpdatesFlag) {
					if (jsvar.allowFunctionalNames && varname.slice(-1) == ";") {
						var command = varname;
						jsvar.variableUpdate[varname] = function(index, value) {
							evalSplitCommand(command, value);
						}
					} else {
						jsvar.variableUpdate[varname] = function(index, value) {
							evalAssign(varname, value);
						};
					}
                } else {
                    jsvar.variableUpdate[varname] = null;
                }
            }
        }\n;

        function defaultResizeFunction(varname, value) {
            if (jsvar.debuglevel > 10) console.log("JsVar: Resizing ", varname, " to ", value);
            if (jsvar.variableArrayResize[varname] !== null) {
                if (jsvar.variableArrayResize[varname] == undefined) setVariableArrayResizeFunctionToDefault(varname);
                if (jsvar.variableArrayResize[varname] !== null) jsvar.variableArrayResize[varname](value);
            }
        }\n;

        function setVariableArrayResizeFunctionToDefault(varname) {
			// TODO: variable names from here may clash with suer variable names, make them unique
			var baseTypeFlag = (varname.substr(-1) == "]");
            if (varname.match(/\\~$/)) {
                jsvar.variableArrayResize[varname] = null;
            } else {
                var varsplit = varname.split(/\\[\\?\\]/);
                if (varsplit.length < 2) {
                    jsvar.variableArrayResize[varname] = null;
                    return;
                }
                jsvar.variableArrayResize[varname] = function(newDim) {
					try {
						var _arr = eval(varsplit[0]);
					} catch (e) {
						console.log("JsVar: Warning: Problem while resizing variable ", varsplit[0], "! Trying to fix it by creating intermediate objects/arrays.");
						createVariableIntermediateObjectsOrArrays(varsplit[0]);
						var _arr = eval(varsplit[0] + '= [];'); 
					}
                    // console.log("resizing A", varsplit[0], _arr);
                    // if (_arr == undefined) _arr = eval(varsplit[0] + '= [];'); 
                    var oldSize = _arr.length;
                    var userresize = jsvar.variableArrayResize[varsplit[0]];
                    if (userresize === null) return;
                    // console.log("resizing B", oldSize, newDim);
                    if (oldSize != newDim) {
                        if (userresize != undefined) {
                            if (userresize != null) userresize(newDim);
                        } else {
							// I do not know what is better to do in case if new size is smaller than old size.
							// If I shrink the size to the new size, then if we have two variables
							// pointing to the same array, like a[?].x and a[?].y. If a[?].y has smaller dimension (in C) than a[?].x, then 
							// the shrinking would shrink a[?] to the size of a[?].y and delete valid values of a[?].x.
							// If I do not shrink than the sizes of vectors will not match between C and JS.
                            if (jsvar.debuglevel > 20) console.log("JsVar: Default Array Resize: ", varname, oldSize, newDim);
                            if (newDim > oldSize) {
								// expand / shrink
								_arr.length = newDim;
								for(var i=oldSize; i<newDim; i++) if (baseTypeFlag) _arr[i] = 0; else if (typeof _arr[i] !== 'object') _arr[i] = {};
							} else {
								// soft shrink
								for(var i=newDim; i<oldSize; i++) defaultAssignFunction(varname, i, undefined);
							}
                        }
                    }
                }
            }
        }\n;

        function maybeAddDefaultInitJavascriptToCupdateFunction(varname) {
            // check if this is an input variable
            // create javascript to c updaters and return their type
            if (varname.match(/\\.value$/)) {
                var ss = varname.split(/\\.value$/);
                var elem = eval(ss[0]);
                if (elem != null && elem != undefined) {
                    var res = setJsToCupdateFunctionValueSuffix(elem, varname);
                    return(res);
                }
            }
            return(true);
        }\n;

        function dataJsvarPush(elem, field) {
            var js = elem['data-jsvar'];
            if (js == null || js == undefined) {
                js = elem['data-jsvar'] = [];
            }
            js.push(field);
        }\n;

        function dataJsvarContains(elem, field) {
            var js = elem['data-jsvar'];
            if (js == null || js == undefined) return(false);
            var jslen = js.length;
            for(var i=0; i<jslen; i++) {
                if (js[i] == field) return(true);
            }
            return(false);
        }\n;

        function jsToCupdateDoubleFunctionFunction(varname, previousFunction) {
            // console.log("jsToCupdateDoubleFunctionFunction", varname, previousFunction);
            var res = function(event) {
                if (previousFunction !== null && previousFunction !== undefined) previousFunction(event);
                jsvar.noteUpdateNumber(varname);
            };
            return(res);
        }\n;
        function jsToCupdateStringFunctionFunction(varname, previousFunction) {
            // console.log("jsToCupdateStringFunctionFunction", varname, previousFunction);
            var res = function(event) {
                if (previousFunction !== null && previousFunction !== undefined) previousFunction(event);
                jsvar.noteUpdateString(varname);
            };
            return(res);
        }\n;

        function setJsToCupdateField(elem, varname, field, type) {
            // console.log("setJsToCupdateField", elem, varname, field, type);
            // If our function is yet there, do not add another one (on reconnection for example)
            if (dataJsvarContains(elem, field)) return;
            switch(type) {
            case 'double':
                elem[field] = jsToCupdateDoubleFunctionFunction(varname, elem[field]);
                dataJsvarPush(elem, field);
                break;
            case 'string':
                elem[field] = jsToCupdateStringFunctionFunction(varname, elem[field]);
                dataJsvarPush(elem, field);
                break;
            default:
                console.log("setJsToCupdateField: unknown type", type);
                break;
            }
        }\n;

        function setJsToCupdateFunctionValueSuffix(elem, varname) {
            var dtype = null;
            var allowCtoJscriptUpdates = true;
            switch (elem.nodeName) {
            case "BUTTON":
                dtype = 'string';
                setJsToCupdateField(elem, varname, 'onclick', dtype);
                allowCtoJscriptUpdates = false;
                break;
            case "INPUT":
                switch (elem.type) {
                case "number":
                    dtype = 'double';
                    setJsToCupdateField(elem, varname, 'onchange', dtype);
                    break;
                case "button":
                    dtype = 'string';
                    setJsToCupdateField(elem, varname, 'onclick', dtype);
                    allowCtoJscriptUpdates = false;
                    break;
                default:
                    dtype = 'string';
                    setJsToCupdateField(elem, varname, 'onchange', dtype);
                    break;
                }
                break;
            case "SELECT":
                dtype = 'string';
                setJsToCupdateField(elem, varname, 'onchange', dtype);
                allowCtoJscriptUpdates = false;
                break;
            }
            return(allowCtoJscriptUpdates);
        }\n;

        function startJsvar() {
            if (jsvar.debuglevel > 0) console.log("JsVar: start.");
            // initial websocket opening
            jsvarOpenWebSocket();
        }\n;
    }\n;
    \n
    jsvar.initialize();
    \n
    );

//////////////////////////////////////////////////////////////////////////////////

static int jsvarCallbackAcceptFilter(struct wsaio *ww, unsigned ip, unsigned port) {
    struct jsVaraio     *jj;
    int                 r;

    jj = (struct jsVaraio *) ww;
    r = 0;
    JSVAR_CALLBACK_CALL(jj->callBackAcceptFilter, (r = callBack(jj, ip, port)));
    return(r);
}

static int jsvarCallbackOnAccept(struct wsaio *ww) {
    struct jsVaraio     *jj;
    jj = (struct jsVaraio *) ww;
    JSVAR_CALLBACK_CALL(jj->callBackOnAccept, callBack(jj));
    return(0);
}

static int jsvarCallbackOnTcpIpAccept(struct wsaio *ww) {
    struct jsVaraio     *jj;
    jj = (struct jsVaraio *) ww;

    jsVarCallBackCloneHook(&jj->callBackAcceptFilter, NULL);
    jsVarCallBackCloneHook(&jj->callBackOnTcpIpAccept, NULL);
    jsVarCallBackCloneHook(&jj->callBackOnAccept, NULL);
    jsVarCallBackCloneHook(&jj->callBackOnWwwGetRequest, NULL);
    jsVarCallBackCloneHook(&jj->callBackOnWwwPostRequest, NULL);
    jsVarCallBackCloneHook(&jj->callBackOnWebsocketAccept, NULL);
    jsVarCallBackCloneHook(&jj->callBackOnDelete, NULL);

    JSVAR_CALLBACK_CALL(jj->callBackOnTcpIpAccept, callBack(jj));
    return(0);
}


static int jsvarCallbackOnWebSocketAccept(struct wsaio *ww, char *uri) {
    struct jsVaraio             *jj;
    struct jsVarCompressionData *dd;
    struct jsVar                *v;

    jj = (struct jsVaraio *) ww;

    jj->w.b.optimizeForSpeed = 0;

    dd = jsVarCompressionGlobal;
    if (dd != NULL) {
        // this is a hack, will not work in multithreaded environment !
        jsVarCompressionGlobal = NULL;

        // send the global dictionary to the client
        // In fact, we create a new variable, bind it to ww and then delete it.
        // This makes the trick, as the values will be submitted to ww only.
        v = jsVarSynchronizeStringArray(jsVarSpaceGlobal, "jsvar.dictionary[?]", dd->dictionary,  sizeof(dd->dictionary[0]), JSVAR_DICTIONARY_SIZE);
        jsVarBindVariableToConnection(v, jj);
        jsVarDeleteVariable(v);

        // now install the new compression dictionary for next messages
        jsVarCompressionGlobal = dd;
    }

    // user's callbacks
    JSVAR_CALLBACK_CALL(jj->callBackOnWebsocketAccept, callBack(jj, uri));

    return(1);
}

static int jsvarCallbackOnWebSocketGet(struct wsaio *ww, int fromj, int n) {
    struct jsVaraio *jj;
    char            *ss, *ss0;
    
    jj = (struct jsVaraio *) ww;
    ss = ss0 = jj->w.b.readBuffer.b + fromj;
    ss = jsVarParseVariableUpdate(jj, ss, n);
    if (ss == NULL) {
        printf("%s: %s:%d: Error in message: %.*s\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, n, ss0);    
        return(-1);
    }
    return(1);
}

static int jsvarCallbackOnWwwGetRequest(struct wsaio *ww, char *uri) {
    struct jsVaraio     *jj;
    int                 r, jslen;
    struct baio         *bb;

    jj = (struct jsVaraio *) ww;
    bb = &ww->b;
    
    if (strcmp(uri, "/jsvar.js") == 0) {
        // TODO: This shall be done by some assynchronous string copy in order not to loose connection when write buffer is full
        jslen = strlen(jsvarJavascriptText);
#if 1 || EDEBUG
        wsaioHttpForwardString(ww, jsvarJavascriptText, jslen, "text/javascript");
#else
        r = baioPrintfToBuffer(bb, "%s", jsvarJavascriptText);
        if (r < jslen) {
            printf("%s: %s:%d: Error: Can't send jsvar.js. Probably two small write buffer. Try to increase 'maxWriteBufferSize' !\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
            baioCloseOnError(&ww->b);
            return(-1);
        }
        wsaioHttpFinishAnswer(ww, "200 OK", "text/javascript", NULL);
#endif
        return(1);
    }
    
    // user's callbacks
    r = 0;
    JSVAR_CALLBACK_CALL(jj->callBackOnWwwGetRequest, (r = callBack(jj, uri)));

    return(r);
}

static int jsvarCallbackOnWwwPostRequest(struct wsaio *ww, char *uri) {
    struct jsVaraio     *jj;
	int					r;

    jj = (struct jsVaraio *) ww;

    r = 0;
    JSVAR_CALLBACK_CALL(jj->callBackOnWwwPostRequest, (r = callBack(jj, uri)));
    return(r);
}


static int jsvarCallbackOnDelete(struct wsaio *ww) {
    struct jsVaraio *jj;

    jj = (struct jsVaraio *) ww;

    // call user's callbacks
    JSVAR_CALLBACK_CALL(jj->callBackOnDelete, callBack(jj));

    if (jj->w.b.baioType == BAIO_TYPE_TCPIP_SERVER) {
        JSVAR_FREE(jj->singlePageHeader);
        JSVAR_FREE(jj->singlePageBody);
    }

    jsVarCallBackFreeHook(&jj->callBackAcceptFilter);
    jsVarCallBackFreeHook(&jj->callBackOnTcpIpAccept);
    jsVarCallBackFreeHook(&jj->callBackOnAccept);
    jsVarCallBackFreeHook(&jj->callBackOnWwwGetRequest);
    jsVarCallBackFreeHook(&jj->callBackOnWwwPostRequest);
    jsVarCallBackFreeHook(&jj->callBackOnWebsocketAccept);
    jsVarCallBackFreeHook(&jj->callBackOnDelete);

    // clear compression data
    // if (jj->compressionData != NULL) jsVarCompressionDataFree(jj->compressionData);
    // jj->compressionData = NULL;

    return(1);
}

void jsVarServerSetFilter(struct jsVaraio *jj, int (*filterFunction)(struct jsVaraio *jj, struct jsVar *v, int index)) {
    jj->filterFunction = filterFunction;
}

void jsVarInit() {
    if (jsVarSpaceGlobal == NULL) {
        // Maybe reset var space table
        jsVarSpaceGlobal = jsVarSpaceCreate();
    }
}

struct jsVaraio *jsVarNewServer(int port, enum baioSslFlags sslFlag, int additionalSpaceToAllocate) {
    struct wsaio        *ww;
    struct jsVaraio     *jj;
    
    ww = wsaioNewServer(port, sslFlag, sizeof(struct jsVaraio) - sizeof(struct wsaio) + additionalSpaceToAllocate);
    if (ww == NULL) return(NULL);

    jj = (struct jsVaraio *) ww;
    jj->filterFunction = NULL;
    jj->singlePageHeader = NULL;
    jj->singlePageBody = NULL;

    // we need to be able to write jsvar.js at once. TODO: Copy jsvarJavascriptText by some assynchronous pipe and remove this requirement
    // jj->w.minSpaceInWriteBufferToProcessRequest = strlen(jsvarJavascriptText) + 1024;

    jsVarCallBackAddToHook(&ww->callBackAcceptFilter, (void *) jsvarCallbackAcceptFilter);
    jsVarCallBackAddToHook(&ww->callBackOnTcpIpAccept, (void *) jsvarCallbackOnTcpIpAccept);
    jsVarCallBackAddToHook(&ww->callBackOnAccept, (void *) jsvarCallbackOnAccept);
    jsVarCallBackAddToHook(&ww->callBackOnWwwGetRequest, (void *) jsvarCallbackOnWwwGetRequest);
    jsVarCallBackAddToHook(&ww->callBackOnWwwPostRequest, (void *) jsvarCallbackOnWwwPostRequest);
    jsVarCallBackAddToHook(&ww->callBackOnWebsocketAccept, (void *) jsvarCallbackOnWebSocketAccept);
    jsVarCallBackAddToHook(&ww->callBackOnWebsocketGetMessage, (void *) jsvarCallbackOnWebSocketGet);
    jsVarCallBackAddToHook(&ww->callBackOnDelete, (void *) jsvarCallbackOnDelete);

    return(jj);
}

////////////

static int jsVarOnFileServerGetRequest(struct jsVaraio *jj, char *uri) {
    struct baio             *bb;
    int                     i,j;
    struct wsaio            *ww;
    char                    path[JSVAR_PATH_MAX];
    char                    fname[2*JSVAR_PATH_MAX];
    struct stat             st;

    ww = &jj->w;
    bb = &ww->b;

    // printf("%s: FileServer: GET %s\n", JSVAR_PRINT_PREFIX(), uri);

    if (uri == NULL) goto fail;
    if (uri[0] != '/') goto fail;

    // resolve the name to avoid toxic names like "../../secret.txt"
    for(i=0,j=0; i<JSVAR_PATH_MAX && uri[j];) {
        path[i] = uri[j];
        if (uri[j]=='/' && uri[j+1] == '.' && (uri[j+2] == '/' || uri[j+2] == 0)) {
            j += 2;
        } else if (uri[j]=='/' && uri[j+1] == '.' && uri[j+2] == '.' && (uri[j+3] == '/' || uri[j+3] == 0)) {
            j += 3;
            if (i <= 0) goto fail;
            for(i--; i>=0 && path[i] != '/'; i--) ;
            if (i < 0) goto fail;
            i ++;
        } else {
            path[i++] = uri[j++];
        }
    }
    if (i <= 0 || i >= sizeof(path) - 1) goto fail;
    if (path[i-1] == '/' && i < sizeof(path) - sizeof(JSVAR_INDEX_FILE_NAME) - 1) {
        strcpy(&path[i], JSVAR_INDEX_FILE_NAME);
        i += sizeof(JSVAR_INDEX_FILE_NAME);
    }
    path[i] = 0;

    // printf("%s: Resolved to %s\n", JSVAR_PRINT_PREFIX(), path);

    snprintf(fname, sizeof(fname), "%s%s", jj->fileServerRootDir, path);
    fname[sizeof(fname)-1] = 0;
    
    // printf("%s: File name %s\n", JSVAR_PRINT_PREFIX(), fname);

    // check for file existance
    if (stat(fname, &st) != 0) goto fail;
    
    // return the file
    wsaioHttpSendFileAsync(ww, fname, NULL);
    return(1);

fail:
    wsaioHttpFinishAnswer(ww, "404 Not Found", "text/html", NULL);
    return(-1);
}

static int jsVarOnDefaultServersWebsocketAccept(struct jsVaraio *jj, char *uri) {
    jsVarBindPrefixVariables(jj, jsVarSpaceGlobal, "");
    return(0);
}

struct jsVaraio *jsVarNewFileServer(int port, enum baioSslFlags sslFlag, int additionalSpaceToAllocate, char *rootDirectory) {
    struct jsVaraio *res;

    res = jsVarNewServer(port, sslFlag, additionalSpaceToAllocate);
	if (res == NULL) return(res);

    res->fileServerRootDir = strDuplicate(rootDirectory);
    jsVarCallBackAddToHook(&res->callBackOnWwwGetRequest, (void *) jsVarOnFileServerGetRequest);
    jsVarCallBackAddToHook(&res->callBackOnWebsocketAccept, (void *) jsVarOnDefaultServersWebsocketAccept);
    return(res);
}


/////////

static char *jsVarSinglePageServerJavascriptFormat = JSVAR_STRINGIFY(
    <html>
    <head>
      <script type="text/javascript" src="/jsvar.js"></script>
      %s
    </head>
    <body>
      <script>
        if (typeof jsvar==="undefined") {
          jsvar={};
          jsvar.connected=false;
        };
      </script>
      %s
    </body>
    </html>
    );

static int jsVarOnSinglePageServerGetRequest(struct jsVaraio *jj, char *uri) {
    struct baio             *bb;
    struct wsaio            *ww;

    ww = &jj->w;
    bb = &ww->b;

    // printf("%s: SinglePageServer: GET %s\n", JSVAR_PRINT_PREFIX(), uri);

    baioPrintfToBuffer(
        bb, 
        "<html><head><script type=\"text/javascript\" src=\"/jsvar.js\"></script>%s</head><body><script>if(typeof jsvar===\"undefined\") {jsvar={};jsvar.connected=false;};</script>%s</body></html>",
        jj->singlePageHeader, 
        jj->singlePageBody
        );
    wsaioHttpFinishAnswer(ww, "200 OK", "text/html", NULL);
    return(1);
}

struct jsVaraio *jsVarNewSinglePageServer(int port, enum baioSslFlags sslFlag, int additionalSpaceToAllocate, char *header, char *body) {
    struct jsVaraio *res;

    res = jsVarNewServer(port, sslFlag, additionalSpaceToAllocate);
	if (res == NULL) return(res);

    res->singlePageHeader = strDuplicate(header);
    res->singlePageBody = strDuplicate(body);
    res->w.minSpaceInWriteBufferToProcessRequest = strlen(header)+strlen(body)+strlen(jsVarSinglePageServerJavascriptFormat)+256;

    jsVarCallBackAddToHook(&res->callBackOnWwwGetRequest, (void *) jsVarOnSinglePageServerGetRequest);
    jsVarCallBackAddToHook(&res->callBackOnWebsocketAccept, (void *) jsVarOnDefaultServersWebsocketAccept);
    return(res);
}

