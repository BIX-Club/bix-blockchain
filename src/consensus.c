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

// accepted engagement can not expire anymore
#define CONSENSUS_ENGAGEMENT_EXPIRED(nn) (								\
		(nn)->firstAnnonceTimeUsec < currentTime.usec - CONSENSUS_EXPIRATION_TIMEOUT \
		&& (nn)->engagementRound < CONSENSUS_ENGAGEMENT_ROUND_MAX-1		\
		)

struct consensusRoundsStr consensusRounds[CONSENSUS_ENGAGEMENT_ROUND_MAX] = {
	{CONSENSUS_TICK_USEC,						50,		0,	0, 0},
	{CONSENSUS_TICK_USEC*5,						50, 	0, 	0, 0},  // dummy round, just increase timeout
	{CONSENSUS_TICK_USEC*10,					50, 	1, 	0, 0},	// have data
	{CONSENSUS_TICK_USEC*20,					50, 	0, 	1, 0},  // content correct
	{CONSENSUS_TICK_USEC*25,					50, 	0, 	0, 1},  // signed
	{CONSENSUS_ENGAGEMENT_EXPIRATION_INFINITY,  50, 	0,	0, 1},
};

//////////////////////
// forward declaration


/////////////////////

static int consensusBlockContentComparator(struct blockContentDataList *b1, struct blockContentDataList *b2) {
	int r;
	if (b1->blockSequenceNumber < b2->blockSequenceNumber) return(-1);
	if (b1->blockSequenceNumber > b2->blockSequenceNumber) return(1);
	r = strcmp(b1->blockId, b2->blockId);
	return(r);
}

struct blockContentDataList *consensusBlockContentCreateCopy(struct blockContentDataList *bc) {
	struct blockContentDataList *res;
	ALLOC(res, struct blockContentDataList);
	*res = *bc;
	res->blockId = strDuplicate(bc->blockId);
	res->blockContentHash = strDuplicate(bc->blockContentHash);
	res->signatures = strDuplicate(bc->signatures);
	if (bc->msgIds == NULL) {
		res->msgIdsLength = 0;
		res->msgIds = NULL;
	} else {
		res->msgIdsLength = bc->msgIdsLength;
		ALLOCC(res->msgIds, bc->msgIdsLength, char);
		memcpy(res->msgIds, bc->msgIds, bc->msgIdsLength);
	}
	if (bc->blockContent == NULL) {
		res->blockContent = NULL;
		res->blockContentLength = 0;
	} else {
		res->blockContentLength = bc->blockContentLength;
		ALLOCC(res->blockContent, bc->blockContentLength, char);
		memcpy(res->blockContent, bc->blockContent, bc->blockContentLength);
	}
	return(res);
}

void bchainBlockContentFree(struct blockContentDataList **bc) {
	struct blockContentDataList *bb, *bbnext;

	if (bc == NULL || *bc == NULL) return;
	bb = *bc;
	while (bb != NULL) {
		bbnext = bb->next;
		FREE(bb->blockId);
		FREE(bb->msgIds);
		FREE(bb->blockContent);
		FREE(bb->blockContentHash);
		FREE(bb->signatures);
		FREE(bb);
		bb = bbnext;
	}
	*bc = NULL;
}

struct blockContentDataList *consensusBlockContentFind(int blockSequenceNumber, char *hash) {
	int							diff;
	struct blockContentDataList	bb, *memb;

	diff = blockSequenceNumber - node->currentBlockSequenceNumber;
	// ignore old announce
	if (diff < 0) return(NULL);
	if (diff >= CONSENSUS_MAX_MEMORIZED_SEQUENCE_DIFF) return(NULL);

	bb.blockSequenceNumber = blockSequenceNumber;
	bb.blockId = hash;
	SGLIB_LIST_FIND_MEMBER(struct blockContentDataList, receivedBlockContents[diff], &bb, consensusBlockContentComparator, next, memb);
	return(memb);
}

int consensusBlockContentValidation(int blockSequenceNumber, char *hash) {
	int							r, diff, res;
	struct blockContentDataList	bb, *memb;

	memb = consensusBlockContentFind(blockSequenceNumber, hash);
	if (memb == NULL) return(0);
	r = bchainVerifyBlockContent(memb);
	return(r);
}

int consensusBlockSignatureValidation(struct blockContentDataList *memb, int signode, char *signature) {
	int							r, diff, res;

	if (memb == NULL) return(0);
	// PRINTF("S1: %p\n", memb->blockContent);
	if (memb->blockContent == NULL) return(0);
	r = verifyMessageSignature(memb->blockContent, memb->blockContentLength, MESSAGE_DATATYPE_BYTECOIN, getAccountName_st(0, signode), signature, strlen(signature), 1);
	// PRINTF("S3: r == %d\n", r);
	return(r);
}

static int consensusIsTheSameEngagementAndBlockProposal(struct consensusBlockEngagementInfo *rr, struct blockContentDataList *bb) {
	if (rr == NULL || bb == NULL) return(0);
	if (rr->blockSequenceNumber != bb->blockSequenceNumber) return(0);
	if (strcmp(rr->blockId, bb->blockId) != 0) return(0);
	return(1);	
}

/////////////////////


static int consensusIsTheSameBlockProposal(struct consensusBlockEngagementInfo *r1, struct consensusBlockEngagementInfo *r2) {
	// whether those two proposals reffer to the same block
	if (r1 == NULL || r2 == NULL) return(0);
	if (r1->blockSequenceNumber != r2->blockSequenceNumber) return(0);
	if (strcmp(r1->blockId, r2->blockId) != 0) return(0);
	return(1);
}

static int consensusIsTheSameProposal(struct consensusBlockEngagementInfo *r1, struct consensusBlockEngagementInfo *r2) {
	// whether those two proposals reffer to the same block
	if (consensusIsTheSameBlockProposal(r1, r2) == 0) return(0);
	if (r1->engagedNode != r2->engagedNode) return(0);
	return(1);
}

static int consensusCountEngagementsWithinExpiration(struct consensusBlockEngagementInfo *rr, int64_t expirationUsec) {
	int							i, count;
	struct consensusBlockEngagementInfo 	*ee;
	count = 0;
	for(i=0; i<coreServersMax; i++) {
		ee = engagementsPerServer[0][i];
		if (ee != NULL && consensusIsTheSameProposal(ee, rr)) {
			if (ee->expirationUsec <= expirationUsec) count++;
		}
	}
	return(count);
}

static int consensusProposalsEngagementSuitabilityCmp(struct consensusBlockEngagementInfo *r1, struct consensusBlockEngagementInfo *r2) {
	int r, e1, e2;

	// accepted is the best
	if (r1->engagementRound == CONSENSUS_ENGAGEMENT_ROUND_MAX - 1) return(1);
	if (r2->engagementRound == CONSENSUS_ENGAGEMENT_ROUND_MAX - 1) return(-1);

	// before expiration is better
	// if (CONSENSUS_ENGAGEMENT_EXPIRED(r1)) return(-1);
	// if (CONSENSUS_ENGAGEMENT_EXPIRED(r2)) return(1);

	// close to expiration
	if (r1->expirationUsec < currentTime.usec + CONSENSUS_TICK_USEC/32) return(-1);
	if (r2->expirationUsec < currentTime.usec + CONSENSUS_TICK_USEC/32) return(1);

	// smaller hash is better
	r = strcmp(r1->blockId, r2->blockId);
	if (r < 0) return(-1);
	if (r > 0) return(1);

	// highest round is better
	if (r1->engagementRound < r2->engagementRound) return(-1);
	if (r1->engagementRound > r2->engagementRound) return(1);

	// more subscribed servers is better
	e1 = consensusCountEngagementsWithinExpiration(r1, currentTime.usec + 3*CONSENSUS_TICK_USEC/4);
	e2 = consensusCountEngagementsWithinExpiration(r2, currentTime.usec + 3*CONSENSUS_TICK_USEC/4);
	if (e1 < e2) return(-1);
	if (e1 > e2) return(1);

	// older is better
	if (r1->blockCreationTimeUsec > r2->blockCreationTimeUsec) return(-1);
	if (r1->blockCreationTimeUsec < r2->blockCreationTimeUsec) return(1);

	// later expiration is better
	if (r1->expirationUsec < r2->expirationUsec) return(-1);
	if (r1->expirationUsec > r2->expirationUsec) return(1);

	return(0);
}

static struct consensusBlockEngagementInfo *consensusFindTheBestProposal() {
	int							i;
	struct consensusBlockEngagementInfo 	*res, *rr;

	res = NULL;
	for(i=0; i<coreServersMax; i++) {
		rr = engagementsPerServer[0][i];
		if (rr != NULL) {
			if (res == NULL || consensusProposalsEngagementSuitabilityCmp(rr, res) > 0) res = rr;
		}
	}
	if (res == NULL || res->expirationUsec < currentTime.usec || CONSENSUS_ENGAGEMENT_EXPIRED(res)) return(NULL);
	return(res);
}

static void consensusSignCurrentEngagement() {
	struct blockContentDataList *data;

	if (currentEngagement->blocksignature == NULL || currentEngagement->blocksignature[0] == 0) {
		// time to sign block
		data = consensusBlockContentFind(currentEngagement->blockSequenceNumber, currentEngagement->blockId);
		if (data == NULL || data->blockContent == NULL) {
			PRINTF("Internal error: can't sign my engagement for %s. Data not available.", currentEngagement->blockId);
			return;
		}
		FREE(currentEngagement->blocksignature);
		currentEngagement->blocksignature = signatureCreateBase64(data->blockContent, data->blockContentLength, myNodePrivateKey);
	}
}

static int consensusMapOnValidEngagements(void (*fun)(struct consensusBlockEngagementInfo*rr,void*arg), void *funarg) {
	struct consensusBlockEngagementInfo 	*rr;
	struct blockContentDataList				*memb;
	int 									count, i;

	memb = NULL;
	if (consensusRounds[currentEngagement->engagementRound].blockSignatureRequired) {
		memb = consensusBlockContentFind(currentEngagement->blockSequenceNumber, currentEngagement->blockId);
	}
	count = 0;
	for(i=0; i<coreServersMax; i++) {
		rr = engagementsPerServer[0][i];
		if (rr != NULL && consensusIsTheSameBlockProposal(rr, currentEngagement)) {
			if (rr->engagementRound >= currentEngagement->engagementRound
				&& rr->expirationUsec >= currentTime.usec - CONSENSUS_TICK_USEC/2
				&& ! CONSENSUS_ENGAGEMENT_EXPIRED(rr)
				) {
				if (consensusRounds[currentEngagement->engagementRound].blockSignatureRequired == 0
					|| consensusBlockSignatureValidation(memb, rr->engagedNode, rr->blocksignature) == 0) {
					count ++;
					if (fun != NULL) fun(rr, funarg);
				}
			}
		}
	}
	return(count);
}

static void consensusAddValidSignatureToDstr(struct consensusBlockEngagementInfo *rr, void *sss) {
	struct jsVarDynamicString *ss;
	ss = (struct jsVarDynamicString *)sss;
	jsVarDstrAppendPrintf(ss, "src=%d;sig=%s;", rr->engagedNode, rr->blocksignature);
}

static void consensusAddSignaturesToAcceptedBlock() {
	struct blockContentDataList *data;
	struct jsVarDynamicString 	*ss;

	data = consensusBlockContentFind(currentEngagement->blockSequenceNumber, currentEngagement->blockId);
	// PRINTF0("ADDING SIGS\n");
	FREE(data->signatures);
	ss = jsVarDstrCreate();
	// first add my engagement signature
	consensusAddValidSignatureToDstr(currentEngagement, ss);
	// then all other voting servers
	consensusMapOnValidEngagements(consensusAddValidSignatureToDstr, ss);
	data->signatures = jsVarDstrGetStringAndReinit(ss);
	// PRINTF("SIGS == %s\n", data->signatures);
	jsVarDstrFree(&ss);
}

void consensusMaybeProlongCurrentEngagement(int forceBroadcastEngagementFlag) {
	int									i, r, count, quorum;
	int									furtherProlongationPossibleFlag;
	int									passToNextRoundFlag;
	struct consensusBlockEngagementInfo *res, *rr;

	if (currentEngagement == NULL) return;

	furtherProlongationPossibleFlag = 1;
	while (furtherProlongationPossibleFlag) {
		furtherProlongationPossibleFlag = 0;
		count = consensusMapOnValidEngagements(NULL, NULL);
		// there is count + 1 (me) votes for this engagement
		// if not passing over quorum, do nothing
		quorum = coreServersMax * consensusRounds[currentEngagement->engagementRound].quorumPercentage / 100;
		if (debuglevel > 10) PRINTF("Round %d: my engagement %"PRId64" %s has support of %d other server. Quorum is %d.\n", currentEngagement->engagementRound, currentEngagement->blockSequenceNumber, currentEngagement->blockId, count, quorum);
		if (count >= coreServersMax - 1 || count >= quorum) {
			// passed the quorum, make next round
			if (debuglevel > 10) PRINTF("Engagement for %"PRId64" %s may pass to next round from %d.\n", currentEngagement->blockSequenceNumber, currentEngagement->blockId, currentEngagement->engagementRound);
			if (
				currentEngagement->engagementRound > 0 
				&& consensusRounds[currentEngagement->engagementRound-1].blockContentRequired == 0
				&& consensusRounds[currentEngagement->engagementRound].blockContentRequired == 1
				&& currentProposedBlock != NULL
				&& strcmp(currentProposedBlock->blockId, currentEngagement->blockId) == 0
				) {
				// They needs the content of my block
				bixBroadcastCurrentProposedBlockContent();
			}
			// check special conditions (block content received and block content valid)
			passToNextRoundFlag = 1;
#if 0
			if (
				(consensusEngagementRoundsTimes[currentEngagement->engagementRound].blockContentRequired == 0 
				 || consensusBlockContentFind(currentEngagement->blockSequenceNumber, currentEngagement->blockId) != NULL
					)
				&& 
				(consensusEngagementRoundsTimes[currentEngagement->engagementRound].blockValidationRequired == 0 
				 || consensusBlockContentIsValid(currentEngagement->blockSequenceNumber, currentEngagement->blockId)
					)
				) {
 				passToNextRoundFlag = 1;
			}
#else
			if (consensusRounds[currentEngagement->engagementRound].blockContentRequired) {
				if (consensusBlockContentFind(currentEngagement->blockSequenceNumber, currentEngagement->blockId) == NULL) {
					passToNextRoundFlag = 0;
				}
			}
			if (consensusRounds[currentEngagement->engagementRound].blockValidationRequired) {
				r = consensusBlockContentValidation(currentEngagement->blockSequenceNumber, currentEngagement->blockId);
				if (r != 0) {
					passToNextRoundFlag = 0;
					if (r < 0) {
						// block is unreparably invalid. I think that I do not need to keep my engagement
						// even if time is not over, we can abandon the block immediately
						consensusOnCurrentBlockProposalIsInconsistent();
					}
				}
			}
#endif
			if (passToNextRoundFlag) {
				if (currentEngagement->engagementRound < CONSENSUS_ENGAGEMENT_ROUND_MAX - 2) {
					currentEngagement->engagementRound ++;
					if (debuglevel > 10) PRINTF("Engagement for %"PRId64" %s passed to round %d.\n", currentEngagement->blockSequenceNumber, currentEngagement->blockId, currentEngagement->engagementRound);
					currentEngagement->expirationUsec = currentEngagement->expirationUsec 
						- consensusRounds[currentEngagement->engagementRound-1].roundTimeout
						+ consensusRounds[currentEngagement->engagementRound].roundTimeout
						;
					if (consensusRounds[currentEngagement->engagementRound].blockSignatureRequired) consensusSignCurrentEngagement();
					forceBroadcastEngagementFlag = 1;
					furtherProlongationPossibleFlag = 1;
				} else if (currentEngagement->engagementRound == CONSENSUS_ENGAGEMENT_ROUND_MAX - 2) {
					consensusAddSignaturesToAcceptedBlock();
					currentEngagement->engagementRound ++;
					if (debuglevel > 10) PRINTF("Engagement for %"PRId64" %s passed to final round %d.\n", currentEngagement->blockSequenceNumber, currentEngagement->blockId, currentEngagement->engagementRound);
					// permanent engagement, can be voided by human operator only
					// PRINTF("Permanent engagement for block %d:%s\n", currentEngagement->engagedNode, currentEngagement->hash);
					currentEngagement->expirationUsec = CONSENSUS_ENGAGEMENT_EXPIRATION_INFINITY;
					forceBroadcastEngagementFlag = 1;
					furtherProlongationPossibleFlag = 1;
				}
			}
		}
	}
	if (forceBroadcastEngagementFlag) {
		// PRINTF("Engagement of node %d round %d for block %s\n", currentEngagement->engagedNode, currentEngagement->engagementRound, currentEngagement->hash);
		bixBroadcastCurrentBlockEngagementInfo();
	}
}

static void consensusEngagementFree(struct consensusBlockEngagementInfo **rr) {
	if (rr == NULL || *rr == NULL) return;
	FREE((*rr)->blockId);
	FREE((*rr)->blocksignature);
	FREE(*rr);
	*rr = NULL;
}

static void consensusCurrentEngagementReset(struct consensusBlockEngagementInfo *newEngagement) {
	int64_t			exp;

	consensusEngagementFree(&currentEngagement);
	if (newEngagement == NULL) return;
	ALLOC(currentEngagement, struct consensusBlockEngagementInfo);
	memcpy(currentEngagement, newEngagement, sizeof(*currentEngagement));
	currentEngagement->engagedNode = myNode;
	currentEngagement->engagementRound = 0;
	exp = newEngagement->expirationUsec;
	if (exp > currentTime.usec + CONSENSUS_TICK_USEC) exp = currentTime.usec + CONSENSUS_TICK_USEC;
	currentEngagement->expirationUsec = exp;
	currentEngagement->blocksignature = strDuplicate(newEngagement->blocksignature);
	currentEngagement->blockId = strDuplicate(newEngagement->blockId);
	currentEngagement->blockSequenceNumber = newEngagement->blockSequenceNumber;
	currentEngagement->serverMsgSequenceNumber = 0;
}

void consensusReconsiderMyEngagement() {
	int									r;
	int 								forceBroadcastEngagementFlag, blockAccepted;
	struct consensusBlockEngagementInfo *newEngagement;
	struct blockContentDataList 		*data;

	// we are not going to take any engagement if we are in resynchronization mode
	if (nodeState == NODE_STATE_RESYNC) return;

	// keep this loop, so that I can quickly catch other servers if they are ahead of me.
	for(;;) {
		forceBroadcastEngagementFlag = 0;
		if (currentEngagement == NULL || currentEngagement->expirationUsec < currentTime.usec) {
			// I am not engaged (anymore), find the best suitable proposal and answer it
			// PRINTF("Taking new engagement\n", 0);
			newEngagement = consensusFindTheBestProposal();
			// do not blindly continue in self proposed block. Rather recreate it (some messsages may expired in between).
			if (consensusIsTheSameEngagementAndBlockProposal(newEngagement, currentProposedBlock)) newEngagement = NULL;
			if (debuglevel > 60 && newEngagement == NULL) PRINTF("No suitable engagement found for seqnum %"PRId64" !!!!\n", node->currentBlockSequenceNumber);
			consensusCurrentEngagementReset(newEngagement);
			forceBroadcastEngagementFlag = 1;
		}
		// I am engaged in a solution, but maybe the conditions are satisfied to prolong the engagement
		// PRINTF("Checking for prolongation\n", 0);
		consensusMaybeProlongCurrentEngagement(forceBroadcastEngagementFlag);
		if (currentEngagement == NULL) return;
		if (currentEngagement->engagementRound < CONSENSUS_ENGAGEMENT_ROUND_MAX - 1) return;

		// more than a half of servers are in permanent engagement for this solution
		if (debuglevel > 10) PRINTF("Block accepted on node %2d. Id: %s; Seq: %4"PRId64"\n", currentEngagement->engagedNode, currentEngagement->blockId, currentEngagement->blockSequenceNumber);
		data = consensusBlockContentFind(currentEngagement->blockSequenceNumber, currentEngagement->blockId);
		bchainOnNewBlockAccepted(data);
		consensusOnBlockAccepted();
		assert(currentEngagement == NULL);
	}
}

static int consensusNumberOfNodesWithEngagementOutOfOrder() {
	int i, res;
	res = 0;
	for(i=0; i<coreServersMax; i++) {
		if (nodeEngagementOutOfOrderFlag[i]) res ++;
	}
	return(res);
}

int consensusIsThereSomeFutureBlockProposal() {
	int i, j;
	for(j=1; j<CONSENSUS_MAX_MEMORIZED_SEQUENCE_DIFF; j++) {
		for(i=0; i<coreServersMax; i++) {
			if (engagementsPerServer[j][i] != NULL) return(1);
		}
	}
	return(0);
}

void consensusNewEngagementReceived(struct consensusBlockEngagementInfo *nn) {
	int										r, diff, cclen;
	struct consensusBlockEngagementInfo 	*rr, **rrr;
	struct blockContentDataList				*data;
	char									*cc;
	
#if 0
	if (nn->engagementRound == CONSENSUS_ENGAGEMENT_ROUND_MAX-1) {
		// store information about accepted block by node for gui
		InternalCheck(nn->engagedNode >= 0 && nn->engagedNode < coreServersMax);
		networkNodes[nn->engagedNode].blockSeqn = nn->blockSequenceNumber;
		guiUpdateNodeStatus(nn->engagedNode);
	}
#endif

	diff = nn->blockSequenceNumber - node->currentBlockSequenceNumber;
	// ignore old announce
	if (diff < 0) return;
	// ignore expired blocks
	if (CONSENSUS_ENGAGEMENT_EXPIRED(nn)) return;

	if (diff >= CONSENSUS_MAX_MEMORIZED_SEQUENCE_DIFF) {
		nodeEngagementOutOfOrderFlag[nn->engagedNode] = 1;
		if (nodeState != NODE_STATE_RESYNC) {
			PRINTF("Warning: Node %d is too much in advance. Ignoring engagement for sequence %"PRId64"\n", nn->engagedNode, nn->blockSequenceNumber);
			if (consensusNumberOfNodesWithEngagementOutOfOrder() > coreServersMax / 2) {
				// It is probably my problem. Switch to resynchronization state
				// For the moment, suppose that after 10 more nodes I will be able to get synced.
				nodeResyncUntilSeqNum = nn->blockSequenceNumber + 10;
				timeLineInsertEvent(UTIME_AFTER_USEC(0), bchainStartResynchronization, NULL);
			}
		}
		return;
	}

	nodeEngagementOutOfOrderFlag[nn->engagedNode] = 0;
	rrr = &engagementsPerServer[diff][nn->engagedNode];
	rr = *rrr;
	if (rr == NULL) {
		// PRINTF("New proposal\n", 0);
		ALLOC(rr, struct consensusBlockEngagementInfo);
		memcpy(rr, nn, sizeof(*nn));
		rr->blockId = strDuplicate(nn->blockId);
		rr->blocksignature = strDuplicate(nn->blocksignature);
		*rrr = rr;
	} else {
		// from the two engagements coming from the same server, take the one having larger serverMsgSequenceNumber
		// PRINTF("Replacing proposal \n", 0);
		if (rr->serverMsgSequenceNumber < nn->serverMsgSequenceNumber) {
			FREE(rr->blockId);
			FREE(rr->blocksignature);
			*rr = *nn;
			rr->blockId = strDuplicate(nn->blockId);
			rr->blocksignature = strDuplicate(nn->blocksignature);
		}
	}
}

void consensusBlockContentReceived(struct blockContentDataList *bc) {
	int 							diff;
	struct blockContentDataList		**bb, *memb;

	diff = bc->blockSequenceNumber - node->currentBlockSequenceNumber;
	if (debuglevel > 30) PRINTF("Got block content %"PRId64" %s size %d; diff %d\n", bc->blockSequenceNumber, bc->blockId, bc->msgIdsLength, diff);
	// ignore old announce
	if (diff < 0) return;
	if (diff >= CONSENSUS_MAX_MEMORIZED_SEQUENCE_DIFF) {
		if (nodeState != NODE_STATE_RESYNC) {
			PRINTF("Warning: Node %d is too much late. Ignoring block content with sequence %"PRId64"\n", myNode, bc->blockSequenceNumber);
		}
		return;
	}
	bb = &receivedBlockContents[diff];
	SGLIB_LIST_FIND_MEMBER(struct blockContentDataList, *bb, bc, consensusBlockContentComparator, next, memb);
	if (memb == NULL) {
		// new block content info
		// PRINTF("Got block %s content\n", bc->blockHash);
		memb = consensusBlockContentCreateCopy(bc);
		memb->next = *bb;
		*bb = memb;
	} else {
		PRINTF("Warning: Redundand block content message for block %s seq %"PRId64" ignored!\n", bc->blockId, bc->blockSequenceNumber);
	}
}

///////////////////

void consensusOnCurrentBlockProposalIsInconsistent() {
	int				i, j;

	// If currently proposed block has a logical failure, abandon it
	consensusEngagementFree(&currentEngagement);
	for(i=0; i<coreServersMax; i++) {
		consensusEngagementFree(&engagementsPerServer[0][i]);
		engagementsPerServer[0][i] = NULL;
	}
	currentProposedBlock = NULL;
	bchainBlockContentFree(&receivedBlockContents[0]);
}

void consensusOnBlockAccepted() {
	int				i, j;

	// PRINTF("HERE I AM\n", 0); fflush(stdout);
	consensusEngagementFree(&currentEngagement);
	for(i=0; i<coreServersMax; i++) {
		consensusEngagementFree(&engagementsPerServer[0][i]);
		for(j=0; j<CONSENSUS_MAX_MEMORIZED_SEQUENCE_DIFF-1; j++) engagementsPerServer[j][i] = engagementsPerServer[j+1][i];
		engagementsPerServer[j][i] = NULL;
	}
	currentProposedBlock = NULL;
	bchainBlockContentFree(&receivedBlockContents[0]);
	for(j=0; j<CONSENSUS_MAX_MEMORIZED_SEQUENCE_DIFF-1; j++) receivedBlockContents[j] = receivedBlockContents[j+1];
	receivedBlockContents[j] = NULL;
}


