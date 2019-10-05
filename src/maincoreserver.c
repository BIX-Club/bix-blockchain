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

// forward declarations
static void mainCoreConnectToNode(void *n);

/////////////////////////////////////////////////////////////////////////////////////////////////

static void mainCoreShutDown(void *d) {
	bchainWriteGlobalStatus();
	fclose(nodeFile);
    PRINTF0("Exiting.\n"); fflush(stdout);
	exit(0);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
// gui

struct connectionGraphElem {
	int a, b;
	int status;
};

struct connectionGraphElem *connectionGraph;
int							connectionGraphSize;

static void mainCoreDisconnectionTest2(void *sss) {
	int seconds;

	seconds = (int)sss;
	msSleep(seconds*1000);
	setCurrentTime();
	PRINTF0("Waking up.\n");
}

static void mainCoreDisconnectionTest(void *sss) {
	int				i;
	int seconds;

	seconds = (int)sss;
	for(i=0; i<SERVERS_MAX; i++) {
		baioCloseMagicOnError(coreServersConnections[i].activeConnectionBaioMagic);
		baioCloseMagicOnError(coreServersConnections[i].passiveConnectionBaioMagic);
	}
	timeLineInsertEvent(UTIME_AFTER_MSEC(750), mainCoreDisconnectionTest2, (void*)seconds);
}

static void onAction(struct jsVar *v, int index, struct jsVaraio *jj) {
	char 	*msg;
	char	*action;
	char	*request;
	int		seconds;

	msg = jsVarGetString(v);
	if (debuglevel > 60) PRINTF("New message from gui: %s.\n", msg);
	action = jsVarGetEnv_st(msg, "action");

	if (strcmp(action, "disconnectionTest") == 0) {
		// this is a little bit brute force freezing
		seconds = 60;
		PRINTF("Disconnecting for %d seconds.\n", seconds);
		timeLineInsertEvent(UTIME_AFTER_MSEC(250), mainCoreDisconnectionTest, (void*)seconds);
	} else if (strcmp(action, "pendingRequestExplore") == 0) {
		request = jsVarGetEnv_st(msg, "request");
		pendingRequestExplore(request, jj);
	} else if (strcmp(action, "pendingRequestApprove") == 0) {
		request = jsVarGetEnv_st(msg, "request");
		pendingRequestApprove(request);
	} else if (strcmp(action, "pendingRequestDelete") == 0) {
		request = jsVarGetEnv_st(msg, "request");
		pendingRequestDelete(request);
	}
}

static void mainCoreGuiCreateConnectionGraph() {
	int 							i, j, k;
	struct serverTopology			*ss;
	struct jsVar					*edgea;
	struct jsVar					*edgeb;
	struct jsVar					*edgewatch;
	struct jsVar					*v;
	struct networkConnectionsStr 	*cc;


	v = jsVarCreateNewSynchronizedInt(jsVarSpaceGlobal, "myNode");
	jsVarSetInt(v, myNode);

	edgea = jsVarCreateNewSynchronizedIntVector(jsVarSpaceGlobal, "serverEdge[?].a");
	edgeb = jsVarCreateNewSynchronizedIntVector(jsVarSpaceGlobal, "serverEdge[?].b");
	guiEdgeStatus = jsVarCreateNewSynchronizedIntVector(jsVarSpaceGlobal, "serverEdge[?].status");
	guiEdgeLatency = jsVarCreateNewSynchronizedIntVector(jsVarSpaceGlobal, "serverEdge[?].latency");
	edgewatch = jsVarCreateNewSynchronizedInt(jsVarSpaceGlobal, "serverEdge~");

	k = 0;
	for(i=0; i<coreServersMax; i++) {
		ss = &coreServers[i];
		for(j=0; j<ss->connectionsMax; j++) {
			jsVarResizeVector(edgea, k+1);
			jsVarResizeVector(edgeb, k+1);
			jsVarResizeVector(guiEdgeStatus, k+1);
			jsVarResizeVector(guiEdgeLatency, k+1);
			jsVarSetIntVector(edgea, k, i);
			jsVarSetIntVector(edgeb, k, ss->connections[j]);
			cc = &networkConnections[i][ss->connections[j]];
			jsVarSetIntVector(guiEdgeStatus, k, cc->status);
			jsVarSetIntVector(guiEdgeLatency, k, cc->latency);
			cc->guiEdgeIndex = k;
			k++;
		}
	}
}

static int onGuiConnectionAccepted(struct jsVaraio *jj) {
	guiConnectionsInsertBaioMagic(jj->w.b.baioMagic);
	return(0);
}

void coreServerGuiInit() {
	struct jsVar	*inp, *title;
	struct jsVaraio	*jj;
	char			ttt[TMP_STRING_SIZE];
	int				i;

	if (coreServers[myNode].guiListeningPort < 0) return;

	jj = jsVarNewFileServer(
		coreServers[myNode].guiListeningPort, BAIO_SSL_YES, 0,
		"wwwcore"
		);
	if (jj == NULL) {
		PRINTF0("Can't open GUI port. Exiting.\n");
		mainCoreShutDown(NULL);
		return;
	}
 	jsVarCallBackAddToHook(&jj->callBackOnWebsocketAccept, onGuiConnectionAccepted);
    jsVarCallBackAddToHook(&jj->callBackOnWwwGetRequest, mainServerOnWwwGetRequest);
	inp = jsVarCreateNewSynchronizedStringInput(jsVarSpaceGlobal, "action", onAction);

	guiMyRoutingPathLatency = jsVarCreateNewSynchronizedIntArray(jsVarSpaceGlobal, "myNodeRoutingPaths[?].latency", coreServersMax);
	guiMyRoutingPathPath = jsVarCreateNewSynchronizedStringArray(jsVarSpaceGlobal, "myNodeRoutingPaths[?].path", coreServersMax);
	guiMyBroadcastPath = jsVarCreateNewSynchronizedString(jsVarSpaceGlobal, "myNodeBroadcastPath");
	jsVarCreateNewSynchronizedInt(jsVarSpaceGlobal, "myNodeRoutingPaths~");

	guiNodeStateName = jsVarCreateNewSynchronizedStringArray(jsVarSpaceGlobal, "nodeState[?].name", coreServersMax);
	guiNodeStateStatus = jsVarCreateNewSynchronizedIntArray(jsVarSpaceGlobal, "nodeState[?].status", coreServersMax);
	guiNodeStateUsedSpace = jsVarCreateNewSynchronizedLlongArray(jsVarSpaceGlobal, "nodeState[?].uspace", coreServersMax);
	guiNodeStateAvailableSpace = jsVarCreateNewSynchronizedLlongArray(jsVarSpaceGlobal, "nodeState[?].aspace", coreServersMax);
	guiNodeStateSeqn = jsVarCreateNewSynchronizedLlongArray(jsVarSpaceGlobal, "nodeState[?].seqn", coreServersMax);
	jsVarCreateNewSynchronizedInt(jsVarSpaceGlobal, "nodeState~");
	guiUpdateNodeStatus(myNode);
	
	for(i=0;i<coreServersMax; i++) {
		jsVarSetStringArray(guiNodeStateName, i, networkNodes[i].name);
	}

	mainCoreGuiCreateConnectionGraph();

    guiPendingRequestsVector = jsVarCreateNewSynchronizedStringVector(jsVarSpaceGlobal, "pendingRequests[?].name");
    jsVarCreateNewSynchronizedInt(jsVarSpaceGlobal, "pendingRequests~");
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////

static int mainCoreFindMyIndex(int argc, char **argv) {
	int 	argi, i;

	argi = 1;

	if (argi>=argc) EXIT_WITH_FAIL(-1, "Missing command line arguments.\n", 0);

	for(i=0; i<coreServersMax; i++) {
		if (strcmp(argv[argi], coreServers[i].name) == 0 /* && atoi(argv[argi+1]) == coreServers[i].bchainCoreServersListeningPort */) {
			if (myNode < 0) { 
				myNode = i;
			} else {
				PRINTF("Conflicting setting of servers, nodes %d <-> %d\n", myNode, i);
			}
		}
	}
	if (myNode < 0) {
		EXIT_WITH_FAIL(-1, "Can't identify myself as a valid server. Put server name and port as command line arguments.\n", 0);
	}
	sprintf(mynodeidstring, "%02d", myNode);
	return(myNode);
}

static void mainCoreInitResynchronizationRequestTable() {
	bchainResynchronizationRequest = NULL;
}

static void mainCoreServerNetworkNodesInit() {
	int 	i;
	for(i=0; i<coreServersMax; i++) {
		networkNodes[i].index = i;
		networkNodes[i].name = coreServers[i].guiname;
	}
	networkNodes[myNode].status = nodeState;
	networkNodes[myNode].blockSeqn = node->currentBlockSequenceNumber-1;
	networkNodes[myNode].usedSpace = node->currentUsedSpaceBytes;
	networkNodes[myNode].totalAvailableSpace = node->currentTotalSpaceAvailableBytes;
}

static int mainCoreAcceptCallback(struct baio *bb) {
	struct bcaio			*cc;
	int						i;

	cc = (struct bcaio *) bb;

	if (debuglevel > 0) PRINTF("Connection accepted from ip %s -> node %d\n", sprintfIpAddress_st(cc->b.remoteComputerIp), cc->fromNode);
	return(0);
}

static int mainCoreListeningDeleteCallback(struct baio *bb) {
	struct bcaio			*cc;
	int						i;

	cc = (struct bcaio *) bb;
	if (debuglevel > 0) PRINTF("Connection closed from ip %s -> node %d\n", sprintfIpAddress_st(cc->b.remoteComputerIp), cc->fromNode);
	networkConnectionStatusChange(cc->fromNode, cc->toNode, NCST_CLOSED);
	return(0);
}

static int mainCoreConnectCallback(struct baio *bb) {
	struct bcaio			*cc;

	cc = (struct bcaio *) bb;
	if (debuglevel > 0) PRINTF("Connected to node %d\n", cc->toNode);
	coreServersConnections[cc->toNode].activeConnectionBaioMagic = cc->b.baioMagic;
	bixSendLoginToConnection(cc);
	networkConnectionStatusChange(myNode, cc->toNode, NCST_OPEN);

	bixSendImmediatePingAndScheduleNext(cc);
	return(0);
}

static int mainCoreDeleteCallback(struct baio *bb) {
	struct bcaio			*cc;

	cc = (struct bcaio *) bb;
	if (debuglevel > 10) PRINTF("Connection %d->%d closed\n", cc->fromNode, cc->toNode);
	if (cc->fromNode == myNode) {
		// PRINTF("Will reconnect later\n", cc->fromNode, cc->toNode);
#if EDEBUG
		timeLineInsertEvent(UTIME_AFTER_SECONDS(3), mainCoreConnectToNode, (void*)cc->toNode);
#else
		timeLineInsertEvent(UTIME_AFTER_SECONDS(10), mainCoreConnectToNode, (void*)cc->toNode);
#endif
	}
	networkConnectionStatusChange(cc->fromNode, cc->toNode, NCST_CLOSED);
	return(0);
}

static int mainCoreReadCallback(struct baio *bb, int fromj, int n) {
	struct bcaio			*cc;

	cc = (struct bcaio *) bb;
	// PRINTF("Got %d bytes. Nodes %d -> %d.\n", n, cc->fromNode, cc->toNode);
	// PRINTF("MSG: %.*s\n", n, bb->readBuffer.b + bb->readBuffer.i); 
	// fflush(stdout);
	bixParseAndProcessAvailableMessages(cc);
	return(1);
}

static int mainCoreEofCallback(struct baio *bb) {
	// If EOF was read, process read buffer and dalete the connection
	mainCoreReadCallback(bb, bb->readBuffer.j, 0);
	baioClose(bb);
	return(1);
}

static void mainCoreOpenBchainListeningConnection(void *d) {
	struct baio				*bb;
	struct bcaio			*cc;
	bb = baioNewTcpipServer(coreServers[myNode].bchainCoreServersListeningPort, BAIO_SSL_YES, BAIO_ADDITIONAL_SIZE(struct bcaio));
	if (bb == NULL) {
		timeLineInsertEvent(UTIME_AFTER_SECONDS(10), mainCoreOpenBchainListeningConnection, NULL);
	} else {
		bchainSetBufferSizes(bb);
		cc = (struct bcaio *) bb;
		cc->connectionType = CT_CORE_TO_CORE_SERVER;
		cc->remoteNode = -1;
		cc->fromNode = -1;
		cc->toNode = myNode;
		jsVarCallBackAddToHook(&bb->callBackOnRead, mainCoreReadCallback);
		jsVarCallBackAddToHook(&bb->callBackOnAccept, mainCoreAcceptCallback);
		jsVarCallBackAddToHook(&bb->callBackOnDelete, mainCoreListeningDeleteCallback);
	}
}

static void mainCoreOpenSecondaryListeningConnection(void *d) {
	struct baio				*bb;
	struct bcaio			*cc;
	bb = baioNewTcpipServer(coreServers[myNode].secondaryServersListeningPort, BAIO_SSL_YES, BAIO_ADDITIONAL_SIZE(struct bcaio));
	if (bb == NULL) {
		timeLineInsertEvent(UTIME_AFTER_SECONDS(11), mainCoreOpenSecondaryListeningConnection, NULL);
	} else {
		bchainSetBufferSizes(bb);
		cc = (struct bcaio *) bb;
		cc->connectionType = CT_SECONDARY_TO_CORE_IN_CORE_SERVER;
		cc->remoteNode = -1;
		cc->fromNode = -1;
		cc->toNode = myNode;
		jsVarCallBackAddToHook(&bb->callBackOnRead, mainCoreReadCallback);
		jsVarCallBackAddToHook(&bb->callBackOnAccept, mainCoreAcceptCallback);
		jsVarCallBackAddToHook(&bb->callBackOnDelete, mainCoreListeningDeleteCallback);
	}
}

static void mainCoreConnectToNode(void *nn) {
	struct bcaio		*cc;
	struct baio 		*bb;
	struct sockaddr_in 	serv_addr;
	int					n;

	n = (int) nn;

	assert(n >= 0 && n < SERVERS_MAX);

	bb = baioNewTcpipClient(coreServers[n].host, coreServers[n].bchainCoreServersListeningPort, BAIO_SSL_YES, BAIO_ADDITIONAL_SIZE(struct bcaio));
	if (bb == NULL) {
		timeLineInsertEvent(UTIME_AFTER_SECONDS(10), mainCoreConnectToNode, (void*)n);
	} else {
		bchainSetBufferSizes(bb);
		cc = (struct bcaio *) bb;
		cc->connectionType = CT_CORE_TO_CORE_SERVER;
		cc->fromNode = myNode;
		cc->toNode = n;
		cc->remoteNode = n;
		jsVarCallBackAddToHook(&bb->callBackOnRead, mainCoreReadCallback);
		jsVarCallBackAddToHook(&bb->callBackOnEof, mainCoreEofCallback);
		jsVarCallBackAddToHook(&bb->callBackOnConnect, mainCoreConnectCallback);
		jsVarCallBackAddToHook(&bb->callBackOnDelete, mainCoreDeleteCallback);
	}
}

static void mainCoreConnectToOtherNodes() {
	struct serverTopology	*ss;
	int 					i, j;

	ss = &coreServers[myNode];

	for(i=0; i<ss->connectionsMax; i++) {
		j = ss->connections[i];
		mainCoreConnectToNode((void*)j);
	}
}

void mainCoreRegularReBroadcastStatuses(void *d) {
	struct serverTopology	*ss;
	int 					i, j, status;

	ss = &coreServers[myNode];

	for(i=0; i<ss->connectionsMax; i++) {
		j = ss->connections[i];
		status = networkConnections[myNode][j].status;
		networkConnectionStatusChange(myNode, j, status);
	}
	bixBroadcastMyNodeStatusMessage();
	timeLineRescheduleUniqueEvent(UTIME_AFTER_SECONDS(60), mainCoreRegularReBroadcastStatuses, 0);
}


static void mainCoreAfterSelectActions(int maxfd, fd_set *r, fd_set *w, fd_set *e) {
	setCurrentTime();
}

static void mainCoreInitializeData(int argc, char **argv) {
	setCurrentTime();
	mainInitialization();
	mainCoreServersTopologyInit();
	mainCoreFindMyIndex(argc, argv);
	mainCoreInitResynchronizationRequestTable();
	lastAcceptedBlockTime = currentTime.sec;
	coreServerSupportedMessageTableLoad();
	bchainInitializeMessageTypes();
	// PRINTF("LOADING from %s", bAccountDirectory_st(getAccountName_st(0, myNode)));
	myNodePrivateKey = fileLoad(0, "%s/%s", bAccountDirectory_st(getAccountName_st(0, myNode)), WALLET_PRIVATE_KEY_FILE);
}

static void mainCoreIssueGenesisBlockMessages(void *d) {
	char						*newAccount;
	char						*publicKey;
	char						*ooo;
	struct jsVarDynamicString	*ss;
	char						*signAccount, *signature;

	PRINTF0("Generating genessis block messages!\n");

	newAccount = generateNewAccountName_st();
	if (strcmp(newAccount, BYTECOIN_BASE_ACCOUNT) != 0) {
		PRINTF0("Error: Genesis block problem\n\n\n");
		PRINTF("Error: BIX base account problem. Returned account: %s!\n", newAccount);
		PRINTF0("Error: Wrong blockchain initialization! Exiting!\n\n\n");
		exit(-1);
	}
	// TODO: put this to some config file
	if (PILOT_VERSION_FLAG) {
		publicKey = "-----BEGIN PUBLIC KEY-----\nMIGbMBAGByqGSM49AgEGBSuBBAAjA4GGAAQAO7RdgwvlqrIc08EKbyraR37ic2Pv\nnJBj8++N1LULBM7BREeQXx8ZpWsU8h5IdU5EgYncG/PDHfkzZMrHPbtGLusAAE2L\nblNtyOBeO5Pn75GWQVhDnYkXFekHOS4B/tU5GL0yHTBS0EHF6g08CHwg1k6g/WyL\nHK8iHeepZTJ6pxw124k=\n-----END PUBLIC KEY-----";
/*

  To login to pilot BIX base account use account number 1081 and the following private key:

-----BEGIN PRIVATE KEY-----
MIHuAgEAMBAGByqGSM49AgEGBSuBBAAjBIHWMIHTAgEBBEIBi3CurwLqHAyUqXc+
2y8Xqa9N7ocR/FOviF+EN9MsLMwLBMcxnDVbyYlhy1/hmLnU2JRG4R1UeL/r2Jx+
NeRJQ5ehgYkDgYYABAA7tF2DC+WqshzTwQpvKtpHfuJzY++ckGPz743UtQsEzsFE
R5BfHxmlaxTyHkh1TkSBidwb88Md+TNkysc9u0Yu6wAATYtuU23I4F47k+fvkZZB
WEOdiRcV6Qc5LgH+1TkYvTIdMFLQQcXqDTwIfCDWTqD9bIscryId56llMnqnHDXb
iQ==
-----END PRIVATE KEY-----

*/
	} else {
		publicKey = "-----BEGIN PUBLIC KEY-----\nMIGbMBAGByqGSM49AgEGBSuBBAAjA4GGAAQAHA7vhNuY6DtpuEaU+APlZwK+KTFG\nhGW7Bgw5sFiGWlV2VlgD7ijZanA1N5s17rdeQElXc66BZlyRhjMNc163F+0AWHCX\n+k7t4aHfjA4VdGgGOQTZDbnk/imfufti+ZsZ3gA+7if3MwpJ2pMzCDx1mxN+k412\nbUbY5Ssg3mu1EEuO+nI=\n-----END PUBLIC KEY-----";
	}
	submitAccountApprovedMessage(MESSAGE_DATATYPE_BYTECOIN, newAccount, publicKey, strlen(publicKey), "", "", 0);
	if (PILOT_VERSION_FLAG) {
		ss = jsVarDstrCreateByPrintf("base_account=%s&base_balance=10000000&new_account_balance=10000&fees=%f&tokenfees=%f&tokencapitalizationfeepercent=%f&new_account_approval=auto", newAccount, BIX_TRANSFER_FEES, BIX_TRANSFER_FEES_OTHER_TOKENS, NEW_TOKEN_CAPITALIZATION_FEE_PERCENTAGE);
	} else {
		ss = jsVarDstrCreateByPrintf("base_account=%s&base_balance=10000000&new_account_balance=0&fees=%f&tokenfees=%f&tokencapitalizationfeepercent=%f&new_account_approval=manual", newAccount, BIX_TRANSFER_FEES, BIX_TRANSFER_FEES_OTHER_TOKENS, NEW_TOKEN_CAPITALIZATION_FEE_PERCENTAGE);
	}
	ooo = createTokenOptionsMsgText_st(MESSAGE_DATATYPE_BYTECOIN, MESSAGE_METATYPE_SIMPLE_TOKEN, ss->s);
	signAccount = THIS_CORE_SERVER_ACCOUNT_NAME_ST();
	signature = signatureCreateBase64(ooo, strlen(ooo), myNodePrivateKey);
	bchainCreateNewMessage(ooo, strlen(ooo), MESSAGE_DATATYPE_BYTECOIN, signAccount, "", "", signature, NULL);
	jsVarDstrFree(&ss);	
	FREE(signature);
}

////////////////////////////////////////////////////////////////////

static void mainCoreRegularBlockProposal(void *d) {
	if (nodeState != NODE_STATE_RESYNC) {
		bchainProposeNewBlock();
	}
	timeLineInsertEvent(UTIME_AFTER_MSEC((long long)(rand()) % (BLOCK_NODE_AVERAGE_PROPOSAL_PERIOD_USEC/1000*coreServersMax) + (BLOCK_NODE_AVERAGE_PROPOSAL_PERIOD_USEC/1000*coreServersMax)/2), mainCoreRegularBlockProposal, NULL);
}

static void mainCoreSwitchToResyncModeBecauseBlockAcceptanceIsStalled(void *d) {
	// This probably happens when the whole blockchain is stalled.
	// It is a "last resort" attempt to get back to working state.
	if (lastAcceptedBlockTime <= currentTime.sec - BLOCK_ACCEPTANCE_TIMEOUT && msgNumberOfPendingMessages > 0) {
		PRINTF("Warning: No block accepted before timeout while having %d pending messages. Switching to resynchronization mode.\n", msgNumberOfPendingMessages);
		// Usually there is so much difference between leading nodes, so make this number small, so that we return to normal
		// state as soon as possible
		nodeResyncUntilSeqNum = node->currentBlockSequenceNumber + 20;
		bchainStartResynchronization(NULL);
	}
}

static void mainCoreRegularServerResynchronizationRequests(void *d) {
	if (nodeState == NODE_STATE_RESYNC) {
		// dont remain stalled in resync mode
		if (lastResynchronizationActionTime < currentTime.sec - BLOCK_RESYNCHRONIZATION_TIMEOUT) {
			PRINTF0("Warning: Resynchronization is stalled. Switching back to normal mode.\n");
			changeNodeStatus(NODE_STATE_NORMAL);
		}
	} else {
		if (lastAcceptedBlockTime < currentTime.sec - BLOCK_ACCEPTANCE_TIMEOUT / 2 
			&& msgNumberOfPendingMessages > 0
			&& consensusIsThereSomeFutureBlockProposal()
			) {
			timeLineInsertUniqEventIfNotYetInserted(
				UTIME_AFTER_SECONDS(BLOCK_ACCEPTANCE_TIMEOUT / 2 + rand() % BLOCK_ACCEPTANCE_TIMEOUT), 
				mainCoreSwitchToResyncModeBecauseBlockAcceptanceIsStalled, 
				NULL
				);
		}
	}
	// I am serving resynchronization requests even if I am in resynchronization myself
	// This is because when blockchain is completely frozen, all nodes may switch to resync mode at the same time
	// but we need them to get to the last accepted block.
	bchainServeResynchronizationRequests();
	timeLineInsertEvent(UTIME_AFTER_USEC(BLOCK_NODE_AVERAGE_PROPOSAL_PERIOD_USEC/5), mainCoreRegularServerResynchronizationRequests, NULL);
}

static void mainCoreRegularReconsiderMyEngagament(void *d) {
	if (nodeState != NODE_STATE_RESYNC) {
		consensusReconsiderMyEngagement();
	}
	bchainWriteGlobalStatus();
	timeLineInsertEvent(UTIME_AFTER_MSEC(50), mainCoreRegularReconsiderMyEngagament, NULL);
}

static void mainCoreSigintHandler(int s) {
	timeLineInsertEvent(UTIME_AFTER_USEC(0), mainCoreShutDown, NULL);
}

int main(int argc, char **argv) {
	int				r;
	struct timeval 	tv;

	// debuglevel = 33;
	// jsVarDebugLevel = 1;

#if ! _WIN32
	signal(SIGPIPE, SIG_IGN);					// ignore SIGPIPE Signals
	signal(SIGCHLD, zombieHandler);				// avoid system of keeping child zombies
#endif
	signal(SIGINT, mainCoreSigintHandler);	
	signal(SIGTERM, mainCoreSigintHandler);	

	mainCoreInitializeData(argc, argv);

	// PRINTF("Random seed == %d\n", currentTime.usecPart);
	srand(currentTime.usecPart);

	bchainOpenAndLoadGlobalStatusFile();

	// check
	assert(consensusRounds[CONSENSUS_ENGAGEMENT_ROUND_MAX-1].roundTimeout == CONSENSUS_ENGAGEMENT_EXPIRATION_INFINITY);

	timeLineInsertEvent(UTIME_AFTER_SECONDS(0), regularFflush, stdout);

	// timeLineInsertEvent(UTIME_AFTER_SECONDS(6), mainCoreRegularTest, NULL);
	timeLineInsertEvent(UTIME_AFTER_SECONDS(1), mainCoreRegularReconsiderMyEngagament, NULL);
	timeLineInsertEvent(UTIME_AFTER_SECONDS(1), coreGuiPendingRequestsRegularUpdateList, NULL);
	// 5 seconds is enough to explore servers topology, i.e. latencies and broadcast paths are calculated yet
	if (node->currentBlockSequenceNumber == 0 && myNode == 0 && ALLOW_GENERATION_OF_GENESIS_BLOCK) {
		timeLineInsertEvent(UTIME_AFTER_SECONDS(6), mainCoreIssueGenesisBlockMessages, NULL);
	}
	// start proposing block after genesis block messages are broadcasted
	timeLineInsertEvent(UTIME_AFTER_SECONDS(7), mainCoreRegularBlockProposal, NULL);
	timeLineInsertEvent(UTIME_AFTER_SECONDS(8), mainCoreRegularReBroadcastStatuses, NULL);
	timeLineInsertEvent(UTIME_AFTER_SECONDS(9), mainCoreRegularServerResynchronizationRequests, NULL);

	mainCoreOpenBchainListeningConnection(NULL);
	mainCoreOpenSecondaryListeningConnection(NULL);
	mainCoreServerNetworkNodesInit();
	coreServerGuiInit();
	msSleep(100);
	mainCoreConnectToOtherNodes();

	for(;;) {
		setCurrentTime();
		timeLineTimeToNextEvent(&tv, 60);
		jsVarPoll2(tv.tv_sec*1000000+tv.tv_usec, NULL, mainCoreAfterSelectActions);
		timeLineExecuteScheduledEvents();
	}

	return(0);
}
