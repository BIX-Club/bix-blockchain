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

#include "common.h"
#include "bixtags.h"

#define BIX_HEADER   					"hdr=bix00;len="
#define BIX_FOOTER   					"end=yes;"
#define BIX_LEN_DIGITS					8

#define BIX_BLOCK_DATA_HEADER_LENGTH 	12

// TODO: Remove this by putting it to paramaters
static struct bixParsingContext bixParsingContentData = {{NULL,}, {0,}, {0, }, -1};
static struct bixParsingContext *bixc = &bixParsingContentData;

// no matter the value, we are testing pointer to strict equality
char bixBinaryDataPtr[] = "%binary-data%";
char bixForwardDataPtr[] = "%forward-data%";
char bixSecondaryToCoreForwardDataPtr[] = "%secondary2core-forward-data%";

// static char *bixc->tag[BIX_TAG_MAX];
// static int bixc->tagLen[BIX_TAG_MAX] = {0, };
// static int bixc->usedTags[BIX_TAG_MAX];
// static int bixc->usedTagsi = -1;


void bixClose(struct bcaio *cc) {
	// HMM. We shall ensure some kind of reconnection
	if (debuglevel > 0) PRINTF("Closing connection %d->%d.\n", cc->fromNode, cc->toNode);
	baioClose(&cc->b);
}

int bixSendVMsgToConnection(struct bcaio *cc, char *dstpath, char *fmt, va_list arg_ptr) {
	int				n, n0, k, slen, fwdFlag, s2cFwdFlag, binFlag, sig0, secondaryBaioMagic;
	char			*lend, *signedpart, *s;
	char			*signature;

	binFlag = fwdFlag = s2cFwdFlag = 0;
	if (fmt == bixBinaryDataPtr) binFlag = 1;
	else if (fmt == bixForwardDataPtr) fwdFlag = 1;
	else if (fmt == bixSecondaryToCoreForwardDataPtr) s2cFwdFlag = 1;
	baioMsgStartNewMessage(&cc->b);
	n = 0;
	k = baioPrintfToBuffer(&cc->b, "%s", BIX_HEADER);
	if (k <= 0) goto fail;
	n += k;
	n0 = n;
	k = baioMsgReserveSpace(&cc->b, BIX_LEN_DIGITS+1);
	if (k <= 0) goto fail;
	n += k;
	k = baioPrintfToBuffer(&cc->b, "dst=%s;", dstpath);
	if (k <= 0) goto fail;
	n += k;
	sig0 = n;
	if (s2cFwdFlag) {
	}
	if (fwdFlag == 0) {
		k = baioPrintfToBuffer(&cc->b, "ssn=%"PRId64";", node->currentBixSequenceNumber);
		if (k <= 0) goto fail;
		n += k;
		node->currentBixSequenceNumber ++;
	}
	if (binFlag || fwdFlag || s2cFwdFlag) {
		// built in binary data or msg forward data 
		// in such case the argument after fmt is data and the next is the length of data
		s = va_arg(arg_ptr, char *);
		slen = va_arg(arg_ptr, int);
		k = baioWriteToBuffer(&cc->b, s, slen);
		if (s2cFwdFlag && k > 0) {
			n += k;
			secondaryBaioMagic = va_arg(arg_ptr, int);
			k = baioPrintfToBuffer(&cc->b, "ssf=%d;src=%d;", secondaryBaioMagic, coreServers[myNode].index);
		}
	} else {
		k = baioVprintfToBuffer(&cc->b, fmt, arg_ptr);
	}
	if (k <= 0) goto fail;
	n += k;
	// For now, I am checking myNodePrivateKey, it can be NULL for miner and wallet, etc.
	if (! fwdFlag && myNodePrivateKey != NULL) {
		// append my checksum signature
		signedpart = cc->b.writeBuffer.b+cc->b.writeBuffer.j-n+sig0;
		signature = signatureCreateBase64(signedpart, n-sig0, myNodePrivateKey);
		if (signature == NULL) goto fail;
		// printf("Calculating signature for %.*s --> %s\n", n-sig0, signedpart, signature);
		k = baioPrintfToBuffer(&cc->b, "crs=%s;", signature);
		if (k <= 0) goto fail;
		n += k;
		FREE(signature);
	}
	k = baioPrintfToBuffer(&cc->b, "%s", BIX_FOOTER);
	if (k <= 0) goto fail;
	n += k;

	// n holds the whole message length
	lend = cc->b.writeBuffer.b+cc->b.writeBuffer.j-n+n0;
	k = sprintf(lend, "%0*d", BIX_LEN_DIGITS, n);
	lend[k] = ';' ;

	if (debuglevel > 30) {
		if (n > 100 && debuglevel <= 70) {
			PRINTF("SENDING %d bytes to %d: '%.*s'...\n", n, cc->remoteNode, (n>100?100:n), cc->b.writeBuffer.b+cc->b.writeBuffer.j-n);
		} else {
			PRINTF("SENDING %d bytes to %d: '%.*s'\n", n, cc->remoteNode, n, cc->b.writeBuffer.b+cc->b.writeBuffer.j-n);
		}
	}

	baioMsgResetStartIndexForNewMsgSize(&cc->b, n);
	baioMsgSend(&cc->b);

	// baioMsgPut(&cc->b, cc->b.writeBuffer.j-n, cc->b.writeBuffer.j);

	return(n);

fail:
	PRINTF0("Error: Unable to send message \n");
#if EDEBUG
	assert(0);
#endif
	bixClose(cc);
	return(0);
}


int bixSendMsgToConnection(struct bcaio *cc, char *dstpath, char *fmt, ...) {
	int				r;
	va_list 		arg_ptr;

	va_start(arg_ptr, fmt);
	r = bixSendVMsgToConnection(cc, dstpath, fmt, arg_ptr);
	va_end(arg_ptr);
	return(r);
}


void bixSendLoginToConnection(struct bcaio *cc) {
	bixSendMsgToConnection(cc, 
						   "",
						   "msg=lgn;src=%d;pwd=%s;",
						   coreServers[myNode].index,
						   coreServersHash
						   
		);
}

void bixSendSecondaryServerLoginToConnection(struct bcaio *cc) {
	bixSendMsgToConnection(cc, 
						   "",
						   "msg=lgs;pwd=%s;",
						   coreServers[cc->toNode].loginPasswordKey
		);
}

void bixSendPingToConnection(struct bcaio *cc) {
	int	r;
	r = bixSendMsgToConnection(cc, 
							   "",
							   "msg=pin;src=%d;",
							   coreServers[myNode].index
		);
	if (r >= 0) {
		cc->lastPingRequestUtime = currentTime.usec;
	}
}

void bixSendPongToConnection(struct bcaio *cc) {
	int	r;
	r = bixSendMsgToConnection(cc, 
							   "",
							   "msg=pon;src=%d;",
							   coreServers[myNode].index
		);
}

static void bixSendPingToBaioMagic(void *d) {
	int				baioMagic;
	struct bcaio	*cc;
	baioMagic = (int) d;
	cc = (struct bcaio *) baioFromMagic(baioMagic);
	if (cc == NULL) return;

	// only send if not waiting for answer (after 5 seconds the previous ping is considered as lost)
	if (cc->lastPingRequestUtime == 0 || currentTime.usec - cc->lastPingRequestUtime > 5000000LL) {
		bixSendPingToConnection(cc);
	}
}

static void bixSendRegularPingRequestToBaioMagic(void *mmm) {
	bixSendPingToBaioMagic(mmm);
	timeLineRescheduleUniqueEvent(UTIME_AFTER_SECONDS(5*60+(rand() % (1*60))), bixSendRegularPingRequestToBaioMagic, mmm);
}

void bixSendImmediatePingAndScheduleNext(struct bcaio *cc) {
	// after connection send ping to meassure latency and update routing tables
	// theoretically there shall be as much of pings as is the longest path between servers to get all paths
	// correctly initialized at the system start
	// do a few fast initial pings to speed things up
	bixSendPingToConnection(cc);
	timeLineInsertEvent(UTIME_AFTER_MSEC(1000+(rand()%1000)), bixSendPingToBaioMagic, (void*)cc->b.baioMagic);
	timeLineInsertEvent(UTIME_AFTER_MSEC(2000+(rand()%1000)), bixSendPingToBaioMagic, (void*)cc->b.baioMagic);
	timeLineInsertEvent(UTIME_AFTER_MSEC(3000+(rand()%1000)), bixSendPingToBaioMagic, (void*)cc->b.baioMagic);
	timeLineInsertEvent(UTIME_AFTER_MSEC(4000+(rand()%1000)), bixSendPingToBaioMagic, (void*)cc->b.baioMagic);
	timeLineRescheduleUniqueEvent(UTIME_AFTER_MSEC(8000+(rand()%2000)), bixSendRegularPingRequestToBaioMagic, (void*)cc->b.baioMagic);
}


static struct bcaio *bixPathFirstConnection(char **_p) {
	struct serverTopology 	*ss;
	struct bcaio 			*cc;
	int 					node, i, ssfFlag;
	char					*path;
	char 					*p;

	p = path = *_p;
	if (path == NULL || *p == 0) return(NULL);

	ssfFlag = 0;
	if (*p == '-') {
		// this is forwarding to secondary server
		ssfFlag = 1;
		p ++;
	}
	node = parseInt(&p);
	if (*p != ':' && *p != 0) {
		PRINTF("Error in path %s\n", path);
#if EDEBUG
		assert(0);
#endif
		return(NULL);
	}
	if (*p == ':') p++;
	if (node < 0 || (ssfFlag == 0 && node >= coreServersMax)) {
		PRINTF("Error invalid forward node %d\n", node);
		return(NULL);
	}
	if (ssfFlag) {
		cc = (struct bcaio *) baioFromMagic(node);
	} else {
		cc = (struct bcaio *) baioFromMagic(coreServersConnections[node].activeConnectionBaioMagic);
		if (cc == NULL) {
			coreServersConnections[node].activeConnectionBaioMagic = 0;
			cc = (struct bcaio *) baioFromMagic(coreServersConnections[node].passiveConnectionBaioMagic);
			if (cc == NULL) coreServersConnections[node].passiveConnectionBaioMagic = 0;
		}
	}
	if (cc != NULL) *_p = p;

	return(cc);
}

static int bixForwardOrBroadcastBinMessageToPath(char *fmtPtr, char *path, char *msgBody, int msgBodyLen, int secondaryForwardBaioMagic) {
	struct bcaio 			*cc;
	int						r;

	if (debuglevel > 30) {
		PRINTF("%sING to path '%s' the msg: %.*s...\n", (fmtPtr==bixForwardDataPtr?"FORWARD":"BROADCAST"), path, (msgBodyLen>160?160:msgBodyLen), msgBody);
	} else if (debuglevel > 70) {
		PRINTF("%sING to path '%s' the msg: %.*s\n", (fmtPtr==bixForwardDataPtr?"FORWARD":"BROADCAST"), path, msgBodyLen, msgBody);
	}
	r = 0;
	cc = bixPathFirstConnection(&path);
	if (cc != NULL) {
		r = bixSendMsgToConnection(cc, path, fmtPtr, msgBody, msgBodyLen, secondaryForwardBaioMagic);
	}
	return(r);
}

static int bixSendMessageToPath(char *path, char *fmt, ...) {
	struct bcaio 			*cc;
	int						r;
	va_list 				arg_ptr;

	if (debuglevel > 30) {
		PRINTF("SENDING to path '%s' the msg: %s...\n", path, fmt);
	}
	r = 0;
	cc = bixPathFirstConnection(&path);
	if (cc != NULL) {
		va_start(arg_ptr, fmt);
		r = bixSendVMsgToConnection(cc, path, fmt, arg_ptr);
		va_end(arg_ptr);
	}
	return(r);
}

static int bixMaybeForwardOrBroadcastToMultiPath(char *fmtPtr, char *p, char *pathend, char *msgBody, int msgBodyLen, int secondaryForwardBaioMagic) {
	int 		res, depth, bc;
	char 		*pn;

	res = 0;
	if (*p == '{') {
		if (*(pathend-1) != '}') {
			PRINTF("Internal Error: wrongly formed destibation path %.*s", (int)(pathend-p), p);
			return(res);
		}
		p ++;
		pathend --;
	}
	if (p == pathend) res = 1;
	while (p<pathend+1) {
		pn = p;
		depth = 0;
		for(pn=p; pn<pathend && (*pn != '+' || depth != 0); pn++) {
			if (*pn == '{') depth ++;
			else if (*pn == '}') depth --;
		}
		if (pn == p) {
			res = 1;
		} else {
			bc = *pn; *pn = 0;
			bixForwardOrBroadcastBinMessageToPath(fmtPtr, p, msgBody, msgBodyLen, secondaryForwardBaioMagic);
			*pn = bc;
		}
		p = pn + 1;
	}
	return(res);
}


static void bixBroadcastDstr(struct jsVarDynamicString *bb) {
#if 0
	char *dpath;
	struct bcaio *cc;
	int i;
	for(i=0; i<coreServersMax; i++) {
		dpath = myNodeRoutingPaths[i].path;
		cc = bixPathFirstConnection(&dpath);
		if (cc != NULL) {
			bixSendMsgToConnection(cc, dpath, bixBinaryDataPtr, bb->s, bb->size);
		}
	}
#else
	if (myNodeBroadcastPath == NULL) return;
	bixMaybeForwardOrBroadcastToMultiPath(bixBinaryDataPtr, myNodeBroadcastPath+1, myNodeBroadcastPath+strlen(myNodeBroadcastPath), bb->s, bb->size, 0);
#endif
}

//////////


#define BIX_GET_BCAIO_FROM_MAGIC(cc, baioMagic) {						\
		cc = (struct bcaio *) baioFromMagic(baioMagic);					\
		if (cc == NULL) {												\
			PRINTF("Error: Can't send message to baioMagic %d. Not connected.\n", baioMagic); \
			return(-1);													\
		}																\
	}

int bixSendNewMessageInsertOrProcessRequest_s2c(char *dstpath, int insertProcessFlag, int baioMagic, int secondaryServerInfo, char *msg, int msgLen, char *messageDataType, char *ownerAccount, char *signature) {
	int 						i, n, r, res;
	struct jsVarDynamicString 	*bb;
	struct bcaio 				*cc;

	if (debuglevel > 10) {PRINTF("Sending message with size %d to baio %d\n", msgLen, baioMagic);fflush(stdout);}

	res = msgLen;

	BIX_GET_BCAIO_FROM_MAGIC(cc, baioMagic);
	bb = jsVarDstrCreate();
	if (insertProcessFlag == NMF_INSERT_MESSAGE) {
		n = jsVarDstrAppendPrintf(bb, "msg=nma;ssi=%d;acc=%s;sig=%s;mdt=%s;dat:%d=", secondaryServerInfo, ownerAccount, signature, messageDataType, msgLen);
	} else if (insertProcessFlag == NMF_PROCESS_MESSAGE) {
		n = jsVarDstrAppendPrintf(bb, "msg=nme;ssi=%d;acc=%s;sig=%s;mdt=%s;dat:%d=", secondaryServerInfo, ownerAccount, signature, messageDataType, msgLen);
	} else {
		PRINTF0("Internal error wrong insertProcessFlag.\n");
		res = -1;
		goto finito;
	}
	jsVarDstrAppendData(bb, msg, msgLen);
	jsVarDstrAppendPrintf(bb, ";");
	r = bixSendMsgToConnection(cc, dstpath, bixBinaryDataPtr, bb->s, bb->size);
	if (r < 0) res = -1;
finito:
	jsVarDstrFree(&bb);
	return(msgLen);
}

int bixSendNewMessageAccepted_c2s(char *returnPath, char *ownerAccount, char *notifyAccount, char *secServerInfo, char *msgId, int64_t timeStampUsec) {
	struct bcaio 	*cc;
	int 			r;
	if (debuglevel > 10) {PRINTF("Sending message %s accept to baio %s\n", msgId, returnPath);fflush(stdout);}

	r = bixSendMessageToPath(returnPath, "msg=msa;acc=%s;nac=%s;ssi=%s;mid=%s;tsu="PRId64";", ownerAccount, notifyAccount, secServerInfo, msgId, timeStampUsec);
	return(r);
}

int bixSendNewMessageRejected_c2s(char *returnPath, char *ownerAccount, char *notifyAccount, char *secServerInfo) {
	struct bcaio 	*cc;
	int 			r;
	if (debuglevel > 10) {PRINTF("Sending message reject to baio %s\n", returnPath);fflush(stdout);}

	r = bixSendMessageToPath(returnPath, "msg=msr;acc=%s;nac=%s;ssi=%s;", ownerAccount, notifyAccount, secServerInfo);
	return(r);
}

int bixSendNewMessageExecutedBin_c2s(char *returnPath, char *secServerInfo, char *account, char *notifyAccount, struct jsVarDynamicString *answer) {
	int 						r;
	struct jsVarDynamicString	*ss;

	if (debuglevel > 10) {PRINTF("Sending message executed (bin) to path %s\n", returnPath);fflush(stdout);}

	ss = jsVarDstrCreateByPrintf("src=%d;msg=mse;ssi=%s;acc=%s;nac=%s;", coreServers[myNode].index, secServerInfo, account, notifyAccount);
	if (answer != NULL) {
		jsVarDstrAppendPrintf(ss, "dat:%d=", answer->size);
		jsVarDstrAppendData(ss, answer->s, answer->size);
		jsVarDstrAppendPrintf(ss, ";");
	}
	// r = bixSendMsgToConnection(cc, "", bixBinaryDataPtr, ss->s, ss->size);
	r = bixForwardOrBroadcastBinMessageToPath(bixBinaryDataPtr, returnPath, ss->s, ss->size, 0);
	jsVarDstrFree(&ss);
	return(r);
}

// This is probably useless, notification shall be done through account based notifications
// Actually, this is still usefull for account creation before the account number is attached to the conenction
int bixSendMessageStatusUpdated_c2s(int baioMagic, char *secServerInfo, char *account, char *newstatus) {
	struct bcaio 				*cc;
	int 						r;

	if (debuglevel > 10) {PRINTF("Sending message status updated to baio %d\n", baioMagic);fflush(stdout);}

	BIX_GET_BCAIO_FROM_MAGIC(cc, baioMagic);
	r = bixSendMsgToConnection(cc, "", "msg=msu;ssi=%s;acc=%s;sts=%s;", secServerInfo, account, newstatus);
	return(r);
}

void bixSendResynchronizationRequestToConnection(struct bcaio *cc) {

	// PRINTF("Sending resynchronization request to %s\n", dpath);
	if (cc == NULL) {
		if (debuglevel > 5) PRINTF("Failed to send resynchronization request from %"PRId64" until %"PRId64".\n", node->currentBlockSequenceNumber, nodeResyncUntilSeqNum);
	} else {
		if (debuglevel > 15) PRINTF("Sending resynchronization request from %"PRId64" until %"PRId64" to %d\n", node->currentBlockSequenceNumber, nodeResyncUntilSeqNum, cc->remoteNode);
		bixSendMsgToConnection(cc, 
							   "",
							   "msg=rsr;src=%d;frm=%"PRId64";til=%"PRId64";",
							   coreServers[myNode].index,
							   node->currentBlockSequenceNumber,
							   nodeResyncUntilSeqNum
			);
	}
}

void bixSendBlockResynchronizationAnswerToConnection(struct bcaio *cc, struct blockContentDataList *block) {
	struct jsVarDynamicString 	*bb;

	if (cc != NULL) {
		bb = jsVarDstrCreate();
		jsVarDstrAppendPrintf(bb, 							   
							  "msg=rsa;src=%d;seq=%"PRId64";bch=%s;bsg:%d=%s;rsb:%d=",
							  coreServers[myNode].index,
							  block->blockSequenceNumber,
							  block->blockContentHash,
							  strlen(block->signatures), block->signatures,
							  block->blockContentLength
			);
		jsVarDstrAppendData(bb, block->blockContent, block->blockContentLength);
		jsVarDstrAppendPrintf(bb, ";");
		bixSendMsgToConnection(cc, "", bixBinaryDataPtr, bb->s, bb->size);
		if (debuglevel > 85) PRINTF("Sending resynchronization block answer %.*s to %d\n", bb->size, bb->s, cc->remoteNode);
		jsVarDstrFree(&bb);
	}	
}


///////////////////////////////////////////////////////////////////////////
// consensus

void bixBroadcastCurrentBlockEngagementInfo() {
	int 						i;
	struct bcaio 				*cc;
	char						*dpath;
	struct jsVarDynamicString	*ss;

	if (currentEngagement == NULL) return;

	if (debuglevel > 10) {PRINTF("BCAST: node %d round %d block %s\n", currentEngagement->engagedNode, currentEngagement->engagementRound, currentEngagement->blockId);fflush(stdout);}

	ss = jsVarDstrCreateByPrintf(
		"msg=ben;src=%d;seq=%"PRId64";bid=%s;ron=%d;bdl=%d;bct=%"PRId64";exp=%"PRId64";fir=%"PRId64";bsg=%s;",
		currentEngagement->engagedNode,
		currentEngagement->blockSequenceNumber,
		currentEngagement->blockId,
		currentEngagement->engagementRound,
		currentEngagement->msgIdsLength,
		currentEngagement->blockCreationTimeUsec,
		currentEngagement->expirationUsec,
		currentEngagement->firstAnnonceTimeUsec,
		(currentEngagement->blocksignature==NULL?"":currentEngagement->blocksignature)
		);
	bixBroadcastDstr(ss);
	jsVarDstrFree(&ss);
}

int bchainBlockContentAppendCurrentBlockHeader(struct jsVarDynamicString *bb) {
	int res;
	// block header consists of:
	// seq - block sequence number, 
	// pbh - previous block hash,
	// sth - hash of core servers topology (not used yet)
	res = jsVarDstrAppendPrintf(
		bb, 
		"hdr=block;seq=%"PRId64";pbh=%s;sth=%s;", 
		node->currentBlockSequenceNumber, 
		node->previousBlockHash,
		coreServersHash
		);
	return(res);
}

int bchainBlockContentAppendSingleMessage(struct jsVarDynamicString *bb, struct bchainMessage *msg) {
	int res;
	res = 0;
	res += jsVarDstrAppendPrintf(bb, "mid=%s;tsu=%"PRId64";acc=%s;nac=%s;nat=%s;sig=%s;mdt=%s;dat:%d=", msg->msgId, msg->timeStampUsec, msg->account, msg->notifyAccount, msg->notifyAccountToken, msg->signature, msg->dataType, msg->dataLength);
	res += jsVarDstrAppendData(bb, msg->data, msg->dataLength);
	res += jsVarDstrAppendPrintf(bb, ";");
	return(res);
}

void bixFillParsedBchainMessage(struct bixParsingContext *bix, struct bchainMessage *msg) {
	memset(msg, 0, sizeof(*msg));
	msg->msgId = bix->tag[BIX_TAG_mid];
	msg->account = bix->tag[BIX_TAG_acc];
	msg->notifyAccount = bix->tag[BIX_TAG_nac];
	msg->notifyAccountToken = bix->tag[BIX_TAG_nat];
	msg->dataType = bix->tag[BIX_TAG_mdt];
	msg->signature = bix->tag[BIX_TAG_sig];
	msg->signatureVerifiedFlag = 0;
	msg->timeStampUsec = atoll(bix->tag[BIX_TAG_tsu]);
	msg->data = bix->tag[BIX_TAG_dat];
	msg->dataLength = bix->tagLen[BIX_TAG_dat];
	setSignatureVerificationFlagInMessage(msg);
}

void bixBroadcastNewMessage(struct bchainMessageList *mm) {
	int 						i, n;
	struct jsVarDynamicString 	*ss;

	if (mm == NULL) return;

	if (debuglevel > 10) {PRINTF("BCAST: message with size %d\n", mm->msg.dataLength);fflush(stdout);}

	ss = jsVarDstrCreate();
	n = jsVarDstrAppendPrintf(ss, "msg=msg;src=%d;", myNode);
	bchainBlockContentAppendSingleMessage(ss, &mm->msg);
	bixBroadcastDstr(ss);
	jsVarDstrFree(&ss);
}

void bixBroadcastCurrentProposedBlockContent() {
	int							i, ii, n, k, contentLength;
	struct jsVarDynamicString	*ss;

	if (debuglevel > 10) {PRINTF("BROADCASTING BLOCK DATA for block with id %s == %s\n", currentEngagement->blockId, currentProposedBlock->blockId);}

	assert(currentEngagement->engagedNode == myNode);
	assert(currentEngagement->blockSequenceNumber == currentProposedBlock->blockSequenceNumber);
	assert(currentEngagement->blockCreationTimeUsec == currentProposedBlock->blockCreationTimeUsec);
	assert(strcmp(currentEngagement->blockId,currentProposedBlock->blockId) == 0);
	ss = jsVarDstrCreate();
	n = jsVarDstrAppendPrintf(ss,
							  "msg=blc;src=%d;seq=%"PRId64";bct=%"PRId64";bid=%s;bch=%s;blk:%d=",
							  currentEngagement->engagedNode,
							  currentProposedBlock->blockSequenceNumber,
							  currentProposedBlock->blockCreationTimeUsec,
							  currentProposedBlock->blockId,
							  currentProposedBlock->blockContentHash,
							  currentProposedBlock->msgIdsLength
		);
	jsVarDstrAppendData(ss, currentProposedBlock->msgIds, currentProposedBlock->msgIdsLength);
	jsVarDstrAppendPrintf(ss, ";");
	bixBroadcastDstr(ss);
	jsVarDstrFree(&ss);
}

void bixBroadcastNetworkConnectionStatusMessage(int fromNode, int toNode) {
	int 						i, n, stat;
	int64_t						latency;
	struct jsVarDynamicString 	*bb;

	InternalCheck(fromNode >= 0 && fromNode < coreServersMax && toNode >= 0 && toNode < coreServersMax);

	stat = networkConnections[fromNode][toNode].status;
	latency = networkConnections[fromNode][toNode].latency;

	if (debuglevel > 10) {PRINTF("BCAST: connection status %d -> %d : %d\n", fromNode, toNode, stat);fflush(stdout);}

	bb = jsVarDstrCreate();
	n = jsVarDstrAppendPrintf(bb, "msg=cns;src=%d;frn=%d;ton=%d;sts=%d;lat=%"PRId64";", myNode, fromNode, toNode, stat, latency);
	// TODO: Maybe I shall sign this messagess too?
	bixBroadcastDstr(bb);
	jsVarDstrFree(&bb);	
}

void bixBroadcastMyNodeStatusMessage() {
	int 						i, n, stat;
	int64_t						latency;
	struct jsVarDynamicString 	*bb;

	if (debuglevel > 10) {PRINTF("BCAST: node status %d\n", nodeState);fflush(stdout);}

	bb = jsVarDstrCreate();
	n = jsVarDstrAppendPrintf(bb, "msg=nst;src=%d;sts=%d;seq=%"PRId64";usp=%"PRId64";asp=%"PRId64";", 
							  myNode, nodeState, node->currentBlockSequenceNumber-1, 
							  node->currentUsedSpaceBytes, node->currentTotalSpaceAvailableBytes
		);
	// TODO: Maybe I shall sign this messagess too?
	bixBroadcastDstr(bb);
	jsVarDstrFree(&bb);	
}

///////////////////////////////////////////////////////////////////////////
// parsing

static char *bixField_st(char *s) {
	char 	*res;
	int		i;

	res = staticStringRingGetTemporaryStringPtr_st();
	for(i=0; i < TMP_STRING_SIZE-1 && s[i] != ';' && s[i] != 0; i++) {
		res[i] = s[i];
	}
	if (i < TMP_STRING_SIZE-1) {
		res[i] = 0;
	} else {
		sprintf(res+TMP_STRING_SIZE-4, "...");
	}
	return(res);
}

void bixDumpParsedMsg(char *msg, int len) {
	int		i, c;
	char	*s;
	for(i=0; i<len; i++) {
		c = msg[i];
		if (c == 0) c = ';';
		PRINTF_NOP("%c", c);
	}
}

static int bixOnError(struct bcaio *cc) {
	cc->errorCounter ++;
	if (cc->errorCounter >= 10) {
		bixClose(cc);
		return(-1);
	}
	return(0);
}

static void bixOnErrorFindAnotherMessageStartInBuffer(struct bcaio *cc) {
	int 					r;
	char					*s;
	struct baioReadBuffer	*b;

	b = &cc->b.readBuffer;
	PRINTF("Bix: Error in message %.20s\n", b->b + b->i);
	r = bixOnError(cc);
	if (r == 0) {
		s = strstr(b->b + b->i + 1, BIX_HEADER);
		if (s == NULL) {
			// skip the whole buffer
			b->i = b->j;
		} else {
			// there is another message in the buffer, skip to it
			b->i = s - b->b;
		}
	}
}

static int bixTagCmp(char *tag, char *val) {
	int 	tagn, r;
	if (strlen(tag) != 3) return(-1);
	tagn = BIX_TAG(tag[0], tag[1], tag[2]);
	if (tagn < 0 || tagn >= BIX_TAG_MAX) return(-1);
	r = strcmp(bixc->tag[tagn], val);
	return(r);
}

#define BIX_GET_SRC(res) {												\
		res = atoi(bixc->tag[BIX_TAG_src]);							\
		if (res < 0 || res >= coreServersMax) {								\
			PRINTF("Error wrong field src=%s: connection %d -> %d\n", bixc->tag[BIX_TAG_src], cc->fromNode, cc->toNode); \
			bixClose(cc);												\
			return;														\
		}																\
	}

static void bixOnLoginReceived(struct bcaio *cc) {
	char 			*unm;
	int 			src;
	struct bcaio 	*previouscc;

	// TODO: Maybe also check the timestamp and do not allow to login a server with too shifted time
	// Hmm. Even such a server can be usefull for forwarding messages from servers having right time.

	if (cc->fromNode != -1) {
		PRINTF("Error: probably double login on connection %d -> %d\n", cc->fromNode, cc->toNode);
		bixClose(cc);
		return;
	}

	BIX_GET_SRC(src);
	if (cc->b.remoteComputerIp != coreServers[src].ipaddress) {
		PRINTF0("Error: wrong IP address on login. ");
		PRINTF_NOP("Expected %s, ", sprintfIpAddress_st(coreServers[src].ipaddress));
		PRINTF_NOP("connected from %s.\n", sprintfIpAddress_st(cc->b.remoteComputerIp));
		bixClose(cc);
		return;
	}

	if (strcmp(bixc->tag[BIX_TAG_pwd], coreServersHash) != 0) {
		PRINTF("Error: an attempt to login from server %d with wrong server topology.\n", src);
		// PRINTF("Checking %s <-> %s\n", bixc->tag[BIX_TAG_pwd], coreServersHash);
		bixClose(cc);
		return;
	}

	previouscc = (struct bcaio *) baioFromMagic(coreServersConnections[src].passiveConnectionBaioMagic);
	if (previouscc != NULL) {
		PRINTF("Double connection %d -> %d, closing previous one\n", src, cc->toNode);
		bixClose(previouscc);
	}

	cc->fromNode = src;
	cc->remoteNode = src;
	coreServersConnections[cc->fromNode].passiveConnectionBaioMagic = cc->b.baioMagic;

	bixSendImmediatePingAndScheduleNext(cc);

	if (debuglevel > 0) PRINTF("Successfull login on connection %d -> %d\n", cc->fromNode, cc->toNode);
}

static void bixOnLoginReceivedFromSecondaryServer(struct bcaio *cc) {
	char 			*unm;
	int 			src;
	struct bcaio 	*previouscc;

	if (strcmp(bixc->tag[BIX_TAG_pwd], coreServers[myNode].loginPasswordKey) != 0) {
		PRINTF("wrong pwd on login from %s\n", sprintfIpAddress_st(cc->b.remoteComputerIp));
		bixClose(cc);
		return;
	}
	if (debuglevel > 0) PRINTF("Secondary server logged in from %s\n", sprintfIpAddress_st(cc->b.remoteComputerIp));
}

static void bixOnPingReceived(struct bcaio *cc) {
	bixSendPongToConnection(cc);
}

static void bixOnPongReceived(struct bcaio *cc) {
	int fromNode, toNode;
	fromNode = cc->fromNode;
	toNode = cc->toNode;
	if (fromNode >= 0 && fromNode < coreServersMax && toNode >= 0 && toNode < coreServersMax
		&& networkConnections[fromNode][toNode].status == NCST_OPEN 
		&& cc->lastPingRequestUtime > 0
		&& cc->lastPingRequestUtime < currentTime.usec
		) {
		networkConnections[fromNode][toNode].latency = currentTime.usec - cc->lastPingRequestUtime;
		cc->lastPingRequestUtime = 0;
		bixBroadcastNetworkConnectionStatusMessage(fromNode, toNode);
	}
}

// msg=msg;src=%d;mid=%s;tsu=%"PRId64";dat:%d="
static void bixOnNewMessageReceived(struct bcaio *cc) {
	struct bchainMessage		mmm;
	struct jsVarDynamicString 	*ss;
	int							r, srcnode;

	BIX_GET_SRC(srcnode);
	bixFillParsedBchainMessage(bixc, &mmm);
#if 0 && EDEBUG
	// this can happen, there are several messages which does not need to be signed like, NewAccountNumber, ...
	if (mmm.signatureVerifiedFlag == 0) PRINTF("Warning: default signature verification failed in msg %s: %.100s\n", mmm.msgId, mmm.data);
#endif
	bchainEnlistPendingMessage(&mmm);
}

static void bixOnNewMessageAddToBlockchainFromSecondaryServer(struct bcaio *cc, char *returnPath) {
	char						*data;
	int							datalen;
	char						*secServerInfo;
	char						*ownerAccount;
	char						*notifyAccount;
	char						*notifyAccountToken;
	char						*signature;
	char						*messageDataType;
	struct bchainMessageList	*mm;

	// For secondary server message, I generate my own id, and timestamp
	// maybe I shall do some more tests on the message, 
	data = bixc->tag[BIX_TAG_dat];
	datalen = bixc->tagLen[BIX_TAG_dat];
	secServerInfo = bixc->tag[BIX_TAG_ssi];
	ownerAccount = bixc->tag[BIX_TAG_acc];
	notifyAccount = bixc->tag[BIX_TAG_nac];
	notifyAccountToken = bixc->tag[BIX_TAG_nat];
	signature = bixc->tag[BIX_TAG_sig];
	messageDataType = bixc->tag[BIX_TAG_mdt];
	// PRINTF("Got new messsage %s\n", data);
	// anotificationAddOrRefresh(ownerAccount, cc->b.baioMagic, NCT_BIX);
	mm = bchainCreateNewMessage(data, datalen, messageDataType, ownerAccount, notifyAccountToken, notifyAccount, signature, secServerInfo);
	if (mm == NULL) {
		bixSendNewMessageRejected_c2s(returnPath, ownerAccount, notifyAccount, secServerInfo);
	} else {
		// Unfortunately we can not save this notification into history file, because there is only one core server receiving requests.
		// The request itself is not propagated through servers while we want that all servers shall have the same notification history.
		anotificationShowMsgNotification(&mm->msg, "%s: Info: Request \"%s\" has message id: %s", SPRINT_CURRENT_TIME(), bchainPrettyPrintMsgContent_st(&mm->msg), mm->msg.msgId);
		bixSendNewMessageAccepted_c2s(returnPath, ownerAccount, notifyAccount, secServerInfo, mm->msg.msgId, mm->msg.timeStampUsec);
	}
}

static void bixOnNewMessageExecuteFromSecondaryServer(char *returnPath) {
	struct bchainMessage		mmm;
	char						*secServerInfo;
	struct jsVarDynamicString	*answer;
	char						*messageDataType;
	char						ttt[TMP_STRING_SIZE];

	bixFillParsedBchainMessage(bixc, &mmm);
	secServerInfo = bixc->tag[BIX_TAG_ssi];
	messageDataType = bixc->tag[BIX_TAG_mdt];

	// PRINTF("Got new messsage for immediate processing type %s: data: %s\n", mmm.dataType, mmm.data); fflush(stdout);
	answer = bchainImmediatelyProcessSingleMessage_st(&mmm, messageDataType);
	bixSendNewMessageExecutedBin_c2s(returnPath, secServerInfo, mmm.account, mmm.notifyAccount, answer);
	jsVarDstrFree(&answer);
}

static void bixOnMessageAcceptedReceivedFromCoreServer(struct bcaio *cc) {
	// No action to take here. We get all info about accepted message through notification.
#if 0
	char	*account;
	char	*notifyAccount;
	char 	*msgId;

	msgId = bixc->tag[BIX_TAG_mid];
	account = bixc->tag[BIX_TAG_acc];
	notifyAccount = bixc->tag[BIX_TAG_nac];

	anotificationShowNotification(account, NCT_GUI, "Message/transaction id: %s\n", msgId);
	if (notifyAccount != NULL && notifyAccount[0] != 0 && strcmp(account, notifyAccount) != 0) {
		anotificationShowNotification(notifyAccount, NCT_GUI, "Message/transaction id: %s\n", msgId);
	}
#endif
}

static void bixOnMessageRejectedReceivedFromCoreServer(struct bcaio *cc) {
	char	*account;
	char	*notifyAccount;

	account = bixc->tag[BIX_TAG_acc];
	notifyAccount = bixc->tag[BIX_TAG_nac];

	anotificationShowNotification(account, NCT_GUI, "Message/transaction rejected.\n");
	if (notifyAccount != NULL && notifyAccount[0] != 0 && strcmp(account, notifyAccount) != 0) {
		anotificationShowNotification(notifyAccount, NCT_GUI, "Message/transaction rejected.\n");
	}
}

static void bixOnImmediateMessageExecutedFromCoreServer(struct bcaio *cc) {
	struct secondaryToCoreServerPendingRequestsStr 	*pp;
	char											*account;
	char											*notifyAccount;
	char 											*msgId;
	int												secondaryServerInfo;
	char 											*answerMsg;
	int												answerMsgLength;
	char 											*answerData;
	int												answerDataLength;
	int												reqid;
	char											errmsg[TMP_STRING_SIZE];

	secondaryServerInfo = atoi(bixc->tag[BIX_TAG_ssi]);
	account = bixc->tag[BIX_TAG_acc];
	notifyAccount = bixc->tag[BIX_TAG_nac];
	answerData = answerMsg = bixc->tag[BIX_TAG_dat];
	answerDataLength = answerMsgLength = bixc->tagLen[BIX_TAG_dat];

	if (secondaryServerInfo == SSI_NONE) {
		// this is a confirmation for request which does not not require any attention
		return;
	}

	pp = secondaryToCorePendingRequestFindAndUnlist(secondaryServerInfo);
	if (pp == NULL) {
		PRINTF("Internal error: received answer for unknow request type.\n", pp->requestType);
		return;
	}

	if (answerMsgLength < 0) {
		// TODO: send answerData directly to all cases and manage this (error) case in all cases separately
		snprintf(errmsg, sizeof(errmsg), "%s: Error: wrong message", SPRINT_CURRENT_TIME());
		answerMsg = errmsg;
		answerMsgLength = strlen(answerMsg);
	}

	switch (pp->requestType) {
	case STC_NEW_ACCOUNT_NUMBER:
		walletGotNewAccountNumber(pp, account, answerData, answerDataLength);
		break;
	case STC_NEW_ACCOUNT:
		walletNewAccountCreated(pp, account, answerMsg);
		break;
	case STC_LOGIN:
		walletShowAnswerInNotifications(pp, account, answerMsg, answerMsgLength);
		break;
	case STC_MORE_NOTIFICATIONS:
		walletShowMoreNotifications(pp, account, answerMsg, answerMsgLength);
		break;
	case STC_GET_BALANCE:
		walletGotBalance(pp, account, answerMsg);
		break;
	case STC_GET_ALL_BALANCES:
		walletGotAllBalances(pp, account, answerMsg, answerMsgLength);
		break;
	case STC_GET_MSGTYPES:
		ssGotSupportedMsgTypes(pp, account, answerMsg, answerMsgLength);
		break;
	case STC_PENDING_REQ_LIST:
	case STC_GET_ACCOUNTS:
		ssGotPendingReqList(pp, account, answerMsg, answerMsgLength);
		break;
	case STC_PENDING_REQ_EXPLORE:
	case STC_ACCOUNT_EXPLORE:
		ssGotPendingReqExplore(pp, account, answerMsg, answerMsgLength);
		break;
	case STC_PENDING_REQ_APPROVE:
	case STC_PENDING_REQ_DELETE:
		walletShowAnswerInMessages(pp, account, answerMsg, answerMsgLength);
		break;
	case STC_EXCHANGE_GET_ORDER_BOOK:
		walletGotOrdebook(pp, account, answerMsg, answerMsgLength);
		break;
	default:
		PRINTF("Internal error: received answer for unknow request type.\n", pp->requestType);
	}
	secondaryToCorePendingRequestFree(pp);
}

static void bixOnMessageNotificationFromCoreServer(struct bcaio *cc) {
	struct secondaryToCoreServerPendingRequestsStr 	*pp, *pp2, **ppa;
	char											*data;
	int												datalen;
	char											*account;
	char											*notifyAccount;
	int												r, secondaryServerInfo;

	data = bixc->tag[BIX_TAG_dat];
	datalen = bixc->tagLen[BIX_TAG_dat];
	account = bixc->tag[BIX_TAG_acc];
	notifyAccount = bixc->tag[BIX_TAG_nac];
	secondaryServerInfo = atoi(bixc->tag[BIX_TAG_ssi]);

	anotificationShowNotification(account, NCT_GUI, "%s", data);
	if (notifyAccount != NULL && notifyAccount[0] != 0 && strcmp(account, notifyAccount) != 0) {
		anotificationShowNotification(notifyAccount, NCT_GUI, "%s", data);
	}

	// maybe some automated process is waiting for this notification (like base account creation is waiting for new coin creation)
	ppa = secondaryToCorePendingRequestFind(secondaryServerInfo);
	r = 1;
	if (ppa != NULL) {
		pp = *ppa;
		if (pp != NULL) {
			switch (pp->requestType) {
			case STC_NEW_TOKEN:
				r = walletNewTokenCreationStateUpdated(pp, account, data, datalen);
				break;
			default:
				r = -1;
				PRINTF("Internal error: received answer for unknow request type.\n", pp->requestType);
			}
		}
	}
	if (r != 0) {
		pp2 = secondaryToCorePendingRequestFindAndUnlist(secondaryServerInfo);
		if (ppa != NULL) assert(pp2 == pp);
		secondaryToCorePendingRequestFree(pp2);
	}
}


// "msg=mis;src=%d;mid=%s"
static void bixOnMissingMessageInfoReceived(struct bcaio *cc) {
	char	*msgId, *data;
	int		datalen;
	int64_t	timeStampUsec;

	msgId = bixc->tag[BIX_TAG_mid];
	bchainNoteMissingMessage(msgId);
}


// ""msg=ben;src=%d;seq=%"PRId64";bid=%s;ron=%d;bdl=%d;bct=%"PRId64";exp=%"PRId64";fir=%"PRId64";",
void bixOnBlockEngagementInfoReceived(struct bcaio *cc) {
	struct consensusBlockEngagementInfo  	nn;
	struct jsVarDynamicString				*ss;
	int64_t									expiration;
	int										r;

	BIX_GET_SRC(nn.engagedNode);
	nn.blockSequenceNumber = atoll(bixc->tag[BIX_TAG_seq]);
	nn.serverMsgSequenceNumber = atoll(bixc->tag[BIX_TAG_ssn]);
	nn.blockId = bixc->tag[BIX_TAG_bid];
	nn.engagementRound = atoi(bixc->tag[BIX_TAG_ron]);
	nn.msgIdsLength = atoi(bixc->tag[BIX_TAG_bdl]);
	nn.blockCreationTimeUsec = atoll(bixc->tag[BIX_TAG_bct]);
	nn.expirationUsec = atoll(bixc->tag[BIX_TAG_exp]);
	nn.firstAnnonceTimeUsec = atoll(bixc->tag[BIX_TAG_fir]);
	nn.blocksignature = bixc->tag[BIX_TAG_bsg];

	consensusNewEngagementReceived(&nn);
}

// "msg=blc;src=%d;seq=%d;bid=%s;blk:",
void bixOnBlockContentReceived(struct bcaio *cc) {
	struct blockContentDataList		bc;
	struct jsVarDynamicString		*ss;
	int								r, engagedNode;

	BIX_GET_SRC(engagedNode);
	bc.blockSequenceNumber = atoll(bixc->tag[BIX_TAG_seq]);
	bc.serverMsgSequenceNumber = atoll(bixc->tag[BIX_TAG_ssn]);
	bc.blockCreationTimeUsec = atoll(bixc->tag[BIX_TAG_bct]);
	bc.blockId = bixc->tag[BIX_TAG_bid];
	bc.msgIdsLength = bixc->tagLen[BIX_TAG_blk];
	bc.msgIds = bixc->tag[BIX_TAG_blk];
	bc.blockContentHash = bixc->tag[BIX_TAG_bch];
	bc.blockContent = NULL;
	bc.blockContentLength = 0;
	bc.signatures = NULL;

	consensusBlockContentReceived(&bc);
}

// "msg=rsr;src=%d;frm=%"PRId64";til=%"PRId64";",
void bixOnBlockResynchronizationRequestReceived(struct bcaio *cc) {
	int 										requestingNode;
	int64_t										fromSeq, untilSeq;
	struct bchainResynchronizationRequestList	*rr;

	// don't use BIX_GET_SRC(requestingNode), because this function is used by explorer secondary server as well
	requestingNode = atoi(bixc->tag[BIX_TAG_src]);
	fromSeq = atoll(bixc->tag[BIX_TAG_frm]);
	untilSeq = atoll(bixc->tag[BIX_TAG_til]);

	if (debuglevel > 15) PRINTF("Got resynchronization request from %d seq %"PRId64" until %"PRId64"\n", requestingNode, fromSeq, untilSeq);

	if (fromSeq >= 0 && fromSeq <= node->currentBlockSequenceNumber) {
		for (rr=bchainResynchronizationRequest; rr!=NULL; rr=rr->next) {
			if (rr->reqBaioMagic == cc->b.baioMagic) break;
		}
		if (rr == NULL) {
			ALLOC(rr, struct bchainResynchronizationRequestList);
			rr->reqBaioMagic = cc->b.baioMagic;
			rr->next = bchainResynchronizationRequest;
			bchainResynchronizationRequest = rr;
		}
		rr->fromSeqn = fromSeq;
		rr->untilSeqn = untilSeq;
		rr->reqNode = requestingNode;
	}
}

static int blockContentVerifyPreviousBlockHash(struct blockContentDataList	*bc, char *previousBlockHash) {
	int							r, res;
	struct bixParsingContext	*bix;

	res = 0;
	bix = bixParsingContextGet();
	r = bixParseString(bix, bc->blockContent, bc->blockContentLength);
	if (r == 0) {
		// PRINTF("checking hashes \n%.*s\n%s\n", bix->tagLen[BIX_TAG_pbh], bix->tag[BIX_TAG_pbh], previousBlockHash);
		if (bix->tag[BIX_TAG_pbh] != NULL && strncmp(bix->tag[BIX_TAG_pbh], previousBlockHash, bix->tagLen[BIX_TAG_pbh]) == 0) {
			res = 1;
		}
	}
	bixParsingContextFree(&bix);
	return(res);
}

// Signatures avoid the possibility to corrupt whole blockchain by 
// multiple resynchronizations from a single hacked computer.
static int blockContentVerifySignatures(struct blockContentDataList	*bc) {
	int							i, r, src, res, sslen, count;
	char						*ss;
	struct bixParsingContext	*bix;
	int							checkedFlags[SERVERS_MAX];

	if (bc->signatures == NULL) return(0);

	// PRINTF("verify signatures of block %"PRId64": sigs: %s\n", bc->blockSequenceNumber, bc->signatures);

	res = 0;
	memset(checkedFlags, 0, sizeof(checkedFlags));
	bix = bixParsingContextGet();
	ss = bc->signatures;
	sslen = strlen(ss);

	BIX_PARSE_AND_MAP_STRING(ss, sslen, bix, 0, {
			if (strncmp(tagName, "sig", 3) == 0) {
				if (bix->tag[BIX_TAG_src] != NULL) {
					src = atoi(bix->tag[BIX_TAG_src]);
					if (src >= 0 && src < SERVERS_MAX) {
						// PRINTF("checking signature %d %s\n", src, tagValue);
						r = consensusBlockSignatureValidation(bc, src, tagValue);
						if (r == 0) checkedFlags[src] = 1;
					}
				}
			}
		}, 
		{
			PRINTF("Error: in bix message: %.*s\n", sslen, ss);
			res = -1;
		}
		);
	bixParsingContextFree(&bix);

	if (res < 0) return(0);
	count = 0;
	for(i=0; i<SERVERS_MAX; i++) if (checkedFlags[i]) count ++;
	// PRINTF("collected %d valid signatures\n", count);
	if (count <= coreServersMax / 2) return(0);
	return(1);
}

// "msg=rsa;src=%d;seq=%"PRId64";bch=%s;rsb:%d="
#define BIX_GET_RESYNCHRONIZATION_BLOCK(cc, bc, answeringNode) {	\
		BIX_GET_SRC(answeringNode);									\
		bc.blockSequenceNumber = atoll(bixc->tag[BIX_TAG_seq]);		\
		bc.blockContentHash = bixc->tag[BIX_TAG_bch];				\
		bc.blockContentLength = bixc->tagLen[BIX_TAG_rsb];			\
		bc.blockContent = bixc->tag[BIX_TAG_rsb];					\
		bc.signatures = bixc->tag[BIX_TAG_bsg];						\
		bc.blockId = "from resynchronization";						\
		bc.msgIds = "";												\
		bc.msgIdsLength = 0;										\
	}

void bixOnBlockResynchronizationAnswerReceived(struct bcaio *cc) {
	struct blockContentDataList		bc;
	int64_t							seq;
	int 							answeringNode;
	int								blockSignaturesVerified;
	int								previousBlockHashVerified;
	
	BIX_GET_RESYNCHRONIZATION_BLOCK(cc, bc, answeringNode);

	lastResynchronizationActionTime = currentTime.sec;

	if (bc.blockSequenceNumber == node->currentBlockSequenceNumber) {
		if (debuglevel > 35) PRINTF("Got resynchronization answer from %d: block %"PRId64"\n", answeringNode, bc.blockSequenceNumber);
		previousBlockHashVerified = blockContentVerifyPreviousBlockHash(&bc, node->previousBlockHash);
		blockSignaturesVerified = blockContentVerifySignatures(&bc);
		if (previousBlockHashVerified && blockSignaturesVerified) {
			bchainOnNewBlockAccepted(&bc);
			consensusOnBlockAccepted();
			if (bc.blockSequenceNumber == nodeResyncUntilSeqNum - 1) {
				PRINTF0("All requested resync messages received. Switching to normal mode.\n");
				changeNodeStatus(NODE_STATE_NORMAL);
			}
		} else {
			PRINTF("Warning: resynchronization block %"PRId64" from %d is wrong. %s%s\n", 
				   bc.blockSequenceNumber, answeringNode, 
				   (previousBlockHashVerified == 0 ? "Previous block hash mismatch! ":""),
				   (blockSignaturesVerified == 0 ? "Block signatures problem! ":"")
				);
		}
	} else {
		PRINTF("Warning: resynchronization block out of order: %"PRId64" expected: %"PRId64".\n", bc.blockSequenceNumber, node->currentBlockSequenceNumber);
	}
}

// This is the case when secondary server asked for resynchronization (for example the explorer)
// "msg=rsa;src=%d;seq=%"PRId64";bch=%s;rsb:%d="
void bixOnBlockResynchronizationAnswerReceivedFromCoreserverBySecondaryServer(struct bcaio *cc) {
	struct blockContentDataList		bc;
	int64_t							seq;
	int 							answeringNode;
	
	BIX_GET_RESYNCHRONIZATION_BLOCK(cc, bc, answeringNode);

	bchainWriteBlockToFile(&bc);
	if (bc.blockSequenceNumber == node->currentBlockSequenceNumber) {
		if (debuglevel > 5) PRINTF("Got answer from %d: block %"PRId64"\n", answeringNode, bc.blockSequenceNumber);
		node->currentBlockSequenceNumber = bc.blockSequenceNumber + 1;
	} else {
		PRINTF("Warning: block out of order: %"PRId64" expected: %"PRId64".\n", bc.blockSequenceNumber, node->currentBlockSequenceNumber);
	}
}

/////////////////////////////////////////

// "msg=cns;src=%d;frn=%d;ton=%d;sts=%d", myNode, fromNode, toNode);
static void bixOnNetworkConnectionStatusChanged(struct bcaio *cc) {
	int 	fromNode, toNode;
	int		status;
	int64_t	latency;

	fromNode = atoi(bixc->tag[BIX_TAG_frn]);
	toNode = atoi(bixc->tag[BIX_TAG_ton]);
	status = atoi(bixc->tag[BIX_TAG_sts]);
	latency = atoi(bixc->tag[BIX_TAG_lat]);

	if (fromNode >= 0 && fromNode < coreServersMax
		&& toNode >= 0 && toNode < coreServersMax) {
		// TODO: check if there is a connection fromNode -> toNode in the topology table.
		// If not, then the node issuing this message may be corrupted
		networkConnections[fromNode][toNode].status = status;
		networkConnections[fromNode][toNode].latency = latency;
		networkConnectionOnStatusChanged(fromNode, toNode);
	} else {
		PRINTF("Internall error: wrong nodes in connection status info message: %d -> %d.", fromNode, toNode);
	}	
}

// "msg=nst;src=%d;sts=%d;seq="PRId64";asp=%"PRId64";"
static void bixOnNodeStatusChanged(struct bcaio *cc) {
	int 	node;
	int		status;
	int64_t	blockSeqn;
	int64_t	usedSpace, availableSpace;

	BIX_GET_SRC(node);
	status = atoi(bixc->tag[BIX_TAG_sts]);
	blockSeqn = atoll(bixc->tag[BIX_TAG_seq]);
	usedSpace = atoll(bixc->tag[BIX_TAG_usp]);
	availableSpace = atoll(bixc->tag[BIX_TAG_asp]);

	networkNodes[node].index = node;
	networkNodes[node].status = status;
	networkNodes[node].blockSeqn = blockSeqn;
	networkNodes[node].usedSpace = usedSpace;
	networkNodes[node].totalAvailableSpace = availableSpace;

	guiUpdateNodeStatus(node);
}

/////////////////////////////////////////

static int bixMaybeForwardCoreMessage(struct bcaio *cc, char *msg, int msglen) {
	char 	*p, *pathend, *msgBody, *pn;
	int		msgBodyLen, res, bc;
	int		depth;

	res = 0;
	p = bixc->tag[BIX_TAG_dst];
	if (p == NULL) {
		PRINTF("Internal Error: bix message without destination %d->%d", cc->fromNode, cc->toNode);
		return(res);
	}
	pathend = p + bixc->tagLen[BIX_TAG_dst];
	msgBody = p + bixc->tagLen[BIX_TAG_dst] + 1;
	msgBodyLen = bixc->tag[BIX_TAG_end] - 4 - msgBody;
	res = bixMaybeForwardOrBroadcastToMultiPath(bixForwardDataPtr, p, pathend, msgBody, msgBodyLen, 0);
	return(res);
}

static int bixMaybeForwardSecondaryToCoreMessage(struct bcaio *cc, char *msg, int msglen) {
	char 	*p, *path, *pathend, *msgBody, *pn;
	int		msgBodyLen, res, bc;
	int		dstnode;

	res = 0;
	p = bixc->tag[BIX_TAG_dst];
	if (p == NULL) {
		PRINTF("Internal Error: bix message without destination %d->%d", cc->fromNode, cc->toNode);
		return(res);
	}
	// secondary server only set the final node where to send message
	if (bixc->tagLen[BIX_TAG_dst] <= 0) dstnode = myNode;
	else dstnode = atoi(bixc->tag[BIX_TAG_dst]);
	if (dstnode == myNode) {
		res = 1;
	} else {
		path = myNodeRoutingPaths[dstnode].path;
		pathend = path + strlen(path);
		msgBody = p + bixc->tagLen[BIX_TAG_dst] + 1;
		msgBodyLen = bixc->tag[BIX_TAG_end] - 4 - msgBody;
		// PRINTF("forwarding message %.20s... to %d.\n", msgBody, dstnode);
		res = bixMaybeForwardOrBroadcastToMultiPath(bixSecondaryToCoreForwardDataPtr, path, pathend, msgBody, msgBodyLen, cc->b.baioMagic);
	}
	return(res);
}

static int bixConnectionStatusNormal(struct bcaio *cc) {
	if (nodeState != NODE_STATE_NORMAL) return(0);
	if (cc->fromNode < 0) return(0);
	return(1);
}

static int bixConnectionStatusResync(struct bcaio *cc) {
	if (nodeState != NODE_STATE_RESYNC) return(0);
	return(1);
}


///////////////////////////
// main message dispatchers
///////////////////////////

// core server -> core server
static void bixProcessCoreServersMessage(struct bcaio *cc) {
	int			signatureVerified;
	int 		r, srcnode, msgType;
	char 		*s;

	s = bixc->tag[BIX_TAG_msg];
	msgType = BIX_TAG(s[0], s[1], s[2]);
	switch (msgType) {
	case BIX_TAG_lgn:
		// another core server logged in
		bixOnLoginReceived(cc);
		break;
	case BIX_TAG_pin:	
		if (bixConnectionStatusNormal(cc) || bixConnectionStatusResync(cc)) {
			bixOnPingReceived(cc);
		}
		break;
	case BIX_TAG_pon:	
		if (bixConnectionStatusNormal(cc) || bixConnectionStatusResync(cc)) {
			bixOnPongReceived(cc);
		}
		break;
	case BIX_TAG_cns:	
		if (bixConnectionStatusNormal(cc) || bixConnectionStatusResync(cc)) {
			bixOnNetworkConnectionStatusChanged(cc);
		}
		break;
	case BIX_TAG_nst:	
		if (bixConnectionStatusNormal(cc) || bixConnectionStatusResync(cc)) {
			bixOnNodeStatusChanged(cc);
		}
		break;
	case BIX_TAG_msg:	
		if (bixConnectionStatusNormal(cc) || bixConnectionStatusResync(cc)) {
			bixOnNewMessageReceived(cc);
		}
		break;
	case BIX_TAG_mis:	
		if (bixConnectionStatusNormal(cc) || bixConnectionStatusResync(cc)) {
			bixOnMissingMessageInfoReceived(cc);
		}
		break;
	case BIX_TAG_ben:	
		if (bixConnectionStatusNormal(cc) || bixConnectionStatusResync(cc)) {
			bixOnBlockEngagementInfoReceived(cc);
		}
		break;
	case BIX_TAG_blc:	
		if (bixConnectionStatusNormal(cc) || bixConnectionStatusResync(cc)) {
			bixOnBlockContentReceived(cc);
		}
		break;
	case BIX_TAG_rsr:	
		if (bixConnectionStatusNormal(cc)) {
			bixOnBlockResynchronizationRequestReceived(cc);
		}
		break;
	case BIX_TAG_rsa:
		if (bixConnectionStatusResync(cc)) {
			bixOnBlockResynchronizationAnswerReceived(cc);
		}
		break;
	default:
		PRINTF("Unknown message type %s received on connection %d -> %d\n", s, cc->fromNode, cc->toNode);
		bixClose(cc);
	}
}

#define DIRECT_SECONDARY_SERVER_CONNECTION_VERIFICATION(cc, msgType) {	\
		if (cc->connectionType != CT_SECONDARY_TO_CORE_IN_CORE_SERVER) { \
			PRINTF("%s: Error: This message type can not be forwarded: %d\n", SPRINT_CURRENT_TIME(), msgType); \
			return;														\
		}																\
	}
// secondary server -> core server
static void bixProcessSecondaryServersMessageInCoreServer(struct bcaio *cc) {
	int 	src, msgType, ssbaioMagic;
	char 	*s;
	char	returnPath[TMP_STRING_SIZE];

	ssbaioMagic = 0;
	if (bixc->tagLen[BIX_TAG_ssf] > 0) {
		// a forwarded request
		ssbaioMagic = atoi(bixc->tag[BIX_TAG_ssf]);
		BIX_GET_SRC(src);
		snprintf(returnPath, sizeof(returnPath), "%s:-%d", myNodeRoutingPaths[src].path, ssbaioMagic);
	} else {
		// direct secondary server connection, note account for notifications
		anotificationAddOrRefresh(bixc->tag[BIX_TAG_acc], cc->b.baioMagic, NCT_BIX);
		anotificationAddOrRefresh(bixc->tag[BIX_TAG_nac], cc->b.baioMagic, NCT_BIX);
		snprintf(returnPath, sizeof(returnPath), "-%d", cc->b.baioMagic);
	}
	s = bixc->tag[BIX_TAG_msg];
	msgType = BIX_TAG(s[0], s[1], s[2]);
	switch (msgType) {
	case BIX_TAG_pin:
		DIRECT_SECONDARY_SERVER_CONNECTION_VERIFICATION(cc, msgType);
		bixOnPingReceived(cc);
		break;
	case BIX_TAG_lgs:	
		// a secondary server logged in
		DIRECT_SECONDARY_SERVER_CONNECTION_VERIFICATION(cc, msgType);
		bixOnLoginReceivedFromSecondaryServer(cc);
		break;	
	case BIX_TAG_nma:	
		// new message add
		DIRECT_SECONDARY_SERVER_CONNECTION_VERIFICATION(cc, msgType);
		bixOnNewMessageAddToBlockchainFromSecondaryServer(cc, returnPath);
		break;
	case BIX_TAG_nme:	
		// new message process
		bixOnNewMessageExecuteFromSecondaryServer(returnPath);
		break;
	case BIX_TAG_rsr:	
		// secondary explorers are using resync requests to get the blockchain
		DIRECT_SECONDARY_SERVER_CONNECTION_VERIFICATION(cc, msgType);
		bixOnBlockResynchronizationRequestReceived(cc);
		break;
	default:
		PRINTF("Unknown message type %s received from secondary server %s\n", s, sprintfIpAddress_st(cc->b.remoteComputerIp));
		bixClose(cc);
	}
}

// core server -> secondary server
static void bixProcessCoreServersMessageInSecondaryServer(struct bcaio *cc) {
	int 	msgType;
	char 	*s;

	s = bixc->tag[BIX_TAG_msg];
	msgType = BIX_TAG(s[0], s[1], s[2]);
	switch (msgType) {
	case BIX_TAG_pin:	
		if (bixConnectionStatusNormal(cc)) {
			bixOnPingReceived(cc);
		}
		break;
	case BIX_TAG_msa:	
		bixOnMessageAcceptedReceivedFromCoreServer(cc);
		break;
	case BIX_TAG_msr:	
		bixOnMessageRejectedReceivedFromCoreServer(cc);
		break;
	case BIX_TAG_mse:
		bixOnImmediateMessageExecutedFromCoreServer(cc);
		break;
	case BIX_TAG_msn:
		bixOnMessageNotificationFromCoreServer(cc);
		break;
	case BIX_TAG_rsa:
		bixOnBlockResynchronizationAnswerReceivedFromCoreserverBySecondaryServer(cc);
		break;
	default:
		PRINTF("Unknown message type %s received from core server %s\n", s, sprintfIpAddress_st(cc->b.remoteComputerIp));
		bixClose(cc);
	}
}

void bixMakeTagsNullTerminating(struct bixParsingContext *bix) {
	int j, i, k;

	// make fields to be null terminating
	for(i=0; i<bix->usedTagsi; i++) {
		j = bix->usedTags[i];
		k = bix->tagLen[j];
		assert(k >= 0 && bix->tag[j][k] == ';');
		bix->tag[j][k] = 0;
	}
}

void bixMakeTagsSemicolonTerminating(struct bixParsingContext *bix) {
	int i, j, k;

	if (bix == NULL) return;

	// make fields back to be ';' terminating
	for(i=0; i<bix->usedTagsi; i++) {
		j = bix->usedTags[i];
		k = bix->tagLen[j];
		assert(k>=0 && (bix->tag[j][k] == ';' || bix->tag[j][k] == 0));
		bix->tag[j][k] = ';';
	}
}

int bixAddToUsedTags(struct bixParsingContext *bix, int tag, int makeTagsNullTerminatingFlag) {
	int i;

	if (bix->tagLen[tag] < 0) {
		bix->usedTags[bix->usedTagsi] = tag;
		bix->usedTagsi ++;
	} else {
		// it is already used, do not reinsert, just make its previous occurence  non null terminating
		if (makeTagsNullTerminatingFlag) {
			assert(bix->tag[tag][bix->tagLen[tag]] == 0);
			bix->tag[tag][bix->tagLen[tag]] = ';';
		}
	}

	return(0);
}

void bixCleanBixTags(struct bixParsingContext *bix) {
	int i, n;
	n = bix->usedTagsi;
	if (n < 0) {
		// initialization
		for(i=0; i<BIX_TAG_MAX; i++) {
			bix->tag[i] = "";
			bix->tagLen[i] = -1;
		}
		bix->usedTagsi = 0;
	}
	for(i=0; i<n; i++) {
		// clear only previous used indexes
		bix->tag[bix->usedTags[i]] = "";
		bix->tagLen[bix->usedTags[i]] = -1;
	}
	bix->usedTagsi = 0;
}

static struct bixParsingContext *bixParsingContextStack = NULL;

struct bixParsingContext *bixParsingContextGet() {
	static int 					bixParsingContextStackDepth = 0;
	int							i;
	struct bixParsingContext 	*res;

	if (bixParsingContextStack == NULL) {
		ALLOC(bixParsingContextStack, struct bixParsingContext);
		memset(bixParsingContextStack, 0, sizeof(*bixParsingContextStack));
		for(i=0; i<BIX_TAG_MAX; i++) {
			bixParsingContextStack->tag[i] = "";
			bixParsingContextStack->tagLen[i] = -1;
		}
		bixParsingContextStack->usedTagsi = 0;
		bixParsingContextStack->nextInContextStack = NULL;
		bixParsingContextStackDepth ++;
		if (bixParsingContextStackDepth > 10) {
			PRINTF0("Warning: deep parsing stack. Probably a forgottent bixFreeContext ?\n");
		}
	}
	
	res = bixParsingContextStack;
	bixParsingContextStack = bixParsingContextStack->nextInContextStack;

	res->nextInContextStack = NULL;
	return(res);
}

void bixParsingContextFree(struct bixParsingContext **bixa) {
	int							i;
	struct bixParsingContext 	*bix;

	bix = *bixa;
	if (bix == NULL) return;

	// TODOL inline it
	bixCleanBixTags(bix);
	bix->nextInContextStack = bixParsingContextStack;
	bixParsingContextStack = bix;

	// TODO: You can remove this check later
	for(i=0; i<BIX_TAG_MAX; i++) assert(strcmp(bixParsingContextStack->tag[i], "") == 0 && bixParsingContextStack->tagLen[i] == -1);

	*bixa = NULL;
}

int bixParseString(struct bixParsingContext *bix, char *str, int strLength) {
	int 		r, tagn;

	BIX_PARSE_AND_MAP_STRING(str, strLength, bix, 0, {}, {return(-1);});
	return(0);
}

static void bixProcessParsedMessage(struct bcaio *cc, char *msg, int len) {
	char	*s;
	int		i, j, msgType;

	if (debuglevel > 20) {
		PRINTF0("Processing message: "); 
		if (debuglevel > 60 || len < 100) {
			bixDumpParsedMsg(msg, len);
		} else {
			bixDumpParsedMsg(msg, 100);
			printf("..."); 
		}
		printf("\n"); 
	}

	s = bixc->tag[BIX_TAG_msg];
	if (strlen(s) != 3) {
		if (debuglevel > 0) {
			PRINTF0("Received Wrongly Formed Message: "); 
			bixDumpParsedMsg(msg, len);
			printf(". Closing connection %d -> %d\n", cc->fromNode, cc->toNode); 
		}
		bixClose(cc);
		return;
	}

	// process message
	if (cc->connectionType  == CT_CORE_TO_CORE_SERVER) {
		if (bixc->tagLen[BIX_TAG_ssf] >= 0) {
			bixProcessSecondaryServersMessageInCoreServer(cc);
		} else {
			bixProcessCoreServersMessage(cc);
		}
	} else if (cc->connectionType == CT_SECONDARY_TO_CORE_IN_CORE_SERVER) {
		bixProcessSecondaryServersMessageInCoreServer(cc);
	} else if (cc->connectionType == CT_SECONDARY_TO_CORE_IN_SECONDARY_SERVER) {
		bixProcessCoreServersMessageInSecondaryServer(cc);
	} else {
		PRINTF("Internal error: wrong connection type %d. Closing connection", cc->connectionType); 
		bixClose(cc);
		return;
	}

}

// #define GOTO_SKIP_MESSAGE_WITH_LEN goto skipMessageWithLen;
#define GOTO_SKIP_MESSAGE_WITH_LEN {if (debuglevel > 0) {PRINTF("Error at %s: %d\n", __FILE__, __LINE__); } goto skipMessageWithLen;}

void bixParseAndProcessAvailableMessages(struct bcaio *cc) {
	int 						len, blen, i, r, tagn, etagn, tlen, processFlag, srcnode;
	struct baioReadBuffer		*b;
	char						*s, *sbeg, *send, *tagv, *sig0;

	bixc = bixParsingContextGet();

entrypoint:

	// if connection was closed in between, do nothing
	if ((cc->b.status & BAIO_STATUS_ACTIVE) == 0) goto exitPoint;

	b = &cc->b.readBuffer;
	// we can always write at least one byte at the end msg 
	// (ensured by default value of baio->minFreeSizeAtEndOfReadBuffer).
	b->b[b->j] = 0;
	blen = b->j - b->i;
	s = sbeg = b->b + b->i;

	// first check if we actually have enough of the message
	if (blen < sizeof(BIX_HEADER) + sizeof(BIX_FOOTER) - 1 + BIX_LEN_DIGITS) goto exitPoint;
	// check if message starts by the header
	if (strncmp(s, BIX_HEADER, sizeof(BIX_HEADER)-1) != 0) goto skipMessageWithoutLen;

	s += sizeof(BIX_HEADER)-1;
	len = parseInt(&s);
	if (*s != ';') goto skipMessageWithoutLen;
	s ++;
	if (len <= 0) goto skipMessageWithoutLen;

	// if we do not have the whole message, do nothing
	if (b->j < b->i + len) goto exitPoint;
	send = sbeg + len;

	// check that the message ends with the right footer
	if (strncmp(send-(sizeof(BIX_FOOTER)-1), BIX_FOOTER, sizeof(BIX_FOOTER)-1) != 0) GOTO_SKIP_MESSAGE_WITH_LEN;

	// check that the message has dst just after len
	if (strncmp(s, "dst=", 4) != 0) GOTO_SKIP_MESSAGE_WITH_LEN;

	r = bixParseString(bixc, sbeg, len);
	if (r != 0) GOTO_SKIP_MESSAGE_WITH_LEN;

	// O.K. we have a correctly parsed message
	// time to verify message signature
	if (cc->connectionType  == CT_CORE_TO_CORE_SERVER) {
		if (bixc->tagLen[BIX_TAG_src] >= 0 && bixc->tagLen[BIX_TAG_crs] >= 0 && bixc->tagLen[BIX_TAG_dst] >= 0) {
			srcnode = atoi(bixc->tag[BIX_TAG_src]);
			sig0 = bixc->tag[BIX_TAG_dst]+bixc->tagLen[BIX_TAG_dst]+1;
			r = verifyMessageSignature(sig0, bixc->tag[BIX_TAG_crs]-sig0-4, MESSAGE_DATATYPE_BYTECOIN, getAccountName_st(0, srcnode), bixc->tag[BIX_TAG_crs], bixc->tagLen[BIX_TAG_crs], 0);
			if (r < 0) {
				PRINTF("Error: Bix: Wrong signature in core %d to core server message.\n", srcnode);
			}
		} else {
			PRINTF("Error: Bix: Missing src, dst or crs in core %d to core server message.\n", cc->remoteNode);
			r = -1;
		}
		if (r < 0) {
			PRINTF("Error: Bix: The message was %.*s. Closing connection.\n",  len, sbeg);
			baioCloseMagicOnError(cc->b.baioMagic);
			goto exitPoint;
		}
	}

	// forward or process it (only core servers can forward message to another core server)
	processFlag = 1;
	if (cc->connectionType  == CT_CORE_TO_CORE_SERVER) {
		processFlag = bixMaybeForwardCoreMessage(cc, sbeg, len);
	} else if (cc->connectionType  == CT_SECONDARY_TO_CORE_IN_CORE_SERVER) {
		processFlag = bixMaybeForwardSecondaryToCoreMessage(cc, sbeg, len);
	}

	if (processFlag) {
		bixMakeTagsNullTerminating(bixc);
		bixProcessParsedMessage(cc, sbeg, len);
		// not necessary in this particular case
		// be careful, processing of message can close baio connection
		if (cc->b.readBuffer.b != NULL) bixMakeTagsSemicolonTerminating(bixc);
	}
	b->i += len;
	goto entrypoint;

skipMessageWithLen:
	PRINTF0("Bix: Error in message: ");
	bixDumpParsedMsg(sbeg, len);
	printf(". Skipping it.\n");
	r = bixOnError(cc);
	if (r == 0) {
		b->i += len;
	}
	goto entrypoint;

skipMessageWithoutLen:
	bixOnErrorFindAnotherMessageStartInBuffer(cc);
	goto entrypoint;		

exitPoint:
	bixParsingContextFree(&bixc);
	return;
}

////////////////////////////////////////

struct jsVarDynamicString *kycToBix(struct kycData *kk) {
	struct jsVarDynamicString	*ss;

	if (kk == NULL) return(jsVarDstrCreate());

	ss = jsVarDstrCreateByPrintf("fnm:%d=%s;lnm:%d=%s;eml:%d=%s;tel:%d=%s;cnt:%d=%s;adr:%d=%s;mac:%d=%s;idf:%d=%s;pdf:%d=%s;",
								 strlen(kk->firstName), kk->firstName,
								 strlen(kk->lastName), kk->lastName,
								 strlen(kk->email), kk->email,
								 strlen(kk->telephone), kk->telephone,
								 strlen(kk->country), kk->country,
								 strlen(kk->address), kk->address,
								 strlen(kk->masterAccount), kk->masterAccount,
								 strlen(kk->idscanfname), kk->idscanfname,
								 strlen(kk->podscanfname), kk->podscanfname
		);
	if (kk->idscan != NULL) {
		jsVarDstrAppendPrintf(ss, "ids:%d=", kk->idscan->size);
		jsVarDstrAppendData(ss,  kk->idscan->s, kk->idscan->size);
		jsVarDstrAppendPrintf(ss, ";");
	}
	if (kk->podscan != NULL) {
		jsVarDstrAppendPrintf(ss, "pds:%d=", kk->podscan->size);
		jsVarDstrAppendData(ss,  kk->podscan->s, kk->podscan->size);
		jsVarDstrAppendPrintf(ss, ";");
	}

	return(ss);
}

struct kycData *bixToKyc(char *msg, int msglen) {
	struct kycData 				*kyc;
	struct bixParsingContext	*bix;
	int							r;

	kyc = NULL;
	bix = bixParsingContextGet();
	r = bixParseString(bix, msg, msglen);
	if (r != 0) goto finito;

	ALLOC(kyc, struct kycData);
	memset(kyc, 0, sizeof(*kyc));
	bixMakeTagsNullTerminating(bix);
	STRN_CPY_TMP(kyc->firstName, bix->tag[BIX_TAG_fnm]);
	STRN_CPY_TMP(kyc->lastName, bix->tag[BIX_TAG_lnm]);
	STRN_CPY_TMP(kyc->email, bix->tag[BIX_TAG_eml]);
	STRN_CPY_TMP(kyc->telephone, bix->tag[BIX_TAG_tel]);
	STRN_CPY_TMP(kyc->country, bix->tag[BIX_TAG_cnt]);
	STRN_CPY_TMP(kyc->address, bix->tag[BIX_TAG_adr]);
	STRN_CPY_TMP(kyc->masterAccount, bix->tag[BIX_TAG_mac]);
	STRN_CPY_TMP(kyc->idscanfname, bix->tag[BIX_TAG_idf]);
	STRN_CPY_TMP(kyc->podscanfname, bix->tag[BIX_TAG_pdf]);
	kyc->idscan = jsVarDstrCreateFromCharPtr(bix->tag[BIX_TAG_ids], bix->tagLen[BIX_TAG_ids]);
	kyc->podscan = jsVarDstrCreateFromCharPtr(bix->tag[BIX_TAG_pds], bix->tagLen[BIX_TAG_pds]);
	bixMakeTagsSemicolonTerminating(bix);

	finito:
	bixParsingContextFree(&bix);
	return(kyc);
}

void kycFree(struct kycData **kyc) {
	if (kyc == NULL || *kyc == NULL) return;
	jsVarDstrFree(&(*kyc)->idscan);
	jsVarDstrFree(&(*kyc)->podscan);
	FREE(*kyc);
	*kyc = NULL;
}

/////////////////
