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

#include <malloc.h>
#include "expmem.h"

#if 1
#define ALLOC(p,t)          {(p) = (t*) expmemMalloc(sizeof(t)); if((p)==NULL) {printf("Out of memory\n"); assert(0); exit(1);}}
#define REALLOC(p,t)        {(p) = (t*) expmemRealloc((p), sizeof(t)); if((p)==NULL && (n)!=0) {printf("Out of memory\n"); assert(0); exit(1);}}
#define ALLOCC(p,n,t)       {(p) = (t*) expmemMalloc((n)*sizeof(t)); if((p)==NULL && (n)!=0) {printf("Out of memory\n"); assert(0); exit(1);}}
#define REALLOCC(p,n,t)     {(p) = (t*) expmemRealloc((p), (n)*sizeof(t)); if((p)==NULL && (n)!=0) {printf("Out of memory\n"); assert(0); exit(1);}}
#define ALLOC_SIZE(p,t,n)   {(p) = (t*) expmemMalloc(n); if((p)==NULL && (n)!=0) {printf("Out of memory\n"); assert(0); exit(1);}}
#define FREE(p)             {expmemFree(p); }
#define NEXT_RECOMMENDED_BUFFER_SIZE(size) expmemNextRecommendedBufferSize(size);
#else
#define ALLOC(p,t)          {(p) = (t*) malloc(sizeof(t)); if((p)==NULL) {printf("Out of memory\n"); assert(0); exit(1);}}
#define REALLOC(p,t)        {(p) = (t*) realloc((p), sizeof(t)); if((p)==NULL && (n)!=0) {printf("Out of memory\n"); assert(0); exit(1);}}
#define ALLOCC(p,n,t)       {(p) = (t*) malloc((n)*sizeof(t)); if((p)==NULL && (n)!=0) {printf("Out of memory\n"); assert(0); exit(1);}}
#define REALLOCC(p,n,t)     {(p) = (t*) realloc((p), (n)*sizeof(t)); if((p)==NULL && (n)!=0) {printf("Out of memory\n"); assert(0); exit(1);}}
#define ALLOC_SIZE(p,t,n)   {(p) = (t*) malloc(n); if((p)==NULL && (n)!=0) {printf("Out of memory\n"); assert(0); exit(1);}}
#define FREE(p)             {free(p); }
#define NEXT_RECOMMENDED_BUFFER_SIZE(size) (size*2+1);
#endif
////

#define JSVAR_ALLOC(p,t)					ALLOC(p,t)
#define JSVAR_REALLOC(p,t)					REALLOC(p,t)
#define JSVAR_ALLOCC(p,n,t)					ALLOCC(p,n,t)
#define JSVAR_REALLOCC(p,n,t)				REALLOCC(p,n,t)
#define JSVAR_ALLOC_SIZE(p,t,n)				ALLOC_SIZE(p,t,n)
#define JSVAR_FREE(p)						FREE(p)


#define JSVAR_INITIAL_DYNAMIC_STRING_SIZE	(1<<16)
#define JSVAR_PRINT_PREFIX()				bchainPrintPrefix_st()
#define JSVAR_STATIC

#define JSVAR_USE_OPEN_SSL					1
// regexp is not available under Windows
#define JSVAR_USE_REGEXP					0

////////////////////////////////////////////////

char *bchainPrintPrefix_st();

