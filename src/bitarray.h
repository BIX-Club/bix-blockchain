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

#ifndef __BITARRAY_H
#define __BITARRAY_H

/*  some constants depending on architercture	*/

#define USE_LONG_BITARRAYS 0
#if USE_LONG_BITARRAYS
typedef unsigned long long bitArray;
#define __BIT_ARR_NBITSLOG 	6		/* log_2(__BIT_ARR_NBITS) */
//typedef unsigned long bitArray;
//#define __BIT_ARR_NBITSLOG 	5		/* log_2(__BIT_ARR_NBITS) */
#else
typedef unsigned char bitArray;
#define __BIT_ARR_NBITSLOG 	3		/* log_2(__BIT_ARR_NBITS) */
#endif

#define __BIT_ARR_NBITS 	(1<<__BIT_ARR_NBITSLOG)			/* number of bits */



/* auxiliary macros */
#define __BIT_ARR_MODMSK 	(__BIT_ARR_NBITS-1)
#define __BIT_ARR_DIVMSK 	(~(__BIT_ARR_MODMSK))

#define __BIT_ARR_DIVNBITS(n) 	(((n) & __BIT_ARR_DIVMSK) >> __BIT_ARR_NBITSLOG)
#define __BIT_ARR_N_TH_BIT(n) 	(1 << ((n) & __BIT_ARR_MODMSK))


/* main macros    */
#define BIT_ARR_DIM(nn) 		(((nn)+__BIT_ARR_NBITS-1)/__BIT_ARR_NBITS)

#define BIT_VALUE(bitarr,s) 	((bitarr[__BIT_ARR_DIVNBITS(s)] & __BIT_ARR_N_TH_BIT(s))!=0)
#define BIT_SET(bitarr,s) 		{bitarr[__BIT_ARR_DIVNBITS(s)] |= __BIT_ARR_N_TH_BIT(s);}
#define BIT_CLEAR(bitarr,s) 	{bitarr[__BIT_ARR_DIVNBITS(s)] &= ~(__BIT_ARR_N_TH_BIT(s));}
#define BIT_ASSIGN(bitarr,s,v) 	{if (v) {BIT_SET(bitarr,s);} else {BIT_CLEAR(bitarr,s);}}

#define BIT_ARRAY_UNION(res, aa1, aa2, bitcount) {			\
		int 		_i_, _n_;								\
		bitArray	*_r_, *_a1_, *_a2_;						\
		_n_ = BIT_ARR_DIM(bitcount);						\
		_r_ = (res);										\
		_a1_ = (aa1);										\
		_a2_ = (aa2);										\
		for(_i_=0; _i_<_n_; _i_++) _r_[_i_] = _a1_[_i_] | _a2_[_i_];	\
	}

#define BIT_ARRAY_INTERSECTION(res, aa1, aa2, bitcount) {	\
		int 		_i_, _n_;							\
		bitArray	*_r_, *_a1_, *_a2_;						\
		_n_ = BIT_ARR_DIM(bitcount);						\
		_r_ = (res);										\
		_a1_ = (aa1);										\
		_a2_ = (aa2);										\
		for(_i_=0; _i_<_n_; _i_++) _r_[_i_] = _a1_[_i_] & _a2_[_i_];	\
	}


#define BIT_ARRAY_CLEAR(aa, bitcount) {				\
		int 		_i_, _n_;						\
		bitArray	*_a_;							\
		_n_ = BIT_ARR_DIM(bitcount);				\
		_a_ = (aa);									\
		for(_i_=0; _i_<_n_; _i_++) _a_[_i_] = 0 ;	\
	}

#define BIT_ARRAY_SET(aa, bitcount) {				\
		int 		_i_, _n_;						\
		bitArray	*_a_;							\
		_n_ = BIT_ARR_DIM(bitcount);				\
		_a_ = (aa);									\
		for(_i_=0; _i_<_n_; _i_++) _a_[_i_] = ~0 ;	\
	}

#define BIT_ARRAY_IS_ZERO(aa, bitCount) 		( __bitArrayIsZero(aa, bitCount))
#define BIT_ARRAY_EQUALS(aa1, aa2, bitCount) 	( __bitArrayEquals(aa1, aa2, bitCount))
#define BIT_ARRAY_COPY(dest, src, bitCount) 	( __bitArrayCopy(dest, src, bitCount))

static inline int __bitArrayIsZero(bitArray *a, int bitCount) {
	int 		i, n;
	bitArray	lm;
	n = bitCount / __BIT_ARR_NBITS;
	for(i=0; i<n; i++) if (a[i]) return(0);
	n = bitCount % __BIT_ARR_NBITS;
	if (n != 0) {
		lm = 0;
		for(i=0; i<n; i++) lm = (lm<<1)+1;
		// lm = ((bitArray)1 << (n+1)) - 1;
		if (a[i] & lm) return(0);
	}
	return(1);
}

static inline int __bitArrayEquals(bitArray *a1, bitArray *a2, int bitCount) {
	int 		i, n;
	bitArray	lm;
	n = bitCount / __BIT_ARR_NBITS;
	for(i=0; i<n; i++) if (a1[i] != a2[i]) return(0);
	n = bitCount % __BIT_ARR_NBITS;
	if (n != 0) {
		lm = 0;
		for(i=0; i<n; i++) lm = (lm<<1)+1;
		// lm = ((bitArray)1 << (n+1)) - 1;
		if ((a1[i]&lm) != (a2[i]&lm)) return(0);
	}
	return(1);
}


static inline void __bitArrayCopy(bitArray *a1, bitArray *a2, int bitCount) {
	int 		i, n;
	bitArray	lm;
	n = bitCount / __BIT_ARR_NBITS;
	for(i=0; i<n; i++) a1[i] = a2[i];
	n = bitCount % __BIT_ARR_NBITS;
	if (n != 0) {
		lm = 0;
		for(i=0; i<n; i++) lm = (lm<<1)+1;
		// lm = ((bitArray)1 << (n+1)) - 1;
		a1[i] = ((a1[i]&(~lm)) | (a2[i]&lm));
	}
}


// extern void bitmapdump(bitArray *m, int len, char *name);

#endif





