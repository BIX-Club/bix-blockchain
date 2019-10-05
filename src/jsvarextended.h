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

#ifndef __JSVAR_EXTENDED__H
#define __JSVAR_EXTENDED__H


#include "jsvar.h"

#define BAIO_ADDITIONAL_SIZE(x) (sizeof(x) - sizeof(struct baio))

#include <stdarg.h>

// common
char *sprintfIpAddress_st(long unsigned ip) ;
int intDigitToHexChar(int x) ;
int hexDigitCharToInt(int hx) ;
void jsVarCallBackClearHook(struct jsVarCallBackHook *h) ;
void jsVarCallBackFreeHook(struct jsVarCallBackHook *h) ;
int jsVarCallBackAddToHook(struct jsVarCallBackHook *h, void *ptr) ;
void jsVarCallBackRemoveFromHook(struct jsVarCallBackHook *h, void *ptr) ;
void jsVarCallBackCloneHook(struct jsVarCallBackHook *dst, struct jsVarCallBackHook *src) ;

struct jsVarDynamicString *jsVarDstrCreate() ;
struct jsVarDynamicString *jsVarDstrCreatePureMalloc() ;
void jsVarDstrFree(struct jsVarDynamicString **p) ;
char *jsVarDstrGetStringAndReinit(struct jsVarDynamicString *s) ;
void jsVarDstrExpandToSize(struct jsVarDynamicString *s, int size) ;
struct jsVarDynamicString *jsVarDstrTruncateToSize(struct jsVarDynamicString *s, int size) ;
struct jsVarDynamicString *jsVarDstrTruncate(struct jsVarDynamicString *s) ;
int jsVarDstrAddCharacter(struct jsVarDynamicString *s, int c) ;
int jsVarDstrDeleteLastChar(struct jsVarDynamicString *s) ;
int jsVarDstrAppendData(struct jsVarDynamicString *s, char *appendix, int len) ;
int jsVarDstrAppend(struct jsVarDynamicString *s, char *appendix) ;
int jsVarDstrAppendVPrintf(struct jsVarDynamicString *s, char *fmt, va_list arg_ptr) ;
int jsVarDstrAppendPrintf(struct jsVarDynamicString *s, char *fmt, ...) ;
int jsVarDstrAppendEscapedStringUsableInJavascriptEval(struct jsVarDynamicString *ss, char *s, int slen) ;
int jsVarDstrAppendBase64EncodedData(struct jsVarDynamicString *ss, char *s, int slen) ;
int jsVarDstrAppendBase64DecodedData(struct jsVarDynamicString *ss, char *s, int slen) ;
void jsVarDstrClear(struct jsVarDynamicString *s) ;
int jsVarDstrReplace(struct jsVarDynamicString *s, char *str, char *byStr, int allOccurencesFlag) ;
int jsVarDstrAppendFile(struct jsVarDynamicString *res, FILE *ff) ;
struct jsVarDynamicString *jsVarDstrCreateByVPrintf(char *fmt, va_list arg_ptr) ;
struct jsVarDynamicString *jsVarDstrCreateByPrintf(char *fmt, ...) ;
struct jsVarDynamicString *jsVarDstrCreateFromCharPtr(char *s, int slen) ;
struct jsVarDynamicString *jsVarDstrCreateCopy(struct jsVarDynamicString *ss) ;
struct jsVarDynamicString *jsVarDstrCreateByVFileLoad(int useCppFlag, char *fileNameFmt, va_list arg_ptr) ;
struct jsVarDynamicString *jsVarDstrCreateByFileLoad(int useCppFlag, char *fileNameFmt, ...) ;

int base64_encode(char *data, int input_length, char *encoded_data, int output_length) ;
int base64_decode(char *data, int input_length, char *decoded_data, int output_length) ;

// baio.c
extern struct baio *baioTab[BAIO_MAX_CONNECTIONS];
extern int 		baioTabMax;
struct baio *baioFromMagic(int baioMagic) ;
struct baio *baioNewBasic(int baioType, int ioDirections, int additionalSpaceToAllocate) ;
struct baio *baioNewFile(char *path, int ioDirection, int additionalSpaceToAllocate) ;
struct baio *baioNewPseudoFile(char *string, int stringLength, int additionalSpaceToAllocate) ;
struct baio *baioNewPipedFile(char *path, int ioDirection, int additionalSpaceToAllocate) ;
struct baio *baioNewSocketFile(char *path, int ioDirection, int additionalSpaceToAllocate) ;
struct baio *baioNewPipedCommand(char *command, int ioDirection, int additionalSpaceToAllocate) ;
struct baio *baioNewTcpipClient(char *hostName, int port, enum baioSslFlags sslFlag, int additionalSpaceToAllocate) ;
struct baio *baioNewTcpipServer(int port, enum baioSslFlags sslFlag, int additionalSpaceToAllocate) ;
int baioGetSockAddr(char *hostName, int port, struct sockaddr *out_saddr, int *out_saddrlen) ;
int baioReadBufferResize(struct baioReadBuffer *b, int minSize, int maxSize) ;
int baioClose(struct baio *bb) ;
int baioCloseMagic(int baioMagic) ;
int baioCloseMagicOnError(int baioMagic) ;
int baioAttachFd(struct baio *bb, int fd) ;
int baioDettachFd(struct baio *bb, int fd) ;
int baioPossibleSpaceForWrite(struct baio *b) ;
int baioWriteBufferUsedSpace(struct baio *b) ;
int baioWriteToBuffer(struct baio *bb, char *s, int len) ;
int baioMsgReserveSpace(struct baio *bb, int len) ;
int baioWriteMsg(struct baio *bb, char *s, int len) ;
int baioVprintfToBuffer(struct baio *bb, char *fmt, va_list arg_ptr) ;
int baioPrintfToBuffer(struct baio *bb, char *fmt, ...) ;
int baioVprintfMsg(struct baio *bb, char *fmt, va_list arg_ptr) ;
int baioPrintfMsg(struct baio *bb, char *fmt, ...) ;
void baioLastMsgDump(struct baio *bb) ;
char *baioStaticRingGetTemporaryStringPtr() ;
void baioWriteBufferDump(struct baioWriteBuffer *b) ;

void baioMsgsResizeToHoldSize(struct baio *bb, int newSize) ;
void baioMsgStartNewMessage(struct baio *bb);
struct baioMsg *baioMsgPut(struct baio *bb, int startIndex, int endIndex) ;
int baioMsgLastStartIndex(struct baio *bb) ;
int baioMsgInProgress(struct baio *bb) ;
void baioMsgRemoveLastMsg(struct baio *bb) ;
int baioMsgResetStartIndexForNewMsgSize(struct baio *bb, int newSize) ;
void baioMsgSend(struct baio *bb) ;
void baioMsgLastSetSize(struct baio *bb, int newSize) ;

int baioSetSelectParams(int maxfd, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval	*t) ;
int baioOnSelectEvent(int maxfd, fd_set *readfds, fd_set *writefds, fd_set *exceptfds) ;
int baioCloseOnError(struct baio *bb);
int baioSelect(int maxfd, fd_set *r, fd_set *w, fd_set *e, struct timeval *t) ;
int baioPoll2(int timeOutUsec, int (*addUserFds)(int maxfd, fd_set *r, fd_set *w, fd_set *e), void (*processUserFds)(int maxfd, fd_set *r, fd_set *w, fd_set *e)) ;
int baioPoll(int timeOutUsec) ;

// wsaio.c
struct wsaio *wsaioNewServer(int port, enum baioSslFlags sslFlag, int additionalSpaceToAllocate) ;
char *wsaioGetFileMimeType(char *fname) ;
void wsaioHttpStartNewAnswer(struct wsaio *ww) ;
void wsaioHttpFinishAnswer(struct wsaio *ww, char *statusCodeAndDescription, char *contentType, char *additionalHeaders) ;
void wsaioHttpFinishAnswerHeaderAndStartChunkedAnswer(struct wsaio *ww, char *statusCodeAndDescription, char *contentType, char *additionalHeaders) ;
void wsaioHttpStartChunkInChunkedAnswer(struct wsaio *ww) ;
void wsaioHttpFinalizeAndSendCurrentChunk(struct wsaio *ww, int finalChunkFlag) ;
int wsaioHttpSendFile(struct wsaio *ww, char *fname) ;
int wsaioHttpSendFileAsync(struct wsaio *ww, char *fname, char *additionalHeaders) ;
int wsaioHttpForwardFd(struct wsaio *ww, int fd, char *mimeType, char *additionalHeaders) ;
int wsaioHttpForwardString(struct wsaio *ww, char *str, int strlen, char *mimeType) ;
void wsaioWebsocketStartNewMessage(struct wsaio *ww) ;
void wsaioWebsocketCompleteMessage(struct wsaio *ww) ;
char *jsVarGetEnv_st(char *env, char *key) ;
int jsVarGetEnvInt(char *env, char *key, int defaultValue) ;
double jsVarGetEnvDouble(char *env, char *key, double defaultValue) ;

// jsvar.c
struct jsVarCompressionData *jsVarCompressionDataCreate(char **dictionary, int dictionarySize) ;
void jsVarCompressionDataFree(struct jsVarCompressionData *dd) ;
void jsVarCompressionDataDump(struct jsVarCompressionData *dd) ;
void jsVarCompressionDataResetGlobal(struct jsVarCompressionData *dd) ;
void jsVarCompressionSetGlobalDictionary(char *dictionary[JSVAR_DICTIONARY_SIZE]);
// void jsVarCompressMystr(struct jsVarDynamicString *mm, struct jsVarCompressionData *dd) ;

#endif
