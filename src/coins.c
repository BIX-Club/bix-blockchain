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


static int currentWalletToCoreRequestId = 1;

static int bAccBalComparator(struct bAccountBalance *t1, struct bAccountBalance *t2) {
	int r;
	r = strcmp(t1->coin, t2->coin);
	if (r != 0) return(r);
	return(0);
}

typedef struct bAccountBalance bAccBal;
SGLIB_DEFINE_RBTREE_PROTOTYPES(bAccBal, leftSubtree, rightSubtree, color, bAccBalComparator);
SGLIB_DEFINE_RBTREE_FUNCTIONS(bAccBal, leftSubtree, rightSubtree, color, bAccBalComparator);

static int bAccTreeComparator(struct bAccountsTree *t1, struct bAccountsTree *t2) {
	int r;
	r = strcmp(t1->account, t2->account);
	if (r != 0) return(r);
	//r = strcmp(t1->coin, t2->coin);
	//if (r != 0) return(r);
	return(0);
}

typedef struct bAccountsTree bAccTree;
SGLIB_DEFINE_RBTREE_PROTOTYPES(bAccTree, leftSubtree, rightSubtree, color, bAccTreeComparator);
SGLIB_DEFINE_RBTREE_FUNCTIONS(bAccTree, leftSubtree, rightSubtree, color, bAccTreeComparator);

//////////////////////////////////////////////////////////////////////////////////////////

static int powcoinListMsgCmp(struct powcoinBlockPendingMessagesListStr *m1, struct powcoinBlockPendingMessagesListStr *m2) {
	int r;
	// PRINTF("checking %s <-> %s\n", m1->msg.msgId, m2->msg.msgId);
	r = strcmp(m1->msg.msgId, m2->msg.msgId);
	if (r != 0) return(r);
	r = strcmp(m1->msg.account, m2->msg.account);
	if (r != 0) return(r);
	r = strcmp(m1->msg.dataType, m2->msg.dataType);
	if (r != 0) return(r);
	r = strcmp(m1->msg.signature, m2->msg.signature);
	if (r != 0) return(r);
	r = m1->msg.dataLength - m2->msg.dataLength;
	if (r != 0) return(r);
	r = memcmp(m1->msg.data, m2->msg.data, m1->msg.dataLength);
	if (r != 0) return(r);
	return(r);
}

//////////////////////////////////////////////////////////////////////////////////////////

static void bchainAccountCreateApprovePrettyPrint(char *res, struct bixParsingContext *bix, char *notePrefix) {
	snprintf(res, LONG_LONG_TMP_STRING_SIZE, "%s %s in %s", 
			 notePrefix,
			 bix->tag[BIX_TAG_acc],
			 bix->tag[BIX_TAG_mdt]
		);
}

char *bchainPrettyPrintMetaMsgContent(struct bixParsingContext *bix) {
	char *res;
	res = staticStringRingGetLongLongTemporaryStringPtr_st();
	snprintf(res, LONG_LONG_TMP_STRING_SIZE, "%s", bix->tag[BIX_TAG_mvl]);
	return(res);
}

char *bchainPrettyPrintUserFileMsgContent(struct bixParsingContext *bix) {
	char *res;
	res = staticStringRingGetLongLongTemporaryStringPtr_st();
	snprintf(res, LONG_LONG_TMP_STRING_SIZE, "upload file %s: %d bytes", bix->tag[BIX_TAG_fnm], bix->tagLen[BIX_TAG_dat]);
	return(res);
}

char *bchainPrettyPrintExchangeMsgContent(struct bixParsingContext *bix) {
	char 			*res;
	char			*mvl;
	char			*opt;
	struct kycData	*kyc;

	res = staticStringRingGetLongLongTemporaryStringPtr_st();
	mvl = bix->tag[BIX_TAG_mvl];
	if (strcmp(mvl, "NewLimitOrder") == 0) {
		// mvl=NewLimitOrder;acc=%s;btk=%s;qtk=%s;sid=%s;odt=%s;qty=%a;prc=%a;non=%"PRIu64";
		// mvl=NewLimitOrder;acc=xfc;btk=BIX;qtk=xfc;sid=Buy;odt=Limit;qty=0x1p+0;prc=0x1p+0;non=1528883951059741;;
		snprintf(res, LONG_LONG_TMP_STRING_SIZE, "New Limit Order: %s %f * %s/%s @ %f", bix->tag[BIX_TAG_sid], strtod(bix->tag[BIX_TAG_qty], NULL), bix->tag[BIX_TAG_btk], bix->tag[BIX_TAG_qtk], strtod(bix->tag[BIX_TAG_prc], NULL));
	} else {
		snprintf(res, LONG_LONG_TMP_STRING_SIZE, "%s ???", bix->tag[BIX_TAG_mvl]);
	}
	return(res);
}

char *bchainPrettyPrintTokenMsgContent(struct bixParsingContext *bix) {
	char 			*res;
	char			*mvl;
	char			*opt;
	struct kycData	*kyc;

	res = staticStringRingGetLongLongTemporaryStringPtr_st();
	mvl = bix->tag[BIX_TAG_mvl];
	if (strcmp(mvl, "NewAccountRequest") == 0) {
		bchainAccountCreateApprovePrettyPrint(res, bix, "Create account request: ");
	} else if (strcmp(mvl, "NewAccountApproval") == 0) {
		bchainAccountCreateApprovePrettyPrint(res, bix, "Account approval: ");
	} else if (strcmp(mvl, "SendCoins") == 0) {
		snprintf(res, LONG_LONG_TMP_STRING_SIZE, 
				 "Send %f %s from %s to %s", 
				 strtod(bix->tag[BIX_TAG_amt], NULL),
				 bix->tag[BIX_TAG_tok],
				 bix->tag[BIX_TAG_fra],
				 bix->tag[BIX_TAG_toa]
			);
	} else if (strcmp(mvl, "CreateNewCoin") == 0) {
		snprintf(res, LONG_LONG_TMP_STRING_SIZE, 
				 "Create new token %s: %s", 
				 bix->tag[BIX_TAG_cnm],
				 bix->tag[BIX_TAG_ctp]
			);
	} else if (strcmp(mvl, "CoinOptions") == 0) {
		opt = bix->tag[BIX_TAG_opt];
		snprintf(res, LONG_LONG_TMP_STRING_SIZE, 
				 "Token %s parameters:  %s", 
				 bix->tag[BIX_TAG_cnm],
				 opt
			);
	} else if (strcmp(mvl, "Mining") == 0) {
		snprintf(res, LONG_LONG_TMP_STRING_SIZE, 
				 "BIX mining of %f to core servers", 
				 strtod(bix->tag[BIX_TAG_amt], NULL)
			);
	} else if (strcmp(mvl, "PowMinedBlock") == 0) {
		snprintf(res, LONG_LONG_TMP_STRING_SIZE, 
				 "Proof of work mined block"
			);
	} else {
		snprintf(res, LONG_LONG_TMP_STRING_SIZE, "%s", bix->tag[BIX_TAG_mvl]);
	}
	return(res);
}

char *bchainPrettyPrintMsgContent_st(struct bchainMessage *msg) {
	char						*res;
	struct bixParsingContext	*bix;
	struct supportedMessageStr 	*sss;
	int							r;

	bix = bixParsingContextGet();
	r = bixParseString(bix, msg->data, msg->dataLength);
	if (r != 0) {
		res = "Wrongly formed message.";
		goto finito2;
	}
	bixMakeTagsNullTerminating(bix);

	sss = coreServerSupportedMessageFind(msg->dataType);
	if (sss == NULL) {
		res = "Unknown message type / token.";
		goto finito;
	}

	if (strcmp(sss->messageMetaType, MESSAGE_METATYPE_META_MSG) == 0) {
		res = bchainPrettyPrintMetaMsgContent(bix);
	} else 	if (strcmp(sss->messageMetaType, MESSAGE_METATYPE_USER_FILE) == 0) {
		res = bchainPrettyPrintUserFileMsgContent(bix);
	} else 	if (strcmp(sss->messageMetaType, MESSAGE_METATYPE_EXCHANGE) == 0) {
		res = bchainPrettyPrintExchangeMsgContent(bix);
	} else 	if (strcmp(sss->messageMetaType, MESSAGE_METATYPE_SIMPLE_TOKEN) == 0) {
		res = bchainPrettyPrintTokenMsgContent(bix);
	} else 	if (strcmp(sss->messageMetaType, MESSAGE_METATYPE_POW_TOKEN) == 0) {
		res = bchainPrettyPrintTokenMsgContent(bix);
	} else {
		res = "Unknown message metatype.";
	}

finito:
	bixMakeTagsSemicolonTerminating(bix);
finito2:
	bixParsingContextFree(&bix);
	return(res);
}

//////////////////////////////////////////////////////////////////////////////////////////
// signed messages

static uint64_t genMsgNonce() {
	uint64_t		res;
	// maybe find out something more sophisticated to ensure monotonicity
	res = (uint64_t)currentTime.usec;
	if (node->lastNonce != 0 && res <= node->lastNonce) res = node->lastNonce + 1;
	node->lastNonce = res;
	return(res);
}

//////////////////////////////////////////////////////////////////////////////////////////

char *bAccountDirectory_st(char *account) {
	char *res;
	res = staticStringRingGetTemporaryStringPtr_st();
	assert(account != NULL);
	sprintf(res, "%s/%s/%s", PATH_TO_ACCOUNTS_DIRECTORY, mynodeidstring, account);
	return(res);
}

char *bAccountNotesDirectory_st(char *coin, char *account) {
	char *res;
	res = staticStringRingGetTemporaryStringPtr_st();
	sprintf(res, "%s/notes", bAccountDirectory_st(account));
	return(res);	
}

enum accountGetSpecialFlagEnum {
	ASF_NONE,
	ASF_EXISTING_ACCOUNT_ONLY,
	ASF_CREATE_IF_NOT_EXISTS,
	// ASF_CREATE_MULTICURRENCY_IF_EXISTS,
	ASF_MAX,
};

static char *bAccountBalanceFileName_st(char *account, char *coin) {
	char	*res;
	int		r;

	res = staticStringRingGetTemporaryStringPtr_st();
	r = snprintf(res, TMP_STRING_SIZE, "%s/balance-%s.txt", bAccountDirectory_st(account), coin);
	if (r <= 0 || r >= TMP_STRING_SIZE) return("");
	return(res);
}

static struct bAccountBalance *bAccountLoadOrCreateNewBalance(struct bAccountsTree *memb, char *coin) {
	struct bAccountBalance 	*res;
	char 					*sbalance;

	if (checkFileAndTokenName(coin, 0) < 0) return(NULL);
	sbalance = fileLoad(0, "%s", bAccountBalanceFileName_st(memb->account, coin));
	ALLOC(res, struct bAccountBalance);
	memset(res, 0, sizeof(*res));
	res->coin = strDuplicate(coin);
	res->myAccount = memb;
	if (sbalance == NULL) {
		res->balance = 0;
	} else {
		res->balance = strtod(sbalance, NULL);
		FREE(sbalance);
	}
	sglib_bAccBal_add(&memb->balances, res);
	return(res);
}

static void bAccountLoadAllBalances(struct bAccountsTree *memb, struct bchainContext *context) {
	int 					i;
	struct stat				st;
	char					*coin;

	// TODO: Do this more efficiently
	if (memb->allBalancesLoadedFlag) return;
	for(i=0; i<supportedMessagesTabMaxi; i++) {
		if (strcmp(supportedMessagesTab[i].messageType, MESSAGE_DATATYPE_LAST_NON_COIN) == 0) break;
	}
	i++;
	for(; i<supportedMessagesTabMaxi; i++) {
		coin = supportedMessagesTab[i].messageType;
		if (stat(bAccountBalanceFileName_st(memb->account, coin), &st) == 0 && (st.st_mode & S_IFMT) == S_IFREG) {
			bAccountBalanceLoadOrFind(coin, memb->account, context);
		}
	}
	memb->allBalancesLoadedFlag = 1;
}


static struct bAccountsTree *bAccountGet(char *account, struct bchainContext *context, int accountGetSpecialFlag) {
	char 					*pubkey;
	struct bAccountsTree	aaa, *memb;

	if (account == NULL || account[0] == 0) return(NULL);
	aaa.account = account;
	memb = sglib_bAccTree_find_member(context->accs, &aaa);
	if (memb == NULL) {
		pubkey = fileLoad(0, "%s/%s", bAccountDirectory_st(account), WALLET_PUBLIC_KEY_FILE);
		if (pubkey == NULL && accountGetSpecialFlag == ASF_EXISTING_ACCOUNT_ONLY) return(NULL);
		ALLOC(memb, struct bAccountsTree);
		memset(memb, 0, sizeof(*memb));
		memb->account = strDuplicate(account);
		memb->allBalancesLoadedFlag = 0;
		if (pubkey == NULL) {
			memb->d.publicKey = strDuplicate("unknown-key");
		} else {
			memb->d.publicKey = pubkey;
		}
		sglib_bAccTree_add(&context->accs, memb);
	}
	memb->d.lastUsedTimeSec = currentTime.sec;
	return(memb);
}

static struct bAccountBalance *bAccountGetBalance(char *coin, char *account, struct bchainContext *context, int accountGetSpecialFlag) {
	char 					*sbalance;
	struct bAccountsTree	*memb;
	struct bAccountBalance	rrr, *res;

	memb = bAccountGet(account, context, accountGetSpecialFlag);
	if (memb == NULL) return(NULL);
	rrr.coin = coin;
	res = sglib_bAccBal_find_member(memb->balances, &rrr);
	if (res == NULL) res = bAccountLoadOrCreateNewBalance(memb, coin);
	return(res);
}

struct bAccountBalance *bAccountBalanceLoadFindOrCreateMulticurrency(char *coin, char *account, struct bchainContext *context) {
	// return(bAccountGet(coin, account, context, ASF_CREATE_MULTICURRENCY_IF_EXISTS));
	return(bAccountGetBalance(coin, account, context, ASF_CREATE_IF_NOT_EXISTS));
}

struct bAccountBalance *bAccountBalanceLoadFindOrCreate(char *coin, char *account, struct bchainContext *context) {
	return(bAccountGetBalance(coin, account, context, ASF_CREATE_IF_NOT_EXISTS));
}

struct bAccountBalance *bAccountBalanceLoadOrFind(char *coin, char *account, struct bchainContext *context) {
	return(bAccountGetBalance(coin, account, context, ASF_EXISTING_ACCOUNT_ONLY));
}

struct bAccountsTree *bAccountLoadOrFind(char *account, struct bchainContext *context) {
	return(bAccountGet(account, context, ASF_EXISTING_ACCOUNT_ONLY));
}

struct bAccountsTree *bAccountLoadFindOrCreate(char *account, struct bchainContext *context) {
	return(bAccountGet(account, context, ASF_CREATE_IF_NOT_EXISTS));
}

void bAccountSave(struct bAccountsTree *aa) {
	char			ttt[TMP_STRING_SIZE];
	char			ddd[TMP_STRING_SIZE];
	struct stat		st;
	char			*dir;

	if (aa == NULL) return;
	dir = bAccountDirectory_st(aa->account);
	if (stat(dir, &st) == -1) mkdirRecursivelyCreateDirectory(dir);
	//snprintf(ttt, sizeof(ttt), "%s", aa->coin);
	//fileSave(ttt, "%s/token.txt", bAccountDirectory_st(aa->account));
	//snprintf(ttt, sizeof(ttt), "%a\n", aa->d.balance);
	//fileSave(ttt, "%s/balance-%s.txt", bAccountDirectory_st(aa->account), aa->coin);
	snprintf(ttt, sizeof(ttt), "%d\n", aa->d.approved);
	fileSave(ttt, "%s/approved.txt", bAccountDirectory_st(aa->account));
	fileSave(aa->d.publicKey, "%s/%s", bAccountDirectory_st(aa->account), WALLET_PUBLIC_KEY_FILE);
	// also save additional index
	// accounts per currency directory
#if 0
	snprintf(ddd, sizeof(ddd), "%s/%s/%s/%s", PATH_TO_ACCOUNTS_PER_TOKEN_DIRECTORY, mynodeidstring, aa->coin, aa->account);
	if (stat(ddd, &st) == -1) mkdirRecursivelyCreateDirectory(ddd);
	fileSave("Y", ddd);
#endif
}

static void bAccountRecoveryListFreeData(struct blockMsgUndoForAccountOp *bb) {
	FREE(bb);
}

void bAccountRecoveryListRecover(struct blockMsgUndoForAccountOp *bb) {
	struct bAccountsTree *aa;
	aa = bb->acc;
	if (aa->d.publicKey != bb->recoveryData.publicKey) {
		FREE(aa->d.publicKey);
	}
	aa->d = bb->recoveryData;
}

void bAccountRecoveryListApplyAndSaveChanges(struct blockMsgUndoForAccountOp *bb) {
	struct bAccountsTree *aa;
	aa = bb->acc;
	bAccountSave(aa);
	if (aa->d.lastMiningTime > node->lastMiningTime) node->lastMiningTime = aa->d.lastMiningTime;
}

static void bAccountBalanceSave(struct bAccountBalance *bb) {
	char			ttt[TMP_STRING_SIZE];
	char			ddd[TMP_STRING_SIZE];
	struct stat		st;
	char			*dir;

	if (bb == NULL) return;
	if (checkFileAndTokenName(bb->coin, 0) < 0) return;
	dir = bAccountDirectory_st(bb->myAccount->account);
	if (stat(dir, &st) == -1) mkdirRecursivelyCreateDirectory(dir);
	snprintf(ttt, sizeof(ttt), "%a\n", bb->balance);
	fileSave(ttt, "%s/balance-%s.txt", bAccountDirectory_st(bb->myAccount->account), bb->coin);
}

static void bAccountBalanceRecoveryListFreeData(struct blockMsgUndoForAccountBalanceOp *bb) {
	FREE(bb);
}

void bAccountBalanceRecoveryListRecover(struct blockMsgUndoForAccountBalanceOp *bb) {
	bb->bal->balance = bb->previousBalance;
}

void bAccountBalanceRecoveryListApplyAndSaveChanges(struct blockMsgUndoForAccountBalanceOp *bb) {
	bAccountBalanceSave(bb->bal);
}

/////////////////////////////////////////////////////////////////////////////////////

static void bchainInsertToUndoList(struct blockMsgUndoList **undoList, int (*undoOrConfirmFunction)(void *arg, int applyChangesFlag), void *arg) {
	struct blockMsgUndoList *uu;
	ALLOC(uu, struct blockMsgUndoList);
	uu->undoOrConfirmFunction = undoOrConfirmFunction;
	uu->undoOrConfirmArg = arg;
	uu->next = *undoList;
	*undoList = uu;
}

/////////////////////////

static void blockFinalizeNoteAccountModification(struct bFinalActionList **list, struct bAccountsTree *acc) {
	struct bFinalActionList 	*aa;
	ALLOC(aa, struct bFinalActionList);
	memset(aa, 0, sizeof(*aa));
	aa->action = BFA_ACCOUNT_MODIFIED;
	aa->u.accountModified.acc = acc;
	aa->u.accountModified.recoveryData = acc->d;
	SGLIB_LIST_ADD(struct bFinalActionList, *list, aa, next);
}

void blockFinalizeNoteAccountBalanceModification(struct bFinalActionList **list, struct bAccountBalance *bal) {
	struct bFinalActionList 	*aa;
	ALLOC(aa, struct bFinalActionList);
	memset(aa, 0, sizeof(*aa));
	aa->action = BFA_ACCOUNT_BALANCE_MODIFIED;
	aa->u.accountBalanceModified.bal = bal;
	aa->u.accountBalanceModified.previousBalance = bal->balance;
	SGLIB_LIST_ADD(struct bFinalActionList, *list, aa, next);
}

static void blockFinalizeNoteCoinOptionsModification(struct bFinalActionList **list, char **place) {
	struct bFinalActionList 	*aa;
	ALLOC(aa, struct bFinalActionList);
	memset(aa, 0, sizeof(*aa));
	aa->action = BFA_TOKEN_OPTIONS_MODIFIED;
	aa->u.tokenOptions.place = place;
	aa->u.tokenOptions.options = *place;
	SGLIB_LIST_ADD(struct bFinalActionList, *list, aa, next);
}

static void blockFinalizeNoteMsgTabModification(struct bFinalActionList **list) {
	struct bFinalActionList 	*aa;
	ALLOC(aa, struct bFinalActionList);
	memset(aa, 0, sizeof(*aa));
	aa->action = BFA_TOKEN_TAB_MODIFIED;
	aa->u.tokenTableModified.previousTable = supportedMessagesTab;
	aa->u.tokenTableModified.previousTableMax = supportedMessagesTabMaxi;
	SGLIB_LIST_ADD(struct bFinalActionList, *list, aa, next);
}

static void blockFinalizeNotePendingRequestInsert(struct bFinalActionList **list, char *reqName, char *reqData, int datalen) {
	struct bFinalActionList 	*aa;
	ALLOC(aa, struct bFinalActionList);
	memset(aa, 0, sizeof(*aa));
	aa->action = BFA_PENDING_REQUEST_INSERT;
	aa->u.pendingRequest.reqName = strDuplicate(reqName);
	ALLOCC(aa->u.pendingRequest.reqData, datalen, char);
	memcpy(aa->u.pendingRequest.reqData, reqData, datalen);
	aa->u.pendingRequest.reqDataLen = datalen;
	SGLIB_LIST_ADD(struct bFinalActionList, *list, aa, next);
}

static void blockFinalizeNotePendingRequestDelete(struct bFinalActionList **list, char *reqName) {
	struct bFinalActionList 	*aa;
	ALLOC(aa, struct bFinalActionList);
	memset(aa, 0, sizeof(*aa));
	aa->action = BFA_PENDING_REQUEST_DELETE;
	aa->u.pendingRequest.reqName = strDuplicate(reqName);
	aa->u.pendingRequest.reqData = NULL;
	aa->u.pendingRequest.reqDataLen = 0;
	SGLIB_LIST_ADD(struct bFinalActionList, *list, aa, next);
}

static void blockFinalizeNotePowCoinAddMessage(struct bFinalActionList **list, struct powcoinBlockPendingMessagesListStr *msg) {
	struct bFinalActionList 	*aa;
	ALLOC(aa, struct bFinalActionList);
	memset(aa, 0, sizeof(*aa));
	aa->action = BFA_POW_MESSAGE_ADD;
	aa->u.powMessage.powcoinMessage = msg;
	SGLIB_LIST_ADD(struct bFinalActionList, *list, aa, next);
}

static void blockFinalizeNotePowCoinDelMessage(struct bFinalActionList **list, struct powcoinBlockPendingMessagesListStr *msg, uint64_t blockSequenceNumber, char *miningmsg) {
	struct bFinalActionList 	*aa;
	ALLOC(aa, struct bFinalActionList);
	memset(aa, 0, sizeof(*aa));
	aa->action = BFA_POW_MESSAGE_DELETE;
	aa->u.powMessage.powcoinMessage = msg;
	aa->u.powMessage.miningBlockSequenceNumber = blockSequenceNumber;
	aa->u.powMessage.miningMsg = strDuplicate(miningmsg);
	SGLIB_LIST_ADD(struct bFinalActionList, *list, aa, next);
}


static void blockFinalizeNoteAccountNotification(struct bFinalActionList **list, int notificationFlag, char *coin, char *account, char *fmt, ...) {
	struct bFinalActionList 	*aa;
	struct jsVarDynamicString 	*ss;
	va_list 					arg_ptr;

	va_start(arg_ptr, fmt);
	ss = jsVarDstrCreateByVPrintf(fmt, arg_ptr);
	va_end(arg_ptr);
	ALLOC(aa, struct bFinalActionList);
	memset(aa, 0, sizeof(*aa));
	assert(notificationFlag == BFA_ACCOUNT_NOTIFICATION_ON_SUCCESS || notificationFlag == BFA_ACCOUNT_NOTIFICATION_ON_FAIL);
	aa->action = notificationFlag;
	aa->u.notification.notificationFlag = notificationFlag;
	aa->u.notification.account = strDuplicate(account);
	aa->u.notification.coin = strDuplicate(coin);
	aa->u.notification.message = jsVarDstrGetStringAndReinit(ss);
	jsVarDstrFree(&ss);
	SGLIB_LIST_ADD(struct bFinalActionList, *list, aa, next);
}

// must be called before the order is actually deleted from tree
void blockFinalizeNoteExchangeOrderModified(int opcode, struct bFinalActionList **list, struct bookedOrder *o) {
	struct bFinalActionList 	*aa;
	ALLOC(aa, struct bFinalActionList);
	memset(aa, 0, sizeof(*aa));
	aa->action = opcode;
	aa->u.exchangeOrderModified.o = o;
	aa->u.exchangeOrderModified.price = o->myPriceLevel->price;
	aa->u.exchangeOrderModified.quantity = o->quantity;
	aa->u.exchangeOrderModified.executedQuantity = o->executedQuantity;
	aa->u.exchangeOrderModified.halfbook = o->myPriceLevel->myHalfBook;
	SGLIB_LIST_ADD(struct bFinalActionList, *list, aa, next);
}

/////////////////////////////////////////////////////////////////////////////////////


struct jsVarDynamicString *immediatelyExecuteMetamsgMessage(struct bchainMessage *msg) {
	struct jsVarDynamicString		*res;
	struct supportedMessageStr		*sss;
	int								i, r;
	char							*opt;
	struct bixParsingContext		*mbixc;
	char							*mvl;
	uint64_t						nonce;
	struct bAccountsTree 			*aa;
	struct bAccountBalance 			*bb;
	struct sglib_bAccBal_iterator	iii;

	res = jsVarDstrCreate();
	mbixc = bixParsingContextGet();
	r = bixParseString(mbixc, msg->data, msg->dataLength);
	if (r != 0) {
		jsVarDstrAppendPrintf(res, "%s: Internal error: wrongly formed coin message", SPRINT_CURRENT_TIME());
		PRINTF("%s\n", res->s);
		bixParsingContextFree(&mbixc);
		return(res);
	}

	bixMakeTagsNullTerminating(mbixc);
	mvl = mbixc->tag[BIX_TAG_mvl];
	// check fields
	if (1
		// Messages which do not need to be signed
		&& strcmp(mvl, "List") != 0
		// && strcmp(mvl, "") != 0
		&& ! msg->signatureVerifiedFlag
		) {
		jsVarDstrAppendPrintf(res, "%s: Error: message %s has wrong signature.", SPRINT_CURRENT_TIME(), mvl);
		PRINTF("%s\n", res->s);
	} else if (1
			   // Messages where acc tag does not need to match the signing account
			   && strcmp(mvl, "List") != 0			   
			   // && strcmp(mvl, "") != 0
			   && strcmp(msg->account, mbixc->tag[BIX_TAG_acc]) != 0
		) {
		jsVarDstrAppendPrintf(res, "%s: Error: account mismatch %s <> %s.", SPRINT_CURRENT_TIME(), msg->account, mbixc->tag[BIX_TAG_acc]);
		PRINTF("%s\n", res->s);
	} else if (strcmp(mbixc->tag[BIX_TAG_mvl], "List") == 0) {
		// list available message type
		for(i=0; i<supportedMessagesTabMaxi; i++) {
			sss = &supportedMessagesTab[i];
			opt = sss->opt;
			if (opt == NULL) opt = "";
			jsVarDstrAppendPrintf(res, "msm=%s;mac=%s;opt:%d=%s;mst=%s;", sss->messageMetaType, sss->masterAccount, strlen(opt), opt, sss->messageType);
		}
	} else if (strcmp(mvl, "GetAllBalances") == 0) {
		// balance request
		if (mbixc->tagLen[BIX_TAG_acc] < 0 || mbixc->tagLen[BIX_TAG_non] < 0) {
			jsVarDstrAppendPrintf(res, "%s: Internall error: wrongly formed getbalance message.", SPRINT_CURRENT_TIME());
			PRINTF("%s\n", res->s);
		} else {
			nonce = atoll(mbixc->tag[BIX_TAG_non]);
			aa = bAccountLoadOrFind(mbixc->tag[BIX_TAG_acc], &currentBchainContext);
			if (aa == NULL) {
				jsVarDstrAppendPrintf(res, "%s: Error: wrong account.", SPRINT_CURRENT_TIME());
				PRINTF("%s\n", res->s);
			} else {
				bAccountLoadAllBalances(aa, &currentBchainContext);
				for(bb=sglib_bAccBal_it_init_inorder(&iii, aa->balances);bb!=NULL;bb=sglib_bAccBal_it_next(&iii)) {
					// if (bb->balance != 0) {
						jsVarDstrAppendPrintf(res, "tkn=%s;amt=%a;eog=;", bb->coin, bb->balance);
						//}
				}
			}
		}
	} else {
		jsVarDstrAppendPrintf(res, "%s: Error: unknown metamsg operation", SPRINT_CURRENT_TIME());
		PRINTF("%s\n", res->s);
	}

	bixMakeTagsSemicolonTerminating(mbixc);
	bixParsingContextFree(&mbixc);
	return(res);
}

struct jsVarDynamicString *immediatelyExecuteExchangeMessage(struct bchainMessage *msg) {
	struct jsVarDynamicString		*res;
	struct supportedMessageStr		*sss;
	int								i, r;
	char							*opt;
	struct bixParsingContext		*mbixc;
	char							*mvl;
	uint64_t						nonce;
	struct bAccountsTree 			*aa;
	struct bAccountBalance 			*bb;
	struct sglib_bAccBal_iterator	iii;

	res = jsVarDstrCreate();
	mbixc = bixParsingContextGet();
	r = bixParseString(mbixc, msg->data, msg->dataLength);
	if (r != 0) {
		jsVarDstrAppendPrintf(res, "%s: Internal error: wrongly formed exchange message", SPRINT_CURRENT_TIME());
		PRINTF("%s\n", res->s);
		bixParsingContextFree(&mbixc);
		return(res);
	}

	bixMakeTagsNullTerminating(mbixc);
	mvl = mbixc->tag[BIX_TAG_mvl];
	// check fields
	if (1
		// Messages which do not need to be signed
		// && strcmp(mvl, "") != 0
		&& ! msg->signatureVerifiedFlag
		) {
		jsVarDstrAppendPrintf(res, "%s: Error: message %s has wrong signature.", SPRINT_CURRENT_TIME(), mvl);
		PRINTF("%s\n", res->s);
	} else if (1
			   // Messages where acc tag does not need to match the signing account
			   // && strcmp(mvl, "") != 0
			   && strcmp(msg->account, mbixc->tag[BIX_TAG_acc]) != 0
		) {
		jsVarDstrAppendPrintf(res, "%s: Error: account mismatch %s <> %s.", SPRINT_CURRENT_TIME(), msg->account, mbixc->tag[BIX_TAG_acc]);
		PRINTF("%s\n", res->s);
	} else if (strcmp(mbixc->tag[BIX_TAG_mvl], "GetOrderbook") == 0) {
		// get the book
		exchangeAppendOrderbookAsString(res, mbixc->tag[BIX_TAG_btk], mbixc->tag[BIX_TAG_qtk]);
	} else {
		jsVarDstrAppendPrintf(res, "%s: Error: unknown metamsg operation", SPRINT_CURRENT_TIME());
		PRINTF("%s\n", res->s);
	}

	bixMakeTagsSemicolonTerminating(mbixc);
	bixParsingContextFree(&mbixc);
	return(res);
}


static int coinOnNewAccountRequested(struct bchainMessage *msg,  struct bixParsingContext *bix, char *coin, struct bFinalActionList **finalActionList) {
	struct bAccountsTree 	*aa;
	int 					res;
	char					*account;

	account = bix->tag[BIX_TAG_acc];
	// just verify that the account does not exists
	aa = bAccountLoadOrFind(account, &currentBchainContext);
	// PRINTF("%d: Pending account creation %s, %p, %d\n", applyChangesFlag, mbixc->tag[BIX_TAG_acc], aa, (aa==NULL?0:aa->d.created));
	if (aa != NULL && aa->d.created) {
		PRINTF("Can't create account %s. Account exists.\n", account);
		blockFinalizeNoteAccountNotification(finalActionList, BFA_ACCOUNT_NOTIFICATION_ON_FAIL, coin, account, "Error: Account %s exists.", account);
		res = -1;
	} else {
		// blockFinalizeNoteAccountNotification(finalActionList, BFA_ACCOUNT_NOTIFICATION_ON_SUCCESS, coin, account, "Request to create account %s was inserted to blockchain.", account);
		res = 0;
	}
	return(res);
}

static int coinOnNewAccountApproved(struct bchainMessage *msg,  struct bixParsingContext *bix, char *coin, struct bFinalActionList **finalActionList) {
	struct bAccountsTree 		*aa;
	struct bAccountBalance		*ba, *bbase;
	int 						res;
	double						balance;
	struct supportedMessageStr 	*sss;
	struct jsVarDynamicString	*ss;
	char						*account;

	account = bix->tag[BIX_TAG_acc];
	if (! msg->signatureVerifiedFlag) {
		// This is uselss, signature was checked yet
		PRINTF("Internall error. Wrong signature in account approval. Account: %s\n", account);
		blockFinalizeNoteAccountNotification(finalActionList, BFA_ACCOUNT_NOTIFICATION_ON_FAIL, coin, account, "Error: Wrong signature in account %s approval.", account);
		res = -1;
	} else if (! isCoreServerAccount(msg->account)) {
		PRINTF("Internall error. Account approval is not signed by core server. Account %s\n", account);
		blockFinalizeNoteAccountNotification(finalActionList, BFA_ACCOUNT_NOTIFICATION_ON_FAIL, coin, account, "Error: Wrong signing account in account %s approval.", account);
		res = -1;			
	} else {
		aa = bAccountLoadFindOrCreate(account, &currentBchainContext);
		// PRINTF("%d: Account approval %s, %p, %d, %d\n", applyChangesFlag, mbixc->tag[BIX_TAG_acc], aa, (aa==NULL?0:aa->d.created), (aa==NULL?0:aa->d.approved));
		if (aa == NULL) {
			PRINTF("Internall error. Can't create account %s\n", account);
			blockFinalizeNoteAccountNotification(finalActionList, BFA_ACCOUNT_NOTIFICATION_ON_FAIL, coin, account, "Error: Approved account does not exists");
			res = -1;
		} else if (aa->d.approved) {
			PRINTF("Warning: Account to approve is approved yet: %s\n", account);
			blockFinalizeNoteAccountNotification(finalActionList, BFA_ACCOUNT_NOTIFICATION_ON_FAIL, coin, account, "Error: Account %s is approved yet.", account);
			res = 0;
		} else {
			blockFinalizeNoteAccountModification(finalActionList, aa);
			aa->d.created = 1;
			aa->d.approved = 1;
			aa->d.publicKey = strDuplicate(bix->tag[BIX_TAG_pbk]);
			sss = coreServerSupportedMessageFind(coin);
			if (sss == NULL) {
				PRINTF("Internall error. Account approved while coin does not exists: %s.\n", coin);
				blockFinalizeNoteAccountNotification(finalActionList, BFA_ACCOUNT_NOTIFICATION_ON_FAIL, coin, account, "Error: token %s does not exists.", coin);
				res = -1;
			} else {
				ba = bAccountBalanceLoadFindOrCreate(coin, account, &currentBchainContext);
				blockFinalizeNoteAccountBalanceModification(finalActionList, ba);
				balance = jsVarGetEnvDouble(sss->opt, "new_account_balance", 0);
				// blockFinalizeNoteAccountNotification(finalActionList, BFA_ACCOUNT_NOTIFICATION_ON_SUCCESS, coin, account, "Account approval was inserted into blockchain. Account %s is enabled.", account);
				bbase = bAccountBalanceLoadFindOrCreate(coin, jsVarGetEnv_st(sss->opt, "base_account"), &currentBchainContext);
				if (bbase != NULL) {
					// discount the initial balance from base account
					blockFinalizeNoteAccountBalanceModification(finalActionList, bbase);
					if (bbase->balance >= balance) {
						bbase->balance -= balance;
						ba->balance = balance;
					} else {
						ba->balance = 0;
						res = -1;
					}
				} else {
					// this is probably creation of the base account itself
					ba->balance = 0;
				}
				res = 0;
			}
		}
	}
	return(res);
}

int applyMetamsgBlockchainMessage(struct bchainMessage *msg, struct blockContentDataList *fromBlock, struct bFinalActionList **finalActionList) {
	int									i, r, res, setBaseBalanceFlag;
	char								*coin, *cointype, *coinopt;
	struct bAccountsTree				*base0, *base1;
	struct supportedMessageStr			*smm, smsg, *previousTab;
	struct bAccountsTree 				*aa;
	double								amount;
	struct bixParsingContext			*mbixc;
	char								*mvl;

	res = -1;

	// PRINTF("VERIFY MSG %s\n", dat);
	mbixc = bixParsingContextGet();
	r = bixParseString(mbixc, msg->data, msg->dataLength);
	if (r != 0) {
		PRINTF("Internal error: wrongly formed metacoin message: %s\n", msg->data);
		bixParsingContextFree(&mbixc);
		return(-1);
	}
	bixMakeTagsNullTerminating(mbixc);
	coin = mbixc->tag[BIX_TAG_mdt];

	// if this is not the one of messages not requiring signature, check signature
	mvl = mbixc->tag[BIX_TAG_mvl];
	if (1
		// && strcmp(mvl, "") != 0
		&& ! msg->signatureVerifiedFlag
		) {
		PRINTF("Error: message %s has wrong signature.\n", mvl);
		res = -1;
	} else if (1
			   // Messages where acc tag does not need to match the signing account
			   && strcmp(mvl, "NewAccountRequest") != 0			   
			   && strcmp(mvl, "NewAccountApproval") != 0			   
			   // && strcmp(mvl, "") != 0
			   && strcmp(msg->account, mbixc->tag[BIX_TAG_acc]) != 0
		) {
		PRINTF("Error: account mismatch %s <> %s in message %s.", msg->account, mbixc->tag[BIX_TAG_acc], mvl);
		res = -1;
	} else if (0 && strcmp(mbixc->tag[BIX_TAG_mvl], "NewAccountRequest") == 0) {
		// new account creation
		res = coinOnNewAccountRequested(msg, mbixc, coin, finalActionList);
	} else if (strcmp(mbixc->tag[BIX_TAG_mvl], "NewAccountApproval") == 0) {
		// new account approval
		res = coinOnNewAccountApproved(msg, mbixc, coin, finalActionList);
	} else if (strcmp(mbixc->tag[BIX_TAG_mvl], "DeletePendingRequest") == 0) {
		// Message is not used at the moment
		if (! isCoreServerAccount(msg->account)) {
			res = -1;
		} else {
			blockFinalizeNotePendingRequestDelete(finalActionList, mbixc->tag[BIX_TAG_req]);
			res = 0;
		}
	} else {
		PRINTF("Internal error: unknown metacoin operation in: %s\n", msg->data);
		res = -1;
	}

	bixMakeTagsSemicolonTerminating(mbixc);
	bixParsingContextFree(&mbixc);
	return(res);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// coin common

char *coinBaseAccount_st(char *coin) {
	struct supportedMessageStr 	*sss;
	char						*baseAccName;

	sss = coreServerSupportedMessageFind(coin);
	if (sss == NULL) return(NULL);
	baseAccName = jsVarGetEnv_st(sss->opt, "base_account");
	return(baseAccName);
}

int coinGdprs(char *coin) {
	struct supportedMessageStr 	*sss;
	char						*srvName;
	int							i;

	sss = coreServerSupportedMessageFind(coin);
	if (sss == NULL) return(-1);
	srvName = jsVarGetEnv_st(sss->opt, "gdprs");
	if (srvName == NULL) return(-1);
	for(i=0; i<coreServersMax; i++) {
		if (strcmp(coreServers[i].name, srvName) == 0) return(i);
	}
	return(-1);
}

char *coinType_st(char *coin) {
	struct bAccountsTree 		*aa;
	struct supportedMessageStr 	*sss;
	char						*baseAccName;

	sss = coreServerSupportedMessageFind(coin);
	if (sss == NULL) return(NULL);
	return(sss->messageMetaType);
}

struct bchainMessageList *submitAccountApprovedMessage(char *coin, char *account, char *publicKey, int publicKeyLength, char *secServerInfo, char *kychash, int kychashLength) {
	struct bchainMessageList 	*nnn;
	struct jsVarDynamicString 	*ss;
	char						*signAccount;
	char						*signature;

	ss = jsVarDstrCreateByPrintf("mvl=NewAccountApproval;mdt=%s;ssi=%s;acc=%s;non=%"PRIu64";pbk:%d=%s;kyh:%d=", coin, secServerInfo, account, genMsgNonce(), publicKeyLength, publicKey, kychashLength);
	jsVarDstrAppendData(ss, kychash, kychashLength);
	jsVarDstrAppendPrintf(ss, ";");
	// PRINTF("Approval msg: %s\n", ss->s);
	signAccount = THIS_CORE_SERVER_ACCOUNT_NAME_ST();
	signature = signatureCreateBase64(ss->s, ss->size, myNodePrivateKey);
	nnn = bchainCreateNewMessage(ss->s, ss->size, MESSAGE_DATATYPE_META_MSG, signAccount, coin, account, signature, NULL);
	jsVarDstrFree(&ss);	
	FREE(signature);
	return(nnn);
}

static void saveAccountKycData(char *account, char *coin, char *kycdata, int kycdatalen) {
	struct jsVarDynamicString	*ss;
	// the kyc database has same format as pending requests
	ss = jsVarDstrCreateByPrintf("mvl=AccountKycData;acc=%s;mdt=%s;non=%"PRIu64";kyc:%d=", account, coin, genMsgNonce(), kycdatalen);
	jsVarDstrAppendData(ss, kycdata, kycdatalen);
	jsVarDstrAppendPrintf(ss, ";");
	databasePut(DATABASE_KYC_DATA, account, ss->s, ss->size);
	jsVarDstrFree(&ss);
}

static int coinImmediateActionVerifyBasenessOfAccount(struct jsVarDynamicString *res, char *account, char *coin) {
	struct bAccountBalance 		*aa;
	struct supportedMessageStr 	*sss;
	char						*baseAccName;

	aa = bAccountBalanceLoadOrFind(coin, account, &currentBchainContext);
	if (aa == NULL) {
		jsVarDstrAppendPrintf(res, "%s: Error: wrong account.", SPRINT_CURRENT_TIME());
		PRINTF("%s\n", res->s);
		return(-1);
	} else {
		sss = coreServerSupportedMessageFind(coin);
		if (sss == NULL) {
			jsVarDstrAppendPrintf(res, "%s: Error: token %s has no base account.", SPRINT_CURRENT_TIME(), coin);
			PRINTF("%s\n", res->s);
			return(-1);
		} else {
			baseAccName = jsVarGetEnv_st(sss->opt, "base_account");
			if (baseAccName == NULL || strcmp(baseAccName, account) != 0) {
				jsVarDstrAppendPrintf(res, "%s: Error: requesting account is not the base account of token %s.", SPRINT_CURRENT_TIME(), coin);
				PRINTF("%s\n", res->s);
				return(-1);
			} else {
				return(0);
			}
		}
	}
	assert(0);
}

struct jsVarDynamicString *immediatelyExecuteCoinMessage(struct bchainMessage *msg) {
	struct jsVarDynamicString		*res;
	int								r;
	uint64_t						nonce;
	struct bAccountsTree 			*aa;
	struct bAccountBalance 			*bb;
	char							*mvl;
	char							*newAccount;
	char							*notifyAccount;
	char							*notifyAccountToken;
	struct jsVarDynamicString		*value;
	char							*pubkey;
	int								reqid;
	int								manuallyApprovedFlag;
	char							*secServerInfo;
	char							*account, *coin, *signature;
	char							*baseAccName;
	struct jsVarDynamicString		*ss;
	struct bchainMessageList 		*nnn;
	struct supportedMessageStr 		*sss;
	struct supportedMessageStr		*smm;
	char							ttt[TMP_STRING_SIZE];
	char							kyh[SHA256_DIGEST_LENGTH];
	char							kyh64[SHA256_DIGEST_LENGTH*2];
	int								kyh64len;
	struct bixParsingContext		*mbixc;
	char							*approvalMethod;

	res = jsVarDstrCreate();
	coin = msg->dataType;
	signature = msg->signature;

	mbixc = bixParsingContextGet();
	r = bixParseString(mbixc, msg->data, msg->dataLength);
	if (r != 0) {
		jsVarDstrAppendPrintf(res, "%s: Internal Error: wrongly formed coin message", SPRINT_CURRENT_TIME());
		PRINTF("%s\n", res->s);
		bixParsingContextFree(&mbixc);
		return(res);
	}
	bixMakeTagsNullTerminating(mbixc);

	account = mbixc->tag[BIX_TAG_acc];
	nonce = atoll(mbixc->tag[BIX_TAG_non]);

	// if this is not one of messages not requiring signature, check signature
	mvl = mbixc->tag[BIX_TAG_mvl];
	// first check signature and only if it is O.K. process message
	if (
		strcmp(mvl, "CheckLogin") != 0
		&& strcmp(mvl, "NewAccountNumber") != 0
		&& strcmp(mvl, "NewAccount") != 0
		&& strcmp(mvl, "NOP") != 0
		&& strcmp(mvl, "MoreNotifications") != 0
		// && strcmp(mvl, "") != 0
		&& ! msg->signatureVerifiedFlag
		) {
		jsVarDstrAppendPrintf(res, "%s: Error: message %s has wrong signature.", SPRINT_CURRENT_TIME(), mvl);
		PRINTF("%s\n", res->s);
	} else if (1
			   // Messages where acc tag does not need to match the signing account
			   && strcmp(mvl, "NewAccountNumber") != 0			   
			   && strcmp(mvl, "NewAccount") != 0			   
			   && strcmp(mvl, "NOP") != 0			   
			   // && strcmp(mvl, "") != 0
			   && strcmp(msg->account, mbixc->tag[BIX_TAG_acc]) != 0
		) {
		jsVarDstrAppendPrintf(res, "%s: Error: account mismatch %s <> %s.", SPRINT_CURRENT_TIME(), msg->account, mbixc->tag[BIX_TAG_acc]);
		PRINTF("%s\n", res->s);
	} else if (strcmp(mvl, "NewAccountNumber") == 0) {
		newAccount = generateNewAccountName_st();
		jsVarDstrAppendPrintf(res, "%s", newAccount);
	} else if (strcmp(mvl, "NewAccount") == 0) {
		reqid = atoi(mbixc->tag[BIX_TAG_rid]);
		pubkey = mbixc->tag[BIX_TAG_pbk];
		secServerInfo = mbixc->tag[BIX_TAG_ssi];
		// check for coin parameters
		smm = coreServerSupportedMessageFind(coin);
		if (smm == NULL) {
			jsVarDstrAppendPrintf(res, "%s: Error: can't create account. Coin does not exist: %s.\n", SPRINT_CURRENT_TIME(), coin);
			PRINTF("%s\n", res->s);
		} else {
			// printf("OPT == %s\n", smm->opt);
			if (smm->opt == NULL) {
				// the accounts before setting options are auto approved. This is because the base account must be auto approved.
				manuallyApprovedFlag = 0;
			} else {
				approvalMethod = jsVarGetEnv_st(smm->opt, "new_account_approval");
				if (approvalMethod == NULL || strcmp(approvalMethod, "manual") == 0) {
					manuallyApprovedFlag = 1;
				} else {
					manuallyApprovedFlag = 0;
				}
			}

			aa = bAccountLoadOrFind(account, &currentBchainContext);
			// PRINTF("%d: Pending account creation %s, %p, %d\n", applyChangesFlag, mbixc->tag[BIX_TAG_acc], aa, (aa==NULL?0:aa->d.created));
			if (aa != NULL && aa->d.created) {
				jsVarDstrAppendPrintf(res, "%s: Error: can't create account. Account exists: %s (%s).\n", SPRINT_CURRENT_TIME(), account, coin);
				PRINTF("%s\n", res->s);
			} else {
				notifyAccount = coinBaseAccount_st(coin);
				notifyAccountToken = coin;
				jsVarDstrAppendPrintf(res, "%s: Info: Creating account %s.\\n", SPRINT_CURRENT_TIME(), account);
				SHA256(mbixc->tag[BIX_TAG_kyc], mbixc->tagLen[BIX_TAG_kyc], kyh);
				kyh64len =  base64_encode(kyh, SHA256_DIGEST_LENGTH, kyh64, sizeof(kyh64));
				if (manuallyApprovedFlag) {
					ss = jsVarDstrCreateByPrintf("mvl=NewAccountRequest;acc=%s;mdt=%s;non=%"PRIu64";pbk:%d=%s;kyh:%d=%s;kyc:%d=", 
												 account, coin, genMsgNonce(),
												 mbixc->tagLen[BIX_TAG_pbk], mbixc->tag[BIX_TAG_pbk], 
												 kyh64len, kyh64,
												 mbixc->tagLen[BIX_TAG_kyc]
						);
					jsVarDstrAppendData(ss, mbixc->tag[BIX_TAG_kyc], mbixc->tagLen[BIX_TAG_kyc]);
					jsVarDstrAppendPrintf(ss, ";");
					// nnn = bchainCreateNewMessage(ss->s, ss->size, coin, CORE_SERVER_ACCOUNT_NAME_ST(), notifyAccountToken, notifyAccount, "not-signed", NULL);
					jsVarDstrAppendPrintf(res, "Waiting for approval.\\n");
					snprintf(ttt, TMP_STRING_SIZE, "%s-%s-%d-%d", PENDING_NEW_ACCOUNT_REQUEST, coin, myNode, node->currentpPendingRequestNumber);
					ttt[TMP_STRING_SIZE-1] = 0;
					node->currentpPendingRequestNumber ++;
					databasePut(DATABASE_PENDING_REQUESTS, ttt, ss->s, ss->size);
					jsVarDstrFree(&ss);
				} else {
					// automatic approval
					// save kyc data
					saveAccountKycData(account, coin, mbixc->tag[BIX_TAG_kyc], mbixc->tagLen[BIX_TAG_kyc]);
					nnn = submitAccountApprovedMessage(coin, account, mbixc->tag[BIX_TAG_pbk], mbixc->tagLen[BIX_TAG_pbk], secServerInfo, kyh64, kyh64len);
					jsVarDstrAppendPrintf(res, "Approval msg id: %s.\\n", nnn->msg.msgId);
				}
			}
		}
	} else if (strcmp(mvl, "GetBalance") == 0) {
		// balance request
		if (mbixc->tagLen[BIX_TAG_acc] < 0 || mbixc->tagLen[BIX_TAG_non] < 0) {
			jsVarDstrAppendPrintf(res, "%s: Internall error: wrongly formed getbalance message.", SPRINT_CURRENT_TIME());
			PRINTF("%s\n", res->s);
		} else {
			bb = bAccountBalanceLoadOrFind(coin, mbixc->tag[BIX_TAG_acc], &currentBchainContext);
			nonce = atoll(mbixc->tag[BIX_TAG_non]);
			if (bb == NULL) {
				jsVarDstrAppendPrintf(res, "%s: Error: wrong account.", SPRINT_CURRENT_TIME());
				PRINTF("%s\n", res->s);
			} else {
				jsVarDstrAppendPrintf(res, "%f", bb->balance);
			}
		}
	} else if (strcmp(mvl, "CheckLogin") == 0) {
		if (msg->signatureVerifiedFlag) {
			jsVarDstrAppendPrintf(res, "%s: Info: Login successful", SPRINT_CURRENT_TIME());
		} else {
			jsVarDstrAppendPrintf(res, "%s: Error: Login failed", SPRINT_CURRENT_TIME());
		}

	} else if (strcmp(mvl, "MoreNotifications") == 0) {
		if (msg->signatureVerifiedFlag) {
			anotificationAppendSavedNotifications(res, coin, account, 1);
			anotificationAppendSavedNotifications(res, coin, account, 0);
		}
	} else if (strcmp(mvl, "getAccounts") == 0) {
		if (mbixc->tagLen[BIX_TAG_acc] < 0 || mbixc->tagLen[BIX_TAG_non] < 0|| mbixc->tagLen[BIX_TAG_tok] < 0) {
			jsVarDstrAppendPrintf(res, "%s: Error: wrongly formed getAccounts message.", SPRINT_CURRENT_TIME());
			PRINTF("%s\n", res->s);
		} else {
			r = coinImmediateActionVerifyBasenessOfAccount(res, mbixc->tag[BIX_TAG_acc], coin);
			if (r >= 0) {
				listAccountsToBixMsg(res, coin);
			}
		}
	} else if (strcmp(mvl, "prMap") == 0) {
		// db map serves to retrieve pending new account requests from core server to secondary server
		if (mbixc->tagLen[BIX_TAG_acc] < 0 || mbixc->tagLen[BIX_TAG_non] < 0|| mbixc->tagLen[BIX_TAG_dbs] < 0 || mbixc->tagLen[BIX_TAG_pre] < 0) {
			jsVarDstrAppendPrintf(res, "%s: Internal error: wrongly formed prMap message.", SPRINT_CURRENT_TIME());
			PRINTF("%s\n", res->s);
		} else {
			r = coinImmediateActionVerifyBasenessOfAccount(res, mbixc->tag[BIX_TAG_acc], coin);
			if (r >= 0) {
				r = snprintf(ttt, TMP_STRING_SIZE, "%s-%s", PENDING_NEW_ACCOUNT_REQUEST, coin);
				if (r >= TMP_STRING_SIZE || r < 0) {
					jsVarDstrAppendPrintf(res, "%s: Error: Problem forming update list request. Probably too long coin name %s.\n", SPRINT_CURRENT_TIME(), coin);
					PRINTF("%s\n", res->s);
				} else {
					pendingRequestsAddToBixMsg(res, mbixc->tag[BIX_TAG_dbs], mbixc->tag[BIX_TAG_pre]);
				}
			}
		}
	} else if (strcmp(mvl, "dbGet") == 0) {
		// db get not used for the moment?
		// TODO: verify signature and verify that the requesting account has the right to access the information!
		if (mbixc->tagLen[BIX_TAG_acc] < 0 || mbixc->tagLen[BIX_TAG_non] < 0|| mbixc->tagLen[BIX_TAG_dbs] < 0 || mbixc->tagLen[BIX_TAG_key] < 0) {
			jsVarDstrAppendPrintf(res, "%s: Error: wrongly formed prGet message.", SPRINT_CURRENT_TIME());
			PRINTF("%s\n", res->s);
		} else {
			r = coinImmediateActionVerifyBasenessOfAccount(res, mbixc->tag[BIX_TAG_acc], coin);
			if (r >= 0) {
				value = databaseGet(mbixc->tag[BIX_TAG_dbs], mbixc->tag[BIX_TAG_key]);
				if (value == NULL) {
					jsVarDstrAppendPrintf(res, "%s: Error: prGet: Can't get value %s", SPRINT_CURRENT_TIME(), mbixc->tag[BIX_TAG_key]);
				} else {
					jsVarDstrAppendData(res, value->s, value->size);
					jsVarDstrFree(&value);
				}
			}
		}
	} else if (strcmp(mvl, "prApprove") == 0) {
		if (mbixc->tagLen[BIX_TAG_acc] < 0 || mbixc->tagLen[BIX_TAG_non] < 0|| mbixc->tagLen[BIX_TAG_dbs] < 0 || mbixc->tagLen[BIX_TAG_key] < 0) {
			jsVarDstrAppendPrintf(res, "%s: Error: wrongly formed prApprove message.", SPRINT_CURRENT_TIME());
			PRINTF("%s\n", res->s);
		} else {
			r = coinImmediateActionVerifyBasenessOfAccount(res, mbixc->tag[BIX_TAG_acc], coin);
			if (r >= 0) {
				r = pendingRequestApprove(mbixc->tag[BIX_TAG_key]);
				if (r < 0) jsVarDstrAppendPrintf(res, "%s: Error: during approval. Probable cause: request approved or deleted previously.", SPRINT_CURRENT_TIME());
				else jsVarDstrAppendPrintf(res, "%s: Info: Approval submitted.", SPRINT_CURRENT_TIME());
			}
		}
	} else if (strcmp(mvl, "prDelete") == 0) {
		if (mbixc->tagLen[BIX_TAG_acc] < 0 || mbixc->tagLen[BIX_TAG_non] < 0|| mbixc->tagLen[BIX_TAG_dbs] < 0 || mbixc->tagLen[BIX_TAG_key] < 0) {
			jsVarDstrAppendPrintf(res, "%s: Error: wrongly formed prDelete message.", SPRINT_CURRENT_TIME());
			PRINTF("%s\n", res->s);
		} else {
			r = coinImmediateActionVerifyBasenessOfAccount(res, mbixc->tag[BIX_TAG_acc], coin);
			if (r >= 0) {
				pendingRequestDelete(mbixc->tag[BIX_TAG_key]);
				jsVarDstrAppendPrintf(res, "%s: Deletion submitted.", SPRINT_CURRENT_TIME());
			}
		}
	} else if (strcmp(mvl, "NOP") == 0) {
		// No operation, do nothing.
		// This kind of request is used only to attach an account to its notification channel
	} else {
		jsVarDstrAppendPrintf(res, "%s: Error: unknown coin operation", SPRINT_CURRENT_TIME());
		PRINTF("%s\n", res->s);
	}


	bixMakeTagsSemicolonTerminating(mbixc);
	bixParsingContextFree(&mbixc);
	return(res);
}

int onAccountApproved(char *reqMsg, int reqMsgLen) {
	int 						r;
	struct jsVarDynamicString	*ss;
	struct bchainMessageList 	*nnn;
	struct bixParsingContext	*mbixc;

	// PRINTF("Pending new account approval body is: %s\n", reqMsg);
	mbixc = bixParsingContextGet();
	r = bixParseString(mbixc, reqMsg, reqMsgLen);
	if (r != 0) {
		PRINTF("Internal error: wrongly formed request: %s\n", reqMsg);
		bixParsingContextFree(&mbixc);
		return(-1);
	}

	bixMakeTagsNullTerminating(mbixc);
	saveAccountKycData(mbixc->tag[BIX_TAG_acc], mbixc->tag[BIX_TAG_mdt], mbixc->tag[BIX_TAG_kyc], mbixc->tagLen[BIX_TAG_kyc]);
	submitAccountApprovedMessage(mbixc->tag[BIX_TAG_mdt], mbixc->tag[BIX_TAG_acc], mbixc->tag[BIX_TAG_pbk], mbixc->tagLen[BIX_TAG_pbk], mbixc->tag[BIX_TAG_ssi], mbixc->tag[BIX_TAG_kyh], mbixc->tagLen[BIX_TAG_kyh]);
	bixMakeTagsSemicolonTerminating(mbixc);
	bixParsingContextFree(&mbixc);
	return(0);
}

int onPendingRequestDelete(char *request) {
	int 						r;
	struct jsVarDynamicString	*ss;
	struct bchainMessageList 	*nnn;
	char						*notifyAccount;
	char						*notifyAccountToken;
	char						*signature;

	ss = jsVarDstrCreateByPrintf("mvl=DeletePendingRequest;non=%"PRIu64";req:%d=%s;", strlen(request), genMsgNonce(), request);
	// PRINTF("Inserting pending request delete to bchain\n", request);
	notifyAccount = ""; // TODO put there account from deleted request
	notifyAccountToken = "";
	signature = signatureCreateBase64(ss->s, ss->size, myNodePrivateKey);
	nnn = bchainCreateNewMessage(ss->s, ss->size, MESSAGE_DATATYPE_META_MSG, THIS_CORE_SERVER_ACCOUNT_NAME_ST(), notifyAccountToken, notifyAccount, signature, NULL);
	// jsVarDstrAppendPrintf(res, "Approval msg id: %s.", nnn->msg.msgId);
	jsVarDstrFree(&ss);	
	FREE(signature);
	return(0);
}

int onPendingGdprRequestDelete(char *request) {
	// GDPR requests are only in local database, no need to go through the blockchain
	// just delete it here.
	databaseDelete(DATABASE_PENDING_REQUESTS, request);
	return(0);
}

static int coinOnSendCoins(struct bchainMessage *msg, struct bixParsingContext *bix, char *signature, char *coin, int powCheckOnlyFlag, struct bFinalActionList **finalActionList) {
	uint64_t 					nonce;
	double 						amount;
	struct bAccountBalance 		*toa, *fra, *fea, *bixmsa, *bixbsa;
	int 						res, r;
	struct supportedMessageStr 	*sss, *bixsss;
	double						fees, bixfees;
	char						*account;

	account = bix->tag[BIX_TAG_fra];
	if (bix->tagLen[BIX_TAG_amt] < 0 || bix->tagLen[BIX_TAG_fra] < 0 || bix->tagLen[BIX_TAG_toa] < 0 || bix->tagLen[BIX_TAG_non] < 0) {
		PRINTF("Internall error: wrongly formed coin transfer message for token %s\n", coin);
		blockFinalizeNoteAccountNotification(finalActionList, BFA_ACCOUNT_NOTIFICATION_ON_FAIL, coin, account, "Internall error: wrongly formed transfer message.");
		res = -1;
	} else {
		fra = bAccountBalanceLoadOrFind(coin, bix->tag[BIX_TAG_fra], &currentBchainContext);
		toa = bAccountBalanceLoadOrFind(coin, bix->tag[BIX_TAG_toa], &currentBchainContext);
		amount = strtod(bix->tag[BIX_TAG_amt], NULL);
		nonce = atoll(bix->tag[BIX_TAG_non]);
		if (fra == NULL || toa == NULL || amount <= 0) {
			PRINTF("Error: wrong fra, toa or amount in send coin message for %s\n", coin);
			blockFinalizeNoteAccountNotification(finalActionList, BFA_ACCOUNT_NOTIFICATION_ON_FAIL, coin, account, "Error: wrong source or destination account or amount in message: %s.", bchainPrettyPrintTokenMsgContent(bix));
			res = -1;
		} else if (strcmp(msg->account, bix->tag[BIX_TAG_fra]) != 0) {
			PRINTF("Error: money send from %s submitted by account %s %s\n", bix->tag[BIX_TAG_fra], msg->account, coin);
			blockFinalizeNoteAccountNotification(finalActionList, BFA_ACCOUNT_NOTIFICATION_ON_FAIL, coin, account, "Error: message submitted by wrong account %s: %s.", msg->account, bchainPrettyPrintTokenMsgContent(bix));
			res = -1;			
		} else {
			assert(msg->signatureVerifiedFlag);
			if (powCheckOnlyFlag) {
				res = 0;
			} else {
				blockFinalizeNoteAccountBalanceModification(finalActionList, fra);
				blockFinalizeNoteAccountBalanceModification(finalActionList, toa);
				// Fees
				sss = coreServerSupportedMessageFind(coin);
				if (sss == NULL) {
					PRINTF("Error: token not found: message: %s.", bchainPrettyPrintTokenMsgContent(bix));
					blockFinalizeNoteAccountNotification(finalActionList, BFA_ACCOUNT_NOTIFICATION_ON_FAIL, coin, account, "Error: Token not found: message: %s.", bchainPrettyPrintTokenMsgContent(bix));
					res = -1;
				} else {
					fees = jsVarGetEnvDouble(sss->opt, "fees", 0);
					fea = bAccountBalanceLoadOrFind(coin, jsVarGetEnv_st(sss->opt, "base_account"), &currentBchainContext);

					bixfees = 0;  
					bixmsa = bixbsa = NULL;
					if (strcmp(coin, MESSAGE_DATATYPE_BYTECOIN) != 0) {
						bixsss = coreServerSupportedMessageFind(MESSAGE_DATATYPE_BYTECOIN);
						bixmsa = bAccountBalanceLoadOrFind(MESSAGE_DATATYPE_BYTECOIN, sss->masterAccount, &currentBchainContext);
						bixbsa = bAccountBalanceLoadOrFind(MESSAGE_DATATYPE_BYTECOIN, BYTECOIN_BASE_ACCOUNT, &currentBchainContext);
						if (bixsss != NULL && bixmsa != NULL && bixbsa != NULL) {
							bixfees = jsVarGetEnvDouble(bixsss->opt, "tokenfees", 0);
						}
					}

					if (fra->balance < amount + fees) {
						PRINTF("Error: insufficient balance: message: %s.", bchainPrettyPrintTokenMsgContent(bix));
						blockFinalizeNoteAccountNotification(finalActionList, BFA_ACCOUNT_NOTIFICATION_ON_FAIL, coin, account, "Error: Insufficient balance: message: %s.", bchainPrettyPrintTokenMsgContent(bix));
						res = -1;
					} else if (fra->balance / amount > 1.0E9) {
						PRINTF("Error: disproportional transaction: message: %s.", bchainPrettyPrintTokenMsgContent(bix));
						blockFinalizeNoteAccountNotification(finalActionList, BFA_ACCOUNT_NOTIFICATION_ON_FAIL, coin, account, "Error: Disproportional transaction: message: %s.", bchainPrettyPrintTokenMsgContent(bix));
						res = -1;
					} else if (bixmsa != NULL && bixmsa->balance < bixfees) {
						PRINTF("Error: insufficient balance on token master account %s: message: %s.", sss->masterAccount, bchainPrettyPrintTokenMsgContent(bix));
						blockFinalizeNoteAccountNotification(finalActionList, BFA_ACCOUNT_NOTIFICATION_ON_FAIL, coin, account, "Error: Insufficient balance on token master account %s: message: %s.", sss->masterAccount, bchainPrettyPrintTokenMsgContent(bix));
						res = -1;						
					} else if (fea->balance + fees < 0) {
						PRINTF("Error: insufficient balance on base account: message: %s.", bchainPrettyPrintTokenMsgContent(bix));
						blockFinalizeNoteAccountNotification(finalActionList, BFA_ACCOUNT_NOTIFICATION_ON_FAIL, coin, account, "Error: Insufficient balance on base account: message: %s.", bchainPrettyPrintTokenMsgContent(bix));
						res = -1;
					} else {
						fra->balance -= amount;
						toa->balance += amount;
						if (fees != 0 && fea != NULL) {
							blockFinalizeNoteAccountBalanceModification(finalActionList, fea);
							fea->balance += fees;
							fra->balance -= fees;

						}
						if (bixfees != 0 && bixmsa != NULL && bixbsa != NULL) {
							blockFinalizeNoteAccountBalanceModification(finalActionList, bixmsa);
							blockFinalizeNoteAccountBalanceModification(finalActionList, bixbsa);
							bixbsa->balance += bixfees;
							bixmsa->balance -= bixfees;
							blockFinalizeNoteAccountNotification(finalActionList, BFA_ACCOUNT_NOTIFICATION_ON_SUCCESS, MESSAGE_DATATYPE_BYTECOIN, sss->masterAccount, "Info: Message %s: Transaction fees %f BIX deduced from %s: transfer from %s to %s in %s.", msg->msgId, bixfees, bixmsa->myAccount->account, fra->myAccount->account, toa->myAccount->account, coin);
						}
						blockFinalizeNoteAccountNotification(finalActionList, BFA_ACCOUNT_NOTIFICATION_ON_SUCCESS, coin, account, "Info: Message %s: executed: %s", msg->msgId, bchainPrettyPrintTokenMsgContent(bix));
						res = 0;
					}
				}
			}
		}
	}
	return(res);
}

static int onCreateNewCoin(struct bchainMessage *msg, struct bixParsingContext *mbixc, struct bFinalActionList **finalActionList) {
	struct supportedMessageStr 	smsg, *smm;
	char 						*cointype, *coin;
	struct bAccountBalance 		*aa;
	int 						res;
	char						ttt[TMP_STRING_SIZE];

	// create new coin type
	coin = mbixc->tag[BIX_TAG_cnm];
	cointype = mbixc->tag[BIX_TAG_ctp];
	res = -1;			
	if (coreServerSupportedMessageIsSupported(coin)) {
		PRINTF("Error: can't create coin. Coin exists: %s.\n", coin);
		res = -1;			
	} else {
		smm = supportedMessagesFindMetaTypeTemplate(cointype);
		if (smm == NULL) {
			PRINTF("Error: unknown coin type %s.\n", cointype);
			res = -1;			
		} else {
			aa = bAccountBalanceLoadOrFind(messageDataTypeToAccountCoin(msg->dataType), msg->account, &currentBchainContext);
			if (aa != NULL && aa->balance >= BIX_NEW_TOKEN_CREATION_FEES) {
				blockFinalizeNoteMsgTabModification(finalActionList);
				smsg = *smm;
				smsg.messageType = coin;
				smsg.masterAccount = msg->account;
				coreServerSupportedMessageCreateCopyWithAdditionalMsgType(&smsg);
				blockFinalizeNoteAccountBalanceModification(finalActionList, aa);
				aa->balance -= BIX_NEW_TOKEN_CREATION_FEES;
				res = 0;
			} else {
				// TODO: notify user that he has not enough of balance
			}
		}
	}
	return(res);
}

static int onNewCoinOptions(struct bchainMessage *msg, struct bixParsingContext *mbixc, uint64_t blockSeq, struct bFinalActionList **finalActionList) {
	double 						amount, fee;
	double						tokencapitalizationfeepercent;
	struct bAccountBalance 		*aa, *capfeeaa;
	struct supportedMessageStr 	*smm, *bsmm;
	struct bAccountBalance 		*newbase, *oldbase;
	char						*oldbname, newbname;
	char 						*newcoinopt, *cointype, *coin;
	int 						setBaseBalanceFlag, res;
	struct jsVarDynamicString	*ss;

	// create new coin type
	res = -1;
	coin = mbixc->tag[BIX_TAG_cnm];
	cointype = mbixc->tag[BIX_TAG_ctp];
	newcoinopt = mbixc->tag[BIX_TAG_opt];
	smm = coreServerSupportedMessageFind(coin);
	if (smm == NULL) {
		PRINTF("Error: can't set coin options. Coin does not exist: %s.\n", coin);
		res = -1;			
	} else if (
		(smm->masterAccount == NULL || msg->account == NULL || strcmp(smm->masterAccount,  msg->account) != 0)
		// there is an exception for the genesis block where the core sever 0 can set parameters of BIX
		&& (! (msg->account != NULL && blockSeq == 0 && strcmp(msg->account, getAccountName_st(0, 0)) == 0))
		) {
		// TODO: Probably a simple string checking may be not enough. Check that those two accounts are actually the same (including token type)
		// maybe load the both of them with bAccountLoadOrFind and check
		PRINTF("Error: can't set coin options. Not owner account.\n", coin);
		res = -1;			
	} else {
		setBaseBalanceFlag = 1;
		blockFinalizeNoteMsgTabModification(finalActionList);
		coreServerSupportedMessagesTabCreateCopy();
		// get the new freshly created copy of smm
		smm = coreServerSupportedMessageFind(coin);
		assert(smm != NULL);
		oldbase = bAccountBalanceLoadOrFind(coin, jsVarGetEnv_st(smm->opt, "base_account"), &currentBchainContext);
		newbase = bAccountBalanceLoadOrFind(coin, jsVarGetEnv_st(newcoinopt, "base_account"), &currentBchainContext);
		capfeeaa = bAccountBalanceLoadFindOrCreateMulticurrency(coin, BYTECOIN_BASE_ACCOUNT, &currentBchainContext);
		if (newbase == NULL) {
			PRINTF("Error: can't set coin options. Base account %s in %s does not exists.\n", jsVarGetEnv_st(newcoinopt, "base_account"), coin);
			res = -1;
		} else if (capfeeaa == NULL) {
			PRINTF("Internal Error: can't get %s base account in %s.\n", MESSAGE_DATATYPE_BYTECOIN, coin);
			res = -1;
		} else {
			bsmm = coreServerSupportedMessageFind(MESSAGE_DATATYPE_BYTECOIN);
			if (bsmm != NULL) tokencapitalizationfeepercent = jsVarGetEnvDouble(bsmm->opt, "tokencapitalizationfeepercent", -1);
			// this is not a problem for genesis block
			if ((bsmm == NULL || tokencapitalizationfeepercent < 0) && blockSeq != 0) {
				PRINTF("Warning: can't get %s options or wrong tokencapitalizationfeepercent value.\n", MESSAGE_DATATYPE_BYTECOIN);
				res = -1;
			} else {
				aa =  bAccountBalanceLoadOrFind(MESSAGE_DATATYPE_BYTECOIN, smm->masterAccount,  &currentBchainContext);
				if ((aa != NULL && aa->balance >= BIX_NEW_TOKEN_OPTION_UPDATE_FEES) || blockSeq == 0 ) {
					// set also the initial balance for the account
					if (oldbase == NULL) {
						// new options, coin initialization
						amount = jsVarGetEnvDouble(newcoinopt, "base_balance", -1);
						blockFinalizeNoteAccountBalanceModification(finalActionList, newbase);
						blockFinalizeNoteAccountBalanceModification(finalActionList, capfeeaa);
						fee = amount * tokencapitalizationfeepercent / 100.0;
						newbase->balance = amount - fee;
						capfeeaa->balance += fee;
					} else {
						if (oldbase != newbase) {
							// base account switch
							blockFinalizeNoteAccountBalanceModification(finalActionList, oldbase);
							blockFinalizeNoteAccountBalanceModification(finalActionList, newbase);
							newbase->balance += oldbase->balance;
							oldbase->balance = 0;
						}
					}
					blockFinalizeNoteCoinOptionsModification(finalActionList, &smm->opt);
					smm->opt = strDuplicate(newcoinopt);
// PRINTF("SETTING %s options to %p, %s\n", coin, smm, smm->opt);
					if (aa != NULL) {
						blockFinalizeNoteAccountBalanceModification(finalActionList, aa);
						aa->balance -= BIX_NEW_TOKEN_OPTION_UPDATE_FEES;
					}
					res = 0;
				} else {
					// TODO: notify with message
					res = -1;
				}
			}
		}
	}
	return(res);
}


int pendingApprovalsPutMessage(struct bchainMessage *msg, struct bixParsingContext *mbixc, struct bFinalActionList **finalActionList) {
	char			*reqname;

	reqname = strDuplicate(mbixc->tag[BIX_TAG_pgr]);
	bixMakeTagsSemicolonTerminating(mbixc);
	blockFinalizeNotePendingRequestInsert(finalActionList, reqname, msg->data, msg->dataLength);
	bixMakeTagsNullTerminating(mbixc);
	FREE(reqname);

	return(0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// bytecoins


int applyCoinBlockchainMessage(struct bchainMessage *msg, struct blockContentDataList *fromBlock, struct bFinalActionList **finalActionList) {
	int							i, r, res;
	char						*coin;
	char						*account;
	struct bAccountBalance 		*aa, *fra, *toa;
	double						amount;
	int							nonce;
	int64_t						miningTime;
	struct bixParsingContext	*mbixc;
	char						*mvl;

	coin = msg->dataType;
	res = -1;

	// PRINTF("VERIFY MSG %s\n", dat);
	mbixc = bixParsingContextGet();
	r = bixParseString(mbixc, msg->data, msg->dataLength);
	if (r != 0) {
		PRINTF("Internal error: wrongly formed coin message: %s\n", msg->data);
		bixParsingContextFree(&mbixc);
		return(-1);
	}

	bixMakeTagsNullTerminating(mbixc);
	account = mbixc->tag[BIX_TAG_acc];

	// if this is not the one of messages not requiring signature, check signature
	mvl = mbixc->tag[BIX_TAG_mvl];
	if (1
		// Unsigned messages
		// && strcmp(mvl, "") != 0
		&& ! msg->signatureVerifiedFlag
		) {
		PRINTF("Error: message %s has wrong signature.\n", mvl);
		res = -1;
	} else if (1
			   // Messages where acc tag does not need to match the signing account
			   && strcmp(mvl, "SendCoins") != 0
			   && strcmp(mvl, "CreateNewCoin") != 0
			   && strcmp(mvl, "CoinOptions") != 0
			   && strcmp(mvl, "Mining") != 0
			   // && strcmp(mvl, "") != 0
			   && strcmp(msg->account, mbixc->tag[BIX_TAG_acc]) != 0
		) {
		PRINTF("Error: account mismatch %s <> %s in message %s.", msg->account, mbixc->tag[BIX_TAG_acc], mvl);
		res = -1;
	} else if (mbixc->tagLen[BIX_TAG_pgr] > 0) {
		// pending request group non NULL, put it into list of pending approvals
		res = pendingApprovalsPutMessage(msg, mbixc, finalActionList);
	} else if (strcmp(mbixc->tag[BIX_TAG_mvl], "SendCoins") == 0) {
		// coin transfer
		res = coinOnSendCoins(msg, mbixc, msg->signature, coin, 0, finalActionList);
	} else if (strcmp(mbixc->tag[BIX_TAG_mvl], "CreateNewCoin") == 0) {
		res = onCreateNewCoin(msg, mbixc, finalActionList);
	} else if (strcmp(mbixc->tag[BIX_TAG_mvl], "CoinOptions") == 0) {
		res = onNewCoinOptions(msg, mbixc, fromBlock->blockSequenceNumber, finalActionList);
	} else if (strcmp(mbixc->tag[BIX_TAG_mvl], "Mining") == 0) {
		// Mining: only the first message per mining period, all others are discarted
		// This does not work durin resynchronization
		miningTime = atoi(mbixc->tag[BIX_TAG_tim]);
		// 1511527182 < 1511527123.
		if (miningTime >= (int64_t)currentTime.sec + BCHAIN_MINING_PERIOD_SEC*10/100 || node->lastMiningTime >= miningTime - BCHAIN_MINING_PERIOD_SEC*90/100) {
			// PRINTF("Error: an attempt to mine coins out of the right time. %"PRId64" < %"PRId64" && %"PRId64" < %"PRId64".\n", miningTime, (int64_t)currentTime.sec + BCHAIN_MINING_PERIOD_SEC*10/100, node->lastMiningTime , miningTime - BCHAIN_MINING_PERIOD_SEC*90/100);
			// PRINTF("Error: an attempt to mine coins out of the right time.\n", 0);
			res = -1;
		} else {
			// TODO: Check mining award amount
			amount = strtod(mbixc->tag[BIX_TAG_amt], NULL);
			res = 0;
			for(i=0; i<coreServersMax; i++) {
				// aa = bAccountLoadFindOrCreate(coin, getAccountName_st(0, i) , &currentBchainContext);
				aa = bAccountBalanceLoadOrFind(coin, getAccountName_st(0, i) , &currentBchainContext);
				if (aa == NULL) {
					PRINTF("Internal error: invalid mining account for server %d\n", i);
				} else if (aa->myAccount->d.lastMiningTime >= miningTime - BCHAIN_MINING_PERIOD_SEC*90/100) {
					// remove redundant mining messages
					res = -1;
				} else {
					blockFinalizeNoteAccountBalanceModification(finalActionList, aa);
					blockFinalizeNoteAccountModification(finalActionList, aa->myAccount);
					// if (applyChangesFlag) PRINTF("Mining %f coins into account %s.\n", amount, aa->account);
					aa->balance += amount;
					aa->myAccount->d.lastMiningTime = miningTime;
				}
			}
		}
	} else {
		PRINTF("Internal error: unknown coin operation: %s\n", mbixc->tag[BIX_TAG_mvl]);
		res = -1;
	}

	bixMakeTagsSemicolonTerminating(mbixc);
	bixParsingContextFree(&mbixc);
	return(res);
}

////////

static double powcoinGetMiningAward(char *coin) {
	double 						amount;
	struct supportedMessageStr 	*sss;
	sss = coreServerSupportedMessageFind(coin);
	if (sss == NULL) {
		PRINTF("internal error: powcoinGetMiningAward: unknown coin: %s\n", coin);
		return(-1);
	}
	amount = jsVarGetEnvDouble(sss->opt, "mining_award", 0);
	return(amount);
}

static double powcoinGetMiningDifficulty(char *coin) {
	double 						res;
	struct supportedMessageStr 	*sss;

	sss = coreServerSupportedMessageFind(coin);
	if (sss == NULL) {
		PRINTF("internal error: powcoinGetMiningDifficulty: unknown coin: %s\n", coin);
		return(-1);
	}
	res = jsVarGetEnvDouble(sss->opt, "mining_difficulty", -1);
	return(res);
}

void powcoinHash(char *blockdata, int blockdatasize, char *blockHash) {
	char	shahash[SHA512_DIGEST_LENGTH];

	SHA512(blockdata, blockdatasize, shahash);
	SHA512(shahash, SHA512_DIGEST_LENGTH, blockHash);
}

static int povcoinHashCheck(char *coin, unsigned char *blockHash) {
	int							i, difficulty;

	difficulty = powcoinGetMiningDifficulty(coin);
	if (difficulty <= 0) {
		PRINTF("Invalid mining difficulty for %s\n", coin);
		return(-1);
	}

	for(i=0; i<difficulty; i++) {
		if (((blockHash[i/8]>>(i%8)) & 0x01) != 0) return(1);
	}
	return(0);
}

int applyPowcoinBlockchainMessage(struct bchainMessage *msg, struct blockContentDataList *fromBlock, struct bFinalActionList **finalActionList) {
	int											i, r, res;
	char										*coin;
	struct bAccountBalance 						*aa, *fra, *toa;
	double										amount, mamount;
	int											nonce;
	int64_t										miningTime;
	struct powcoinBlockPendingMessagesListStr 	ttt, *tt, *memb;
	struct bixParsingContext 					*bix, *mbixc, *mbixnc;
	int											miningMsgCount;
	char										blockHash[SHA512_DIGEST_LENGTH];
	char										*mvl;

	coin = msg->dataType;
	res = -1;

	// PRINTF("VERIFY MSG %s\n", dat);
	// I have to calculate hash before I parse the message and put zeros everywhere
	powcoinHash(msg->data, msg->dataLength, blockHash);

	mbixc = bixParsingContextGet();
	r = bixParseString(mbixc, msg->data, msg->dataLength);
	if (r != 0) {
		PRINTF("Internal error: wrongly formed coin message: %s\n", msg->data);
		bixParsingContextFree(&mbixc);
		return(-1);
	}

	bixMakeTagsNullTerminating(mbixc);
	// if this is not the one of messages not requiring signature, check signature
	mvl = mbixc->tag[BIX_TAG_mvl];
	if (1
		&& strcmp(mvl, "PowMinedBlock") != 0
		// && strcmp(mvl, "") != 0
		&& ! msg->signatureVerifiedFlag
		) {
		PRINTF("Error: message %s has wrong signature.\n", mvl);
		res = -1;
	} else if (1
			   // Messages where acc tag does not need to match the signing account
			   && strcmp(mvl, "SendCoins") != 0
			   && strcmp(mvl, "PowMinedBlock") != 0
			   // && strcmp(mvl, "") != 0
			   && strcmp(msg->account, mbixc->tag[BIX_TAG_acc]) != 0
		) {
		PRINTF("Error: account mismatch %s <> %s in message %s.", msg->account, mbixc->tag[BIX_TAG_acc], mvl);
		res = -1;
	} else if (mbixc->tagLen[BIX_TAG_pgr] > 0) {
		// pending request group non NULL, put it into list of pending approvals
		res = pendingApprovalsPutMessage(msg, mbixc, finalActionList);
	} else if (strcmp(mbixc->tag[BIX_TAG_mvl], "SendCoins") == 0) {
		// coin transfer, only check validation
		res = coinOnSendCoins(msg, mbixc, msg->signature, coin, 1, finalActionList);
		if (res == 0) {
			// Operation seems valid, put into the list of pending operations
			ALLOC(tt, struct powcoinBlockPendingMessagesListStr);
			memset(tt, 0, sizeof(*tt));
			// here the whole message has to be copied, so I must make it back semicolon terminating
			bixMakeTagsSemicolonTerminating(mbixc);
			bchainMessageCopy(&tt->msg, msg);
			bixMakeTagsNullTerminating(mbixc);
			tt->fromBlockSequenceNumber = fromBlock->blockSequenceNumber;
			// PRINTF("Adding to pending list %s\n", tt->msg.msgId);
			SGLIB_DL_LIST_ADD(struct powcoinBlockPendingMessagesListStr, powcoinBlockPendingMessagesList, tt, prev, next);
			blockFinalizeNotePowCoinAddMessage(finalActionList, tt);
		}
	} else if (strcmp(mbixc->tag[BIX_TAG_mvl], "PowMinedBlock") == 0) {
		if (povcoinHashCheck(coin, blockHash)) {
			PRINTF0("error: powMinedBlock with wrong hash was submitted to the blockchain.\n");
			res = -1;
		} else {
			res = 0;
			// apply transactions from within the block
			// PRINTF("PARSING MINED BLOCK: %s\n", mbixc->tag[BIX_TAG_mss]);
			miningMsgCount = 0;
			mbixnc = bixParsingContextGet();
			BIX_PARSE_AND_MAP_STRING(mbixc->tag[BIX_TAG_mss], mbixc->tagLen[BIX_TAG_mss], mbixnc, 1, {
					// check our coin messages which did not expired
					if (strncmp(tagName, "dat", 3) == 0) {
						// PRINTF("MINED BLOCK CHECK message: %s\n", tagValue);
						bix = bixParsingContextGet();
						bixParseString(bix, tagValue, tagValueLength);
						bixMakeTagsNullTerminating(bix);
						r = 0;
						if (strcmp(bix->tag[BIX_TAG_mvl], "SendCoins") == 0) {
							// TODO: check that the message is in powcoinBlockPendingMessagesList and remove it from the list !!!
							// here the whole message is checked, so I must make it back semicolon terminating
							bixMakeTagsSemicolonTerminating(bix);
							bixFillParsedBchainMessage(mbixnc, &ttt.msg);
							// PRINTF("looking for %s\n", ttt.msg.msgId);
							SGLIB_DL_LIST_FIND_MEMBER(struct powcoinBlockPendingMessagesListStr, powcoinBlockPendingMessagesList, &ttt, powcoinListMsgCmp, prev, next, memb);
							bixMakeTagsNullTerminating(bix);
							if (memb == NULL) {
								PRINTF0("Error: message included in pow mined block does not exists\n");
								r = -1;
							} else {
								// PRINTF("Applying pow msg/transaction %s\n", ttt.msg.msgId);									
								r = coinOnSendCoins(&ttt.msg, bix, mbixnc->tag[BIX_TAG_sig], coin, 0, finalActionList);
								SGLIB_DL_LIST_DELETE(struct powcoinBlockPendingMessagesListStr, powcoinBlockPendingMessagesList, memb, prev, next);
								blockFinalizeNotePowCoinDelMessage(finalActionList, memb, fromBlock->blockSequenceNumber, msg->msgId);
							}
						} else if (strcmp(bix->tag[BIX_TAG_mvl], "Mining") == 0) {
							miningMsgCount ++;
							if (miningMsgCount != 1) {
								PRINTF0("Error: multiple mining messages in pow mining message\n");
								r = -1;
							} else {									
								amount = powcoinGetMiningAward(coin);
								mamount = strtod(bix->tag[BIX_TAG_amt], NULL);
								// aa = bAccountLoadFindOrCreate(coin, bix->tag[BIX_TAG_acc], &currentBchainContext);
								aa = bAccountBalanceLoadOrFind(coin, bix->tag[BIX_TAG_acc], &currentBchainContext);
								if (aa == NULL) {
									PRINTF0("Error: wrong mining account in pow mining message\n");
									blockFinalizeNoteAccountNotification(finalActionList, BFA_ACCOUNT_NOTIFICATION_ON_FAIL, coin, bix->tag[BIX_TAG_acc], "%s: Error: Mined block rejected: wrong mining account %s", SPRINT_CURRENT_TIME(), bix->tag[BIX_TAG_acc]);
									r = -1;
								} else if (0 && amount != mamount) {	// javascript mining does not put amount into message
									PRINTF("Error: wrong mined amount in pow mining message: %f <-> %f\n", amount, mamount);
									r = -1;
								} else {
									blockFinalizeNoteAccountBalanceModification(finalActionList, aa);
									blockFinalizeNoteAccountNotification(finalActionList, BFA_ACCOUNT_NOTIFICATION_ON_SUCCESS, coin, bix->tag[BIX_TAG_acc], "%s: Info: Mined block accepted: paid mining reward %f %s to %s", SPRINT_CURRENT_TIME(), amount, coin, bix->tag[BIX_TAG_acc]);
									aa->balance += amount;
									r = 0;
								}
							}
						}
						bixMakeTagsSemicolonTerminating(bix);
						bixParsingContextFree(&bix);
						if (r != 0) {
							res = -1;
							break;
						}
					}
				},
				{
					PRINTF0("Error while parsing block content\n");
					res = -1;
				}
				);
			bixMakeTagsSemicolonTerminating(mbixnc);
			bixParsingContextFree(&mbixnc);
		}
	} else {
		PRINTF("Internal error: unknown powcoin operation: %s\n", mbixc->tag[BIX_TAG_mvl]);
		res = -1;
	}

	bixMakeTagsSemicolonTerminating(mbixc);
	bixParsingContextFree(&mbixc);
	return(res);
}

////////

#define POW_COIN_EXPIRATION_USEC 	(60*60*1000000LL)

struct minerMessageList {
	struct bchainMessage		msg;
	struct minerMessageList		*next;
};

struct miningStatusStr {
	struct jsVarDynamicString	*blockContent;
	long long					minTimeStamp, maxTimeStamp;
	int							activeMiningFlag;
	char						nonce[TMP_STRING_SIZE];
	int							nonceSize;
};

struct miningCoinTree {
	char						*coin;
	char						*account;

	struct minerMessageList		*powcoinMinerPendingMessages;
	struct miningStatusStr		currentMiningStatus;
	long long					powcoinMinerMinimalTimeStampForNextBlock;

	// red black tree data
	uint8_t						color;
	struct miningCoinTree		*leftSubtree, *rightSubtree;
};

static int miningCoinTreeComparator(struct miningCoinTree *t1, struct miningCoinTree *t2) {
	int r;
	r = strcmp(t1->coin, t2->coin);
	if (r != 0) return(r);
	return(0);
}

typedef struct miningCoinTree miningTree;
SGLIB_DEFINE_RBTREE_PROTOTYPES(miningTree, leftSubtree, rightSubtree, color, miningCoinTreeComparator);
SGLIB_DEFINE_RBTREE_FUNCTIONS(miningTree, leftSubtree, rightSubtree, color, miningCoinTreeComparator);

struct miningCoinTree	*powcoinMinerTree;

static int minerMessageListTimestampComparator(struct minerMessageList *m1, struct minerMessageList *m2) {
	if (m1->msg.timeStampUsec < m2->msg.timeStampUsec) return(-1);
	if (m1->msg.timeStampUsec > m2->msg.timeStampUsec) return(1);
	return(0);
}

struct miningCoinTree *powminerGetOrCreateContext(char *coin) {
	struct miningCoinTree		ttt, *memb;
	struct supportedMessageStr 	*sss;
	char						*acc, *cointype;

	ttt.coin = coin;
	memb = sglib_miningTree_find_member(powcoinMinerTree, &ttt);
	if (memb == NULL) {
		// PRINTF("maybe creating context for coin %s\n", coin);
		acc = coinBaseAccount_st(coin);
		// PRINTF("maybe creating context for coin %s, base account == %s\n", coin, acc);
		if (acc == NULL) return(NULL);
		cointype = coinType_st(coin);
		// PRINTF("maybe creating context for coin %s, cointype == %s\n", coin, cointype);
		if (strcmp(cointype, MESSAGE_METATYPE_POW_TOKEN) != 0) return(NULL);

		ALLOC(memb, struct miningCoinTree);
		memset(memb, 0, sizeof(*memb));
		memb->coin = strDuplicate(coin);
		memb->account = strDuplicate(coinBaseAccount_st(coin));
		memb->powcoinMinerPendingMessages = NULL;
		memb->powcoinMinerMinimalTimeStampForNextBlock = 0;
		sglib_miningTree_add(&powcoinMinerTree, memb);
	}
	return(memb);
}

static void powminerMessageListAppendMsg(struct bchainMessage *msg) {
	struct minerMessageList	*mm;
	struct miningCoinTree 	*ccc;

	// PRINTF("minerMessageListAppendMsg: %s\n", msg->msgId);
	ccc = powminerGetOrCreateContext(msg->dataType);
	if (ccc == NULL || ccc->account == NULL) return;

	// PRINTF("minerMessageListAppendMsg2: %s\n", msg->msgId);
	ALLOC(mm, struct minerMessageList);
	bchainMessageCopy(&mm->msg, msg);
	mm->next = ccc->powcoinMinerPendingMessages;
	ccc->powcoinMinerPendingMessages = mm;
}

static int minerMessageListRemoveMsg(struct miningCoinTree *ccc, char *msgId) {
	struct minerMessageList	*mm, **mmm;

	// PRINTF("minerMessageListRemoveMsg: %s\n", msgId);
	for(mmm=&ccc->powcoinMinerPendingMessages; *mmm!=NULL; mmm= &(*mmm)->next) {
		mm = *mmm;
		if (strcmp(mm->msg.msgId, msgId) == 0) {
			*mmm = mm->next;
			bchainFreeMessageContent(&mm->msg);
			FREE(mm);
			return(0);
		}
	}
	return(-1);
}

int coinIndexInGuiSupportedTokenTypesVector(char *coin) {
	int i,j;

	i = coreServerSupportedMessageFindIndex(coin);
	j = coreServerSupportedMessageFindIndex(MESSAGE_DATATYPE_LAST_NON_COIN);
	if (i < 0 || j < 0) return(-1);
	return(i - j - 1);
}

static void minerSetBlockForJavascriptMiners(char *coin, char *block) {
	int i, dim, gi;

	gi = coinIndexInGuiSupportedTokenTypesVector(coin);
	if (gi >= 0 && guiMinerCurrentlyMinedBlocksVector != NULL) {
		dim = guiMinerCurrentlyMinedBlocksVector->arrayDimension;
		if (dim <= gi) {
			jsVarResizeVector(guiMinerCurrentlyMinedBlocksVector, gi+1);
			for(i=dim; i<gi; i++) jsVarSetStringVector(guiMinerCurrentlyMinedBlocksVector, i, strDuplicate(""));
		} else {
			// free previous value
			FREE(jsVarGetStringVector(guiMinerCurrentlyMinedBlocksVector, gi));
		}
		jsVarSetStringVector(guiMinerCurrentlyMinedBlocksVector, gi, strDuplicate(block));
	}
}

void powcoinParseReceivedBlocks() {
	int 						i, r, ii;
	struct jsVarDynamicString	*ss;
	struct jsVarDynamicString	*bss;
	uint64_t					seq;
	struct blockContentDataList *bb;
	struct bixParsingContext 	*bix;
	struct bchainMessage		mmm, *msg;
	struct bixParsingContext 	*mbixnc, *mbixnnc;
	struct miningCoinTree 		*ccc;

	// collect coin messages from newly received blocks
	while (node->powMiningBlockSequenceNumber < node->currentBlockSequenceNumber) {
		bb = bchainCreateBlockContentLoadedFromFile(node->powMiningBlockSequenceNumber);
		if (bb == NULL) GOTO_WITH_MSG(nextblock, "internal error: powcoinMineBlock: block %"PRId64" not found.\n", node->powMiningBlockSequenceNumber);
		PRINTF("universalMiner: Parsing block %"PRIu64"\n", node->powMiningBlockSequenceNumber);
		bix = bixParsingContextGet();
		BIX_PARSE_AND_MAP_STRING(bb->blockContent, bb->blockContentLength, bix, 1, {
				// check our coin messages which did not expired
				if (strncmp(tagName, "dat", 3) == 0) {
					ccc = powminerGetOrCreateContext(bix->tag[BIX_TAG_mdt]);
					// PRINTF("powcoinMineBlock: check to append msg to mined block: msg id: %s (%s) body: %s\n", bix->tag[BIX_TAG_mid], bix->tag[BIX_TAG_mdt], tagValue);
					if (ccc != NULL && ccc->account != NULL) {
						mbixnc = bixParsingContextGet();
						bixParseString(mbixnc, tagValue, tagValueLength);
						bixMakeTagsNullTerminating(mbixnc);
						r = -1;
						// PRINTF("TEST strcmp %s %s\n", mbixnc->tag[BIX_TAG_mvl], "SendCoins");
						if (strcmp(mbixnc->tag[BIX_TAG_mvl], "PowMinedBlock") == 0) {
							// Somebody (maybe even me) has mined the block. Remove all accepted messages from pending list and abort currently mined block
							mbixnnc = bixParsingContextGet();						
							BIX_PARSE_AND_MAP_STRING(mbixnc->tag[BIX_TAG_mss], mbixnc->tagLen[BIX_TAG_mss], mbixnnc, 0, {
									// check our coin messages which did not expired
									if (strncmp(tagName, "mid", 3) == 0) {
										minerMessageListRemoveMsg(ccc, tagValue);
									} else if (strncmp(tagName, "tsu", 3) == 0) {
										ccc->powcoinMinerMinimalTimeStampForNextBlock = atoll(tagValue);
									}
								},
								{
									PRINTF0("Error while parsing block content\n");
								}
								);
							bixParsingContextFree(&mbixnnc);
							bixMakeTagsSemicolonTerminating(mbixnc);
							if (ccc->currentMiningStatus.activeMiningFlag 
								&& ccc->powcoinMinerMinimalTimeStampForNextBlock >= ccc->currentMiningStatus.minTimeStamp
								) {
								PRINTF("universalMiner: Stop mining %s block.\n", ccc->coin);
								jsVarDstrFree(&ccc->currentMiningStatus.blockContent);
								ccc->currentMiningStatus.activeMiningFlag = 0;
								minerSetBlockForJavascriptMiners(ccc->coin, "");
							}
						} else if (strcmp(mbixnc->tag[BIX_TAG_mvl], "SendCoins") == 0) {
							bixMakeTagsSemicolonTerminating(mbixnc);
							// append the whole message to messages for the next block
							bixFillParsedBchainMessage(bix, &mmm);
							// PRINTF("Checking %"PRId64" > %lld == %d\n", mmm.timeStampUsec, ccc->powcoinMinerMinimalTimeStampForNextBlock, mmm.timeStampUsec > ccc->powcoinMinerMinimalTimeStampForNextBlock);
							if (mmm.timeStampUsec >= ccc->powcoinMinerMinimalTimeStampForNextBlock) {
								powminerMessageListAppendMsg(&mmm);
							}
						} else {
							bixMakeTagsSemicolonTerminating(mbixnc);
						}
						bixParsingContextFree(&mbixnc);
					}
				}
			},
			{
				PRINTF0("Error while parsing block content");
			}
			);
		bixMakeTagsSemicolonTerminating(bix);
		bixParsingContextFree(&bix);
		bchainBlockContentFree(&bb);

	nextblock:;
		node->powMiningBlockSequenceNumber ++;
	}

}

static void powcoinMinerIncrementNonce(struct miningCoinTree *ttt) {
	unsigned char	*s;
	int				i, n;

	s = (unsigned char *) ttt->currentMiningStatus.nonce;
	n = ttt->currentMiningStatus.nonceSize;
	for(i=0; i<n && s[i] == 255; i++) s[i] = 0;
	if (i == n) {
		ttt->currentMiningStatus.nonceSize ++;
		s[i] = 0;
	} else {
		s[i] ++;
	}
#if 0
	PRINTF("miner: checking nonce: %s: ", ttt->coin);
	for(i=0; i<ttt->currentMiningStatus.nonceSize; i++) PRINTF2("%d ", s[i]);
	PRINTF2("\n", 0);
#endif
}

static int powcoinMiningAppendMiningMessage(char *coin, char *minerAccount, struct jsVarDynamicString *ss) {
	struct bchainMessage 		mmm;
	char						mmsg[TMP_STRING_SIZE];
	struct supportedMessageStr	*sss;
	double						amount;

	// TODO: Put mining amount into mining message
	amount = powcoinGetMiningAward(coin);
	if (amount < 0) return(-1);
	sprintf(mmsg, "mvl=Mining;acc=%s;amt=%a;non=%"PRIu64";", minerAccount, amount, genMsgNonce());
	bchainFillMsgStructure(&mmm, "mining", minerAccount, "", "", currentTime.usec, coin, strlen(mmsg), mmsg, "unsigned", NULL);
	bchainBlockContentAppendSingleMessage(ss, &mmm);
	return(0);
}

long long powcoinMine() {
	int 								i, gi, r, ii, ssize;
	struct jsVarDynamicString			*ss;
	struct minerMessageList				*mm;
	char 								*coin;
	char								*minerAccount;
	char								blockHash[SHA512_DIGEST_LENGTH];
	struct sglib_miningTree_iterator	iii;
	struct miningCoinTree				*ttt;
	long long							res;

	res = MINING_DELAY_100MS;

	for(ttt = sglib_miningTree_it_init(&iii, powcoinMinerTree);
		ttt != NULL;
		ttt = sglib_miningTree_it_next(&iii)
		) {
		
		coin = ttt->coin;
		minerAccount = ttt->account;
		
		if (coin != NULL && minerAccount != NULL) {
			if (ttt->currentMiningStatus.activeMiningFlag == 0) {
				// collect coin messages into a pow block
				SGLIB_LIST_SORT(struct minerMessageList, ttt->powcoinMinerPendingMessages, minerMessageListTimestampComparator, next);
				while (ttt->powcoinMinerPendingMessages != NULL && 
					   (ttt->powcoinMinerPendingMessages->msg.timeStampUsec < ttt->powcoinMinerMinimalTimeStampForNextBlock
						|| ttt->powcoinMinerPendingMessages->msg.timeStampUsec < currentTime.usec - POW_COIN_EXPIRATION_USEC
						   )) {
					mm = ttt->powcoinMinerPendingMessages;
					ttt->powcoinMinerPendingMessages = mm->next;
					bchainFreeMessageContent(&mm->msg);
					FREE(mm);
				}
				if (ttt->powcoinMinerPendingMessages != NULL) {
					// start mining a new block if there are some pending messages
					ss = jsVarDstrCreate();
					ttt->currentMiningStatus.minTimeStamp = ttt->powcoinMinerPendingMessages->msg.timeStampUsec;
					for (mm = ttt->powcoinMinerPendingMessages; mm != NULL; mm=mm->next) {
						bchainBlockContentAppendSingleMessage(ss, &mm->msg);
						ttt->currentMiningStatus.maxTimeStamp = mm->msg.timeStampUsec;
						// this is a hacky value, it can be overwritten to a smaller value if somebody mines a part of my block,
						// so I can continue where he finished
						ttt->powcoinMinerMinimalTimeStampForNextBlock = mm->msg.timeStampUsec;
					}
					minerSetBlockForJavascriptMiners(coin, ss->s);
					powcoinMiningAppendMiningMessage(coin, minerAccount, ss);
					ttt->currentMiningStatus.nonce[0] = 0;
					ttt->currentMiningStatus.nonceSize = 1;
					ttt->currentMiningStatus.activeMiningFlag = 1;
					jsVarDstrFree(&ttt->currentMiningStatus.blockContent);
					ttt->currentMiningStatus.blockContent = jsVarDstrCreateByPrintf("mvl=PowMinedBlock;non=%"PRIu64";mss:%d=", genMsgNonce(), ss->size);
					jsVarDstrAppendData(ttt->currentMiningStatus.blockContent, ss->s, ss->size);
					jsVarDstrAppendPrintf(ttt->currentMiningStatus.blockContent, ";");
					jsVarDstrFree(&ss);
					PRINTF("universalMiner: Starting to mine a new block for %s\n", coin);
					res = MINING_DELAY_10MS;
				}
			} else {
				// TODO: do one mining step
				ss = ttt->currentMiningStatus.blockContent;
				ssize = ss->size;
				jsVarDstrAppendPrintf(ss, "non:%d=", ttt->currentMiningStatus.nonceSize);
				jsVarDstrAppendData(ss, ttt->currentMiningStatus.nonce, ttt->currentMiningStatus.nonceSize);
				jsVarDstrAppendPrintf(ss, ";");
				powcoinHash(ss->s, ss->size, blockHash);
				// PRINTF("Checkinh hash[0] == %d\n", (unsigned char)blockHash[0]);
				r = povcoinHashCheck(coin, blockHash);
				if (r < 0) {
					// there is a serious error, stop mining
					ttt->currentMiningStatus.activeMiningFlag = 0;
				} else if (r == 0) {
					// we got the block
					// mining messages are unsigned, anybody can mine in profit of some account
					bixSendNewMessageInsertOrProcessRequest_s2c("", NMF_INSERT_MESSAGE, secondaryToCoreServerConnectionMagic, SSI_NONE, ttt->currentMiningStatus.blockContent->s, ttt->currentMiningStatus.blockContent->size, coin, ttt->account, "unsigned");
					PRINTF("universalMiner: %s block mined.\n", coin);
					ttt->currentMiningStatus.activeMiningFlag = 0;
					minerSetBlockForJavascriptMiners(coin, "");
					// Large delay is to not start mining with the last mined message before it gots accepted in the blockchain
					res = MINING_DELAY_SECOND;
				} else {
					ss->s[ssize] = 0;
					ss->size = ssize;
					powcoinMinerIncrementNonce(ttt);			
					res = MINING_DELAY_10MS;
				}
			}
		}
	}
	return(res);
}

void powcoinOnUserMinedBlock(struct jsVaraio *jj, char *coin, char *account, struct jsVarDynamicString *bb) {
	if (coin == NULL || bb == NULL) return;

	if (jj != NULL && account != NULL) anotificationAddOrRefresh(account, jj->w.b.baioMagic, NCT_GUI);
	// mining messages are unsigned
	bixSendNewMessageInsertOrProcessRequest_s2c("", NMF_INSERT_MESSAGE, secondaryToCoreServerConnectionMagic, SSI_NONE, bb->s, bb->size, coin, account, "unsigned");
}

////////////////////////////////////////////////////////////////////////////////////////////////

#define RECOMMENDED_MINIMUM_OF_MINING_CORE_SERVERS	10

void bytecoinMineNewCoins(void *d) {
	double 						u, f, t, totalByteAmount, totalCoinAmount, amount, ns;
	struct jsVarDynamicString	*ss;
	char						*signature;

	t = node->currentTotalSpaceAvailableBytes;
	u = node->currentUsedSpaceBytes;
	f = t - u;

	// totalByteAmount = 0.875 * u + 1.125 * f;
	// totalByteAmount = 0.3 * u + 0.7 * f;
	totalByteAmount = 0.6 * u + 1.4 * f;
	if (totalByteAmount < 0) totalByteAmount = 0;

	// reward is (derived from hosting cost): 20 coins per 1TB (per month)
	// We calculate the total mining reward first for 10 servers
	totalCoinAmount = (totalByteAmount / 1.0E+12) * RECOMMENDED_MINIMUM_OF_MINING_CORE_SERVERS * 20.0 * (double)BCHAIN_MINING_PERIOD_SEC / (30 * 24 * 60 * 60);

	// TODO: This shall be total servers active at least 90 % of time
	ns = coreServersActive;
	if (ns < RECOMMENDED_MINIMUM_OF_MINING_CORE_SERVERS) ns = RECOMMENDED_MINIMUM_OF_MINING_CORE_SERVERS;
	amount = totalCoinAmount / ns;

	// PRINTF("Mining data %f %f %d -> %f -> %f\n", u, t, coreServersActive, totalAmount, amount);
	PRINTF("Mining proposal: %f coins.\n", amount);

	ss = jsVarDstrCreateByPrintf("mvl=Mining;usp=%a;asp=%a;srv=%d;amt=%a;tim=%d;non=%"PRIu64";", u, t, coreServersActive, amount, currentTime.sec, genMsgNonce());
	// Hmm. this shall be probably signed by the server key
	signature = signatureCreateBase64(ss->s, ss->size, myNodePrivateKey);
	bchainCreateNewMessage(ss->s, ss->size, MESSAGE_DATATYPE_BYTECOIN, THIS_CORE_SERVER_ACCOUNT_NAME_ST(), "", "", signature, NULL);
	jsVarDstrFree(&ss);
	FREE(signature);
	bytecoinScheduleNextMining(NULL);
}

void bytecoinScheduleNextMining(void *dummy) {
    struct tm   tm;
    time_t      newMiningTime, nextReschedulingTime, fixDisconnectionTime;
    long long   nn, tradingOffNn, tradingOnNn, reschedulingNn;
    int         i;

	// always refill all parts of struct tm, because they are modified by previous calls!
    tm.tm_sec = 01;
    tm.tm_min = 00;
    tm.tm_hour = 00;
    tm.tm_mday = currentTime.lcltm.tm_mday + 1;
    tm.tm_mon = currentTime.lcltm.tm_mon;
    tm.tm_year = currentTime.lcltm.tm_year;
    tm.tm_isdst = -1;

#if 1
    tm.tm_mday = currentTime.lcltm.tm_mday;
    tm.tm_hour = currentTime.lcltm.tm_hour;
    tm.tm_min = currentTime.lcltm.tm_min;
    tm.tm_sec = ROUND_INT(currentTime.lcltm.tm_sec+BCHAIN_MINING_PERIOD_SEC+1, BCHAIN_MINING_PERIOD_SEC);
#endif

    newMiningTime = mktime(&tm);

    // if (tm.tm_wday != 6 && tm.tm_wday != 0) {
    nn = newMiningTime * 1000000LL;
    // PRINTF("Info: Scheduling mining to %s\n", sprintUsecTime_st(nn));
    timeLineInsertEvent(nn, bytecoinMineNewCoins, NULL);
}

void bytecoinInitialize() {
	bytecoinScheduleNextMining(NULL);
}

/////////////////////////////////////////////////////////////////////////////////////
// exchanges

int applyExchangeBlockchainMessage(struct bchainMessage *msg, struct blockContentDataList *fromBlock, struct bFinalActionList **finalActionList) {
	int							i, r, res, side;
	int							nonce;
	struct bixParsingContext 	*mbixc;
	struct bookedOrder			*o;

	res = -1;
	mbixc = bixParsingContextGet();
	r = bixParseString(mbixc, msg->data, msg->dataLength);
	if (r != 0) {
		PRINTF("Internal error: wrongly formed coin message: %s\n", msg->data);
		bixParsingContextFree(&mbixc);
		return(-1);
	}

	bixMakeTagsNullTerminating(mbixc);

	if (1
		// Messages without signature
		// && strcmp(mbixc->tag[BIX_TAG_mvl], "") != 0
		&& ! msg->signatureVerifiedFlag
		) {
		PRINTF0("Error: message has wrong signature.\n");
	} else if (strcmp(mbixc->tag[BIX_TAG_mvl], "NewLimitOrder") == 0) {
		// mvl=NewLimitOrder;acc=xfc;btk=BIX;qtk=xfc;sid=Buy;odt=Limit;qty=0x1p+0;prc=0x1p+0;
		side = enumFromName(sideNames, mbixc->tag[BIX_TAG_sid]);
		r = exchangeAddOrder(mbixc->tag[BIX_TAG_acc], mbixc->tag[BIX_TAG_btk], mbixc->tag[BIX_TAG_qtk], side, strtod(mbixc->tag[BIX_TAG_prc], NULL), strtod(mbixc->tag[BIX_TAG_qty], NULL), 0, finalActionList);
		if (r >= 0) res = 0;
	} else {
		PRINTF("Internal error: unknown exchange operation: %s\n", mbixc->tag[BIX_TAG_mvl]);
		res = -1;
	}

	bixMakeTagsSemicolonTerminating(mbixc);
	bixParsingContextFree(&mbixc);

	return(res);
}

/////////////////////////////////////////////////////////////////////////////////////
// userFiles

int applyUserfileBlockchainMessage(struct bchainMessage *msg, struct blockContentDataList *fromBlock, struct bFinalActionList **finalActionList) {
	int						i, r, res;
	struct bAccountBalance 	*aa;
	double					amount;
	int						nonce;
	int64_t					miningTime;

	res = -1;
	// PRINTF("VERIFY MSG %s\n", dat);

	if (1
		// Messages without signature
		// && strcmp(mvl, "") != 0
		&& ! msg->signatureVerifiedFlag
		) {
		PRINTF0("Error: message has wrong signature.\n");
	} else {
		aa = bAccountBalanceLoadOrFind(MESSAGE_DATATYPE_BYTECOIN, msg->account, &currentBchainContext);
		if (aa == NULL) {
			PRINTF0("Internall error. A non-existant bytecoin account is inserting a message.\n");
		} else {
			blockFinalizeNoteAccountBalanceModification(finalActionList, aa);
			// we want that 1 coin is approximately 1 MB per year, so convert it
			amount = (double) msg->dataLength / 1000000.0;
			if (aa->balance >= amount) {
				aa->balance -= amount;
				res = 0;
			}
		}
	}
	return(res);
}

/////////////////////////////////////////////////////////////////////////////////////

struct secondaryToCoreServerPendingRequestsStr **secondaryToCorePendingRequestFind(int secondaryServerInfo) {
	struct secondaryToCoreServerPendingRequestsStr *pp, **ppa;
	for(ppa= &secondaryToCoreServerPendingRequestsList; *ppa != NULL && (*ppa)->reqid != secondaryServerInfo; ppa = &(*ppa)->next) ;
	return(ppa);
}

struct secondaryToCoreServerPendingRequestsStr *secondaryToCorePendingRequestFindAndUnlist(int secondaryServerInfo) {
	struct secondaryToCoreServerPendingRequestsStr *pp, **ppa;

	ppa = secondaryToCorePendingRequestFind(secondaryServerInfo);
	if (*ppa == NULL) {
		// Now, we use this also for message status update, so it is normal
		// PRINTF0("Internal error: received answer without a request.\n");
		return(NULL);
	}
	pp = *ppa;
	*ppa = pp->next;
	return(pp);
}

void secondaryToCorePendingRequestFree(struct secondaryToCoreServerPendingRequestsStr *pp) {

	if (pp == NULL) return;
	FREE(pp->coin);
	FREE(pp->account);
	FREE(pp->formname);
	FREE(pp->publicKey);
	FREE(pp->privateKey);
	FREE(pp->arg);
	jsVarDstrFree(&pp->kycBix);
	if (pp->pkey != NULL) EVP_PKEY_free(pp->pkey);
	FREE(pp);
}

struct secondaryToCoreServerPendingRequestsStr *secondaryToCorePendingRequestSimpleCreate(struct jsVaraio *jj, char *formname, int requestType) {
	struct secondaryToCoreServerPendingRequestsStr *pp;
	ALLOC(pp, struct secondaryToCoreServerPendingRequestsStr);
	memset(pp, 0, sizeof(*pp));
	pp->reqid = currentWalletToCoreRequestId ++;
	pp->requestType = requestType;
	pp->formname = strDuplicate(formname);
	if (jj == NULL) {
		pp->guiBaioMagic = 0;
	} else {
		pp->guiBaioMagic = jj->w.b.baioMagic;
	}
	pp->next = secondaryToCoreServerPendingRequestsList;
	secondaryToCoreServerPendingRequestsList = pp;
	return(pp);
}

int secondaryServerCreateReqidSignSendToCoreServerAndFreeMessage(int requestType, int dstCoreServer, struct jsVarDynamicString *msg, struct jsVaraio *jj, char *formname, char *coin, char *account, char *privateKey, int insertProcessFlag) {

	struct secondaryToCoreServerPendingRequestsStr	*pp;
	int												r, reqid;
	char											dstPath[TMP_STRING_SIZE];

	if (msg == NULL) return(-1);

	if (requestType == STC_NONE) {
		reqid = REQ_ID_NONE;
	} else {
		pp = secondaryToCorePendingRequestSimpleCreate(jj, formname, requestType);
		reqid = pp->reqid;
		pp->coin = strDuplicate(coin);
		pp->account = strDuplicate(account);
	}

	if (insertProcessFlag == NMF_PROCESS_MESSAGE && reqid == REQ_ID_NONE) {
		PRINTF0("Warning: requesting immediate processing of message without creating request data\n");
	}

	if (jj != NULL && account != NULL) anotificationAddOrRefresh(account, jj->w.b.baioMagic, NCT_GUI);

	dstPath[0] = 0;
	if (dstCoreServer >= 0) snprintf(dstPath, sizeof(dstPath), "%d", dstCoreServer);

	r = secondaryServerSignAndSendMessageToCoreServer(dstPath, insertProcessFlag, jj, reqid, msg->s, msg->size, coin, account, privateKey);
	jsVarDstrFree(&msg);
	return(r);
}

int secondaryServerCreateReqidSignAndSendToCoreServer(int requestType, int dstCoreServer, struct jsVaraio *jj, char *formname, char *coin, char *account, char *privateKey, int insertProcessFlag, char *fmt, ...) {
	struct jsVarDynamicString	*ss;
	va_list 					arg_ptr;
	int							r;

	va_start(arg_ptr, fmt);
	ss = jsVarDstrCreateByVPrintf(fmt, arg_ptr);
	va_end(arg_ptr);
	r = secondaryServerCreateReqidSignSendToCoreServerAndFreeMessage(requestType, dstCoreServer, ss, jj, formname, coin, account, privateKey, insertProcessFlag);
	return(r);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////

char *generateNewAccountName_st() {
	char *res;

	res = getAccountName_st(node->currentNewAccountNumber, myNode);
	node->currentNewAccountNumber ++;
	return(res);
}

char *getAccountName_st(int64_t accnumber, int nodeNumber) {
	char 			*res;
	int				i;
	int64_t 		a;
	int				checksum;

	assert(nodeNumber >= 0 && nodeNumber < coreServersMax);
	res = staticStringRingGetTemporaryStringPtr_st();
	checksum = nodeNumber;
	for(a=accnumber; a>0; a=a/10) checksum += a%10;
	sprintf(res, "1%o8%"PRId64"%01d", nodeNumber, accnumber, checksum%10);
	return(res);
}

int parseAccountName(char *account, int64_t *accnumber, int *nodeNumber) {
	int		r;
	int64_t	acch;

	if (account == NULL || accnumber == NULL || nodeNumber == NULL) return(-1);
	if (account[0] != '1') return(-1);
	r = sscanf(account, "1%o8%"PRId64, nodeNumber, &acch);
	// PRINTF("scanf %s --> %d --> %d %"PRId64"\n", account, r, *nodeNumber, *accnumber);
	if (r != 2) return(-1);
	// discart possibly hacky account name
	*accnumber = acch / 10;
	if (strcmp(getAccountName_st(*accnumber, *nodeNumber), account) != 0) return(-1);
	return(0);
}

int isCoreServerAccount(char *account) {
	int64_t accnumber;
	int 	nodeNumber;
	int		r;

	r = parseAccountName(account, &accnumber, &nodeNumber);
	if (r != 0) return(0);
	if (accnumber == 0) return(1);
	return(0);
}

void walletNewTokenCreation(struct jsVaraio *jj, char *formname, char *newcoin, char *cointype, char *account, char *privateKey, int gdprs) {
	int												r, r2, reqid;
	char											ttt[LONG_LONG_TMP_STRING_SIZE];
	struct kycData									kk;
	struct secondaryToCoreServerPendingRequestsStr	*pp;
	char											*coin;

	coin = MESSAGE_DATATYPE_BYTECOIN;

	if (checkFileAndTokenName(newcoin, 0) < 0) {
		PRINTF0("Internal error: invalid characters in coin name.\n");
		jsVarSendEval(jj, "guiNotify('%s: Error: Invalid token name: %s\\n');", SPRINT_CURRENT_TIME(), newcoin);
		return;
	}
	if (gdprs < 0) {
		jsVarSendEval(jj, "guiNotify('%s: Error: Wrong gdprs server.\\n');", SPRINT_CURRENT_TIME());
		return;
	}

	r = snprintf(ttt, sizeof(ttt), "mvl=CreateNewCoin;cnm=%s;ctp=%s;non=%"PRIu64";", newcoin, cointype, genMsgNonce());
	if (r >= sizeof(ttt) || r < 0) {
		PRINTF0("Internal error: string for signature is too long.\n");
		return;
	}

	pp = secondaryToCorePendingRequestSimpleCreate(jj, formname, STC_NEW_TOKEN);
	reqid = pp->reqid;
	pp->coin = strDuplicate(coin);
	pp->account = strDuplicate(account);
	pp->newcoin = strDuplicate(newcoin);
	pp->gdprs = gdprs;
	if (jj != NULL && account != NULL) anotificationAddOrRefresh(account, jj->w.b.baioMagic, NCT_GUI);
	r = secondaryServerSignAndSendMessageToCoreServer("", NMF_INSERT_MESSAGE, jj, reqid, ttt, strlen(ttt), coin, account, privateKey);
}

int walletNewTokenCreationStateUpdated(struct secondaryToCoreServerPendingRequestsStr *pp, char *account, char *data, int datalen) {
	struct jsVaraio 			*jj;
	struct kycData				kk;

	jj = (struct jsVaraio *) baioFromMagic(pp->guiBaioMagic);
	// TODO: Do this more professionally through some status variable
	if (data != NULL && strstr(data, "was inserted in block") != NULL) {
#if 0
		memset(&kk, 0, sizeof(kk));
		STRN_CPY_TMP(kk.masterAccount, account);
		walletNewAccountCreation(jj, pp->formname, pp->newcoin, &kk, pp->gdprs);
#endif
		jsVarSendEval(jj, "onGotoNewTokenParameters('%s');", pp->formname);
		return(1);
	}
	return(0);
}

char *createTokenOptionsMsgText_st(char *newcoin, char *cointype, char *opt) {
	int							r, r2, toptlen;
	char						exopt[TMP_STRING_SIZE];
	char						*ttt, *aapp;
	struct supportedMessageStr	*smm;

	ttt = staticStringRingGetLongLongTemporaryStringPtr_st();
	exopt[0] = exopt[TMP_STRING_SIZE-1] = 0;
	smm = coreServerSupportedMessageFind(MESSAGE_DATATYPE_BYTECOIN);
	if (smm != NULL) {
		aapp = jsVarGetEnv_st(smm->opt, "new_account_approval");
		if (aapp != NULL) snprintf(exopt, sizeof(exopt)-1, "&new_account_approval=%s", aapp);
	}
	toptlen = strlen(opt)+strlen(exopt);
	r = snprintf(ttt, LONG_LONG_TMP_STRING_SIZE, "mvl=CoinOptions;cnm=%s;ctp=%s;non=%"PRIu64";opt:%d=%s%s;", newcoin, cointype, genMsgNonce(), toptlen, opt, exopt);
	if (r >= LONG_LONG_TMP_STRING_SIZE || r < 0) {
		PRINTF0("Internal error: string for signature is too long.\n");
		return(NULL);
	}
	return(ttt);
}

void walletTokenOptions(struct jsVaraio *jj, char *newcoin, char *cointype, char *opt, char *account, char *privateKey) {
	char	*ttt;

	ttt = createTokenOptionsMsgText_st(newcoin, cointype, opt);
	secondaryServerSignAndSendMessageToCoreServer("", NMF_INSERT_MESSAGE, jj, REQ_ID_NONE, ttt, strlen(ttt), MESSAGE_DATATYPE_BYTECOIN, account, privateKey);
}

void ssSendUpdateTokenListRequest(void *jjj, char *formname) {
	struct secondaryToCoreServerPendingRequestsStr	*pp;
	int							r;
	struct jsVarDynamicString	*ss;
	struct jsVaraio 			*jj;

	jj = (struct jsVaraio *) jjj;

	pp = secondaryToCorePendingRequestSimpleCreate(jj, formname, STC_GET_MSGTYPES);
	ss = jsVarDstrCreateByPrintf("mvl=List;non=%"PRIu64";", genMsgNonce());
	// Token list is a "free service". No signature required.
	bixSendNewMessageInsertOrProcessRequest_s2c("", NMF_PROCESS_MESSAGE, secondaryToCoreServerConnectionMagic, pp->reqid, ss->s, ss->size, MESSAGE_DATATYPE_META_MSG, "", "no-signature");
	jsVarDstrFree(&ss);
}

void ssGotSupportedMsgTypes(struct secondaryToCoreServerPendingRequestsStr *pp, char *account, char *msg, int msglen) {
	int							i,n, nn;
	struct bixParsingContext	*mbixc;
	char						*ss;

	n = INT_MAX/2;
	mbixc = bixParsingContextGet();
	BIX_PARSE_AND_MAP_STRING(msg, msglen, mbixc, 0, {
			if (strncmp(tagName, "mst", 3) == 0) {
				if (strcmp(tagValue, MESSAGE_DATATYPE_LAST_NON_COIN) == 0) n = 0;
				else n++;
			}
		}, {
			goto exitPoint;
		}
		);
	bixParsingContextFree(&mbixc);
	if (n == INT_MAX/2) goto exitPoint;

	// free previously allocated gui strings
	nn = jsVarGetDimension(guiSupportedTokenTypesVector);
	for(i=0; i<nn; i++) FREE(jsVarGetStringVector(guiSupportedTokenTypesVector, i));

	jsVarResizeVector(guiSupportedTokenTypesVector, n);
	jsVarResizeVector(guiSupportedTokenTransferFee, n);
	jsVarResizeVector(guiSupportedTokenDifficultyVector, n);
	i = n;
	// Also reset table of supported messages in secondary server
	coreServerSupportedMessageInit();
	mbixc = bixParsingContextGet();
	BIX_PARSE_AND_MAP_STRING(msg, msglen, mbixc, 1, {
			if (strncmp(tagName, "mst", 3) == 0) {
				coreServerSupportedMessageAddAdditionalMsgWithType(mbixc->tag[BIX_TAG_mst], mbixc->tag[BIX_TAG_msm], mbixc->tag[BIX_TAG_mac], mbixc->tag[BIX_TAG_opt]);
				if (strcmp(tagValue, MESSAGE_DATATYPE_LAST_NON_COIN) == 0) i = 0;
				else if (i < n) {
					// FREE(jsVarGetStringVector(guiSupportedTokenTypesVector, i));
					jsVarSetStringVector(guiSupportedTokenTypesVector, i, strDuplicate(tagValue));
					jsVarSetIntVector(guiSupportedTokenDifficultyVector, i, jsVarGetEnvDouble(mbixc->tag[BIX_TAG_opt], "mining_difficulty", -1));
					jsVarSetDoubleVector(guiSupportedTokenTransferFee, i, jsVarGetEnvDouble(mbixc->tag[BIX_TAG_opt], "fees", -1));					
					i++;
				}
			}
		}, {
			goto exitPoint;
		}
		);

exitPoint:
	bixMakeTagsSemicolonTerminating(mbixc);
	bixParsingContextFree(&mbixc);
}

static int accountViewComparator(char *a1, char *a2) {
	int i1, i2;
	if (isdigit(a1[0]) && isdigit(a2[0])) {
		i1 = atoi(a1);
		i2 = atoi(a2);
		if (i1 != i2) return(i1 - i2);
	}
	return(strcmp(a1, a2));
}

void ssGotPendingReqList(struct secondaryToCoreServerPendingRequestsStr *pp, char *account, char *msg, int msglen) {
	int							i,n;
	struct jsVaraio 			*jj;
	struct bixParsingContext	*mbixc;
	char						**rr;
	char						*requestType;

	switch (pp->requestType) {
	case STC_GET_ACCOUNTS:
		requestType = "account";
		break;
	case STC_PENDING_REQ_LIST:
		requestType = "pendingRequest";
		break;
	default:
		requestType = "unknown";
	}

	mbixc = NULL;
	rr = NULL;
	// PRINTF("GOT %s\n", msg);

	jj = (struct jsVaraio *) baioFromMagic(pp->guiBaioMagic);
	if (strncmp(msg, "Error", 5) == 0) {
		jsVarSendEval(jj, "guiNotify('%s\\n');", msg);
		goto exitPoint;
	}

	n = 0;
	mbixc = bixParsingContextGet();
	BIX_PARSE_AND_MAP_STRING(msg, msglen, mbixc, 0, {
			if (strncmp(tagName, "fil", 3) == 0) n ++;
		}, {
			goto exitPoint;
		}
		);
	bixParsingContextFree(&mbixc);
	ALLOCC(rr, n, char*);
	memset(rr, 0, n * sizeof(*rr));

	i = 0;
	mbixc = bixParsingContextGet();
	BIX_PARSE_AND_MAP_STRING(msg, msglen, mbixc, 0, {
			if (strncmp(tagName, "fil", 3) == 0) {
				rr[i] = strDuplicate(tagValue);
				i++;
			}
		}, {
			goto exitPoint;
		}
		);

	SGLIB_ARRAY_SINGLE_QUICK_SORT(char *, rr, n, accountViewComparator);

	jsVarSendEval(jj, "pendingRequestsTableResize(%d);", n);
	for(i=0; i<n; i++) {
		jsVarSendEval(jj, "pendingRequestsTableSetItem(%d, '%s', '%s');", i, rr[i], requestType);
	}
	

exitPoint:

	if (rr != NULL) for(i=0;i<n;i++) FREE(rr[i]);
	FREE(rr);
	bixParsingContextFree(&mbixc);
}

void ssGotPendingReqExplore(struct secondaryToCoreServerPendingRequestsStr *pp, char *account, char *msg, int msglen) {
	int						i,n;
	struct jsVaraio 		*jj;

	jj = (struct jsVaraio *) baioFromMagic(pp->guiBaioMagic);
	pendingRequestShowNewAccountRequest(msg, msglen, jj);
}

/////////////////////////

void walletNewAccountCreation(struct jsVaraio *jj, char *formname, char *coin, struct kycData *kk, int gdprs) {
	struct secondaryToCoreServerPendingRequestsStr	*pp;
	int							r;
	struct jsVarDynamicString	*ss;
	char						dst[TMP_STRING_SIZE];

	if (gdprs < 0) {
		jsVarSendEval(jj, "guiNotify('%s: Error: Wrong gdprs server.\\n');", SPRINT_CURRENT_TIME());
		return;
	}
	pp = secondaryToCorePendingRequestSimpleCreate(jj, formname, STC_NEW_ACCOUNT_NUMBER);
	pp->coin = strDuplicate(coin);
	pp->gdprs = gdprs;
	pp->kycBix = kycToBix(kk);
	ss = jsVarDstrCreateByPrintf("mvl=NewAccountNumber;non=%"PRIu64";", genMsgNonce());
	sprintf(dst, "%d", gdprs);
	bixSendNewMessageInsertOrProcessRequest_s2c(dst, NMF_PROCESS_MESSAGE, secondaryToCoreServerConnectionMagic, pp->reqid, ss->s, ss->size, coin, "", "new-account-number-request-unsigned");
	jsVarDstrFree(&ss);
}

#define COIN_NOP_MESSAGE "mvl=NOP;"

void walletNewAccountSendRequest(struct jsVaraio *jj, char *account, char *formname, char *coin, struct jsVarDynamicString *kycBix, int gdprs) {
	struct secondaryToCoreServerPendingRequestsStr	*pp;
	int							r;
	struct jsVarDynamicString	*ss;
	char						dst[TMP_STRING_SIZE];

	pp = secondaryToCorePendingRequestSimpleCreate(jj, formname, STC_NEW_ACCOUNT);
	pp->coin = strDuplicate(coin);
	pp->pkey = generateNewEllipticKey();
	pp->publicKey = getPublicKeyAsString(pp->pkey);
	pp->privateKey = getPrivateKeyAsString(pp->pkey);

	ss = jsVarDstrCreateByPrintf("mvl=NewAccount;acc=%s;pbk:%d=%s;non=%"PRIu64";kyc:%d=",
								 account,
								 strlen(pp->publicKey), pp->publicKey,
								 genMsgNonce(),
								 kycBix->size
		);
	jsVarDstrAppendData(ss, kycBix->s, kycBix->size);
	jsVarDstrAppendPrintf(ss, ";");
	anotificationAddOrRefresh(account, pp->guiBaioMagic, NCT_GUI);
	dst[0] = 0;
	if (gdprs >= 0 && gdprs < coreServersMax) {
		sprintf(dst, "%d", gdprs);
		// also send a dummy request just to link this wallet to annotations about this account
		bixSendNewMessageInsertOrProcessRequest_s2c("", NMF_PROCESS_MESSAGE, secondaryToCoreServerConnectionMagic, SSI_NONE, COIN_NOP_MESSAGE, strlen(COIN_NOP_MESSAGE), coin, account, "dummy-msg-unsigned");
	}

	bixSendNewMessageInsertOrProcessRequest_s2c(dst, NMF_PROCESS_MESSAGE, secondaryToCoreServerConnectionMagic, pp->reqid, ss->s, ss->size, coin, account, "new-account-unsigned");
	jsVarDstrFree(&ss);
}

void walletGotNewAccountNumber(struct secondaryToCoreServerPendingRequestsStr *pp, char *account, char *msg, int msglen) {
	struct jsVaraio 			*jj;
	struct jsVarDynamicString	*kk;

	jj = (struct jsVaraio *) baioFromMagic(pp->guiBaioMagic);

	if (msglen < 0) {
		jsVarSendEval(jj, "guiNotify('%s: Error: Can\\'t get account number.\\n');", SPRINT_CURRENT_TIME(), msg);
		return;
	}
	account = msg;
	walletNewAccountSendRequest(jj, account, pp->formname, pp->coin, pp->kycBix, pp->gdprs);
}

void walletNewAccountCreated(struct secondaryToCoreServerPendingRequestsStr *pp, char *account, char *msg) {
	struct jsVaraio 			*jj;
	struct jsVarDynamicString	*kk;

	anotificationAddOrRefresh(account, pp->guiBaioMagic, NCT_GUI);
	jj = (struct jsVaraio *) baioFromMagic(pp->guiBaioMagic);
	jsVarSendEval(jj, "guiNotify('%s\\n');", msg);
	kk = jsVarDstrCreate();
	jsVarDstrAppendEscapedStringUsableInJavascriptEval(kk, pp->privateKey, strlen(pp->privateKey));
	jsVarSendEval(jj, "onNewAccountCreated('%s', '%s', '%s');", account, kk->s, pp->formname);
	jsVarDstrFree(&kk);
}

void walletShowAnswerInMessages(struct secondaryToCoreServerPendingRequestsStr *pp, char *account, char *msg, int msglen) {
	struct jsVaraio 			*jj;
	struct jsVarDynamicString	*ss;

	jj = (struct jsVaraio *) baioFromMagic(pp->guiBaioMagic);
	ss = jsVarDstrCreate();
	jsVarDstrAppendEscapedStringUsableInJavascriptEval(ss, msg, msglen);
	jsVarSendEval(jj, "guiNotify('%s\\n');", ss->s);
	jsVarDstrFree(&ss);
}

void walletShowAnswerInNotifications(struct secondaryToCoreServerPendingRequestsStr *pp, char *account, char *msg, int msglen) {
	struct jsVaraio 			*jj;
	struct jsVarDynamicString	*ss;

	jj = (struct jsVaraio *) baioFromMagic(pp->guiBaioMagic);
	ss = jsVarDstrCreate();
	jsVarDstrAppendEscapedStringUsableInJavascriptEval(ss, msg, msglen);
	jsVarSendEval(jj, "loginNotification('%s', '%s');", ss->s, account);
	jsVarDstrFree(&ss);
}

void walletShowMoreNotifications(struct secondaryToCoreServerPendingRequestsStr *pp, char *account, char *msg, int msglen) {
	struct jsVaraio 			*jj;
	struct jsVarDynamicString	*ss;
	char						*bb, *ee;

	jj = (struct jsVaraio *) baioFromMagic(pp->guiBaioMagic);

	// send messages from the last to the newest one and append them to current list
	ee = msg+msglen;
	while  (ee>msg && ee[-1] == '\n') ee --;
	while (ee>msg) {
		for(bb=ee-1; bb>=msg && *bb != '\n'; bb--) ;
		ss = jsVarDstrCreate();
		jsVarDstrAppendEscapedStringUsableInJavascriptEval(ss, bb+1, ee-bb-1);
		jsVarSendEval(jj, "guiNotifyAppend('%s');", ss->s);
		jsVarDstrFree(&ss);
		ee = bb;
	}
}

void walletGotBalance(struct secondaryToCoreServerPendingRequestsStr *pp, char *account, char *msg) {
	struct jsVaraio 			*jj;
	jj = (struct jsVaraio *) baioFromMagic(pp->guiBaioMagic);
	// jsVarSendEval(jj, "guiNotify('%s: Info: Balance: %s %s');", SPRINT_CURRENT_TIME(), msg, pp->coin);
	jsVarSendEval(jj, "document.forms['%s'].elements['balance'].value = '%s';", pp->formname, msg);	
}

void walletGotAllBalances(struct secondaryToCoreServerPendingRequestsStr *pp, char *account, char *msg, int msgLen) {
	struct jsVaraio 			*jj;
	struct bixParsingContext 	*bix;
	char						*token;
	double						balance;

	jj = (struct jsVaraio *) baioFromMagic(pp->guiBaioMagic);

	jsVarSendEval(jj, "accountBalancesInit();");
	// PRINTF("Parsing %s %d\n", msg, msgLen);
	bix = bixParsingContextGet();
	BIX_PARSE_AND_MAP_STRING(msg, msgLen, bix, 1, {
			if (strncmp(tagName, "eog", 3) == 0) {
				token = bix->tag[BIX_TAG_tkn];
				balance = strtod(bix->tag[BIX_TAG_amt], NULL);
				jsVarSendEval(jj, "accountBalancesAddBalance('%s', %f);", token, balance);
			}
		},
		{
			PRINTF0("Error while parsing balance list\n");
		}
		);
	bixMakeTagsSemicolonTerminating(bix);
	jsVarSendEval(jj, "accountBalancesDone();");
	bixParsingContextFree(&bix);
}

void walletSendCoins(struct jsVaraio *jj, char *coin, double amount, char *fromAccount, char *toAccount, char *privateKey) {
	int			r;

	r = secondaryServerCreateReqidSignAndSendToCoreServer(
		STC_NONE, -1,
		jj, NULL, coin, fromAccount, privateKey, NMF_INSERT_MESSAGE,
		"mvl=SendCoins;amt=%a;tok=%s;fra=%s;toa=%s;non=%"PRIu64";", amount, coin, fromAccount, toAccount, genMsgNonce()
		);
}

void walletLogin(struct jsVaraio *jj, char *formname, char *coin, char *account, char *privateKey) {
	int r;

	r = secondaryServerCreateReqidSignAndSendToCoreServer(
		STC_LOGIN, -1, 
		jj, formname, coin, account, privateKey, NMF_PROCESS_MESSAGE,
		"mvl=CheckLogin;acc=%s;non=%"PRIu64";", account, genMsgNonce()
		);

	if (r < 0) return;

	r = secondaryServerCreateReqidSignAndSendToCoreServer(
		STC_MORE_NOTIFICATIONS, -1,
		jj, formname, coin, account, privateKey, NMF_PROCESS_MESSAGE,
		"mvl=MoreNotifications;acc=%s;non=%"PRIu64";", account, genMsgNonce()
		);
}

void walletGetBalance(struct jsVaraio *jj, char *formname, char *coin, char *account, char *privateKey) {
	secondaryServerCreateReqidSignAndSendToCoreServer(
		STC_GET_BALANCE, -1, 
		jj, formname, coin, account, privateKey, NMF_PROCESS_MESSAGE,
		"mvl=GetBalance;acc=%s;non=%"PRIu64";", account, genMsgNonce()
		);
}

void walletGetAllBalances(struct jsVaraio *jj, char *formname, char *account, char *privateKey) {
	secondaryServerCreateReqidSignAndSendToCoreServer(
		STC_GET_ALL_BALANCES, -1, 
		jj, formname, MESSAGE_DATATYPE_META_MSG, account, privateKey, NMF_PROCESS_MESSAGE,
		"mvl=GetAllBalances;acc=%s;non=%"PRIu64";", account, genMsgNonce()
		);
}

void walletExchangeNewOrder(struct jsVaraio *jj, char *formname, char *account, char *baseToken, char *quoteToken, char *side, char *orderType, double quantity, double limitPrice, double stopPrice, char *privateKey) {
	int 	sd, ot;
	char 	*coin;

	sd = enumFromName(sideNames, side);
	ot = enumFromName(orderTypeNames, orderType);

	if (ot != ORDER_TYPE_LIMIT) {
		jsVarSendEval(jj, "guiNotify('%s: Error: wrong order type: %s. Only Limit orders are supported.');", SPRINT_CURRENT_TIME(), orderType);
		return;
	}

	if (sd != SIDE_BUY && sd != SIDE_SELL) {
		jsVarSendEval(jj, "guiNotify('%s: Error: wrong order side: %s.');", SPRINT_CURRENT_TIME(), side);
		return;
	}

	secondaryServerCreateReqidSignAndSendToCoreServer(
		STC_NONE, -1, 
		jj, formname, MESSAGE_DATATYPE_EXCHANGE, account, privateKey, NMF_INSERT_MESSAGE,
		"mvl=NewLimitOrder;acc=%s;btk=%s;qtk=%s;sid=%s;odt=%s;qty=%a;prc=%a;non=%"PRIu64";", account, baseToken, quoteToken, side, orderType, quantity, limitPrice, genMsgNonce()
		);

	// exchangeAddOrder("10811", baseToken, quoteToken, side, limitPrice, quantity, 0);

}

void walletGotOrdebook(struct secondaryToCoreServerPendingRequestsStr *pp, char *account, char *msg, int msgLen) {
	struct jsVaraio 			*jj;
	struct bixParsingContext 	*bix;
	struct jsVarDynamicString	*ss;

	jj = (struct jsVaraio *) baioFromMagic(pp->guiBaioMagic);

	PRINTF("PARSING %s\n", msg);

	ss = jsVarDstrCreateByPrintf("orderbookUpdate(");
	bix = bixParsingContextGet();
	BIX_PARSE_AND_MAP_STRING(msg, msgLen, bix, 1, {
			if (strncmp(tagName, "mrk", 3) == 0) {
				if (strcmp(tagValue, "eoo") == 0 ) {
					jsVarDstrAppendPrintf(ss, "[%f,%f],", strtod(bix->tag[BIX_TAG_prc], NULL), strtod(bix->tag[BIX_TAG_qty], NULL)-strtod(bix->tag[BIX_TAG_exq], NULL));
				} else if (strcmp(tagValue, "eoh") == 0) {
					jsVarDstrAppendPrintf(ss, "'%s', '%s'", bix->tag[BIX_TAG_btk], bix->tag[BIX_TAG_qtk]);
				} else if (strcmp(tagValue, "bids") == 0 || strcmp(tagValue, "asks") == 0 ) {
					jsVarDstrAppendPrintf(ss, ",[");
				} else if (strcmp(tagValue, "eobids") == 0 || strcmp(tagValue, "eoasks") == 0 ) {
					jsVarDstrAppendPrintf(ss, "null]");
				}
			}
		},	
		{
			PRINTF0("Error while parsing balance list\n");
		}
		);
	jsVarDstrAppendPrintf(ss, ");");
	jsVarSendEval(jj, "%s", ss->s);
	bixMakeTagsSemicolonTerminating(bix);
	bixParsingContextFree(&bix);
}

void walletExchangeUpdateOrderbook(struct jsVaraio *jj, char *formname, char *account, char *baseToken, char *quoteToken, char *privateKey) {
	secondaryServerCreateReqidSignAndSendToCoreServer(
		STC_EXCHANGE_GET_ORDER_BOOK, -1, 
		jj, formname, MESSAGE_DATATYPE_EXCHANGE, account, privateKey, NMF_PROCESS_MESSAGE,
		"mvl=GetOrderbook;acc=%s;btk=%s;qtk=%s;non=%"PRIu64";", account, baseToken, quoteToken, genMsgNonce()
		);
}

//////////////////////////////////////////////////////////////////////

void walletPendingRequestListRequests(struct jsVaraio *jj, char *formname, char *coin, char *account, char *privateKey) {
	char											prefix[TMP_STRING_SIZE];
	int												r, gdprs;

	gdprs = coinGdprs(coin);
	if (gdprs < 0) {
		PRINTF("Error: Problem with getting gdprs server for token %s.\n", coin);
		return;
	}
	r = snprintf(prefix, TMP_STRING_SIZE, "%s-%s", PENDING_NEW_ACCOUNT_REQUEST, coin);
	if (r >= TMP_STRING_SIZE || r < 0) {
		PRINTF("Error: Problem forming update list request. Probably too long coin name %s.\n", coin);
		return;
	}
	secondaryServerCreateReqidSignAndSendToCoreServer(
		STC_PENDING_REQ_LIST, gdprs,
		jj, formname, coin, account, privateKey, NMF_PROCESS_MESSAGE,
		"mvl=prMap;acc=%s;dbs=%s;pre:%d=%s;non=%"PRIu64";", account, DATABASE_PENDING_REQUESTS, strlen(prefix), prefix, genMsgNonce()
		);
}

void walletListAccounts(struct jsVaraio *jj, char *formname, char *coin, char *account, char *privateKey) {
	int												r, gdprs;

	gdprs = coinGdprs(coin);
	if (gdprs < 0) {
		PRINTF("Error: Problem with getting gdprs server for token %s.\n", coin);
		return;
	}
	secondaryServerCreateReqidSignAndSendToCoreServer(
		STC_GET_ACCOUNTS, gdprs, 
		jj, formname, coin, account, privateKey, NMF_PROCESS_MESSAGE,
		"mvl=getAccounts;acc=%s;tok:%d=%s;non=%"PRIu64";", account, strlen(coin), coin, genMsgNonce()
		);
}

void walletPendingRequestOp(struct jsVaraio *jj, char *coin, char *account, char *database, char *request, int opStcCode, char *opString, char *privateKey) {
	struct secondaryToCoreServerPendingRequestsStr	*pp;
	char											msgtosign[TMP_STRING_SIZE];
	int												r, gdprs;

	gdprs = coinGdprs(coin);
	if (gdprs < 0) {
		PRINTF("Error: Problem with getting gdprs server for token %s.\n", coin);
		return;
	}
	secondaryServerCreateReqidSignAndSendToCoreServer(
		opStcCode, gdprs,
		jj, NULL, coin, account, privateKey, NMF_PROCESS_MESSAGE,
		"mvl=%s;acc=%s;dbs=%s;key:%d=%s;non=%"PRIu64";", opString, account, database, strlen(request), request, genMsgNonce()
		);
}

void walletPendingRequestExplore(struct jsVaraio *jj, char *coin, char *account, char *request, char *requestType, char *privateKey) {
	if (strcmp(requestType, "pendingRequest") == 0) {
		walletPendingRequestOp(jj, coin, account, DATABASE_PENDING_REQUESTS, request, STC_PENDING_REQ_EXPLORE, "dbGet", privateKey);
	} else {
		walletPendingRequestOp(jj, coin, account, DATABASE_KYC_DATA, request, STC_ACCOUNT_EXPLORE, "dbGet", privateKey);
	}
}

void walletPendingRequestApprove(struct jsVaraio *jj, char *coin, char *account, char *request, char *requestType, char *privateKey) {
	walletPendingRequestOp(jj, coin, account, DATABASE_PENDING_REQUESTS, request, STC_PENDING_REQ_APPROVE, "prApprove", privateKey);
}

void walletPendingRequestDelete(struct jsVaraio *jj, char *coin, char *account, char *request, char *requestType, char *privateKey) {
	walletPendingRequestOp(jj, coin, account, DATABASE_PENDING_REQUESTS, request, STC_PENDING_REQ_DELETE, "prDelete", privateKey);
}

//////////////////////////////////////////////////////////////////////

#define AESCTR_KEY_SIZE 		(256/8)
#define AESCTR_COUNTER_SIZE 	(128/8)

void walletInsertUserFile(struct jsVaraio *jj, char *account, char *filename, char *msg, int msgsize, char *encryptkey, char *privateKey) {
	int							r;
	double 						fee;
	struct jsVarDynamicString	*ss;
	char						ekey[TMP_STRING_SIZE];
	char						iv[TMP_STRING_SIZE];
	char						ivh[TMP_STRING_SIZE];
	char						*encrypted;
	int							encryptedlen;

	// first encrypt if needed
	ivh[0] = 0;
	encrypted = NULL;
	if (encryptkey != NULL && encryptkey[0] != 0) {
		SHA256(encryptkey, strlen(encryptkey), ekey);
		// hexStringToBin(ekey, sizeof(ekey), encryptkey);
		RAND_bytes(iv, AESCTR_COUNTER_SIZE);
		hexStringFromBin(ivh, iv, AESCTR_COUNTER_SIZE);
		ALLOCC(encrypted, msgsize+256, char);
		encryptedlen = encryptWithAesCtr256(msg, msgsize, ekey, iv, encrypted);
		if (encryptedlen < 0 || encryptedlen >= msgsize+256) {
			jsVarSendEval(jj, "guiNotify('%s: Error: Can\\'t encrypt the file. %d <> %d\\n');", SPRINT_CURRENT_TIME(), encryptedlen, msgsize);
			FREE(encrypted);
			return;
		}
		msg = encrypted;
		msgsize = encryptedlen;
	}

	fee = BCHAIN_FEE_FOR_INSERTING_LENGTH(msgsize);
	ss = jsVarDstrCreateByPrintf("fnm:%d=%s;ivh=%s;fee=%a;non=%"PRIu64";dat:%d=", strlen(filename), filename, ivh, fee, genMsgNonce(), msgsize);
	jsVarDstrAppendData(ss, msg, msgsize);
	jsVarDstrAppendPrintf(ss, ";");
	r = secondaryServerSignAndSendMessageToCoreServer("", NMF_INSERT_MESSAGE, jj, REQ_ID_NONE, ss->s, ss->size, MESSAGE_DATATYPE_USER_FILE, account, privateKey);
	if (r >= 0) {
		jsVarSendEval(jj, "guiNotify('%s: Info: Estimated insertion fee: %f %s.\\n');", SPRINT_CURRENT_TIME(), fee, MESSAGE_DATATYPE_BYTECOIN);
	}
	if (encrypted != NULL) FREE(encrypted);
	jsVarDstrFree(&ss);
}

void walletDownloadUserFile(struct jsVaraio *jj, uint64_t seq, char *msgid, char *encryptkey) {
	struct blockContentDataList *bc;
	struct bixParsingContext 	*bix, *ebixi;
	int							msgidlen;
	struct jsVarDynamicString	*ss;
	int							r, msgNotFoundFlag;
	char						*fcontent;
	int							flen;
	char						ekey[TMP_STRING_SIZE];
	char						iv[TMP_STRING_SIZE];
	char						*decrypted;
	int							decryptedlen;

	msgidlen = strlen(msgid);

    bc = bchainCreateBlockContentLoadedFromFile(seq);
    if (bc == NULL) {
        jsVarSendEval(jj, "guiNotify('Block %"PRIu64" not found. Probably not synchronized yet.');", seq);
        return;
    }

	msgNotFoundFlag = 1;
	bix = bixParsingContextGet();
	BIX_PARSE_AND_MAP_STRING(bc->blockContent, bc->blockContentLength, bix, 0, {
			if (strncmp(tagName, "dat", 3) == 0 && bix->tagLen[BIX_TAG_mid] == msgidlen && strncmp(bix->tag[BIX_TAG_mid], msgid, msgidlen) == 0) {
				// got the message
				ss = jsVarDstrCreate();
				if (strncmp(bix->tag[BIX_TAG_mdt], MESSAGE_DATATYPE_USER_FILE, sizeof(MESSAGE_DATATYPE_USER_FILE)-1) == 0) {
					msgNotFoundFlag = 0;
					ebixi = bixParsingContextGet();
					bixParseString(ebixi, bix->tag[BIX_TAG_dat], bix->tagLen[BIX_TAG_dat]);
					bixMakeTagsNullTerminating(ebixi);
					fcontent = ebixi->tag[BIX_TAG_dat];
					flen = ebixi->tagLen[BIX_TAG_dat];
					decrypted = NULL;
					if (encryptkey != NULL && encryptkey[0] != 0) {
						if (ebixi->tagLen[BIX_TAG_ivh] <= 0) {
							jsVarSendEval(jj, "guiNotify('%s: Warning: Unencrypted file. No decryption performed.');", SPRINT_CURRENT_TIME());
						} else {
							// decrypt it here
							SHA256(encryptkey, strlen(encryptkey), ekey);
							// hexStringToBin(ekey, sizeof(ekey), encryptkey);
							hexStringToBin(iv, sizeof(iv),  ebixi->tag[BIX_TAG_ivh]);
							ALLOCC(decrypted, flen+256, char);
							decryptedlen = decryptWithAesCtr256(fcontent, flen, ekey, iv, decrypted);
							if (decryptedlen < 0 || decryptedlen >= flen+256) {
								jsVarSendEval(jj, "guiNotify('%s: Error: problem while decrypting file.');", SPRINT_CURRENT_TIME());
								break;
							}
							fcontent = decrypted;
							flen = decryptedlen;
						}
					} else {
						if (ebixi->tagLen[BIX_TAG_ivh] > 0) {
							jsVarSendEval(jj, "guiNotify('%s: Error: File is encrypted. Use a key to decrypt it.');", SPRINT_CURRENT_TIME());
							break;
						}				
					}
					jsVarDstrAppendPrintf(ss, "var messageContentBase64=\"");
					jsVarDstrAppendBase64EncodedData(ss, fcontent, flen);
					jsVarDstrAppendPrintf(ss, "\";");
					jsVarDstrAppendPrintf(ss, "saveDownloadedBase64FileContent(\"%s\", messageContentBase64);", ebixi->tag[BIX_TAG_fnm]);
					bixMakeTagsSemicolonTerminating(ebixi);
					bixParsingContextFree(&ebixi);
					if (decrypted != NULL) FREE(decrypted);
				} else {
					jsVarSendEval(jj, "guiNotify('%s: Error: Msg %s block %"PRIu64" has wrong type.');", SPRINT_CURRENT_TIME(), msgid, seq);
					break;
				}
				r = jsVarSendEval(jj, "%s", ss->s);
				jsVarDstrFree(&ss);
				break;
			}
		},
		{
			PRINTF0("Error while parsing block content");
		}
		);
	bixParsingContextFree(&bix);
	if (msgNotFoundFlag) {
		jsVarSendEval(jj, "guiNotify('%s: Error: Msg %s not found in block %"PRIu64".');", SPRINT_CURRENT_TIME(), msgid, seq);
	}
	bchainBlockContentFree(&bc);
}

////////////////////////////////////////////////////
