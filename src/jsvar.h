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

#ifndef __JSVAR__H
#define __JSVAR__H

#include "jsvarcustomization.h"

#include <stdint.h>
#include <wchar.h>
// for fd_set definition
#include <sys/types.h>

#if BAIO_USE_OPEN_SSL || WSAIO_USE_OPEN_SSL || JSVAR_USE_OPEN_SSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

#if _WIN32
#if defined(JSVAR_WIN_FD_SETSIZE)
#define FD_SETSIZE							JSVAR_WIN_FD_SETSIZE
#endif
#include <Winsock2.h>
#define JSVAR_ZERO_SIZED_ARRAY_SIZE         1
#define JSVAR_STR_ERRNO()       			(strerror(WSAGetLastError()))

#else

#define JSVAR_ZERO_SIZED_ARRAY_SIZE         0
#define JSVAR_STR_ERRNO()                   strerror(errno)

#endif


// simplify definition of large strings
#define JSVAR_STRINGIFY(...)                #__VA_ARGS__


enum jsVarDynamicStringAllocationMethods {
    MSAM_NONE,
    MSAM_PURE_MALLOC,
    MSAM_DEFAULT,
    MSAM_MAX,
};

struct jsVarDynamicString {
    char *s;
    int size;
    int allocatedSize;
    int allocationMethod;
};

#define BAIO_MAX_CONNECTIONS                (1<<10)

enum baioDirection {
    BAIO_IO_DIRECTION_NONE,
    BAIO_IO_DIRECTION_READ,
    BAIO_IO_DIRECTION_WRITE,
    BAIO_IO_DIRECTION_RW,
    BAIO_IO_DIRECTION_MAX,
};

/////////////////////////////////////////////////////////////////////////////////////
// max user defined parameters inside assynchronous I/O structures
#define BAIO_MAX_USER_PARAMS                        4

// baio prefix stands for basic assynchronous input output

#define BAIO_WRITE_BUFFER_DO_FULL_SIZE_RING_CYCLES  0
#define BAIO_EACH_WRITTEN_MSG_HAS_RECORD            0

// Not all combinations of following masks are possible
#define BAIO_STATUS_ZOMBIE                          0x000000
#define BAIO_STATUS_ACTIVE                          0x000001
#define BAIO_STATUS_EOF_READ                        0x000002
#define BAIO_STATUS_PENDING_CLOSE                   0x000004

// event statuses, listen is not clearable status
#define BAIO_BLOCKED_FOR_READ_IN_TCPIP_LISTEN       0x000008
// clearable statuses
#define BAIO_BLOCKED_FOR_WRITE_IN_TCPIP_CONNECT     0x000010
#define BAIO_BLOCKED_FOR_READ_IN_SSL_CONNECT        0x000040
#define BAIO_BLOCKED_FOR_WRITE_IN_SSL_CONNECT       0x000080
#define BAIO_BLOCKED_FOR_READ_IN_SSL_ACCEPT         0x000100
#define BAIO_BLOCKED_FOR_WRITE_IN_SSL_ACCEPT        0x000200
#define BAIO_BLOCKED_FOR_READ_IN_READ               0x001000
#define BAIO_BLOCKED_FOR_WRITE_IN_WRITE             0x002000
#define BAIO_BLOCKED_FOR_READ_IN_SSL_READ           0x004000
#define BAIO_BLOCKED_FOR_WRITE_IN_SSL_WRITE         0x008000
#define BAIO_BLOCKED_FOR_WRITE_IN_SSL_READ          0x010000
#define BAIO_BLOCKED_FOR_READ_IN_SSL_WRITE          0x020000
#define BAIO_BUFFERED_DATA_AVAILABLE_FOR_SSL_READ   0x040000

enum baioSslFlags {
    BAIO_SSL_NO,
    BAIO_SSL_YES,
};

// user's can have values stored in structure baio. Those params are stored in an array
// of the following UNION type
union baioUserParam {
    void                *p;         // a pointer parameter
    int                 i;          // an integer parameter
    double              d;          // a double parameter
};

// write buffer is composed of "messages". I.e. continuous chunks of bytes
// with maybe non-empty spaces between them. Those spaces are not sent to socket
struct baioMsg {
    int startIndex;
    int endIndex;
};

struct baioMsgBuffer {
    struct baioMsg  *m;
    int             mi,mj;
    int             msize;
};

struct baioReadBuffer {
    // buffered characters
    char    *b;
    // read buffer is simply a linear array. Values between b[i] and b[j]
    // contains unread chars newly readed chars are appended after b[j].
    // Baio can move whole buffer (i.e. move b[i .. j] -> b[0 .. j-i) at any time.
    int     i,j;
    int     size;
};

struct baioWriteBuffer {
    // baio write buffer consists of two ring buffers.
    // The first one is storing bodies of messages (chars to be sent to 
    // socket) and can contain gaps.
    // The second one is storing message boundaries and hence determining 
    // where are the gaps in the first buffer.

    // The first ring buffer: message bodies
    char            *b;
    // We use a ring buffer for write. b[i] is the first unsent character
    // b[j] is the first free place where the next chracter can be stored.
    // b[i] - b[ij] are bytes ready for immediate send out (write). This is basically
    // the part of the first message which was not yet been written. I.e. b[ij] is 
    // the end of the message which is currently being written.
    // If the buffer was wrapped over the end of buffer (I.e. if j < i) 
    // and the currently written message is at the end of the buffer wrap, then b[jj]
    // is the first unused char at the end of the buffer (the b[jj-1] is the 
    // last used char of the buffer)
    // All indexes must be signed integers to avoid arithmetic problems
    int             i, j, ij, jj;
    int             size;

    // The second ring buffer: message boundaries
    struct baioMsg  *m;
    // This buffer is normally active only if
    // there are gaps in the first buffer or during the time when the first buffer wrapps
    // over the end and there are pending unsent chars at the end.
    // This is a ring buffer so valid messages intervals are stored between m[mi] 
    // and m[mj] (indexes modulo msize).
    int             mi,mj;
    int             msize;
};

typedef int (*jsVarCallBackHookFunType)(void *x, ...);

struct jsVarCallBackHook {
    jsVarCallBackHookFunType    *a;
    int                     i, dim;
};

struct baio {
    int                     baioType;               // type of baio (file, tcpIpClient, tcpIpServer, ...)
    int                     ioDirections;
    int                     fd;
    unsigned                status;
    int                     index;
    int                     baioMagic;
    struct baioReadBuffer   readBuffer;
    struct baioWriteBuffer  writeBuffer;

    // for statistics only
    int                     totalBytesRead;
    int                     totalBytesWritten;

    // when this is client's baio, this keeps client's ip address
    long unsigned           remoteComputerIp;
    // when this is client's baio, this keeps identification of the listening socket (service)
    int                     parentBaioMagic;

    // ssl stuff
    uint8_t                 useSsl;
#if BAIO_USE_OPEN_SSL || WSAIO_USE_OPEN_SSL || JSVAR_USE_OPEN_SSL
    SSL                     *sslHandle;
#else
    void                    *sslHandle;
#endif
	char					*sslSniHostName;
    int                     sslPending;

#if 0
    // Here are specified the actual types of parameters for callbacks
    int                     (*callBackOnRead)(struct baio *, int fromj, int n);
    int                     (*callBackOnWrite)(struct baio *, int fromi, int n);
    int                     (*callBackOnError)(struct baio *);
    int                     (*callBackOnEof)(struct baio *);
    int                     (*callBackOnDelete)(struct baio *);
    int                     (*callBackOnReadBufferShift)(struct baio *, int delta);
    // network client callbacks
    int                     (*callBackOnTcpIpConnect)(struct baio *);
    int                     (*callBackOnConnect)(struct baio *);
    // network server callbacks
    // accept is called as definitive accept (both for ssl and tcpip)
    int                     (*callBackOnAccept)(struct baio *);
    int                     (*callBackAcceptFilter)(struct baio *, unsigned ip, unsigned port);
    int                     (*callBackOnTcpIpAccept)(struct baio *);
#endif

    // If you add a callback hook, do not forget to FREE it in baioFreeZombie and CLONE it in cloneCallBackHooks
    // callbackhooks
    struct jsVarCallBackHook        callBackOnRead;
    struct jsVarCallBackHook        callBackOnWrite;
    struct jsVarCallBackHook        callBackOnError;
    struct jsVarCallBackHook        callBackOnEof;
    struct jsVarCallBackHook        callBackOnDelete;
    struct jsVarCallBackHook        callBackOnReadBufferShift;
    // network client callbacks
    struct jsVarCallBackHook        callBackOnTcpIpConnect;
    struct jsVarCallBackHook        callBackOnConnect;
    // network server callbacks
    // accept is called as definitive accept (both for ssl and tcpip)
    struct jsVarCallBackHook        callBackOnAccept;
    struct jsVarCallBackHook        callBackAcceptFilter;
    struct jsVarCallBackHook        callBackOnTcpIpAccept;

    // config values
    int                     initialReadBufferSize;
    int                     readBufferRecommendedSize;
    int                     initialWriteBufferSize;
    int                     minFreeSizeBeforeRead;
    int                     minFreeSizeAtEndOfReadBuffer;   // if you want to put zero for example
    int                     maxReadBufferSize;
    int                     maxWriteBufferSize;
    uint8_t                 optimizeForSpeed;

    // when we allocate this structure we allocate some additional space
    // for other extensions and we keep the size of additional space
    int                     additionalSpaceAllocated;
    // user defined values, they must be at the end of the structure!
    union baioUserParam     u[BAIO_MAX_USER_PARAMS];

    // here is the end of the structure
    void                    *userData[JSVAR_ZERO_SIZED_ARRAY_SIZE];
};

///////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////
// wsaio prefix stands for web server assynchronous input output

struct wsaioRequestData {
    char                    *uri;
    char                    *getQuery;
    char                    *postQuery;
};

struct wsaio {
    // underlying I/O, must be the first element of the structure!
    struct baio             b;      

    uint32_t                wsaioSecurityCheckCode;     // magic number 0xcacebaf
    int                     state;

#if 0
    // Callback parameter types
    int                     (*callBackAcceptFilter)(struct wsaio *ww, unsigned ip, unsigned port);
    int                     (*callBackOnAccept)(struct wsaio *ww);
    int                     (*callBackOnWwwGetRequest)(struct wsaio *ww, char *uri);
    int                     (*callBackOnWebsocketAccept)(struct wsaio *ww, char *uri);
    int                     (*callBackOnWebsocketGetMessage)(struct wsaio *ww, int fromj, int n);
    int                     (*callBackOnWebsocketSend)(struct wsaio *ww, int fromi, int n);
    int                     (*callBackOnDelete)(struct wsaio *ww);
#endif

    struct jsVarCallBackHook    callBackAcceptFilter;
    struct jsVarCallBackHook    callBackOnTcpIpAccept;
    struct jsVarCallBackHook    callBackOnAccept;
    struct jsVarCallBackHook    callBackOnWwwGetRequest;
    struct jsVarCallBackHook    callBackOnWwwPostRequest;
    struct jsVarCallBackHook    callBackOnWebsocketAccept;
    struct jsVarCallBackHook    callBackOnWebsocketGetMessage;
    struct jsVarCallBackHook    callBackOnWebsocketSend;
    struct jsVarCallBackHook    callBackOnDelete;

    // parsed current request
    struct wsaioRequestData currentRequest;
    // temporary value used for composing http answer
    int                     fixedSizeAnswerHeaderLen;
    int                     requestSize;
    // temporary value for receiving fragmented websocket messages
    int                     previouslySeenWebsocketFragmentsOffset;

    // webserver configuration
    char                    *serverName;
    // there is a space reserved for additional headers added after the message is processed (like "Content-length:xxxx")
    // This is the size of this reserved space
    int                     answerHeaderSpaceReserve;
    // The web request is going to be processed only if there is some minimal space available for writting
    // answer headers, chunk header, etc. (this value shall be larger than answerHeaderSpaceReserve)
    int                     minSpaceInWriteBufferToProcessRequest;  

    // when we allocate this structure we allocate some additional space
    // for other extensions and we keep the size of additional space
    int                     additionalSpaceAllocated;

    // user defined values, they must be at the end of the structure!
    // Hmm. do we need user params in wsaio when we have them in baio?
    // union userParam      u[BAIO_MAX_USER_PARAMS];

    // here is the end of the structure
    void                    *userData[JSVAR_ZERO_SIZED_ARRAY_SIZE];
};



///////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////
// javascript synchronisation

// forward declaration
struct jsVarSpace;

////////////////


enum jsvarMapOnFollowerOperationEnum {
    JSVMOFO_NONE,
    JSVMOFO_INIT_MESSAGE,
    JSVMOFO_WRITE_INDEX_LEN_AND_STRING_TO_BUFFER,
    JSVMOFO_WRITE_INDEX_AND_LLONG_VALUE,
    JSVMOFO_WRITE_ARRAY_RESIZE,
    JSVMOFO_WRITE_VALUES_TERMINAL_CHAR,
    JSVMOFO_COMPLETE_MSG,
    JSVMOFO_ADD_TO_INDEX_SET,
    JSVMOFO_MAX,
};

enum jsVarSyncDirections {
    JSVSD_NONE,
    JSVSD_C_TO_JAVASCRIPT,
    JSVSD_JAVASCRIPT_TO_C,
    JSVSD_BOTH,
    JSVSD_MAX,
};

enum jsVarTypes {
    JSVT_NONE,
    JSVT_CHAR,
    JSVT_SHORT,
    JSVT_INT,
    JSVT_LONG,
    JSVT_LLONG,
    JSVT_FLOAT,
    JSVT_DOUBLE,
    JSVT_STRING,
    JSVT_WSTRING,
    JSVT_U_CHAR,
    JSVT_U_SHORT,
    JSVT_U_INT,
    JSVT_U_LONG,
    JSVT_U_LLONG,

    JSVT_CHAR_ARRAY,
    JSVT_SHORT_ARRAY,
    JSVT_INT_ARRAY,
    JSVT_LONG_ARRAY,
    JSVT_LLONG_ARRAY,
    JSVT_FLOAT_ARRAY,
    JSVT_DOUBLE_ARRAY,
    JSVT_STRING_ARRAY,
    JSVT_WSTRING_ARRAY,
    JSVT_U_CHAR_ARRAY,
    JSVT_U_SHORT_ARRAY,
    JSVT_U_INT_ARRAY,
    JSVT_U_LONG_ARRAY,
    JSVT_U_LLONG_ARRAY,

    JSVT_CHAR_VECTOR,
    JSVT_SHORT_VECTOR,
    JSVT_INT_VECTOR,
    JSVT_LONG_VECTOR,
    JSVT_LLONG_VECTOR,
    JSVT_FLOAT_VECTOR,
    JSVT_DOUBLE_VECTOR,
    JSVT_STRING_VECTOR,
    JSVT_WSTRING_VECTOR,
    JSVT_U_CHAR_VECTOR,
    JSVT_U_SHORT_VECTOR,
    JSVT_U_INT_VECTOR,
    JSVT_U_LONG_VECTOR,
    JSVT_U_LLONG_VECTOR,

    JSVT_CHAR_LIST,
    JSVT_SHORT_LIST,
    JSVT_INT_LIST,
    JSVT_LONG_LIST,
    JSVT_LLONG_LIST,
    JSVT_FLOAT_LIST,
    JSVT_DOUBLE_LIST,
    JSVT_STRING_LIST,
    JSVT_WSTRING_LIST,
    JSVT_U_CHAR_LIST,
    JSVT_U_SHORT_LIST,
    JSVT_U_INT_LIST,
    JSVT_U_LONG_LIST,
    JSVT_U_LLONG_LIST,

    JSVT_MAX,
};

#define JSVT_CHAR_TCODE                             b
#define JSVT_SHORT_TCODE                            w
#define JSVT_INT_TCODE                              i
#define JSVT_LONG_TCODE                             l
#define JSVT_LLONG_TCODE                            ll
#define JSVT_FLOAT_TCODE                            f
#define JSVT_DOUBLE_TCODE                           d
#define JSVT_STRING_TCODE                           s
#define JSVT_WSTRING_TCODE                          ws
#define JSVT_U_CHAR_TCODE                           ub
#define JSVT_U_SHORT_TCODE                          uw
#define JSVT_U_INT_TCODE                            ui
#define JSVT_U_LONG_TCODE                           ul
#define JSVT_U_LLONG_TCODE                          ull

#define JSVAR_TCODE_TO_TYPE_ENUMb                   JSVT_CHAR
#define JSVAR_TCODE_TO_TYPE_ENUMw                   JSVT_SHORT
#define JSVAR_TCODE_TO_TYPE_ENUMi                   JSVT_INT
#define JSVAR_TCODE_TO_TYPE_ENUMl                   JSVT_LONG
#define JSVAR_TCODE_TO_TYPE_ENUMll                  JSVT_LLONG
#define JSVAR_TCODE_TO_TYPE_ENUMf                   JSVT_FLOAT
#define JSVAR_TCODE_TO_TYPE_ENUMd                   JSVT_DOUBLE
#define JSVAR_TCODE_TO_TYPE_ENUMs                   JSVT_STRING
#define JSVAR_TCODE_TO_TYPE_ENUMws                  JSVT_WSTRING
#define JSVAR_TCODE_TO_TYPE_ENUMba                  JSVT_CHAR_ARRAY
#define JSVAR_TCODE_TO_TYPE_ENUMwa                  JSVT_SHORT_ARRAY
#define JSVAR_TCODE_TO_TYPE_ENUMia                  JSVT_INT_ARRAY
#define JSVAR_TCODE_TO_TYPE_ENUMla                  JSVT_LONG_ARRAY
#define JSVAR_TCODE_TO_TYPE_ENUMlla                 JSVT_LLONG_ARRAY
#define JSVAR_TCODE_TO_TYPE_ENUMfa                  JSVT_FLOAT_ARRAY
#define JSVAR_TCODE_TO_TYPE_ENUMda                  JSVT_DOUBLE_ARRAY
#define JSVAR_TCODE_TO_TYPE_ENUMsa                  JSVT_STRING_ARRAY
#define JSVAR_TCODE_TO_TYPE_ENUMwsa                 JSVT_WSTRING_ARRAY
#define JSVAR_TCODE_TO_TYPE_ENUMbv                  JSVT_CHAR_VECTOR
#define JSVAR_TCODE_TO_TYPE_ENUMwv                  JSVT_SHORT_VECTOR
#define JSVAR_TCODE_TO_TYPE_ENUMiv                  JSVT_INT_VECTOR
#define JSVAR_TCODE_TO_TYPE_ENUMlv                  JSVT_LONG_VECTOR
#define JSVAR_TCODE_TO_TYPE_ENUMllv                 JSVT_LLONG_VECTOR
#define JSVAR_TCODE_TO_TYPE_ENUMfv                  JSVT_FLOAT_VECTOR
#define JSVAR_TCODE_TO_TYPE_ENUMdv                  JSVT_DOUBLE_VECTOR
#define JSVAR_TCODE_TO_TYPE_ENUMsv                  JSVT_STRING_VECTOR
#define JSVAR_TCODE_TO_TYPE_ENUMwsv                 JSVT_WSTRING_VECTOR
#define JSVAR_TCODE_TO_TYPE_ENUMbli                 JSVT_CHAR_LIST
#define JSVAR_TCODE_TO_TYPE_ENUMwli                 JSVT_SHORT_LIST
#define JSVAR_TCODE_TO_TYPE_ENUMili                 JSVT_INT_LIST
#define JSVAR_TCODE_TO_TYPE_ENUMlli                 JSVT_LONG_LIST
#define JSVAR_TCODE_TO_TYPE_ENUMllli                JSVT_LLONG_LIST
#define JSVAR_TCODE_TO_TYPE_ENUMfli                 JSVT_FLOAT_LIST
#define JSVAR_TCODE_TO_TYPE_ENUMdli                 JSVT_DOUBLE_LIST
#define JSVAR_TCODE_TO_TYPE_ENUMsli                 JSVT_STRING_LIST
#define JSVAR_TCODE_TO_TYPE_ENUMwsli                JSVT_WSTRING_LIST

#define JSVAR_TCODE_TO_TYPE_ENUMub                  JSVT_U_CHAR
#define JSVAR_TCODE_TO_TYPE_ENUMuw                  JSVT_U_SHORT
#define JSVAR_TCODE_TO_TYPE_ENUMui                  JSVT_U_INT
#define JSVAR_TCODE_TO_TYPE_ENUMul                  JSVT_U_LONG
#define JSVAR_TCODE_TO_TYPE_ENUMull                 JSVT_U_LLONG
#define JSVAR_TCODE_TO_TYPE_ENUMuba                 JSVT_U_CHAR_ARRAY
#define JSVAR_TCODE_TO_TYPE_ENUMuwa                 JSVT_U_SHORT_ARRAY
#define JSVAR_TCODE_TO_TYPE_ENUMuia                 JSVT_U_INT_ARRAY
#define JSVAR_TCODE_TO_TYPE_ENUMula                 JSVT_U_LONG_ARRAY
#define JSVAR_TCODE_TO_TYPE_ENUMulla                JSVT_U_LLONG_ARRAY
#define JSVAR_TCODE_TO_TYPE_ENUMubv                 JSVT_U_CHAR_VECTOR
#define JSVAR_TCODE_TO_TYPE_ENUMuwv                 JSVT_U_SHORT_VECTOR
#define JSVAR_TCODE_TO_TYPE_ENUMuiv                 JSVT_U_INT_VECTOR
#define JSVAR_TCODE_TO_TYPE_ENUMulv                 JSVT_U_LONG_VECTOR
#define JSVAR_TCODE_TO_TYPE_ENUMullv                JSVT_U_LLONG_VECTOR
#define JSVAR_TCODE_TO_TYPE_ENUMubli                JSVT_U_CHAR_LIST
#define JSVAR_TCODE_TO_TYPE_ENUMuwli                JSVT_U_SHORT_LIST
#define JSVAR_TCODE_TO_TYPE_ENUMuili                JSVT_U_INT_LIST
#define JSVAR_TCODE_TO_TYPE_ENUMulli                JSVT_U_LONG_LIST
#define JSVAR_TCODE_TO_TYPE_ENUMullli               JSVT_U_LLONG_LIST

#define JSVAR_TCODE_TO_TYPEb                        signed char
#define JSVAR_TCODE_TO_TYPEw                        short int
#define JSVAR_TCODE_TO_TYPEi                        int
#define JSVAR_TCODE_TO_TYPEl                        long
#define JSVAR_TCODE_TO_TYPEll                       long long
#define JSVAR_TCODE_TO_TYPEf                        float
#define JSVAR_TCODE_TO_TYPEd                        double
#define JSVAR_TCODE_TO_TYPEs                        char*
#define JSVAR_TCODE_TO_TYPEws                       wchar_t*
#define JSVAR_TCODE_TO_TYPEub                       unsigned char
#define JSVAR_TCODE_TO_TYPEuw                       unsigned short int
#define JSVAR_TCODE_TO_TYPEui                       unsigned 
#define JSVAR_TCODE_TO_TYPEul                       unsigned long
#define JSVAR_TCODE_TO_TYPEull                      unsigned long long

#define JSVAR_TCODE_TO_FUNCTION_TIDb                Char
#define JSVAR_TCODE_TO_FUNCTION_TIDw                Short
#define JSVAR_TCODE_TO_FUNCTION_TIDi                Int
#define JSVAR_TCODE_TO_FUNCTION_TIDl                Long
#define JSVAR_TCODE_TO_FUNCTION_TIDll               Llong
#define JSVAR_TCODE_TO_FUNCTION_TIDf                Float
#define JSVAR_TCODE_TO_FUNCTION_TIDd                Double
#define JSVAR_TCODE_TO_FUNCTION_TIDs                String
#define JSVAR_TCODE_TO_FUNCTION_TIDws               Wstring
#define JSVAR_TCODE_TO_FUNCTION_TIDub               Uchar
#define JSVAR_TCODE_TO_FUNCTION_TIDuw               Ushort
#define JSVAR_TCODE_TO_FUNCTION_TIDui               Uint
#define JSVAR_TCODE_TO_FUNCTION_TIDul               Ulong
#define JSVAR_TCODE_TO_FUNCTION_TIDull              Ullong

#define JSVAR_TCODE_FUNCTION_TID(tcode)             JSVAR_TCODE_TO_FUNCTION_TID##tcode
#define JSVAR_TCODE_TYPE_ENUM(tcode)                JSVAR_TCODE_TO_TYPE_ENUM##tcode
#define JSVAR_TCODE_TYPE(tcode)                     JSVAR_TCODE_TO_TYPE##tcode

#define JSVAR_LAST_SENT_VAL(var, tcode, tcodesuffix)    ((var)->lastSentVal.tcode##tcodesuffix)
#define JSVAR_ARRAY_ELEM_VALUE(var, tcode, index)       (*(JSVAR_TCODE_TYPE(tcode) *)((char*)((var)->valptr)+((index) * (var)->arrayVectorElementSize)))
#define JSVAR_ELEM_VALUE(var, tcode)                    JSVAR_ARRAY_ELEM_VALUE(var, tcode, 0)
#define JSVAR_VECTOR_ELEM_VALUE(var, tcode, index)      (*((JSVAR_TCODE_TYPE(tcode) *)((*((char**)((var)->valptr)))+((index) * (var)->arrayVectorElementSize)+(var)->vectorElementOffset)))
#define JSVAR_LIST_ELEM_VALUE(var, tcode, cptr)         (*(JSVAR_TCODE_TYPE(tcode) *)(var)->listValueFromElementFunction(cptr))
#define JSVAR_LIST_NEXT_VALUE(var, cptr, i)             ((var)->listNextElementFunction(cptr/*, i*/))


struct jsVarIndexSet {
    uint8_t     *bits;  // bit array bits[dim]
    int         *a;     // a[dim]
    int         ai;
    int         bitsDim;
    int         aDim;
};

// If changing this, review  also initialization of jsVarSpaceGlobal and adjust sentinel variable init!!!
struct jsVar {
    uint8_t             varType;                    // int, double, string , ....
    uint8_t             syncDirection;              // JS->C or C->JS or both
    struct jsVarSpace   *varSpace;                  // variable space where this variable belongs
    char                *jsname;                    // null terminated string
    int                 jsnameLength;

    struct jsVarCallBackHook    callBackOnChange;

    // the main pointer to access user data structure
    void                *valptr;

    // this is special for self created vectors
    void                *selfCreatedVectorAddr;

    // to get specific value from array and vectors
    int                 arrayDimension;
    int                 *vectorDimensionAddr;   // this is used for both array and vector, in case of array it stores address of arrayDimension field
    int                 arrayVectorElementSize;
    int                 vectorElementOffset;
    // and from lists
    void                *(*listValueFromElementFunction)(void *elem);
    void                *(*listNextElementFunction)(void *previousElem);

    int                 lastSentArrayDimension;

    union jsVarValueUnion {
        // single value
        int                 i;
        long                l;
        long long           ll;
        short int           w;
        signed char         b;
        float               f;
        double              d;
        unsigned int        ui;
        unsigned long       ul;
        unsigned long long  ull;
        unsigned short int  uw;
        unsigned char       ub;
        // array, vectors, list are represented by an array in javascript (as well as sent value)
        char                *s;
        wchar_t             *ws;

        int                 *ia;
        long                *la;
        long long           *lla;
        short int           *wa;
        signed char         *ba;
        float               *fa;
        double              *da;
        char                **sa;
        wchar_t             **wsa;
        unsigned int        *uia;
        unsigned long       *ula;
        unsigned long long  *ulla;
        unsigned short int  *uwa;
        unsigned char       *uba;
    } lastSentVal;

    // bitmap of indexes modified by jsVarSetXa (and updated by fastUpdate)
    // if allocated it has the same dimension as lastSentArrayDimension
    struct jsVarIndexSet    modifiedIndexes;

    // this is a special variable which must be updated whenever this variable is updated
    struct jsVar        *watchVariable;

    char                *compressedCodeLenAndJsname;        // an optimization for faster sending
    int                 compressedCodeLenAndJsnameLength;   // an optimization for faster sending

    int                 *baioMagics;            // connections where the variable is updated
    int                 baioMagicsElemCount;
    int                 baioMagicsSize;

    // red-black tree stuff
    uint8_t             color;
    struct jsVar        *leftSubtree;
    struct jsVar        *rightSubtree;
    // next element in address hashed tree
    struct jsVar        *nextInAddressHash;
    struct jsVar        *previousInAddressHash;
    // temporary stuff
    struct jsVar        *nextInDeletionList;    // a list of vars set for deletion

    // a double linked list of vars noted for update on javascriopt side
    struct jsVar        *nextInUpdateList;          
    struct jsVar        *previousInUpdateList;
};


struct jsVarAddressTable {
    struct jsVar    **t;            // hashed table
    int             elems;          // number of elements in the table
    int             dim;            // dimension of t
};

// If you change the value of the following constant, you have to change it in javascript code of jsvar.c as well !!!!
#define JSVAR_DICTIONARY_ONE_BYTE_ENCODING_INDEX    30  /* must be smaller than 32 */
#define JSVAR_DICTIONARY_SIZE                       (JSVAR_DICTIONARY_ONE_BYTE_ENCODING_INDEX + (32-JSVAR_DICTIONARY_ONE_BYTE_ENCODING_INDEX)*96)

struct jsVarCompressionTrie {
    int                             ccode;
    struct jsVarCompressionTrie     *left;
    struct jsVarCompressionTrie     *right;
};

struct jsVarCompressionData {
    int                             dictionarySize;
    char                            **dictionary;
    int                             *dictionarylengths;
    struct jsVarCompressionTrie     *tree;
};

// If changing this, change also initialization of jsVarSpaceGlobal !!!
struct jsVarSpace {
    int         jsVarSpaceMagic;

    // redblack tree indexed by name
    struct jsVar *jsVars;

    struct jsVar jsVarsUpdatedListFirstElementSentinel;
    struct jsVar *jsVarsUpdatedList; //  = &jsVarsUpdatedListFirstElementSentinel;

};

struct jsVaraio {
    struct wsaio                    w;

    int                             inputVarSpaceMagic;

    // for single page server
    char                            *singlePageHeader;
    char                            *singlePageBody;
    // for file web server
    char                            *fileServerRootDir;

    // struct jsVarCompressionData  *compressionData;
    int                             (*filterFunction)(struct jsVaraio *jj, struct jsVar *v, int index);

    // TODO: Put there all important callbacks
    struct jsVarCallBackHook        callBackAcceptFilter;
    struct jsVarCallBackHook        callBackOnTcpIpAccept;
    struct jsVarCallBackHook        callBackOnAccept;
    struct jsVarCallBackHook        callBackOnWwwGetRequest;
    struct jsVarCallBackHook        callBackOnWwwPostRequest;
    struct jsVarCallBackHook        callBackOnWebsocketAccept;
    struct jsVarCallBackHook        callBackOnDelete;

    // This is a connection 'local' variable holding whether the variable name was sent
    // The variable name is sent only with the first actual update. It is an optimization, when all updates are filtered
    // the name is not sent at all.
    int                             variableNameWrittenFlag;

    // here is the end of the structure
    void                            *userData[JSVAR_ZERO_SIZED_ARRAY_SIZE];
};

/////////////////////////////////////////////////////////////////////////////////////////////////////

extern struct jsVarSpace *jsVarSpaceGlobal;
extern int jsVarDebugLevel;

char *jsVarGetEnvPtr(char *env, char *key) ;
char *jsVarGetEnv(char *env, char *key, char *dst, int dstlen) ;
struct jsVarDynamicString *jsVarGetEnvDstr(char *env, char *key) ;
char *jsVarGetEnv_st(char *env, char *key) ;
int jsVarGetEnvInt(char *env, char *key, int defaultValue) ;
double jsVarGetEnvDouble(char *env, char *key, double defaultValue) ;

#define JSVARS_DECLARE_TYPED_FUNCTIONS(tcode, funtid)                   \
    struct jsVar *jsVarCreateNewSynchronized##funtid(struct jsVarSpace *jss, char *javaScriptName) ; \
    struct jsVar *jsVarCreateNewSynchronized##funtid##Input(struct jsVarSpace *jss, char *javaScriptName, void (*callBackFunction)(struct jsVar *v, int index, struct jsVaraio *jj)) ;\
    struct jsVar *jsVarSynchronize##funtid(struct jsVarSpace *jss, char *javaScriptName, JSVAR_TCODE_TYPE(tcode) *v) ; \
    struct jsVar *jsVarSynchronize##funtid##Input(struct jsVarSpace *jss, char *javaScriptName, JSVAR_TCODE_TYPE(tcode) *v, void (*callBackFunction)(struct jsVar *v, int index, struct jsVaraio *jj)) ; \
    struct jsVar *jsVarCreateNewSynchronized##funtid##Array(struct jsVarSpace *jss, char *javaScriptName, int dimension) ; \
    struct jsVar *jsVarCreateNewSynchronized##funtid##ArrayInput(struct jsVarSpace *jss, char *javaScriptName, int dimension, void (*callBackFunction)(struct jsVar *v, int index, struct jsVaraio *jj)) ;  \
    struct jsVar *jsVarSynchronize##funtid##Array(struct jsVarSpace *jss, char *javaScriptName, JSVAR_TCODE_TYPE(tcode) *v, int arrayElementSize, int dimension) ; \
    struct jsVar *jsVarSynchronize##funtid##ArrayInput(struct jsVarSpace *jss, char *javaScriptName, JSVAR_TCODE_TYPE(tcode) *v, int arrayElementSize, int dimension, void (*callBackFunction)(struct jsVar *v, int index, struct jsVaraio *jj)) ; \
    struct jsVar *jsVarCreateNewSynchronized##funtid##Vector(struct jsVarSpace *jss, char *javaScriptName) ; \
    struct jsVar *jsVarSynchronize##funtid##Vector(struct jsVarSpace *jss, char *javaScriptName, void *v, int arrayElementSize, int vectorElementOffset, int *dimension) ; \
    struct jsVar *jsVarSynchronize##funtid##List(struct jsVarSpace *jss, char *javaScriptName, void *v, JSVAR_TCODE_TYPE(tcode) *(*valueFromElemFunction)(void *), void *(*nextElementFunction)(void *)) ;  \
    void jsVarNoteUp##funtid(struct jsVar *v) ;                         \
    void jsVarNoteUp##funtid##Array(struct jsVar *v, int index) ;           \
    void jsVarNoteUp##funtid##Vector(struct jsVar *v, int index) ;      \
    void jsVarSet##funtid(struct jsVar *v, JSVAR_TCODE_TYPE(tcode) val) ; \
    void jsVarSet##funtid##Array(struct jsVar *v, int index, JSVAR_TCODE_TYPE(tcode) val) ; \
    void jsVarSet##funtid##Vector(struct jsVar *v, int index, JSVAR_TCODE_TYPE(tcode) val) ; \
    JSVAR_TCODE_TYPE(tcode) jsVarGet##funtid(struct jsVar *v) ;         \
    JSVAR_TCODE_TYPE(tcode) jsVarGet##funtid##Array(struct jsVar *v, int index) ; \
    JSVAR_TCODE_TYPE(tcode) jsVarGet##funtid##Vector(struct jsVar *v, int index) ; \
    int jsVarSet##funtid##Addr(JSVAR_TCODE_TYPE(tcode) *a, JSVAR_TCODE_TYPE(tcode) val) ; \
    int jsVarUpdate##funtid##Addr(JSVAR_TCODE_TYPE(tcode) *a) ;

JSVARS_DECLARE_TYPED_FUNCTIONS(i,Int);
JSVARS_DECLARE_TYPED_FUNCTIONS(w,Short);
JSVARS_DECLARE_TYPED_FUNCTIONS(b,Char);
JSVARS_DECLARE_TYPED_FUNCTIONS(l,Long);
JSVARS_DECLARE_TYPED_FUNCTIONS(ll,Llong);
JSVARS_DECLARE_TYPED_FUNCTIONS(f,Float);
JSVARS_DECLARE_TYPED_FUNCTIONS(d,Double);
JSVARS_DECLARE_TYPED_FUNCTIONS(s,String);
JSVARS_DECLARE_TYPED_FUNCTIONS(ws,Wstring);
JSVARS_DECLARE_TYPED_FUNCTIONS(ui,Uint);
JSVARS_DECLARE_TYPED_FUNCTIONS(uw,Ushort);
JSVARS_DECLARE_TYPED_FUNCTIONS(ub,Uchar);
JSVARS_DECLARE_TYPED_FUNCTIONS(ul,Ulong);
JSVARS_DECLARE_TYPED_FUNCTIONS(ull,Ullong);
void jsVarCallBackClearHook(struct jsVarCallBackHook *h) ;
int jsVarCallBackAddToHook(struct jsVarCallBackHook *h, void *ptr) ;
void jsVarCallBackRemoveFromHook(struct jsVarCallBackHook *h, void *ptr) ;
void jsVarNoteUpdate(struct jsVar *v) ;
void jsVarNoteUpdateArray(struct jsVar *v, int index) ;
void jsVarNoteUpdateVector(struct jsVar *v, int index) ;
void jsVarResizeVector(struct jsVar *vv, int newDimension) ;
int jsVarGetDimension(struct jsVar *vv) ;
void jsVarDumpVariables(char *msg, struct jsVarSpace *jss) ;
int jsVarBindVariableToConnection(struct jsVar *v, struct jsVaraio *jj) ;
struct jsVar *jsVarGetVariable(struct jsVarSpace *jss, char *javaScriptNameFmt, ...) ;
void jsVarBindMatchingVariables(struct jsVaraio *jj, struct jsVarSpace *jss, char *regexpfmt, ...) ;
void jsVarBindPrefixVariables(struct jsVaraio *jj, struct jsVarSpace *jss, char *prefixfmt, ...) ;
void jsVarUnbindMatchingVariables(struct jsVaraio *jj, struct jsVarSpace *jss, char *regexpfmt, ...) ;
void jsVarUnbindPrefixVariables(struct jsVarSpace *jss, struct jsVaraio *jj, char *prefixfmt, ...) ;
void jsVarDeleteMatchingVariables(struct jsVarSpace *jss, char *regexpfmt, ...) ;
void jsVarDeletePrefixVariables(struct jsVarSpace *jss, char *regexpfmt, ...);
struct jsVar *jsVarBindAddressToConnection(struct jsVaraio *jj, int varType, void *addr) ;
void jsVarDeleteVariable(struct jsVar *v) ;
void jsVarSendVariableUpdate(struct jsVar *v, int baioMagic, int uniqBindingFlag) ;
void jsVarSendVariableElementUpdate(struct jsVar *v, int index, int baioMagic, int firstBindingFlag) ;
struct jsVarSpace *jsVarSpaceCreate() ;
void jsVarSpaceDelete(struct jsVarSpace *jss) ;
struct jsVarSpace *jsVarSpaceFromMagic(int magic) ;
void jsVarRefreshNotes(struct jsVarSpace *jss) ;
void jsVarRefreshPrefix(struct jsVarSpace *jss, char *jsNamePrefix) ;
void jsVarRefreshRegexp(struct jsVarSpace *jss, char *regexp) ;
void jsVarRefreshAll(struct jsVarSpace *jss) ;
int jsVarPoll2(int timeOutUsec, int (*addUserFds)(int maxfd, fd_set *r, fd_set *w, fd_set *e), void (*processUserFds)(int maxfd, fd_set *r, fd_set *w, fd_set *e)) ;
int jsVarPoll(int timeOutUsec) ;
int jsVarSendVEval(struct jsVaraio *jj, char *fmt, va_list arg_ptr) ;
int jsVarSendEval(struct jsVaraio *jj, char *fmt, ...) ;
void jsVarServerSetFilter(struct jsVaraio *jj, int (*filterFunction)(struct jsVaraio *jj, struct jsVar *v, int index)) ;
struct jsVaraio *jsVarNewServer(int port, enum baioSslFlags sslFlag, int additionalSpaceToAllocate) ;
struct jsVaraio *jsVarNewFileServer(int port, enum baioSslFlags sslFlag, int additionalSpaceToAllocate, char *rootDirectory) ;
struct jsVaraio *jsVarNewSinglePageServer(int port, enum baioSslFlags sslFlag, int additionalSpaceToAllocate, char *header, char *body) ;
void jsVarInit() ;



#endif
