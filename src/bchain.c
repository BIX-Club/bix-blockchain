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

///////////////////////////////////////////////////////////////////////////////////////////
// hashed table of pending messages

#define BCHAIN_MSG_HASH_TABLE_SIZE	(1<<16)

typedef struct bchainMessageList bchainMessageList;
static int bchainMessageListComparator(struct bchainMessageList *o1, struct bchainMessageList *o2) {
    return(strcmp(o1->msg.msgId, o2->msg.msgId));
}
static unsigned bchainMessageListHashFun(struct bchainMessageList *o) {
	unsigned res;
	SGLIB_HASH_FUN_STR(o->msg.msgId, res);
	return(res);
}
SGLIB_DEFINE_LIST_PROTOTYPES(bchainMessageList, bchainMessageListComparator, next);
SGLIB_DEFINE_HASHED_CONTAINER_PROTOTYPES(bchainMessageList, BCHAIN_MSG_HASH_TABLE_SIZE, bchainMessageListHashFun) ;
SGLIB_DEFINE_LIST_FUNCTIONS(bchainMessageList, bchainMessageListComparator, nextInHashTable);
SGLIB_DEFINE_HASHED_CONTAINER_FUNCTIONS(bchainMessageList, BCHAIN_MSG_HASH_TABLE_SIZE, bchainMessageListHashFun) ;

struct bchainMessageList *msgHashTable[BCHAIN_MSG_HASH_TABLE_SIZE] = {NULL,};
int msgNumberOfPendingMessages = 0;

///////////////////////////////////////////////////////////////////////////////////////////
// forward refs

///////////////////////////////////////////////////////////////////////////////////////////

#define BLOCK_ID_HASH_PART_SIZE	16

int bchainBlockIdCreate(char *blockId, char *hash, int64_t serverBlockSequenceNumber, int64_t blockSequenceNumber, int node) {
	int r;
	r = sprintf(blockId, "%*.*s-%"PRIx64"-%"PRIx64"-%03d", BLOCK_ID_HASH_PART_SIZE, BLOCK_ID_HASH_PART_SIZE, hash, serverBlockSequenceNumber, blockSequenceNumber, node);
	return(r);
}

int bchainBlockIdParse(char *blockId, char *hash, int64_t *serverBlockSequenceNumber, int64_t *blockSequenceNumber, int *node) {
	char 			*s;
	int64_t			nn, xx;

	s = blockId;
	if (s == NULL) return(-1);
	if (strlen(s) < BLOCK_ID_HASH_PART_SIZE+2) return(-1);
	if (hash != NULL) {
		strncpy(hash, s, BLOCK_ID_HASH_PART_SIZE);
		hash[BLOCK_ID_HASH_PART_SIZE] = 0;
	}
	s += BLOCK_ID_HASH_PART_SIZE+1;
	xx = parseHexll(&s);
	if (serverBlockSequenceNumber != NULL) *serverBlockSequenceNumber = xx;
	if (*s != '-') return(-1);
	s ++;
	xx = parseHexll(&s);
	if (blockSequenceNumber != NULL) *blockSequenceNumber = xx;
	if (*s != '-') return(-1);
	s ++;
	nn = parseInt(&s);
	if (node != NULL) *node = nn;
	if (*s != 0) return(-1);
	return(s-blockId);
}

int bchainMsgIdCreate(char *msgId, int64_t msgSequenceNumber, int nodeNumber, int nonce) {
	int r;
	r = sprintf(msgId, "%"PRId64".%02d.%04x", msgSequenceNumber, nodeNumber, nonce);
	return(r);
}

int bchainMsgIdParse(char *msgId, int64_t *msgSequenceNumber, int *nodeNumber, int *nonce) {
	char	*s;
	int		n;
	int64_t	nn;

	s = msgId;

	if (s == NULL) return(-1);
	nn = parseLongLong(&s);
	if (msgSequenceNumber != NULL) *msgSequenceNumber = nn;
	if (*s != '.') return(-1);
	s++;
	nn = parseLongLong(&s);
	if (nodeNumber != NULL) *nodeNumber = nn;
	if (*s != '.') return(-1);
	s++;
	nn = parseHexi(&s);
	if (nonce != NULL) *nonce = nn;
	if (*s != 0) return(-1);
	return(s - msgId);
}

void bchainGetBlockContentHash64(struct blockContentDataList *bb, char hash64[MAX_HASH64_SIZE]) {
	char	shahash[SHA512_DIGEST_LENGTH];

	SHA512(bb->blockContent, bb->blockContentLength, shahash);
	base64_encode(shahash, sizeof(shahash), hash64, MAX_HASH64_SIZE);
}

void bchainMessageCopy(struct bchainMessage *dst, struct bchainMessage *src) {
	memcpy(dst, src, sizeof(*src));
	dst->msgId = strDuplicate(src->msgId);
	dst->account = strDuplicate(src->account);
	dst->notifyAccount = strDuplicate(src->notifyAccount);
	dst->notifyAccountToken = strDuplicate(src->notifyAccountToken);
	dst->secondaryServerInfo = strDuplicate(src->secondaryServerInfo);
	dst->signature = strDuplicate(src->signature);
	dst->timeStampUsec = src->timeStampUsec;
	dst->dataType = strDuplicate(src->dataType);
	dst->dataLength = src->dataLength;
	ALLOCC(dst->data, src->dataLength, char);
	memcpy(dst->data, src->data, src->dataLength);
}

struct bchainMessageList *bchainEnlistPendingMessage(struct bchainMessage *msg) {
	struct bchainMessageList	*mm;

	if (msg == NULL) return(NULL);

	if (debuglevel > 25) {
		PRINTF("Appending message %s \n", msg->msgId);
	}

	if (msg->dataLength >= BCHAIN_MAX_BLOCK_SIZE) {
		PRINTF("Message has %d bytes, too long to insert into a block. Discarting it\n", msg->dataLength);
		return(NULL);
	}
	ALLOC(mm, struct bchainMessageList);
	bchainMessageCopy(&mm->msg, msg);
	mm->markedAsMissingBySomeNode = 0;
	mm->markedAsInvalid = 0;
	mm->nextInHashTable = NULL;
	sglib_hashed_bchainMessageList_add(msgHashTable, mm);
	msgNumberOfPendingMessages ++;
	// it may happen that the current block was waiting for this message, this is why I have to reconsider angagement
	// This takes too long. I am putting this into the main 
	// consensusReconsiderMyEngagement();
	return(mm);
}

void bchainFillMsgStructure(struct bchainMessage *msg, char *msgId, char *ownerAccount, char *notifyAccount, char *notifyAccountToken, long long timestamp, char *messageDataType, int datalen, char *data, char *signature, char *secondaryServerInfo) {
	msg->msgId = msgId;
	msg->account = ownerAccount;
	msg->notifyAccount = notifyAccount;
	msg->notifyAccountToken = notifyAccountToken;
	msg->secondaryServerInfo = secondaryServerInfo;
	msg->signature = signature;
	msg->timeStampUsec = timestamp;
	msg->dataType = messageDataType;
	msg->dataLength = datalen;
	msg->data = data;
}

struct bchainMessageList *bchainCreateNewMessage(char *data, int datalen, char *messageDataType, char *ownerAccount, char *notifyAccountToken, char *notifyAccount, char *signature, char *secondaryServerInfo) {
	struct bchainMessage		msg;
	struct bchainMessageList	*mm;
	char						msgId[TMP_STRING_SIZE];
	long long					timestamp;

	// This must be a unique identification of the message
	// It must start by node->currentMsgSequenceNumber  (this is parsed when accepting blocks during recovery)
	bchainMsgIdCreate(msgId, node->currentMsgSequenceNumber, myNode, rand() & 0xffff);
	node->currentMsgSequenceNumber ++;
	timestamp = currentTime.usec;
	bchainFillMsgStructure(&msg, msgId, ownerAccount, notifyAccount, notifyAccountToken, timestamp, messageDataType, datalen, data, signature, secondaryServerInfo);
	mm = bchainEnlistPendingMessage(&msg);
	if (mm == NULL) return(NULL);
	bixBroadcastNewMessage(mm);

	// this is tricky, increase current time so that all messages created within the same tick are ordered correctly
	setCurrentTime();
	if (currentTime.usec == timestamp) currentTime.usec ++;

	return(mm);
}

void bchainFreeMessageContent(struct bchainMessage *msg) {
	FREE(msg->msgId);
	FREE(msg->account);
	FREE(msg->notifyAccount);
	FREE(msg->notifyAccountToken);
	FREE(msg->secondaryServerInfo);
	FREE(msg->signature);
	FREE(msg->dataType);
	FREE(msg->data);
}

static void bchainFreeMessage(struct bchainMessageList *memb) {
	msgNumberOfPendingMessages --;
	bchainFreeMessageContent(&memb->msg);
	FREE(memb);
}

void bchainRemovePendingMessageBecauseItWasSuccesfullyInsertedIntoBlock(char *msgId, struct blockContentDataList *data) {
	int							plen;
	struct bchainMessageList 	bbb, *memb;

	if (debuglevel > 25) {
		PRINTF("Removing message %s\n", msgId);
	}

	bbb.msg.msgId = msgId;
	sglib_hashed_bchainMessageList_delete_if_member(msgHashTable, &bbb, &memb);
	if (memb == NULL) {
		if (nodeState != NODE_STATE_RESYNC) {
			PRINTF("Internall error: the message %s accepted in block is not in pending messages.\n", msgId);
		}
		return;
	} else {
		// The wording of this message is important, the string "was inserted in block" is checked in walletNewTokenCreationStateUpdated !!!
		anotificationSaveMsgNotification(&memb->msg, "%s: Info: Message %s (%s) was inserted in block %"PRId64".", SPRINT_CURRENT_TIME(), memb->msg.msgId, bchainPrettyPrintMsgContent_st(&memb->msg), data->blockSequenceNumber);
		bchainFreeMessage(memb);
	}
}

void bchainInvalidateMessageForNextBlocks(char *msgId) {
	int							plen;
	struct bchainMessageList 	bbb, *memb;

	if (debuglevel > 25) {
		PRINTF("Invalidating message %s \n", msgId);
	}

	bbb.msg.msgId = msgId;
	sglib_hashed_bchainMessageList_delete_if_member(msgHashTable, &bbb, &memb);
	if (memb == NULL) {
		if (nodeState != NODE_STATE_RESYNC) {
			PRINTF("Internall error: the message/transaction %s accepted in block is not in pending messages.\n", msgId);
		}
		return;
	} else {
		anotificationSaveMsgNotification(&memb->msg, "%s: Error: Message %s (%s) was invalid. Not included in the blockchain.", SPRINT_CURRENT_TIME(), memb->msg.msgId, bchainPrettyPrintMsgContent_st(&memb->msg));		
		memb->markedAsInvalid = 1;
	}	
}

void bchainRemoveExpiredMessages() {
	int 						i;
	struct bchainMessageList 	**mm, *mmnext;

	for(i=0; i<BCHAIN_MSG_HASH_TABLE_SIZE; i++) {
		mm = &msgHashTable[i];
		while (*mm != NULL) {
			if ((*mm)->msg.timeStampUsec < currentTime.usec - MESSAGE_EXPIRATION_TIMEOUT_USEC) {
				if (debuglevel > 5) PRINTF("Removing expired message %s from %s\n", (*mm)->msg.msgId, sprintSecTime_st((*mm)->msg.timeStampUsec));
				anotificationSaveMsgNotification(&(*mm)->msg, "%s: Warning: Message %s (%s) expired. Not included in the blockchain.", SPRINT_CURRENT_TIME(), (*mm)->msg.msgId, bchainPrettyPrintMsgContent_st(&(*mm)->msg));
				mmnext = (*mm)->nextInHashTable;
				bchainFreeMessage(*mm);
				*mm = mmnext;
			} else {
				mm = &(*mm)->nextInHashTable;
			}
		}
	}
}

// TODO: Remove this
void bchainNoteMissingMessage(char *msgId) {
	int							plen;
	struct bchainMessageList 	bbb, *memb;

	if (1 || debuglevel > 25) {
		PRINTF("Marking message %s as missing\n", msgId);
	}

	bbb.msg.msgId = msgId;
	sglib_hashed_bchainMessageList_delete_if_member(msgHashTable, &bbb, &memb);
	if (memb != NULL) {
		memb->markedAsMissingBySomeNode = 1;
	}
}

static int bchainMessageTimeComparator(struct bchainMessageList *b1, struct bchainMessageList *b2) {
	if (b1->msg.timeStampUsec < b2->msg.timeStampUsec) return(-1);
	if (b1->msg.timeStampUsec > b2->msg.timeStampUsec) return(1);
	return(0);
}

void bchainProposeNewBlock() {
	struct jsVarDynamicString						*bb, *cc;
	struct blockContentDataList 					bbb, *bc;
	struct bchainMessageList						*mm, *blist, **blistTail;
	int												blockVerificationResult, n, contentSize;
	struct sglib_hashed_bchainMessageList_iterator	iii;
	char											hash64[MAX_HASH64_SIZE];
	char											blockId[SHA512_DIGEST_LENGTH*2+TMP_STRING_SIZE];

	if (debuglevel > 44) PRINTF0("bchainProposeNewBlock: called\n"); fflush(stdout);

bchainProposeNewBlockEntry:

	if (currentEngagement != NULL && currentEngagement->expirationUsec >= currentTime.usec) {
		if (debuglevel > 44) PRINTF("bchainProposeNewBlock: not proposing new block because engaged in %"PRId64" %s\n", currentEngagement->blockSequenceNumber, currentEngagement->blockId); fflush(stdout);
		return;
	}

	if (1 || currentProposedBlock == NULL) {
		bchainRemoveExpiredMessages();
		blist = NULL; 
		blistTail = &blist;
		contentSize = 0;
		for(mm = sglib_hashed_bchainMessageList_it_init(&iii, msgHashTable);
			mm != NULL && contentSize+mm->msg.dataLength < BCHAIN_MAX_BLOCK_SIZE;
			mm = sglib_hashed_bchainMessageList_it_next(&iii)
			) {
			// insert only messages supposed to not to cause "problems"
			if (
				// too old messages may be removed from servers before the block is accepted by the network and would block the chain
				mm->msg.timeStampUsec >= currentTime.usec - MESSAGE_EXPIRATION_TIMEOUT_USEC/2
				// too new messages may not appear on all servers in time and may block the chain
				&& mm->msg.timeStampUsec < currentTime.usec - BLOCK_NODE_AVERAGE_PROPOSAL_PERIOD_USEC/10
				// non problematic messages only
				&& mm->markedAsMissingBySomeNode == 0 
				&& mm->markedAsInvalid == 0
				) {
			contentSize += mm->msg.dataLength;
			*blistTail = mm;
			blistTail = &mm->nextInBlockProposal;
			*blistTail = NULL;
			}
		}
		// If current proposed block != NULL, then propose even empty block
		// because we are probably ???
		// Hmm. removing it.
		// if (blist == NULL && currentProposedBlock == NULL) return;	
		if (blist == NULL) {
			if (debuglevel > 44) PRINTF0("bchainProposeNewBlock: not proposing new block because there are no pending messages\n"); fflush(stdout);
			return;
		}
		bb = jsVarDstrCreate();
		cc = jsVarDstrCreate();
		bchainBlockContentAppendCurrentBlockHeader(cc);
		SGLIB_LIST_SORT(struct bchainMessageList, blist, bchainMessageTimeComparator, nextInBlockProposal);
		for(mm=blist; mm!=NULL; mm=mm->nextInBlockProposal) {
			jsVarDstrAppendPrintf(bb, "mid=%s;", mm->msg.msgId);
			bchainBlockContentAppendSingleMessage(cc, &mm->msg);
		}
		// create new block proposal
		memset(&bbb, 0, sizeof(bbb));
		bbb.blockSequenceNumber = node->currentBlockSequenceNumber;
		bbb.blockCreationTimeUsec = currentTime.usec;
		bbb.msgIds = bb->s;
		bbb.msgIdsLength = bb->size;
		bbb.blockContent = cc->s;
		bbb.blockContentLength = cc->size;
		bchainGetBlockContentHash64(&bbb, hash64);
		bbb.blockContentHash = hash64;
		bbb.signatures = NULL;
		bchainBlockIdCreate(blockId, hash64, node->currentServerBlockSequenceNumber, bbb.blockSequenceNumber, myNode);
		bbb.blockId = blockId;
		// check the consistency now to avoid rejection by all nodes after votes and hard recovery
		blockVerificationResult = bchainVerifyBlockMessagesInBlockContent(&bbb) ;
		if (blockVerificationResult == 0) {
			node->currentServerBlockSequenceNumber ++;
			currentProposedBlock = consensusBlockContentCreateCopy(&bbb);
			currentProposedBlock->next = receivedBlockContents[0];
			receivedBlockContents[0] = currentProposedBlock;
			// PRINTF("Creating block with id %s\n", blockId);
		}
		jsVarDstrFree(&bb);
		jsVarDstrFree(&cc);
		if (blockVerificationResult != 0) {
			// Hmm. I can simply abandon the block, or retry (because inconsistent messagew were invalidated yet).
			// 1.) If continuing then inconsistent messages stays on other servers and may be included into future block
			// by other server which looks ugly (or they are reported inconsistent after having accepted other messages
			// inserted after them (reversing processing order).
			// 2.) If simply returning then somebody can block the blockchain by simply submitting wrong message.
			// return;
			goto bchainProposeNewBlockEntry;
		}
	}

	// create and broadcast my engagement for this block
	ALLOC(currentEngagement, struct consensusBlockEngagementInfo);
	currentEngagement->firstAnnonceTimeUsec = currentTime.usec;
	currentEngagement->blockCreationTimeUsec = currentProposedBlock->blockCreationTimeUsec;
	currentEngagement->msgIdsLength = 0;
	currentEngagement->msgIdsLength = currentProposedBlock->msgIdsLength;
	currentEngagement->engagedNode = myNode;
	currentEngagement->engagementRound = 0;
	currentEngagement->expirationUsec = currentTime.usec + CONSENSUS_TICK_USEC;
	currentEngagement->blockId = strDuplicate(currentProposedBlock->blockId);
	currentEngagement->blocksignature = NULL;
	currentEngagement->blockSequenceNumber = currentProposedBlock->blockSequenceNumber;

	if (debuglevel > 5) PRINTF("Proposing block with id %s; seq %4"PRId64";\n", currentProposedBlock->blockId, currentProposedBlock->blockSequenceNumber);
	if (debuglevel > 50) PRINTF("Data: %.*s \n", currentProposedBlock->msgIdsLength, currentProposedBlock->msgIds);

	consensusMaybeProlongCurrentEngagement(1);
}

// TODO: split this into multiple functions depending on action !!!
static int bchainCheckThatWeHaveAllMessagesFromMsgidsInPendingList(struct blockContentDataList *data) {
	char 						*ss, *s, *tt, *send, *tagName, *tagValue;
	int							sslen, res, len, tagValueLength;
	struct bchainMessageList 	bbb, *mm;
	struct jsVarDynamicString	*dd;
	struct bixParsingContext	*bbixc;

	if (data == NULL) return(-1);
	ss = data->msgIds;
	sslen = data->msgIdsLength;
	if (ss == NULL) return(-1);

	res = 0;
	// PRINTF("CHECKING MSG %.*s\n", sslen, ss);

	bbixc = bixParsingContextGet();
	BIX_PARSE_AND_MAP_STRING(ss, sslen, bbixc, 0, {
			if (strncmp(tagName, "mid", 3) == 0) {
				bbb.msg.msgId = tagValue;
				mm = sglib_hashed_bchainMessageList_find_member(msgHashTable, &bbb);
				if (mm == NULL) {
					if (debuglevel > 25) {
						PRINTF("Delaying block %"PRId64" %s acceptance, because of missing msg %s\n", data->blockSequenceNumber, data->blockId, bbb.msg.msgId);
					}
					// bixBroadcastMissingMessage(bbb.msgId);
					res = -1;
				}
			}
		}, 
		{
			PRINTF("Error: in bix message: %.*s\n", sslen, ss);
			res = -1;
		}
		);
	bixParsingContextFree(&bbixc);

	return(res);
}

static int bchainCheckValidityOfMessagesFromMsgids(struct blockContentDataList *data) {
	char 						*ss, *s, *tt, *send, *tagName, *tagValue;
	int							sslen, res, len, tagValueLength;
	struct bchainMessageList 	bbb, *mm;
	struct bixParsingContext	*bbixc;

	if (data == NULL) return(-1);
	ss = data->msgIds;
	sslen = data->msgIdsLength;
	if (ss == NULL) return(-1);

	res = 0;
	// PRINTF("CHECKING MSG %.*s\n", sslen, ss);

	bbixc = bixParsingContextGet();
	BIX_PARSE_AND_MAP_STRING(ss, sslen, bbixc, 0, {
			if (strncmp(tagName, "mid", 3) == 0) {
				bbb.msg.msgId = tagValue;
				mm = sglib_hashed_bchainMessageList_find_member(msgHashTable, &bbb);
				if (mm == NULL) {
					PRINTF("Warning: message %s was is not in pending list for block %"PRId64" %s.\n", tagValue, data->blockSequenceNumber, data->blockId);
					res = -1;
					_err_ = 1;
				} else if (mm->markedAsInvalid) {
					PRINTF("Warning: message %s was invalidated and is proposed for block %"PRId64" %s.\n", tagValue, data->blockSequenceNumber, data->blockId);
					res = -1;
					_err_ = 1;
				}
			}
		}, 
		{
			PRINTF("Error: in bix message: %.*s\n", sslen, ss);
			res = -1;
		}
		);
	bixParsingContextFree(&bbixc);

	return(res);

}

static int bchainCreateBlockContent(struct blockContentDataList *data) {
	char 						*ss, *s, *tt, *send, *tagName, *tagValue;
	int							sslen, res, len, tagValueLength;
	struct bchainMessageList 	bbb, *mm;
	struct jsVarDynamicString	*dd;
	struct bixParsingContext	*bbixc;

	if (data == NULL) return(-1);
	ss = data->msgIds;
	sslen = data->msgIdsLength;
	if (ss == NULL) return(-1);

	dd = NULL;
	if (data->blockContent != NULL) FREE(data->blockContent);
	data->blockContent = NULL;
	data->blockContentLength = 0;
	dd = jsVarDstrCreate();
	bchainBlockContentAppendCurrentBlockHeader(dd);

	res = 0;
	// PRINTF("CHECKING MSG %.*s\n", sslen, ss);

	bbixc = bixParsingContextGet();
	BIX_PARSE_AND_MAP_STRING(ss, sslen, bbixc, 0, {
			if (strncmp(tagName, "mid", 3) == 0) {
				bbb.msg.msgId = tagValue;
				mm = sglib_hashed_bchainMessageList_find_member(msgHashTable, &bbb);
				if (mm == NULL) {
					PRINTF("Internal error: message %s is missing for block %"PRId64" %s content creation\n", tagValue, data->blockSequenceNumber, data->blockId);
					res = -1;
					_err_ = 1; 
				} else {
					bchainBlockContentAppendSingleMessage(dd, &mm->msg);
				}
			}
		}, 
		{
			PRINTF("Error: in bix message: %.*s\n", sslen, ss);
			res = -1;
		}
		);
	bixParsingContextFree(&bbixc);

	jsVarDstrTruncate(dd);
	data->blockContentLength = dd->size;
	data->blockContent = jsVarDstrGetStringAndReinit(dd);
	jsVarDstrFree(&dd);
	return(res);

}

static int bchainRemoveMessagesAcceptedInBlockContentFromPendingList(struct blockContentDataList *data) {
	char 								*ss, *s, *tt, *send, *tagName, *tagValue;
	int									sslen, res, len, tagValueLength;
	struct bchainMessageList 			bbb, *mm;
	struct jsVarDynamicString			*dd;
	struct bixParsingContext			*bbixc;

	if (data == NULL) return(-1);
	ss = data->blockContent;
	sslen = data->blockContentLength;
	if (ss == NULL) return(-1);

	res = 1;
	// PRINTF("CHECKING MSG %.*s\n", sslen, ss);

	bbixc = bixParsingContextGet();
	BIX_PARSE_AND_MAP_STRING(ss, sslen, bbixc, 0, {
			if (strncmp(tagName, "mid", 3) == 0) {
				bchainRemovePendingMessageBecauseItWasSuccesfullyInsertedIntoBlock(tagValue, data);
			}
		}, 
		{
			PRINTF("Error: in bix message: %.*s\n", sslen, ss);
			res = 0;
		}
		);
	bixParsingContextFree(&bbixc);
	return(res);
}

void bchainInitializeMessageTypes() {
	int									i;
	struct supportedMessageStr 	*sss;
	struct jsVarDynamicString 			*res;

	res = NULL;
	for(i=0; i<supportedMessagesTabMaxi; i++) {
		sss = &supportedMessagesTab[i];
		InternalCheck(sss->messageMetaType != NULL);
		if (sss->messageMetaType != NULL) {
			if (strcmp(sss->messageType, MESSAGE_DATATYPE_BYTECOIN) == 0) {
				bytecoinInitialize();
				break;
			}
		}
	}
}

//////////////////////////////////////////////////
// finalizing actions for accepted / rejected blocks

static void blockFinalizeAcceptForward(struct bFinalActionList *aa) {
	struct powcoinBlockPendingMessagesListStr *mm;

	switch (aa->action) {
	case BFA_ACCOUNT_MODIFIED:
		bAccountRecoveryListApplyAndSaveChanges(&aa->u.accountModified);
		break;
	case BFA_ACCOUNT_BALANCE_MODIFIED:
		bAccountBalanceRecoveryListApplyAndSaveChanges(&aa->u.accountBalanceModified);
		break;
	case BFA_TOKEN_OPTIONS_MODIFIED:
		coreServerSupportedMessageTableSave();
		break;
	case BFA_TOKEN_TAB_MODIFIED:
		coreServerSupportedMessageTableFree(aa->u.tokenTableModified.previousTable, aa->u.tokenTableModified.previousTableMax);
		coreServerSupportedMessageTableSave();
		break;
	case BFA_PENDING_REQUEST_INSERT:
		databasePut(DATABASE_PENDING_REQUESTS, aa->u.pendingRequest.reqName, aa->u.pendingRequest.reqData, aa->u.pendingRequest.reqDataLen);
		break;
	case BFA_PENDING_REQUEST_DELETE:
		databaseDelete(DATABASE_PENDING_REQUESTS, aa->u.pendingRequest.reqName);
		break;
	case BFA_POW_MESSAGE_ADD:
		break;
	case BFA_POW_MESSAGE_DELETE:
		// Operation has been definitely applied
		mm = aa->u.powMessage.powcoinMessage;
		// PRINTF("Deleting pow msg %s from pending list. Msg executed.\n", mm->msg.msgId);
		assert(mm != NULL);
		anotificationSaveMsgNotification(&mm->msg, "%s: Info: Message %s (%s from block %"PRId64"): proof of work done in msg %s block %"PRId64".", SPRINT_CURRENT_TIME(), mm->msg.msgId, bchainPrettyPrintMsgContent_st(&mm->msg), mm->fromBlockSequenceNumber, aa->u.powMessage.miningMsg, aa->u.powMessage.miningBlockSequenceNumber);
		bchainFreeMessageContent(&mm->msg);
		FREE(mm);
		aa->u.powMessage.powcoinMessage = NULL;
		break;
	case BFA_ACCOUNT_NOTIFICATION_ON_SUCCESS:
		anotificationSaveNotification(aa->u.notification.account, aa->u.notification.coin, "%s: %s", SPRINT_CURRENT_TIME(), aa->u.notification.message);
		break;
	case BFA_ACCOUNT_NOTIFICATION_ON_FAIL:
		break;
	case BFA_EXCHANGE_ORDER_ADDED:
		break;
	case BFA_EXCHANGE_ORDER_DELETED:
		FREE(aa->u.exchangeOrderModified.o);
		break;
	case BFA_EXCHANGE_ORDER_EXECUTED:
		break;
	default:
		PRINTF("Internal error: unknown block finalize action type %d\n", aa->action);
		break;
	}
}

static void blockFinalizeAcceptBackward(struct bFinalActionList *aa) {
	switch (aa->action) {
	case BFA_ACCOUNT_MODIFIED:
		break;
	case BFA_ACCOUNT_BALANCE_MODIFIED:
		break;
	case BFA_TOKEN_OPTIONS_MODIFIED:
		break;
	case BFA_TOKEN_TAB_MODIFIED:
		break;
	case BFA_PENDING_REQUEST_INSERT:
	case BFA_PENDING_REQUEST_DELETE:
		break;
	case BFA_POW_MESSAGE_ADD:
	case BFA_POW_MESSAGE_DELETE:
		break;
	case BFA_ACCOUNT_NOTIFICATION_ON_SUCCESS:
	case BFA_ACCOUNT_NOTIFICATION_ON_FAIL:
		break;
	case BFA_EXCHANGE_ORDER_ADDED:
	case BFA_EXCHANGE_ORDER_DELETED:
	case BFA_EXCHANGE_ORDER_EXECUTED:
		break;
	default:
		PRINTF("Internal error: unknown block finalize action type %d\n", aa->action);
		break;
	}
}

static void blockFinalizeRejectForward(struct bFinalActionList *aa) {
	switch (aa->action) {
	case BFA_ACCOUNT_MODIFIED:
		break;
	case BFA_ACCOUNT_BALANCE_MODIFIED:
		break;
	case BFA_TOKEN_OPTIONS_MODIFIED:
		break;
	case BFA_TOKEN_TAB_MODIFIED:
		break;
	case BFA_PENDING_REQUEST_INSERT:
	case BFA_PENDING_REQUEST_DELETE:
		break;
	case BFA_POW_MESSAGE_ADD:
	case BFA_POW_MESSAGE_DELETE:
		break;
	case BFA_ACCOUNT_NOTIFICATION_ON_SUCCESS:
		break;
	case BFA_ACCOUNT_NOTIFICATION_ON_FAIL:
		anotificationSaveNotification(aa->u.notification.account, aa->u.notification.coin, "%s: %s", SPRINT_CURRENT_TIME(), aa->u.notification.message);
		break;
	case BFA_EXCHANGE_ORDER_ADDED:
	case BFA_EXCHANGE_ORDER_DELETED:
	case BFA_EXCHANGE_ORDER_EXECUTED:
		break;
	default:
		PRINTF("Internal error: unknown block finalize action type %d\n", aa->action);
		break;
	}
}

static void blockFinalizeRejectBackward(struct bFinalActionList *aa) {
	struct symbolOrderBook 	*sbook;

	switch (aa->action) {
	case BFA_ACCOUNT_MODIFIED:
		bAccountRecoveryListRecover(&aa->u.accountModified);
		break;
	case BFA_ACCOUNT_BALANCE_MODIFIED:
		bAccountBalanceRecoveryListRecover(&aa->u.accountBalanceModified);
		break;
	case BFA_TOKEN_OPTIONS_MODIFIED:
		FREE(*aa->u.tokenOptions.place);
		*aa->u.tokenOptions.place = aa->u.tokenOptions.options;
		aa->u.tokenOptions.options = NULL;
		break;
	case BFA_TOKEN_TAB_MODIFIED:
		coreServerSupportedMessageTableFree(supportedMessagesTab, supportedMessagesTabMaxi);
		supportedMessagesTab = aa->u.tokenTableModified.previousTable;
		supportedMessagesTabMaxi = aa->u.tokenTableModified.previousTableMax;
		break;
	case BFA_PENDING_REQUEST_INSERT:
	case BFA_PENDING_REQUEST_DELETE:
		break;
	case BFA_POW_MESSAGE_ADD:
		assert(aa->u.powMessage.powcoinMessage != NULL);
		SGLIB_DL_LIST_DELETE(struct powcoinBlockPendingMessagesListStr, powcoinBlockPendingMessagesList, aa->u.powMessage.powcoinMessage, prev, next);
		bchainFreeMessageContent(&aa->u.powMessage.powcoinMessage->msg);
		FREE(aa->u.powMessage.powcoinMessage);
		aa->u.powMessage.powcoinMessage = NULL;
		break;
	case BFA_POW_MESSAGE_DELETE:
		assert(aa->u.powMessage.powcoinMessage != NULL);
		// PRINTF("reinserting %s because changes not applied\n", aa->u.powMessage.powcoinMessage->msg.msgId);
		SGLIB_DL_LIST_ADD(struct powcoinBlockPendingMessagesListStr, powcoinBlockPendingMessagesList, aa->u.powMessage.powcoinMessage, prev, next);
		break;
	case BFA_ACCOUNT_NOTIFICATION_ON_SUCCESS:
	case BFA_ACCOUNT_NOTIFICATION_ON_FAIL:
		break;
	case BFA_EXCHANGE_ORDER_ADDED:
		exchangeDeleteBookedOrder(aa->u.exchangeOrderModified.o, NULL);
		FREE(aa->u.exchangeOrderModified.o);
		break;
	case BFA_EXCHANGE_ORDER_DELETED:
		assert(aa->u.exchangeOrderModified.o!=NULL);
		assert(aa->u.exchangeOrderModified.halfbook!=NULL);
		assert(aa->u.exchangeOrderModified.halfbook->mySymbolBook!=NULL);
		sbook = aa->u.exchangeOrderModified.halfbook->mySymbolBook;
		exchangeAddOrder(aa->u.exchangeOrderModified.o->ownerAccount, sbook->baseToken, sbook->quoteToken, aa->u.exchangeOrderModified.halfbook->side, aa->u.exchangeOrderModified.price, aa->u.exchangeOrderModified.quantity, aa->u.exchangeOrderModified.executedQuantity, NULL);
		FREE(aa->u.exchangeOrderModified.o);
		break;
	case BFA_EXCHANGE_ORDER_EXECUTED:
		aa->u.exchangeOrderModified.o->executedQuantity = aa->u.exchangeOrderModified.executedQuantity;
		break;
	default:
		PRINTF("Internal error: unknown block finalize action type %d\n", aa->action);
		break;
	}
}

static void blockFinalizeFreeItem(struct bFinalActionList *aa) {
	switch (aa->action) {
	case BFA_ACCOUNT_MODIFIED:
		break;
	case BFA_ACCOUNT_BALANCE_MODIFIED:
		break;
	case BFA_TOKEN_OPTIONS_MODIFIED:
		FREE(aa->u.tokenOptions.options);
		break;
	case BFA_TOKEN_TAB_MODIFIED:
		break;
	case BFA_PENDING_REQUEST_INSERT:
	case BFA_PENDING_REQUEST_DELETE:
		FREE(aa->u.pendingRequest.reqName);
		FREE(aa->u.pendingRequest.reqData);
		break;
	case BFA_POW_MESSAGE_ADD:
	case BFA_POW_MESSAGE_DELETE:
		FREE(aa->u.powMessage.miningMsg);
		break;
	case BFA_ACCOUNT_NOTIFICATION_ON_SUCCESS:
	case BFA_ACCOUNT_NOTIFICATION_ON_FAIL:
		FREE(aa->u.notification.account);
		FREE(aa->u.notification.coin);
		FREE(aa->u.notification.message);
		break;
	case BFA_EXCHANGE_ORDER_ADDED:
	case BFA_EXCHANGE_ORDER_DELETED:
	case BFA_EXCHANGE_ORDER_EXECUTED:
		break;
	default:
		PRINTF("Internal error: unknown block finalize action type %d\n", aa->action);
		break;
	}	
	FREE(aa);
}

////////////////

struct jsVarDynamicString *bchainImmediatelyProcessSingleMessage_st(struct bchainMessage *msg, char *messageDataType) {
	int									i;
	struct supportedMessageStr 			*sss;
	struct jsVarDynamicString 			*res;

	res = NULL;
	if (msg == NULL) return(res);

	//anotificationAddOrRefresh(msg->account, secondaryServerConnectionBaioMagic, NCT_BIX);
	//anotificationAddOrRefresh(msg->notifyAccount, secondaryServerConnectionBaioMagic, NCT_BIX);

	sss = coreServerSupportedMessageFind(messageDataType);
	if (sss!=NULL) {
		InternalCheck(sss->messageMetaType != NULL);
		if (strcmp(sss->messageMetaType, MESSAGE_METATYPE_META_MSG) == 0) {
			res = immediatelyExecuteMetamsgMessage(msg);
		} else if (strcmp(sss->messageMetaType, MESSAGE_METATYPE_EXCHANGE) == 0) {
			res = immediatelyExecuteExchangeMessage(msg);
		} else if (strcmp(sss->messageMetaType, MESSAGE_METATYPE_SIMPLE_TOKEN) == 0) {
			res = immediatelyExecuteCoinMessage(msg);
		} else if (strcmp(sss->messageMetaType, MESSAGE_METATYPE_POW_TOKEN) == 0) {
			res = immediatelyExecuteCoinMessage(msg);
		}
	}
	return(res);
}

static int bchainVerifyOrApplyBlockMessagesFromBlockContent(struct blockContentDataList *data, int applyChangesFlag) {
	struct bchainMessage				mmm;
	int64_t								msgseq;
	int 								i, r, res, nodenum;
	struct bFinalActionList				*aa, *finalizeActionList;
	struct supportedMessageStr 			*sss;
	struct bixParsingContext			*bbixc;
	int									signatureVerified;
	
	finalizeActionList = NULL;
	res = 0;

	// mid=%s;tsu=%"PRId64";dat:%d=
	bbixc = bixParsingContextGet();
	BIX_PARSE_AND_MAP_STRING(
		data->blockContent, data->blockContentLength, bbixc, 1, 
		{
			if (strncmp(tagName, "mid", 3) == 0) {
				r = bchainMsgIdParse(tagValue, &msgseq, &nodenum, NULL);
				if (r < 0) {
					PRINTF("Internal error: Can't parse msgId %s.\n", tagValue);
				} else {
					if (applyChangesFlag) {
						// this is a trick for correct recovery after total reset. TODO: Review this.
						if (nodenum == myNode && msgseq >= node->currentMsgSequenceNumber) node->currentMsgSequenceNumber = msgseq + 1;
					}
				}
			} else if (strncmp(tagName, "dat", 3) == 0) {
				if (bbixc->tagLen[BIX_TAG_mid] < 0) {
					res = -1;
				} else {
					if (bbixc->tagLen[BIX_TAG_sig] < 0 || bbixc->tagLen[BIX_TAG_acc] < 0 || bbixc->tagLen[BIX_TAG_tsu] < 0 || bbixc->tagLen[BIX_TAG_mdt] < 0) {
						res = -1;
						bchainInvalidateMessageForNextBlocks(bbixc->tag[BIX_TAG_mid]);
						PRINTF("Internal error: Missing message type, signature, account or timestamp in message %s\n", mmm.msgId);
					} else {
						bixFillParsedBchainMessage(bbixc, &mmm);
						r = -1;
						sss = coreServerSupportedMessageFind(mmm.dataType);
						if (sss != NULL) {
							InternalCheck(sss->messageMetaType != NULL);
							if (strcmp(sss->messageMetaType, MESSAGE_METATYPE_META_MSG) == 0) {
								r = applyMetamsgBlockchainMessage(&mmm, data, &finalizeActionList);
							} else if (strcmp(sss->messageMetaType, MESSAGE_METATYPE_EXCHANGE) == 0) {
								r = applyExchangeBlockchainMessage(&mmm, data, &finalizeActionList);
							} else if (strcmp(sss->messageMetaType, MESSAGE_METATYPE_USER_FILE) == 0) {
								r = applyUserfileBlockchainMessage(&mmm, data, &finalizeActionList);
							} else if (strcmp(sss->messageMetaType, MESSAGE_METATYPE_SIMPLE_TOKEN) == 0) {
								r = applyCoinBlockchainMessage(&mmm, data, &finalizeActionList);
							} else if (strcmp(sss->messageMetaType, MESSAGE_METATYPE_POW_TOKEN) == 0) {
								r = applyPowcoinBlockchainMessage(&mmm, data, &finalizeActionList);
							}
						}
						if (r < 0) {
							res = -1;
							bchainInvalidateMessageForNextBlocks(bbixc->tag[BIX_TAG_mid]);
						}
						// Hmm. commenting out the following line, so that we remove all wrong messages at once
						// This is because next block proposal will come late and removing one msg per cycle is too slow
						// if (r != 0) break;
					}
				}
			}
		},
		{
			PRINTF0("Internal error: wrongly formed message inside a block.\n");
			res = -1;
		}
		);
	bixMakeTagsSemicolonTerminating(bbixc);
	bixParsingContextFree(&bbixc);

	// apply pending actions to apply or undo block
	if (res == 0 && applyChangesFlag) {
		// block accepted, apply final touch actions
		SGLIB_LIST_REVERSE(struct bFinalActionList, finalizeActionList, next);
		SGLIB_LIST_MAP_ON_ELEMENTS(struct bFinalActionList, finalizeActionList, aa, next, {
				blockFinalizeAcceptForward(aa);
			});
		SGLIB_LIST_REVERSE(struct bFinalActionList, finalizeActionList, next);
		SGLIB_LIST_MAP_ON_ELEMENTS(struct bFinalActionList, finalizeActionList, aa, next, {
				blockFinalizeAcceptBackward(aa);
			});
	} else {
		// whole block rejected, undo all changes done in this block
		SGLIB_LIST_REVERSE(struct bFinalActionList, finalizeActionList, next);
		SGLIB_LIST_MAP_ON_ELEMENTS(struct bFinalActionList, finalizeActionList, aa, next, {
				blockFinalizeRejectForward(aa);
			});
		SGLIB_LIST_REVERSE(struct bFinalActionList, finalizeActionList, next);
		SGLIB_LIST_MAP_ON_ELEMENTS(struct bFinalActionList, finalizeActionList, aa, next, {
				blockFinalizeRejectBackward(aa);
			});
	}

	// free the whole list
	SGLIB_LIST_MAP_ON_ELEMENTS(struct bFinalActionList, finalizeActionList, aa, next, {
			blockFinalizeFreeItem(aa);
		});
	return(res);
}

int bchainVerifyBlockMessagesInBlockContent(struct blockContentDataList *data) {
	int r;
	r = bchainVerifyOrApplyBlockMessagesFromBlockContent(data, 0);
	return(r);
}

int bchainApplyBlockMessagesInBlockContent(struct blockContentDataList *data) {
	int r;
	r = bchainVerifyOrApplyBlockMessagesFromBlockContent(data, 1);
	return(r);
}

static void bchainErrorOnBlockContent(int errorCode) {
	switch (errorCode) {
	case BCIE_INTERNAL_ERROR_MISSING_BLOCK:
		PRINTF0("Internall error: block is missing in internal table.\n");
		break;
	case BCIE_INTERNAL_ERROR_MISSING_CONTENT:
		PRINTF0("Internall error: block content missing.\n");
		break;
	case BCIE_INTERNAL_ERROR_HASH_MISMATCH:
		PRINTF0("Internall error: block content hash mismatch.\n");
		break;
	case BCIE_ERROR_INVALID_MSG:
		PRINTF0("Error: a message/transaction inside the block was invalidated previously.\n");
		break;
	case BCIE_ERROR_INCONSISTENT_MSG:
		PRINTF0("Error: a message/transaction inside the block is invalid.\n");
		break;
	default:
		PRINTF0("Internall error: wrong error code.\n");
		break;
	}
}

int bchainVerifyBlockContent(struct blockContentDataList *data) {
	int 						res;
	struct jsVarDynamicString	*cc;
	char						hash64[MAX_HASH64_SIZE];

	res = bchainCheckThatWeHaveAllMessagesFromMsgidsInPendingList(data);
	// If we still do not have all messages nothig happens wait a bit until they come
	if (res != 0) return(1);

	res = bchainCheckValidityOfMessagesFromMsgids(data);
	if (res != 0) {
		bchainErrorOnBlockContent(BCIE_ERROR_INVALID_MSG);
		return(-1);
	}

	res = bchainCreateBlockContent(data);
	// Hmm. Here the situation is more serious and can not be fixed otherwise than abandoning the block.
	// TODO: Avoid further repeated checking of this block and exits or abandon block directly
	// PRINTF("bchainIsValidBlockContent: res %d bc %p\n", res, data->blockContent);
	if (res != 0) {
		bchainErrorOnBlockContent(BCIE_INTERNAL_ERROR_MISSING_BLOCK);
		return(-1);
	}
	if (data->blockContent == NULL) {
		bchainErrorOnBlockContent(BCIE_INTERNAL_ERROR_MISSING_CONTENT);
		return(-1);
	}
	// verify messages/transactions consistency
	res = bchainVerifyBlockMessagesInBlockContent(data);
	if (res != 0) {
		bchainErrorOnBlockContent(BCIE_ERROR_INCONSISTENT_MSG);
		return(-1);
	}
	bchainGetBlockContentHash64(data, hash64);
	// PRINTF("hashes %s <-> %s\n", data->blockContentHash, hash64);
	if (data->blockContent == NULL || strcmp(data->blockContentHash, hash64) != 0) {
		bchainErrorOnBlockContent(BCIE_INTERNAL_ERROR_HASH_MISMATCH);
		return(-1);
	}

	// the block is O.K.
	return(0);
}

void bchainOnNewBlockAccepted(struct blockContentDataList *data) {
	int64_t	serverBlockSequenceNumber;
	int		r, nodeNumber;


	if (data == NULL) {
		PRINTF("Internal Error: Block %"PRId64" %s accepted without data\n", currentEngagement->blockSequenceNumber, currentEngagement->blockId);
	} else {
		if (debuglevel > 5) PRINTF("Block accepted %s: data length %7d seq %"PRId64":\n", data->blockId, data->blockContentLength, data->blockSequenceNumber);
		if (debuglevel > 50) PRINTF("Data: %.*s\n", data->msgIdsLength, data->msgIds);
#if 0
		// Parsing blockId was not a good idea. It is used only for consensus and is not know when block is recovered from files.
		r = bchainBlockIdParse(data->blockId, NULL, &serverBlockSequenceNumber, &blockSequenceNumber, &nodeNumber);
		if (r < 0) {
			PRINTF("Internal error: problem when parsing block id %s\n", data->blockId);
		}
		if (nodeNumber == myNode) {
			// for correct recovery after total reset
			if (serverBlockSequenceNumber >= node->currentServerBlockSequenceNumber) node->currentServerBlockSequenceNumber = serverBlockSequenceNumber + 1;
		}
#endif
		if (data->blockSequenceNumber != node->currentBlockSequenceNumber) {
			PRINTF("Internal error: block %s out of order. Expected sequence number %"PRId64" got %"PRId64".\n", data->blockId, node->currentBlockSequenceNumber, data->blockSequenceNumber);
		}
		// not necessary this is important only when proposing new block 
		// if (serverBlockSequenceNumber >= node->currentServerBlockSequenceNumber) node->currentServerBlockSequenceNumber = serverBlockSequenceNumber + 1;		
		// Hmm. What to do if the block coming from resynchronization (or a new blcok signed by all servers) is not consistent?
		// Shall I forcibly accept it and apply messages or shall I insist on non-acceptance and stop advancing in this node?
		r = bchainApplyBlockMessagesInBlockContent(data);
		bchainRemoveMessagesAcceptedInBlockContentFromPendingList(data);
		if (r != 0) {
			PRINTF("Internal error: block %s was accepted by other servers while it contains inconsistent messages.\n", data->blockId);
			// stay stalled at this node.
			return;
		}
		bchainWriteBlockToFile(data);
		bchainWriteBlockSignaturesToFile(data);
		node->currentUsedSpaceBytes += data->blockContentLength;
		networkNodes[myNode].usedSpace = node->currentUsedSpaceBytes;
		strncpy(node->previousBlockHash, data->blockContentHash, sizeof(node->previousBlockHash));
		networkNodes[myNode].blockSeqn = data->blockSequenceNumber;
	}
	currentProposedBlock = NULL;
	node->currentBlockSequenceNumber ++;
	lastAcceptedBlockTime = currentTime.sec;
	bixBroadcastMyNodeStatusMessage();
	guiUpdateNodeStatus(myNode);
}

static void bchainGetBlockDirectoryAndFileAndSignatureName(int64_t blockSequenceNumber, char *dirname, char *fname, char *signame, int asize) {
	int n;
	if (dirname != NULL) {
		n = snprintf(dirname, asize, "%s/%s/%03"PRId64, PATH_TO_BLOCKCHAIN, mynodeidstring, blockSequenceNumber/1000);
		if (n >= asize) {
			PRINTF("Fatal error: path to blockchain is too long: %s. Exiting.\n", PATH_TO_BLOCKCHAIN);
			exit(-1);
		}
	}
	if (fname != NULL) {
		n = snprintf(fname, asize, "%s/%06"PRId64".blk", dirname, blockSequenceNumber);
		if (n >= asize) {
			PRINTF("Fatal error: path to blockchain is too long: %s. Exiting.\n", PATH_TO_BLOCKCHAIN);
			exit(-1);
		}
	}
	if (signame != NULL) {
		n = snprintf(signame, asize, "%s/%06"PRId64".sig", dirname, blockSequenceNumber);
		if (n >= asize) {
			PRINTF("Fatal error: path to blockchain is too long: %s. Exiting.\n", PATH_TO_BLOCKCHAIN);
			exit(-1);
		}
	}
}

static void bchainGetStatusDirectoryAndFileName(char *dirname, char *fname, char *tmpfname, int asize) {
	int n;
	if (dirname != NULL) {
		n = snprintf(dirname, asize, "%s/%s", PATH_TO_STATUS, mynodeidstring);
		if (n >= asize) {
			PRINTF("Fatal error: path to status is too long: %s. Exiting.\n", PATH_TO_STATUS);
			exit(-1);
		}
	}
	if (fname != NULL) {
		n = snprintf(fname, asize, "%s/status.dat", dirname);
		if (n >= asize) {
			PRINTF("Fatal error: path to status is too long: %s. Exiting.\n", PATH_TO_STATUS);
			exit(-1);
		}
	}
	if (tmpfname != NULL) {
		n = snprintf(tmpfname, asize, "%s/status.dat.tmp", dirname);
		if (n >= asize) {
			PRINTF("Fatal error: path to status is too long: %s. Exiting.\n", PATH_TO_STATUS);
			exit(-1);
		}
	}
}

void bchainWriteBlockToFile(struct blockContentDataList *data) {
	FILE 						*ff;
	char						dirname[TMP_STRING_SIZE];
	char						fname[TMP_STRING_SIZE];
	int							i, n;
	struct stat					st;


	assert(data->blockContent != NULL);
	bchainGetBlockDirectoryAndFileAndSignatureName(data->blockSequenceNumber, dirname, fname, NULL, TMP_STRING_SIZE);
	if (stat(dirname, &st) == -1) mkdirRecursivelyCreateDirectoryForFile(fname);

	ff = fopen(fname, "w");
	if (ff == NULL) {
		PRINTF("Error: Can't open block file for write: %s\n", fname);
		return;
	}
	i = fwrite(data->blockContent, 1, data->blockContentLength, ff);
	if (i < data->blockContentLength) {
		PRINTF0("Error: on writing block to file.\n");
		unlink(fname);
	}
	fclose(ff);
}


void bchainWriteBlockSignaturesToFile(struct blockContentDataList *data) {
	FILE 						*ff;
	char						dirname[TMP_STRING_SIZE];
	char						signame[TMP_STRING_SIZE];
	int							i, n;
	struct stat					st;


	assert(data->blockContent != NULL);
	bchainGetBlockDirectoryAndFileAndSignatureName(data->blockSequenceNumber, dirname, NULL, signame, TMP_STRING_SIZE);
	if (stat(dirname, &st) == -1) mkdirRecursivelyCreateDirectoryForFile(signame);

	if (data->signatures == NULL) {
		PRINTF("Error: No signatures to write: %s\n", signame);
		return;
	}
	n = strlen(data->signatures);
	ff = fopen(signame, "w");
	if (ff == NULL) {
		PRINTF("Error: Can't open block signatures file for write: %s\n", signame);
		return;
	}
	i = fwrite(data->signatures, 1, n, ff);
	if (i < n) {
		PRINTF0("Error: on writing block signatures to file.\n");
		unlink(signame);
	}
	fclose(ff);
}


struct blockContentDataList *bchainCreateBlockContentLoadedFromFile(int64_t blockSequenceNumber) {
	FILE 						*ff;
	char						dirname[TMP_STRING_SIZE];
	char						fname[TMP_STRING_SIZE];
	char						signame[TMP_STRING_SIZE];
	char						hash64[MAX_HASH64_SIZE];
	int							i, n, fsize;
	int							c;
	struct stat					st;
	struct blockContentDataList	*res;
	struct jsVarDynamicString	*cc, *ss;

	bchainGetBlockDirectoryAndFileAndSignatureName(blockSequenceNumber, dirname, fname, signame, TMP_STRING_SIZE);
	if (stat(fname, &st)) {
		PRINTF("Error: Can't stat blockchain file: %s, Probably missing file.\n", fname);
		return(NULL);
	}
	fsize = st.st_size;
	cc = jsVarDstrCreateByFileLoad(0, "%s", fname);
	if (cc == NULL) return(NULL);
	jsVarDstrTruncate(cc);
	ss = jsVarDstrCreateByFileLoad(0, "%s", signame);
#if 0
	if (ss == NULL) return(NULL);
#endif
	if (ss != NULL) jsVarDstrTruncate(ss);

	ALLOC(res, struct blockContentDataList);
	memset(res, 0, sizeof(*res));
	res->blockContentLength = cc->size;
	res->blockContent = jsVarDstrGetStringAndReinit(cc);
	if (ss == NULL) {
		res->signatures = NULL;
	} else {
		res->signatures = jsVarDstrGetStringAndReinit(ss);
	}
	res->blockSequenceNumber = blockSequenceNumber;
	bchainGetBlockContentHash64(res, hash64);
	res->blockContentHash = strDuplicate(hash64);
	jsVarDstrFree(&cc);
	jsVarDstrFree(&ss);
	return(res);
}

void bchainWriteGlobalStatus() {
	static char		lastWrittenStatus[sizeof(*node)] = {0};
	int				r;

	if (nodeFile == NULL) {
		PRINTF0("Internal Error: Can't write to node file. File not open.\n");
		return;
	}

	if (memcmp(lastWrittenStatus, node, sizeof(*node)) != 0) {
		rewind(nodeFile);
		r = fwrite(node, 1, sizeof(*node), nodeFile);
		if (r != sizeof(*node)) {
			PRINTF0("Error: while writting node file.\n");
		} else {
			memcpy(lastWrittenStatus, node, sizeof(*node));
		}
		fflush(nodeFile);
	}
}

static void bchainCreateGenesisBlock(void *d) {
	// TODO: Block zero creation

}

void bchainOpenAndLoadGlobalStatusFile() {
	char			dirname[TMP_STRING_SIZE];
	char			fname[TMP_STRING_SIZE];
	struct stat		st;
	FILE 			*ff;
	int				r;

	bchainGetStatusDirectoryAndFileName(dirname, fname, NULL, TMP_STRING_SIZE);
	if (stat(dirname, &st) == -1) mkdirRecursivelyCreateDirectoryForFile(fname);

	if (nodeFile != NULL) fclose(nodeFile);
	nodeFile = fopen(fname, "r");
	if (nodeFile == NULL) {
		PRINTF("Warning: Can't open status file for read: %s. Initializing to default values.\n", fname);
		node->statusFileVersion = BCHAIN_STATUS_FILE_VERSION;
		node->index = myNode;
		node->currentBlockSequenceNumber = 0;
		memset(node->previousBlockHash, 0, sizeof(MAX_HASH64_SIZE));
		node->currentBixSequenceNumber = 1;
		node->currentMsgSequenceNumber = 1;
		node->currentServerBlockSequenceNumber = 1;
		node->currentNewAccountNumber = 1;
		node->currentUsedSpaceBytes = 0;
		node->currentTotalSpaceAvailableBytes = INITIAL_BLOCKCHAIN_SPACE_AVAILABLE;
		node->magic = BCHAIN_STATUS_FILE_MAGIC;
		timeLineInsertEvent(UTIME_AFTER_MSEC(rand() % 100), bchainCreateGenesisBlock, NULL);
		// create node's bytecoin account
		
	} else {
		r = fread(node, 1, sizeof(*node), nodeFile);
		if (r < sizeof(*node)) {
			PRINTF("Error: while reading current status file: %s.\n", strerror(errno));
		}
		fclose(nodeFile);
		// Eventually make backward compatibility conversions
		switch(node->statusFileVersion) {
		case 0x01:
			break;
		default:
			PRINTF("Internal error: unknown status file version. Possible version mismatch: %s. Exiting.\n", fname);
			exit(-1);
		}
		if (node->magic != BCHAIN_STATUS_FILE_MAGIC) {
			PRINTF("Internal error: Wrong node status format. Possible version mismatch: %s. Exiting.\n", fname);
			exit(-1);
		}
	}
	PRINTF0("Starting in normal state.\n");
	changeNodeStatus(NODE_STATE_NORMAL);
	nodeFile = fopen(fname, "w");
	if (nodeFile == NULL) {
		PRINTF("Error: Can't open status file for write: %s\n", fname);
	}
	bchainWriteGlobalStatus();
}

void bchainStartResynchronization(void *d) {
	int 			i, j;
	struct bcaio	*cc;

	if (nodeState == NODE_STATE_RESYNC) return;

	PRINTF0("Warning: Switching to resynchronization mode.\n");

	changeNodeStatus(NODE_STATE_RESYNC);
	cc = NULL;
	for(i=1; i<=coreServersMax; i++) {
		j = (nodeResyncFromNode + i) % coreServersMax;
		cc = (struct bcaio *) baioFromMagic(coreServersConnections[j].activeConnectionBaioMagic);
		if (cc != NULL) break;
		cc = (struct bcaio *) baioFromMagic(coreServersConnections[j].passiveConnectionBaioMagic);
		if (cc != NULL) break;
	}
	if (cc == NULL) {
		nodeResyncFromNode = (nodeResyncFromNode + 1) % coreServersMax;
		return;
	}
	nodeResyncFromNode = j;
	bixSendResynchronizationRequestToConnection(cc);
	lastResynchronizationActionTime = currentTime.sec;
}

void bchainServeResynchronizationRequests() {
	struct blockContentDataList 				*bb;
	int											i, deleteFlag;
	int64_t										seq;
	struct bchainResynchronizationRequestList	*rr, **rra, **rranext;
	struct bcaio								*cc;

	rra = &bchainResynchronizationRequest; 
	while (*rra != NULL) {
		deleteFlag = 1;
		rr = *rra;
		cc = (struct bcaio *) baioFromMagic(rr->reqBaioMagic);
		if (cc != NULL) {
			// send a few blocks at once (make attention not to overflow writting buffers!)
			for(i=0; i<2; i++) {
				seq = rr->fromSeqn;
				if (seq < rr->untilSeqn) {
					deleteFlag = 0;
					if (seq >= 0 && seq < node->currentBlockSequenceNumber) {
						bb = bchainCreateBlockContentLoadedFromFile(seq);
						if (bb == NULL) {
							// Hmm. we do not have the block? 
							PRINTF("bchainServeResynchronizationRequests: Internal error: missing block %"PRId64".\n", seq);
							deleteFlag = 1;
						} else {
							if (debuglevel > 35) PRINTF("Sending resynchronization block %"PRId64" to %d\n", seq, rr->reqNode);
							bixSendBlockResynchronizationAnswerToConnection(cc, bb);
							rr->fromSeqn = seq + 1;
							bchainBlockContentFree(&bb);
						}
					}
				}
			}
		}
		if (deleteFlag) {
			*rra = (*rra)->next;
			FREE(rr);
		} else {
			rra = &(*rra)->next;
		}
	}
}

void bchainRegularStatusWrite(void *d) {
    bchainWriteGlobalStatus();
    timeLineInsertEvent(UTIME_AFTER_SECONDS(1), bchainRegularStatusWrite, NULL);
}

void ssBchainRegularSendSynchronizationRequests(void *d) {
    struct bcaio *cc;
    cc = (struct bcaio *)baioFromMagic(secondaryToCoreServerConnectionMagic);
    if (cc != NULL) {
        nodeResyncUntilSeqNum = node->currentBlockSequenceNumber + 9999999999LL;
        bixSendResynchronizationRequestToConnection(cc);
    }
    timeLineInsertEvent(UTIME_AFTER_SECONDS(60), ssBchainRegularSendSynchronizationRequests, NULL);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void ssUpdateTokenList(void *d) {
	ssSendUpdateTokenListRequest(NULL, NULL);
}

void ssRegularUpdateTokenList(void *d) {
	ssUpdateTokenList(d);
	// TODO: put there later delay for production version
	timeLineRescheduleUniqueEvent(UTIME_AFTER_SECONDS(10), ssRegularUpdateTokenList, NULL);
}


/////////////////////////////////////////////////////////////////////////////////////////////////
// openssl stuff (from wiki.openssl.org)

int msgSignatureCreate(char *msg, int msglen, char **sig, size_t *slen, EVP_PKEY* key) {
	int ret;
 
	openSslCryptoInit();

	*sig = NULL;
	ret = -1;
 
	if(openSslMdctx == NULL) goto err;
 
	if(1 != EVP_DigestSignInit(openSslMdctx, NULL, EVP_sha512(), NULL, key)) goto err;
	if(1 != EVP_DigestSignUpdate(openSslMdctx, msg, msglen)) goto err;
	if(1 != EVP_DigestSignFinal(openSslMdctx, NULL, slen)) goto err;
	ALLOCC(*sig, *slen, char);
	if(1 != EVP_DigestSignFinal(openSslMdctx, *sig, slen)) goto err;
 
	/* Success */
	ret = 0;
 
err:
	if(ret != 0) {
		/* Do some error handling */
	}
 
	/* Clean up */
	if(*sig && ret) FREE(*sig);
	return(ret);
}

int msgSignatureVerify(char *msg, int msglen, char *sig, int slen, EVP_PKEY* key) {
	int ret;

	openSslCryptoInit();
	ret = -1;

	if (openSslMdctx == NULL) goto err;
 	if (1 != EVP_DigestVerifyInit(openSslMdctx, NULL, EVP_sha512(), NULL, key)) goto err;
	if (1 != EVP_DigestVerifyUpdate(openSslMdctx, msg, msglen)) goto err;

	if (1 == EVP_DigestVerifyFinal(openSslMdctx, sig, slen)) {
		/* Success */
		ret = 0;
	} else {
		/* Failure */
		ret = -1;
	}

err:
 	/* Clean up */
	return(ret);	
}

EVP_PKEY *readPrivateKeyFromFile(char *fmt, ...) {
	FILE 		*ff;
	EVP_PKEY 	*res;
	char		fname[TMP_FILE_NAME_SIZE];
	va_list 	arg_ptr;
	int			r;

	va_start(arg_ptr, fmt);
	r = vsnprintf(fname, sizeof(fname), fmt, arg_ptr);
	if (r < 0 || r >= TMP_FILE_NAME_SIZE) {
		strcpy(fname+TMP_FILE_NAME_SIZE-4, "...");
		PRINTF("Error: File name too long %s\n", fname);
		return(NULL);
	}
	va_end(arg_ptr);


	ff = fopen(fname, "r");
	if (ff == NULL) {
		PRINTF("Can't open file %s\n", fname);
		return(NULL);
	}
	res = EVP_PKEY_new();
	res = PEM_read_PrivateKey(ff, &res, NULL, NULL);
	fclose(ff);
	return(res);
}

EVP_PKEY *readPublicKeyFromFile(char *fmt, ...) {
	FILE 		*ff;
	EVP_PKEY 	*res;
	char		fname[TMP_FILE_NAME_SIZE];
	va_list 	arg_ptr;
	int			r;

	va_start(arg_ptr, fmt);
	r = vsnprintf(fname, sizeof(fname), fmt, arg_ptr);
	if (r < 0 || r >= TMP_FILE_NAME_SIZE) {
		strcpy(fname+TMP_FILE_NAME_SIZE-4, "...");
		PRINTF("Error: File name too long %s\n", fname);
		return(NULL);
	}
	va_end(arg_ptr);


	ff = fopen(fname, "r");
	if (ff == NULL) {
		PRINTF("Can't open file %s\n", fname);
		return(NULL);
	}
	res = EVP_PKEY_new();
	res = PEM_read_PUBKEY(ff, &res, NULL, NULL);
	fclose(ff);
	return(res);
}

EVP_PKEY *readPrivateKeyFromString(char *s) {
	BIO			*bio;
	EVP_PKEY 	*res;

	if (s == NULL) return(NULL);
	bio = BIO_new_mem_buf(s, -1);
	res = EVP_PKEY_new();
	res = PEM_read_bio_PrivateKey(bio, &res, NULL, NULL);
	BIO_free(bio);
	return(res);
}

EVP_PKEY *readPublicKeyFromString(char *s) {
	BIO			*bio;
	EVP_PKEY 	*res;

	if (s == NULL) return(NULL);
	bio = BIO_new_mem_buf(s, -1);
	res = EVP_PKEY_new();
	res = PEM_read_bio_PUBKEY(bio, &res, NULL, NULL);
	BIO_free(bio);
	return(res);
}

char *getPublicKeyAsString(EVP_PKEY *pkey) {
	BIO		*bio;
	char	*res;
	BUF_MEM *bptr;
	int		r;

	bio = BIO_new(BIO_s_mem());
	r = PEM_write_bio_PUBKEY(bio, pkey);
	if (r == 0) return(NULL);
	BIO_get_mem_ptr(bio, &bptr);
	res = memDuplicate(bptr->data, bptr->length);
	BIO_free(bio);
	return(res);
}

char *getPrivateKeyAsString(EVP_PKEY *pkey) {
	BIO		*bio;
	char	*res;
	BUF_MEM *bptr;
	int		r;

	bio = BIO_new(BIO_s_mem());
	r = PEM_write_bio_PrivateKey(bio, pkey, NULL, NULL, 0, 0, NULL);
	if (r == 0) return(NULL);
	BIO_get_mem_ptr(bio, &bptr);
	res = memDuplicate(bptr->data, bptr->length);
	BIO_free(bio);
	return(res);
}


/////////////////////////////////////////////////////////////////////////////////////

EVP_PKEY *generateNewEllipticKey() {
	int r, group;
	EVP_PKEY *pkey;
	EC_KEY *eckey;
	openSslCryptoInit();

	group = OBJ_txt2nid("secp521r1");
	// group = OBJ_txt2nid("secp384r1");
	eckey = EC_KEY_new_by_curve_name(group);
	EC_KEY_set_asn1_flag(eckey, OPENSSL_EC_NAMED_CURVE);

	r = EC_KEY_generate_key(eckey);
	if (r == 0) return(NULL);
		
	pkey=EVP_PKEY_new();
	r = EVP_PKEY_assign_EC_KEY(pkey,eckey);
	if (r == 0) return(NULL);
	return(pkey);
}

int generatePrivateAndPublicKeysToFiles(char *publicKeyFile, char *privateKeyFile) {
	BIO			*bio;
	EC_KEY		*eckey;
	EVP_PKEY	*pkey;
	int			group;
	int			r;

	pkey = generateNewEllipticKey();
	if (pkey == NULL) return(-1);

	bio = BIO_new_file(publicKeyFile, "w");
	r = PEM_write_bio_PUBKEY(bio, pkey);
	if (r == 0) return(-1);
	BIO_free_all(bio);

	bio = BIO_new_file(privateKeyFile, "w");
	r = PEM_write_bio_PrivateKey(bio, pkey, NULL, NULL, 0, 0, NULL);
	if (r == 0) return(-1);
	BIO_free_all(bio);

	EVP_PKEY_free(pkey);
	// EC_KEY_free(eckey); not necessay, freed with parent pkey
	return(0);
}


////////////////////////////////////////////////////////////////////////////////////

int encryptWithAesCtr256(unsigned char *plaintext, int plaintext_len, unsigned char *key, unsigned char *iv, unsigned char *ciphertext) {
  EVP_CIPHER_CTX 		*ctx;
  int 					len;
  int 					ciphertext_len;

  openSslCryptoInit();

  ciphertext_len = -1;

  /* Create and initialise the context */
  if(!(ctx = EVP_CIPHER_CTX_new())) goto finito;

  /* Initialise the encryption operation. IMPORTANT - ensure you use a key
   * and IV size appropriate for your cipher
   * In this example we are using 256 bit AES (i.e. a 256 bit key). The
   * IV size for *most* modes is the same as the block size. For AES this
   * is 128 bits */

  if(1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_ctr(), NULL, key, iv)) goto finito;

  /* Provide the message to be encrypted, and obtain the encrypted output.
   * EVP_EncryptUpdate can be called multiple times if necessary
   */
  if(1 != EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len)) goto finito;
  ciphertext_len = len;

  /* Finalise the encryption. Further ciphertext bytes may be written at
   * this stage.
   */
  if(1 != EVP_EncryptFinal_ex(ctx, ciphertext + len, &len)) goto finito;
  ciphertext_len += len;

finito:

  /* Clean up */
  if (ctx != NULL) {
	  EVP_CIPHER_CTX_free(ctx);
  }

  return ciphertext_len;
}

int decryptWithAesCtr256(unsigned char *ciphertext, int ciphertext_len, unsigned char *key, unsigned char *iv, unsigned char *plaintext) {
  EVP_CIPHER_CTX *ctx;
  int len;
  int plaintext_len;

  openSslCryptoInit();

  plaintext_len = -1;
  /* Create and initialise the context */
  if(!(ctx = EVP_CIPHER_CTX_new())) goto finito;

  /* Initialise the decryption operation. IMPORTANT - ensure you use a key
   * and IV size appropriate for your cipher
   * In this example we are using 256 bit AES (i.e. a 256 bit key). The
   * IV size for *most* modes is the same as the block size. For AES this
   * is 128 bits */
  if(1 != EVP_DecryptInit_ex(ctx, EVP_aes_256_ctr(), NULL, key, iv)) goto finito;

  /* Provide the message to be decrypted, and obtain the plaintext output.
   * EVP_DecryptUpdate can be called multiple times if necessary
   */
  if(1 != EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len)) goto finito;

  plaintext_len = len;

  /* Finalise the decryption. Further plaintext bytes may be written at
   * this stage.
   */
  if(1 != EVP_DecryptFinal_ex(ctx, plaintext + len, &len)) goto finito;
  plaintext_len += len;

finito:
  /* Clean up */
  if (ctx != NULL) EVP_CIPHER_CTX_free(ctx);

  return plaintext_len;
}

////////////////////////////////////////////////////////////////////////////////////

char *signatureCreateBase64(char *msgtosign, int msgToSignLen, char *privateKey) {
	EVP_PKEY 					*pkey;
	size_t						slen;
	int 						r;
	char 						*sigbin, *signature;
	struct jsVarDynamicString 	*tt;

	pkey = readPrivateKeyFromString(privateKey);
	if (pkey == NULL) {
		PRINTF("Error: Wrong private key %.35s ...\n", privateKey);
		return(NULL);
	}
	r = msgSignatureCreate(msgtosign, msgToSignLen, &sigbin, &slen, pkey);
	if (r != 0) {
		if (msgToSignLen < 40) {
			PRINTF("Error: Can't sign %s\n", msgtosign);
		} else {
			PRINTF("Error: Can't sign %.40s...\n", msgtosign);
		}
		EVP_PKEY_free(pkey);
		return(NULL);
	}
	ALLOCC(signature, slen*2, char);
	r = base64_encode(sigbin, slen, signature, TMP_STRING_SIZE);
	if (r < 0) {
		PRINTF0("Internall Error: Signature is too long.\n");
		FREE(signature);
		signature = NULL;
	}
	FREE(sigbin);
	EVP_PKEY_free(pkey);
	return(signature);
}

int secondaryServerSignAndSendMessageToCoreServer(char *dstpath, int insertProcessFlag, struct jsVaraio *jj, int secondaryServerInfo, char *msgtosign, int msgToSignLen, char *messageDataType, char *ownerAccount, char *privateKey) {
	EVP_PKEY 					*pkey;
	int 						slen, r;
	char 						*sigbin, *signature;
	struct jsVarDynamicString 	*tt;

	signature = signatureCreateBase64(msgtosign, msgToSignLen, privateKey);
	if (signature == NULL) {
		jsVarSendEval(jj, "guiNotify('%s: Error: Can\\'t sign the message. Not logged in or wrong private key.\\n');", SPRINT_CURRENT_TIME());
		return(-1);
	}
	tt = jsVarDstrCreateFromCharPtr(msgtosign, msgToSignLen);
	r = bixSendNewMessageInsertOrProcessRequest_s2c(dstpath, insertProcessFlag, secondaryToCoreServerConnectionMagic, secondaryServerInfo, tt->s, tt->size, messageDataType, ownerAccount, signature);
	// PRINTF("Inserting signed msg %s\n", tt->s);
	FREE(signature);
	return(r);
}

int verifyMessageSignature(char *msg, int msgsize, char *coin, char *account, char *signature, int signatureLen, int quietFlag) {
	int 					siglen, r, res;
	EVP_PKEY 				*pkey;
	char 					sig[TMP_STRING_SIZE];
	struct bAccountsTree 	*acc;

	res = -999;

	// PRINTF("VERIFYING %s %s SIGNATURE %.*s of %.*s\n", coin, account, signatureLen, signature, msgsize, msg); quietFlag = 0;

	if (coin == NULL) return(-2);
	if (msg == NULL) return(-3);
	if (signature == NULL || signature[0] == 0) return(-4);

	siglen = base64_decode(signature, signatureLen, sig, TMP_STRING_SIZE);
	if (siglen < 0) {
		if (quietFlag == 0) PRINTF0("Internal error: verifyMessageSignature: can't decode signature\n");
		res = -5;
	} else {
		// acc = bAccountLoadOrFind(coin, account, &currentBchainContext);
		acc = bAccountLoadOrFind(account, &currentBchainContext);
		if (acc == NULL) {
			if (quietFlag == 0) PRINTF0("Internal error: verifyMessageSignature: message signed by non-existant account\n");
			res = -6;
		} else {
			pkey = readPublicKeyFromString(acc->d.publicKey);
			if (pkey == NULL) {
				if (quietFlag == 0) PRINTF0("Internal error: verifyMessageSignature: can't read key public key\n");
				res = -7;
			} else {
				res = msgSignatureVerify(msg, msgsize, sig, siglen, pkey);
				EVP_PKEY_free(pkey);
			}
		}
	}
	return(res);
}

void setSignatureVerificationFlagInMessage(struct bchainMessage *msg) {
	int 					r;
	char					*coin;

	coin = messageDataTypeToAccountCoin(msg->dataType);
	r = verifyMessageSignature(msg->data, msg->dataLength, coin, msg->account, msg->signature, strlen(msg->signature), 1);
	if (r >= 0) {
		msg->signatureVerifiedFlag = 1;
	} else {
		msg->signatureVerifiedFlag = 0;
	}
}
