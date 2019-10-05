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

struct globalTimeInfo       currentTime;
struct timeLineEvent        *timeLine = NULL;

struct nodeStatusPersistentData	nodeStatus = {-1, 0, };
struct nodeStatusPersistentData	*node = &nodeStatus;
FILE *nodeFile;

int myNode = -1;
int debuglevel = 10;
char mynodeidstring[TMP_STRING_SIZE];
char *myNodePrivateKey;

int32_t nodeState;
int32_t nodeResyncFromNode;
int64_t nodeResyncUntilSeqNum;

time_t lastResynchronizationActionTime;
time_t lastAcceptedBlockTime;

struct consensusBlockEngagementInfo	*currentProposals;
struct consensusBlockEngagementInfo	*currentEngagement;

struct bchainMessageList *pendingMessagesList;
struct blockContentDataList	*currentProposedBlock;

struct consensusBlockEngagementInfo	*engagementsPerServer[CONSENSUS_MAX_MEMORIZED_SEQUENCE_DIFF][SERVERS_MAX];
struct blockContentDataList 		*receivedBlockContents[CONSENSUS_MAX_MEMORIZED_SEQUENCE_DIFF];
int									nodeEngagementOutOfOrderFlag[SERVERS_MAX];

struct bchainResynchronizationRequestList	*bchainResynchronizationRequest = NULL;

static char staticStringsRing[STATIC_STRING_RING_SIZE][TMP_STRING_SIZE];
static int staticStringsRingIndex = 0;

static char staticLLStringsRing[STATIC_LLSTRING_RING_SIZE][LONG_LONG_TMP_STRING_SIZE];
static int staticLLStringsRingIndex = 0;

// Nodes with smaller id are more probable to forge the block
// That shall be the nodes with the best connectivity, I think
struct serverTopology coreServers[SERVERS_MAX];
char coreServersHash[MAX_HASH64_SIZE];

#if 0
#if EDEBUG || _WIN32
	{0, "192.168.198.142", "node 0", 55555, 55545, 54333, {1, -1}, 0, "server0"},
	{1, "192.168.198.142", "node 1", 55556, 55546, -1,    {-1}, 0, "server1"},
	{2, "192.168.198.142", "node 2", 55557, 55547, -1,    {1, -1}, 0, "server2"},
	{3, "192.168.198.142", "node 3", 55558, 55548, -1,    {2, -1}, 0, "server3"},
	{4, "192.168.198.142", "node 4", 55559, 55549, 54334,    {1, -1}, 0, "server4"},
	{5, "192.168.198.161", "node 5", 55555, 55545, 54331,    {4, -1}, 0, "server5"},
#elif EDEBUG
	{0, "192.168.198.142", "node 0", 55555, 55545, 54333, {1, -1}, 0, "server0"},
	{1, "192.168.198.142", "node 1", 55556, 55546, -1,    {2, 4, -1}, 0, "server1"},
	{2, "192.168.198.142", "node 2", 55557, 55547, -1,    {3, 5, -1}, 0, "server2"},
	{3, "192.168.198.142", "node 3", 55558, 55548, -1,    {4, -1}, 0, "server3"},
	{4, "192.168.198.142", "node 4", 55559, 55549, 54334, {5, -1}, 0, "server4"},
	{5, "192.168.198.142", "node 5", 55560, 55550, -1,    {0, -1}, 0, "server5"},
#else
	{0, "92.48.90.11", "node 0", 55550, 55540, 54530, {1, -1}, 0, "server0"},
	{1, "92.48.90.11", "node 1", 55551, 55541, 54531, {2, -1}, 0, "server1"},
	{2, "92.48.90.11", "node 2", 55552, 55542, 54532, {3, 5, -1}, 0, "server2"},
	{3, "92.48.90.11", "node 3", 55553, 55543, 54533, {4, -1}, 0, "server3"},
	{4, "92.48.90.11", "node 4", 55554, 55544, 54534, {5, -1}, 0, "server4"},
	{5, "92.48.90.11", "node 5", 55555, 55545, 54535, {0, -1}, 0, "server5"},
#endif
	{-1, NULL, NULL, 0, 0, 0, {-1,}, 0},
};
#endif

struct supportedMessageStr supportedMessageStrNull = {NULL, NULL, NULL};

struct supportedMessageStr supportedMessagesAllTypeTemplate[] = {
	{NULL, MESSAGE_METATYPE_META_MSG, NULL},
	{NULL, MESSAGE_METATYPE_EXCHANGE, NULL},
	{NULL, MESSAGE_METATYPE_USER_FILE, NULL},
	{NULL, MESSAGE_METATYPE_SIMPLE_TOKEN, NULL},
	{NULL, MESSAGE_METATYPE_POW_TOKEN, NULL},
	{NULL, NULL, NULL},
};

static struct supportedMessageStr supportedMessagesTabInitialValue[] = {
	{MESSAGE_DATATYPE_EXCHANGE, MESSAGE_METATYPE_EXCHANGE, NULL},
	{MESSAGE_DATATYPE_META_MSG, MESSAGE_METATYPE_META_MSG, NULL},
	{MESSAGE_DATATYPE_USER_FILE, MESSAGE_METATYPE_USER_FILE, NULL},
	// coins starts after METATYPE_METATYPE_LAST_NON_COIN
	// coins starts with bytecoin
	{MESSAGE_DATATYPE_BYTECOIN, MESSAGE_METATYPE_SIMPLE_TOKEN, NULL},
	// user defined coins starts here
	{NULL, NULL, NULL},
};

struct supportedMessageStr *supportedMessagesTab = NULL; // supportedMessagesTabInitialValue;
int supportedMessagesTabMaxi = 0; // DIMENSION(supportedMessagesTabInitialValue)-1;

int coreServersMax = -1;
int coreServersActive = -1;

// info about connection which I have opened
struct connectionsStruct coreServersConnections[SERVERS_MAX] = {{0,}, };

// overall informations about existing connections in the network
struct networkConnectionsStr 	networkConnections[SERVERS_MAX][SERVERS_MAX];
struct routingPathInfo			myNodeRoutingPaths[SERVERS_MAX];
char 							*myNodeBroadcastPath;
// overall information about other network nodes
struct networkNodeStr			networkNodes[SERVERS_MAX];

// accounts
struct bchainContext 		currentBchainContext = {NULL,};

// proof of work pending messages
struct powcoinBlockPendingMessagesListStr *powcoinBlockPendingMessagesList;


///////////////////////////////////////////////////////////////////////////////////
// openssl
EVP_MD_CTX *openSslMdctx = NULL;

// secondary servers specials

int secondaryToCoreServerConnectionMagic;
struct baioMagicList	*guiConnections = NULL;

struct secondaryToCoreServerPendingRequestsStr *secondaryToCoreServerPendingRequestsList = NULL;

struct jsVar 	*guiEdgeStatus;
struct jsVar 	*guiEdgeLatency;
struct jsVar 	*guiMyRoutingPathPath;
struct jsVar 	*guiMyRoutingPathLatency;
struct jsVar 	*guiMyBroadcastPath;

struct jsVar 	*guiNodeStateName;
struct jsVar 	*guiNodeStateStatus;
struct jsVar 	*guiNodeStateSeqn;
struct jsVar 	*guiNodeStateUsedSpace;
struct jsVar 	*guiNodeStateAvailableSpace;

struct jsVar	*guiSupportedMsgTypesVector;
struct jsVar	*guiSupportedTokenTypesVector;
struct jsVar	*guiSupportedTokenTransferFee;
struct jsVar	*guiPendingRequestsVector;
struct jsVar	*guiCoreServersGdpr;

struct jsVar	*guiMinerCurrentlyMinedBlocksVector;
struct jsVar	*guiSupportedTokenDifficultyVector;


char *sideNames[] = {
	"SIDE_NONE",
	"Buy",
	"Sell",
	"SIDE_MAX",
	NULL,
};

char *orderTypeNames[] = {
	"ORDER_TYPE_NONE",
	"Market",
	"Limit",
	"Stop",
	"ORDER_TYPE_MAX",
	NULL,
};


//////////////////////////////////////////////////////////////

static int anotifyTreeComparator(struct anotificationsTreeStr *t1, struct anotificationsTreeStr *t2) {
	int r;
	r = strcmp(t1->account, t2->account);
	if (r != 0) return(r);
	return(0);
}

typedef struct anotificationsTreeStr anotifyTree;
SGLIB_DEFINE_RBTREE_PROTOTYPES(anotifyTree, leftSubtree, rightSubtree, color, anotifyTreeComparator);
SGLIB_DEFINE_RBTREE_FUNCTIONS(anotifyTree, leftSubtree, rightSubtree, color, anotifyTreeComparator);

struct anotificationsTreeStr *anotificationTree;

////////////////////////////////////////////////////////////////


char *staticStringRingGetTemporaryStringPtr_st() {
	char *res;
	res = staticStringsRing[staticStringsRingIndex];
	staticStringsRingIndex  = (staticStringsRingIndex+1) % STATIC_STRING_RING_SIZE;
	return(res);
}

char *staticStringRingGetLongLongTemporaryStringPtr_st() {
	char *res;
	res = staticLLStringsRing[staticLLStringsRingIndex];
	staticLLStringsRingIndex  = (staticLLStringsRingIndex+1) % STATIC_LLSTRING_RING_SIZE;
	return(res);
}

int enumFromName(char **names, char *nn) {
	int		i;
	for(i=0; names[i] != NULL; i++) {
		if (strcmp(names[i], nn) == 0) return(i);
	}
	return(-1);
}

char *strDuplicate(char *s) {
    int     n;
    char    *ss, *res;
	if (s == NULL) return(NULL);
    n = strlen(s);
    ALLOCC(res, n+1, char);
    strcpy(res, s);
    return(res);
}

char *memDuplicate(char *s, int slen) {
    int     n;
    char    *ss, *res;

	if (s == NULL) return(NULL);
    ALLOCC(res, slen+1, char);
    memcpy(res, s, slen);
	res[slen] = 0;
    return(res);
}

int parseInt(char **pp) {
    int     res, r10;
    char    *p;

    p = *pp;
    if (p == NULL) return(0);

    res = 0;
    while (isdigit(*p)) {
        MUL10(r10, res);
        res = r10 + *p++ - '0';
    }
	*pp = p;
    return(res);
}

int64_t parseLongLong(char **pp) {
    int64_t res, r10;
    char    *p;

    p = *pp;
    if (p == NULL) return(0);

    res = 0;
    while (isdigit(*p)) {
        MUL10(r10, res);
        res = r10 + *p++ - '0';
    }
	*pp = p;
    return(res);
}

int parseHexi(char **ss) {
	int 	res;
	char 	*s;

	if (ss == NULL) return(-1);
	s = *ss;
	if (s == NULL) return(-1);
	res = 0;
	while (isxdigit(*s)) {
		res = res * 16 + hexDigitCharToInt(*s);
		s++;
	}
	*ss = s;
	return(res);
}

int64_t parseHexll(char **ss) {
	int64_t	res;
	char 	*s;

	if (ss == NULL) return(-1);
	s = *ss;
	if (s == NULL) return(-1);
	res = 0;
	while (isxdigit(*s)) {
		res = res * 16 + hexDigitCharToInt(*s);
		s++;
	}
	*ss = s;
	return(res);
}

int hexStringToBin(char *dst, int dstsize, char *src) {
	unsigned char	*d, *dmax, *s;
	dmax = dst + dstsize;
	for(d=dst,s=src; d<dmax && s[0] && s[1]; d++, s+=2) {
		*d = hexDigitCharToInt(s[0]) * 16 + hexDigitCharToInt(s[1]);
	}
	return((char*)d - dst);
}

int hexStringFromBin(char *dst, char *src, int srcsize) {
	unsigned char	*d, *smax, *s;
	smax = src + srcsize;
	for(d=dst,s=src; s<smax; s++) {
		*d++ = toupper(intDigitToHexChar((*s >> 4) & 0xf));
		*d++ = toupper(intDigitToHexChar(*s & 0xf));
	}
	*d = 0;
	return((char*)d - dst);
}

#if _WIN32
int gettimeofday(struct timeval * tp, struct timezone * tzp)
{
    // Note: some broken versions only have 8 trailing zero's, the correct epoch has 9 trailing zero's
    // This magic number is the number of 100 nanosecond intervals since January 1, 1601 (UTC)
    // until 00:00:00 January 1, 1970 
    static const uint64_t EPOCH = ((uint64_t) 116444736000000000ULL);

    SYSTEMTIME  system_time;
    FILETIME    file_time;
    uint64_t    time;

    GetSystemTime( &system_time );
    SystemTimeToFileTime( &system_time, &file_time );
    time =  ((uint64_t)file_time.dwLowDateTime )      ;
    time += ((uint64_t)file_time.dwHighDateTime) << 32;

    tp->tv_sec  = (long) ((time - EPOCH) / 10000000L);
    tp->tv_usec = (long) (system_time.wMilliseconds * 1000);
    return 0;
}
#endif

char *currentLocalTime_st() {
    char            *res;
    time_t          t;
    int             u;
    struct tm       *tm, ttm;

    res = staticStringRingGetTemporaryStringPtr_st();

#if 0 && EDEBUG
	{
		// getsnew time and show time with microsecond precission
		// there can be minutes mismatch ... use for debugging only 
		// to detect source of time jitters
		struct timeval  tv;
		gettimeofday(&tv, NULL);
		snprintf(res, TMP_STRING_SIZE, "%4d-%02d-%02d %02d:%02d:%02d.%06d", 
				 1900+currentTime.lcltm.tm_year, currentTime.lcltm.tm_mon+1, currentTime.lcltm.tm_mday, 
				 currentTime.lcltm.tm_hour, currentTime.lcltm.tm_min, (int)(tv.tv_sec%60),
				 (int)tv.tv_usec);
	}
#else
    snprintf(res, TMP_STRING_SIZE, "%4d-%02d-%02d %02d:%02d:%02d.%03d", 
             1900+currentTime.lcltm.tm_year, currentTime.lcltm.tm_mon+1, currentTime.lcltm.tm_mday, 
             currentTime.lcltm.tm_hour, currentTime.lcltm.tm_min, currentTime.lcltm.tm_sec,
             currentTime.msecPart);
#endif

    return(res);
}

char *sprintSecTime_st(long long int utime) {
    static char     *res;
    time_t          t;
    int             u;
    struct tm       *tm, ttm;

    res = staticStringRingGetTemporaryStringPtr_st();
    t = utime / 1000000;
#if _WIN32
    ttm = *localtime(&t);
	tm = &ttm;
#else
    tm = localtime_r(&t, &ttm);
#endif
    snprintf(res, TMP_STRING_SIZE, "%4d-%02d-%02d %02d:%02d:%02d", 
            1900+tm->tm_year, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec
		);
    return(res);
}

char *sprintUsecTime_st(long long int utime) {
    static char     *res;
    time_t          t;
    int             u;
    struct tm       *tm, ttm;

    res = staticStringRingGetTemporaryStringPtr_st();
    t = utime / 1000000;
    u = utime % 1000000;
#if _WIN32
    ttm = *localtime(&t);
	tm = &ttm;
#else
    tm =  localtime_r(&t, &ttm);
#endif
    snprintf(res, TMP_STRING_SIZE, "%4d-%02d-%02d %02d:%02d:%02d.%03d %03d", 
            1900+tm->tm_year, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec,
            u/1000, u%1000);
    return(res);
}

void setCurrentTime() {
    struct timeval  tv;
    int             previousTimeHour, m, s;

    gettimeofday(&tv, NULL);
    // ensure monotonicity of time
    if (currentTime.sec < tv.tv_sec || (currentTime.sec == tv.tv_sec && currentTime.usecPart < tv.tv_usec)) {

        previousTimeHour = currentTime.hour;

        // update current time
        currentTime.sec = tv.tv_sec;
        currentTime.hour = tv.tv_sec / (60*60);
        currentTime.usecPart = tv.tv_usec;
        currentTime.msecPart = tv.tv_usec / 1000;
        currentTime.usec = ((long long int)tv.tv_sec) * 1000000LL + tv.tv_usec;

        // update tm structures
        if (currentTime.hour == previousTimeHour) {
            // we are the same hour as previously, so update only minutes and seconds in tm structures
            s = currentTime.sec % 60;
            m = (currentTime.sec / 60) % 60;
            currentTime.gmttm.tm_sec = currentTime.lcltm.tm_sec = s;
            currentTime.gmttm.tm_min = currentTime.lcltm.tm_min = m;
        } else {
#if _WIN32
			currentTime.gmttm = *gmtime(&currentTime.sec);
            currentTime.lcltm = *localtime(&currentTime.sec);
#else
            gmtime_r(&currentTime.sec, &currentTime.gmttm);
            localtime_r(&currentTime.sec, &currentTime.lcltm);
#endif
        }
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void guiCoreServersVectorUpdate() {
	int 				i,n;
	struct jsVar		*v;

	v = guiCoreServersGdpr;
	//n = jsVarGetDimension(v);
	//for(i=0; i<n; i++) FREE(jsVarGetStringVector(v, i));
	jsVarResizeVector(v, coreServersMax);
	for(i=0; i<coreServersMax; i++) jsVarSetStringVector(v, i, coreServers[i].gdprInfo);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void guiSupportedMsgTypesVectorUpdate() {
	int i;
	if (guiSupportedMsgTypesVector != NULL) {
		jsVarResizeVector(guiSupportedMsgTypesVector, supportedMessagesTabMaxi);
		for(i=0; i<supportedMessagesTabMaxi; i++) {
			jsVarSetStringVector(guiSupportedMsgTypesVector, i, supportedMessagesTab[i].messageType);
		}
	}
}

int coreServerSupportedMessageFindIndex(char *messageType) {
	int 						i;
	struct supportedMessageStr	*sss;

	if (messageType == NULL) return(-1);

	// TODO: Do binary search here
	for(i=0; i<supportedMessagesTabMaxi; i++) {
		sss = &supportedMessagesTab[i];
		if (sss->messageType != NULL && strcmp(sss->messageType, messageType) == 0) {
			return(i);
		}
	}
	return(-1);
}

struct supportedMessageStr *coreServerSupportedMessageFind(char *messageType) {
	int 						i;
	struct supportedMessageStr	*sss;

	i = coreServerSupportedMessageFindIndex(messageType);
	if (i < 0) return(NULL);
	return(&supportedMessagesTab[i]);
}

int coreServerSupportedMessageIsSupported(char *msgtype) {
	return(coreServerSupportedMessageFind(msgtype) != NULL);
}

static void coreServerSupportedMessageTabAssignCopy(struct supportedMessageStr *ss, int i, struct supportedMessageStr *smsg) {
	ss[i] = *smsg;
	ss[i].messageType = strDuplicate(smsg->messageType);
	ss[i].messageMetaType = strDuplicate(smsg->messageMetaType);
	ss[i].masterAccount = strDuplicate(smsg->masterAccount);
	ss[i].opt = strDuplicate(smsg->opt);
}

void coreServerSupportedMessagesTabCreateCopy() {
	int 						i;
	struct supportedMessageStr	*ss;

	ALLOCC(ss, supportedMessagesTabMaxi, struct supportedMessageStr);
	for(i=0; i<supportedMessagesTabMaxi; i++) {
		coreServerSupportedMessageTabAssignCopy(ss, i, &supportedMessagesTab[i]);
	}
	supportedMessagesTab = ss;
}

void coreServerSupportedMessageCreateCopyWithAdditionalMsgType(struct supportedMessageStr *smsg) {
	int 						i,j;
	struct supportedMessageStr	*ss;

	ALLOCC(ss, supportedMessagesTabMaxi+1, struct supportedMessageStr);
	i=0; j=0;
	for(; i<supportedMessagesTabMaxi && strcmp(supportedMessagesTab[i].messageType, smsg->messageType) < 0; i++,j++) {
		coreServerSupportedMessageTabAssignCopy(ss, j, &supportedMessagesTab[i]);
	}
	coreServerSupportedMessageTabAssignCopy(ss, j, smsg);
	j++;
	for(;i<supportedMessagesTabMaxi; i++,j++) {
		coreServerSupportedMessageTabAssignCopy(ss, j, &supportedMessagesTab[i]);
	}
	supportedMessagesTabMaxi ++;
	supportedMessagesTab = ss;
	// TODO: sort the table here
	guiSupportedMsgTypesVectorUpdate();
}

void coreServerSupportedMessageCreateCopyWithAdditionalMsgWithType(char *msgType, char *msgMetaType, char *masterAccount, char *msgopt) {
	struct supportedMessageStr 		*mm, mmm;

	mm = supportedMessagesFindMetaTypeTemplate(msgMetaType);
	if (mm == NULL) {
		PRINTF("coreServerSupportedMessageTableLoad: internal error: unsupported coin/message type %s\n", msgMetaType);
		memset(&mmm, 0, sizeof(mmm));
	} else {
		mmm = *mm;
	}
	// no need to make strDuplicate, strings are allocated in AddmsgType
	mmm.messageType = msgType;
	mmm.messageMetaType = msgMetaType;
	mmm.masterAccount = masterAccount;
	mmm.opt = msgopt;
	coreServerSupportedMessageCreateCopyWithAdditionalMsgType(&mmm);
}

void coreServerSupportedMessageAddAdditionalMsgWithType(char *msgType, char *msgMetaType, char *masterAccount, char *msgopt) {
	struct supportedMessageStr 	*prevt;
	int							previ;

	prevt = supportedMessagesTab;
	previ = supportedMessagesTabMaxi;
	coreServerSupportedMessageCreateCopyWithAdditionalMsgWithType(msgType, msgMetaType, masterAccount, msgopt);
	coreServerSupportedMessageTableFree(prevt, previ);
}

char *messageDataTypeToAccountCoin(char *datatype) {
	struct supportedMessageStr *sss;

	sss = coreServerSupportedMessageFind(datatype);
	if (sss == NULL) return(NULL);
	if (strcmp(sss->messageMetaType, MESSAGE_METATYPE_SIMPLE_TOKEN) == 0) return(datatype);
	if (strcmp(sss->messageMetaType, MESSAGE_METATYPE_POW_TOKEN) == 0) return(datatype);
	return(MESSAGE_DATATYPE_BYTECOIN);
}

void coreServerSupportedMessageInit() {
	struct supportedMessageStr *sss;

	coreServerSupportedMessageTableFree(supportedMessagesTab, supportedMessagesTabMaxi);
	supportedMessagesTab = NULL; // supportedMessagesTabInitialValue;
	supportedMessagesTabMaxi = 0; // DIMENSION(supportedMessagesTabInitialValue)-1;
	coreServerSupportedMessagesTabCreateCopy();
#if 0
	// TODO: Remove this. Put exception into coinOptions acceptance code
	sss = coreServerSupportedMessageFind(MESSAGE_DATATYPE_BYTECOIN);
	if (sss != NULL) {
		// This is a small hack, it allows to set BIX parameters to server 0 in genesis block
		// However this adds the server 0 the right to change BIX options. Maybe a security hole?
		sss->masterAccount = strDuplicate(getAccountName_st(0, 0));
	}
#endif
}	

void coreServerSupportedMessageSetToDefaultValues() {
	struct supportedMessageStr *sss;

	coreServerSupportedMessageTableFree(supportedMessagesTab, supportedMessagesTabMaxi);
	supportedMessagesTab = supportedMessagesTabInitialValue;
	supportedMessagesTabMaxi = DIMENSION(supportedMessagesTabInitialValue)-1;
	coreServerSupportedMessagesTabCreateCopy();
}	

void coreServerSupportedMessageTableFree(struct supportedMessageStr *tt, int ttsize) {
	int i;
	if (tt == NULL) return;
	for(i=0; i<ttsize; i++) {
		FREE(tt[i].messageType);
		FREE(tt[i].messageMetaType);
		FREE(tt[i].masterAccount);
		FREE(tt[i].opt);
	}
	FREE(tt);
}

static char *supportedMsgsFileName_st() {
	char *res;
	res = staticStringRingGetTemporaryStringPtr_st();
	sprintf(res, "%s/%02d/messages.txt", PATH_TO_SUPPORTED_MSGS_DIRECTORY, myNode);
	return(res);
}


void coreServerSupportedMessageTableSave() {
	int 		i;
	char		*fname;
	FILE		*ff;

	fname = supportedMsgsFileName_st();
	ff = fopen(fname, "w");
	if (ff == NULL) {
		mkdirRecursivelyCreateDirectoryForFile(fname);
		ff = fopen(fname, "w");
		if (ff == NULL) {
			PRINTF("error: can't open %s for write\n", fname);
			return;
		}
	}
	fprintf(ff, "[\n");
	for(i=0; i<supportedMessagesTabMaxi; i++) {
		// for(i=DIMENSION(supportedMessagesTabInitialValue) - 1; i<supportedMessagesTabMaxi; i++) {
		assert(supportedMessagesTab[i].messageMetaType != NULL);
		fprintf(ff, "{\"name\": \"%s\", \"type\": \"%s\", \"master\": \"%s\", \"opt\": \"%s\"},\n", 
				supportedMessagesTab[i].messageType, 
				supportedMessagesTab[i].messageMetaType, 
				(supportedMessagesTab[i].masterAccount == NULL ? "" : supportedMessagesTab[i].masterAccount), 
				(supportedMessagesTab[i].opt == NULL ? "" : supportedMessagesTab[i].opt)
			);
	}
	fprintf(ff, "]\n");
	fclose(ff);
}

struct supportedMessageStr *supportedMessagesFindMetaTypeTemplate(char *messageMetaType) {
	int i;

	for(i=0; supportedMessagesAllTypeTemplate[i].messageMetaType != NULL; i++) {
		if (strcmp(supportedMessagesAllTypeTemplate[i].messageMetaType, messageMetaType) == 0) return(&supportedMessagesAllTypeTemplate[i]);
	}
	return(NULL);
}

void coreServerSupportedMessageTableLoad() {
	int 						i, problemFlag;
	char						*fname;
	char						*ss;
	struct jsonnode				*cc, *msgtype, *msgname, *msgopt, *master;
	struct jsonFieldList		*ll;
	struct supportedMessageStr 	mmm, *mm;

	problemFlag = 1;

	coreServerSupportedMessageInit();

	fname = supportedMsgsFileName_st();
	ss = fileLoad(0, fname);
	if (ss == NULL) {
		PRINTF("Warning: can't load file %s. Initializing to default values.\n", fname);
		coreServerSupportedMessageSetToDefaultValues();
		return;
	}
	jsonDestructibleParseString(ss, &cc);
	if (cc == NULL || cc->type != JSON_NODE_TYPE_ARRAY) GOTO_WITH_MSG(finalize, "Error in json file %s\n", fname);
	i = 0;
	for(ll=cc->u.fields; ll!=NULL; ll=ll->next) {
		msgname = jsonFindField(ll->val, "name");
		msgtype = jsonFindField(ll->val, "type");
		master = jsonFindField(ll->val, "master");
		msgopt = jsonFindField(ll->val, "opt");
		if (msgname == NULL || msgtype == NULL || msgopt == NULL || master == NULL || msgname->type != JSON_NODE_TYPE_STRING || msgtype->type != JSON_NODE_TYPE_STRING || msgopt->type != JSON_NODE_TYPE_STRING || master->type != JSON_NODE_TYPE_STRING) {
			PRINTF("error: problem in json file %s, index %d\n", fname, i);
		} else {
			coreServerSupportedMessageCreateCopyWithAdditionalMsgWithType(msgname->u.s, msgtype->u.s, master->u.s, msgopt->u.s);
		}
		i ++;
	}

finalize:
	FREE(ss);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// timeline functions

static int timeLineEventComparator(struct timeLineEvent *e1, struct timeLineEvent *e2) {
    if (e1->usecond < e2->usecond) return(-1);
    if (e1->usecond > e2->usecond) return(1);
    if (((char *)e1->event) < ((char *)e2->event)) return(-1);
    if (((char *)e1->event) > ((char *)e2->event)) return(1);
    if (((char *)e1->arg) < ((char *)e2->arg)) return(-1);
    if (((char *)e1->arg) > ((char *)e2->arg)) return(1);
    return(0);
}

static int timeFindEventNoMatterTimeComparator(struct timeLineEvent *e1, struct timeLineEvent *e2) {
    if (((char *)e1->event) != ((char *)e2->event)) return(-1);
    if (((char *)e1->arg) != ((char *)e2->arg)) return(-1);
    return(0);
}

static int timeFindEventNoMatterTimeAndArgComparator(struct timeLineEvent *e1, struct timeLineEvent *e2) {
    if (((char *)e1->event) != ((char *)e2->event)) return(-1);
    return(0);
}

// TODO: Make timeline a double linked list, insert will return a pointer and delete will delete in constant time
void timeLineInsertEvent(long long int usec, void (*event)(void *arg), void *arg) {
    struct timeLineEvent *tt;

	// You shall never insert an event at current time (with UTIME_AFTER_SECONDS(0) or UTIME_AFTER_MILLIS(0)) !!!
	// Because if this is entered from a timeline event, it is executed immediately which is probably not what you
	// want, otherwise you can call it directly.
	if (usec <= currentTime.usec) usec = currentTime.usec + 1;

    ALLOC(tt, struct timeLineEvent);
    tt->usecond = usec;
    tt->event = event;
    tt->arg = arg;
    tt->next = NULL;
    SGLIB_SORTED_LIST_ADD(struct timeLineEvent, timeLine, tt, timeLineEventComparator, next);
    // printf("timeLineInsertEvent for %s\n", sprintUsecTime_st(usec));
}

struct timeLineEvent *timeLineFindEventAtUnknownTime(void (*event)(void *arg), void *arg) {
    struct timeLineEvent ttt, *memb;
    ttt.event = event;
    ttt.arg = arg;
    SGLIB_SORTED_LIST_FIND_MEMBER(struct timeLineEvent, timeLine, &ttt, timeFindEventNoMatterTimeComparator, next, memb);
	return(memb);
}

void timeLineRemoveEvent(long long int usec, void (*event)(void *arg), void *arg) {
    struct timeLineEvent ttt, *memb;
    ttt.usecond = usec;
    ttt.event = event;
    ttt.arg = arg;
    SGLIB_SORTED_LIST_DELETE_IF_MEMBER(struct timeLineEvent, timeLine, &ttt, timeLineEventComparator, next, memb);
    if (memb != NULL) {
        FREE(memb);
    }
}

int timeLineRemoveEventAtUnknownTime(void (*event)(void *arg), void *arg) {
    struct timeLineEvent ttt, *memb;
    ttt.event = event;
    ttt.arg = arg;
    SGLIB_SORTED_LIST_DELETE_IF_MEMBER(struct timeLineEvent, timeLine, &ttt, timeFindEventNoMatterTimeComparator, next, memb);
    if (memb != NULL) {
        FREE(memb);
		return(0);
    }
	return(-1);
}

int timeLineRemoveEventAtUnknownTimeAndArg(void (*event)(void *arg)) {
    struct timeLineEvent ttt, *memb;
    ttt.event = event;
    SGLIB_SORTED_LIST_DELETE_IF_MEMBER(struct timeLineEvent, timeLine, &ttt, timeFindEventNoMatterTimeAndArgComparator, next, memb);
    if (memb != NULL) {
        FREE(memb);
		return(0);
    }
	return(-1);
}

int timeLineRescheduleUniqueEvent(long long int usec, void (*event)(void *arg), void *arg) {
	int res;
	res = timeLineRemoveEventAtUnknownTime(event, arg);
	timeLineInsertEvent(usec, event, arg);
	return(res);
}

int timeLineInsertUniqEventIfNotYetInserted(long long int usec, void (*event)(void *arg), void *arg) {
    struct timeLineEvent *memb;
	memb = timeLineFindEventAtUnknownTime(event, arg);
	if (memb == NULL) {
		timeLineInsertEvent(usec, event, arg);
		return(0);
	} else {
		return(1);
	}
}

void timeLineTimeToNextEvent(struct timeval *tv, int maxseconds) {
    long long int diff;

    if (timeLine == NULL) goto timeLineReturnMax;

    diff = timeLine->usecond - currentTime.usec;
    if (diff <= 0) goto timeLineReturnZero;
    tv->tv_sec =  diff / 1000000LL;
    tv->tv_usec = diff % 1000000LL;
    if (tv->tv_sec >= maxseconds) goto timeLineReturnMax;
    goto timeLineExitPoint;

timeLineReturnMax:
    tv->tv_sec = maxseconds;
    tv->tv_usec = 0;
    goto timeLineExitPoint;

timeLineReturnZero:
    tv->tv_sec = 0;
    tv->tv_usec = 0;
    goto timeLineExitPoint;

timeLineExitPoint:;
    // printf("time to next event %lld - %lld == %d %d\n", (long long)currentTime.usec, (long long)timeLine->usecond, tv->tv_sec, tv->tv_usec);
    return;
}

void timeLineExecuteScheduledEvents() {
    struct timeLineEvent *tt;

#if 0

    // timeLine->event can insert another event, so I can not
    // execute and free events in one loop
    for (tt=timeLine; tt != NULL && tt->usecond <= currentTime.usec; tt=tt->next) {
        if (tt->event!=NULL) {
            tt->event(tt->arg);
        }
        tt->event = NULL;
    }

    while (timeLine != NULL && timeLine->event == NULL) {
        tt = timeLine;
        timeLine = timeLine->next;
        FREE(tt);
    }

#else

	while (timeLine != NULL && timeLine->usecond <= currentTime.usec) {
		tt = timeLine;
		timeLine = tt->next;
        if (tt->event != NULL) {
            tt->event(tt->arg);
        }		
        FREE(tt);
	}

#endif
}

void timeLineDump() {
    struct timeLineEvent *ll;
    PRINTF0("TIMELINE DUMP: "); fflush(stdout);
    for(ll=timeLine; ll!=NULL; ll=ll->next) {
        printf("%p:%s:%p(%p) ", ll, sprintUsecTime_st(ll->usecond), ll->event, ll->arg);fflush(stdout);
    }
    PRINTF0(" END.\n"); fflush(stdout);
}



///////////////////////////////////////////////////////////////////////////////////////////////////

char *bchainPrintPrefix_st() {
	char *res;
	res = staticStringRingGetTemporaryStringPtr_st();
	sprintf(res, "%s: %s", mynodeidstring, currentLocalTime_st());
	return(res);
}

void mkdirRecursivelyCreateDirectoryForFile(const char *fname) {
	char 	tmp[TMP_STRING_SIZE];
	int		i;

	strncpy(tmp, fname, sizeof(tmp));
	tmp[sizeof(tmp)-1] = 0;

	snprintf(tmp, sizeof(tmp),"%s",fname);
	for(i=0; tmp[i]!=0; i++) {
		if (tmp[i] == '/') {
			tmp[i] = 0;
			mkdir(tmp, 0755);
			tmp[i] = '/';
		}
	}
}

void mkdirRecursivelyCreateDirectory(const char *dirname) {
	char 	tmp[TMP_STRING_SIZE];
	int		i;

	mkdirRecursivelyCreateDirectoryForFile(dirname);
	mkdir(dirname, 0755);
}


void regularFflush(void *f) {
	fflush(f);
#if EDEBUG
	timeLineInsertEvent(UTIME_AFTER_MSEC(100), regularFflush, stdout);
#else
	timeLineInsertEvent(UTIME_AFTER_MSEC(5000), regularFflush, stdout);
#endif
}

void bchainSetBufferSizes(struct baio *bb) {
	long long	s;

	bb->maxReadBufferSize = (1<<16) + BCHAIN_MAX_BLOCK_SIZE*2;
	s = (long long)(1<<16) + BCHAIN_MAX_BLOCK_SIZE*coreServersMax*2;
	if (s < (1<<16)) s = (1 << 16);
	if (s > ((1LL<<31) - 1)) s = (1LL<<31) - 1;
	bb->maxWriteBufferSize = s;
	bb->maxReadBufferSize = s;
}

void openSslCryptoInit() {
	
	if (openSslMdctx == NULL) {
		OpenSSL_add_all_algorithms();
		ERR_load_BIO_strings();
		ERR_load_crypto_strings();
		// initialize random generator
		RAND_poll();
	} else {
		EVP_MD_CTX_destroy(openSslMdctx);
	}
	openSslMdctx = EVP_MD_CTX_create();
}

char *fileLoad(int useCppFlag, char *fmt, ...) {
	va_list 					arg_ptr;
	struct jsVarDynamicString 	*ss;
	char						*res;

	va_start(arg_ptr, fmt);
	ss = jsVarDstrCreateByVFileLoad(useCppFlag, fmt, arg_ptr);
	va_end(arg_ptr);
	if (ss == NULL) return(NULL);
	res = jsVarDstrGetStringAndReinit(ss);
	jsVarDstrFree(&ss);
	return(res);
}

static int vfileSaveWithLength(char *content, int len, int createDirectoryFlag, char *fmt, va_list arg_ptr) {
	FILE 		*ff;
	int			r;
	char		fname[TMP_FILE_NAME_SIZE];

	if (content == NULL) return(-1);

	r = vsnprintf(fname, sizeof(fname), fmt, arg_ptr);
	if (r < 0 || r >= TMP_FILE_NAME_SIZE) {
		strcpy(fname+TMP_FILE_NAME_SIZE-4, "...");
		PRINTF("Error: File name too long %s\n", fname);
		return(-1);
	}

	ff = fopen(fname, "w");
	if (ff == NULL && createDirectoryFlag) {
		mkdirRecursivelyCreateDirectoryForFile(fname);
		ff = fopen(fname, "w");
	}
	if (ff == NULL) return(-1);
	r = fwrite(content, 1, len, ff);
	fclose(ff);
	return(r);
}

int fileSaveWithLength(char *content, int len, int createDirectoryFlag, char *fmt, ...) {
	int			res;
	va_list 	arg_ptr;

	va_start(arg_ptr, fmt);
	res = vfileSaveWithLength(content, len, createDirectoryFlag, fmt, arg_ptr);
	va_end(arg_ptr);
	return(res);
}

int fileSave(char *content, char *fmt, ...) {
	int			res;
	va_list 	arg_ptr;

	va_start(arg_ptr, fmt);
	res = vfileSaveWithLength(content, strlen(content), 0, fmt, arg_ptr);
	va_end(arg_ptr);
	return(res);
}

#ifdef __WIN32__
int mapPatternFiles( 
					char *pattern,
					void (*fun)(char *fname, void *arg),
					void *arg
	) {
  WIN32_FIND_DATA		fdata;
  HANDLE				han;
  int res;
  res = 0;
  han = FindFirstFile(pattern, &fdata);
  if (han != INVALID_HANDLE_VALUE) {
	do {
	  if (	strcmp(fdata.cFileName,".")!=0
			&&	strcmp(fdata.cFileName,"..")!=0) {
		fun(fdata.cFileName, arg);
		res = 1;
	  }
	} while (FindNextFile(han, &fdata));
	FindClose(han);
  }
  return(res);
}
#endif

int mapDirectoryFiles(
		char *dirname,
		void (*fun)(char *fname, void *arg),
		void *arg
	){
	int res=0;
#ifdef __WIN32__
	WIN32_FIND_DATA		fdata;
	HANDLE				han;
	char				*s,*d;
	char				ttt[TMP_STRING_SIZE];

	for (s=dirname,d=ttt; *s; s++,d++) {
		if (*s=='/') *d='\\';
		else *d = *s;
	}
	InternalCheck(d-ttt < TMP_STRING_SIZE-3);
	sprintf(d, "\\*");
	res = mapPatternFiles(ttt, fun, arg);
#else
	struct stat		stt;
	DIR				*fd;
	struct dirent	*dirbuf;

	if (	stat(dirname, &stt) == 0 
			&& (stt.st_mode & S_IFMT) == S_IFDIR
			&& (fd = opendir(dirname)) != NULL) {
		while ((dirbuf=readdir(fd)) != NULL) {
			if (	
#if (! defined(__QNX__)) && (! defined(__CYGWIN__))
					dirbuf->d_ino != 0 &&
#endif
					strcmp(dirbuf->d_name, ".") != 0 
					&& strcmp(dirbuf->d_name, "..") != 0) {
				fun(dirbuf->d_name, arg);
				res = 1;
			}
		}
		closedir(fd);
	}
#endif
	return(res);
}
/////////////////////////////////////////////////////////////////////

void zombieHandler(int s) {
#if !_WIN32
	// Experts say that it is better to put waitpit into a loop, so doing so.
	while (waitpid((pid_t)(-1), 0, WNOHANG) > 0) ;
#endif
}

void msSleep(int millis) {
#if _WIN32
    Sleep(millis);
#else
    usleep(millis * 1000);   // usleep takes sleep time in us (1 millionth of a second)
#endif
}

/////////////////////////////////////////////////////////////////////

struct sendingPathBreadthFirstQueueElem {
	int 									node;
	int64_t									latency;
	char									path[LONG_LONG_TMP_STRING_SIZE];
	struct sendingPathBreadthFirstQueueElem	*next;
};

static void sendingPathsPrint() {
	int							j;

	PRINTF("Routing paths for %d: ", myNode);
	for(j=0; j<coreServersMax; j++) {
		PRINTF_NOP("%s(%"PRId64")  ", myNodeRoutingPaths[j].path, myNodeRoutingPaths[j].latency);
	}
	PRINTF_NOP0("\n");
}

static struct sendingPathBreadthFirstQueueElem *createDepthFirstSearchQueueElem(int node, int64_t latency, char *path) {
	struct sendingPathBreadthFirstQueueElem *nn;
	ALLOC(nn, struct sendingPathBreadthFirstQueueElem);
	nn->node = node;
	nn->latency = latency;
	strncpy(nn->path, path, sizeof(nn->path));
	nn->next = NULL;
	return(nn);
}

void recalculateMyRoutingPaths(void *d) {
	int											r, i,j,n;
	struct serverTopology						*cc;
	struct sendingPathBreadthFirstQueueElem 	*ff, *ffnext, *nn, **ee;
	int64_t										lat;
	char										ttt[LONG_LONG_TMP_STRING_SIZE];

	for(j=0; j<SERVERS_MAX; j++) {
		myNodeRoutingPaths[j].latency = INT64_MAX;
		FREE(myNodeRoutingPaths[j].path);
		myNodeRoutingPaths[j].path = NULL;
	}

	myNodeRoutingPaths[myNode].latency = 0;
	myNodeRoutingPaths[myNode].path = strDuplicate("");

	nn = createDepthFirstSearchQueueElem(myNode, 0, "");
	ff = nn;
	ee = &ff->next;

	while (ff != NULL) {
		i = ff->node;
		for(j=0; j<coreServersMax; j++) {
			if (networkConnections[i][j].status == NCST_OPEN
				|| networkConnections[j][i].status == NCST_OPEN
				) {
				if (networkConnections[i][j].status == NCST_OPEN) {
					lat = ff->latency + networkConnections[i][j].latency;
				} else {
					lat = ff->latency + networkConnections[j][i].latency;
				}
				if (ff->path[0] == 0) {
					r = snprintf(ttt, LONG_LONG_TMP_STRING_SIZE, "%d", j);
				} else {
					r = snprintf(ttt, LONG_LONG_TMP_STRING_SIZE, "%s:%d", ff->path, j);	
					// add 1 millisecond "charge" for loading another node
					lat += 1000;
				}
				if (lat < myNodeRoutingPaths[j].latency) {
					if (r >= 0 && r < LONG_LONG_TMP_STRING_SIZE) {
						myNodeRoutingPaths[j].latency = lat;
						FREE(myNodeRoutingPaths[j].path);
						myNodeRoutingPaths[j].path = strDuplicate(ttt);
						jsVarSetStringArray(guiMyRoutingPathPath, j, myNodeRoutingPaths[j].path);
						jsVarSetIntArray(guiMyRoutingPathLatency, j, myNodeRoutingPaths[j].latency);
						nn = createDepthFirstSearchQueueElem(j, lat, myNodeRoutingPaths[j].path);
						*ee = nn;
						ee = &nn->next;
					}
				}
			}
		}
		ffnext = ff->next;
		FREE(ff);
		ff = ffnext;
	}

	// sendingPathsPrint();
}

struct bcastTree {
	struct bcastTree *sub[SERVERS_MAX];
};

static void broadcastTreeAdd(struct bcastTree *t, char *path) {
	int					n;
	char				*p;

	if (path == NULL || *path == 0) return;
	p = path; 
	while (*p) {
		n = parseInt(&p);
		if (*p != ':' && *p != 0) return;
		if (*p == ':') p++;
		if (t->sub[n] == NULL) {
			ALLOC(t->sub[n], struct bcastTree);
			memset(t->sub[n], 0, sizeof(struct bcastTree));
		}
		t = t->sub[n];
	}
}

static void broadcastTreePrint(struct bcastTree *t, struct jsVarDynamicString *ss) {
	int 			i, pflag;
	pflag = 0;
	for(i=0; i<SERVERS_MAX; i++) {
		if (t->sub[i] != NULL) {
			if (pflag == 0) jsVarDstrAppendPrintf(ss, ":{");
			else jsVarDstrAppendPrintf(ss, "+");
			jsVarDstrAppendPrintf(ss, "%d", i);
			broadcastTreePrint(t->sub[i], ss);
			pflag = 1;
		}

	}
	if (pflag != 0) jsVarDstrAppendPrintf(ss, "+}");
}

static void broadcastTreeFree(struct bcastTree *t) {
	int i;
	for(i=0; i<SERVERS_MAX; i++) {
		if (t->sub[i] != NULL) broadcastTreeFree(t->sub[i]);
	}
	FREE(t);
}

static void recalculateMyBroadcastPath(void *d) {
	struct bcastTree			*t;
	int							i,j;
	struct jsVarDynamicString	*ss;

	ALLOC(t, struct bcastTree);
	memset(t, 0, sizeof(*t));
	for(j=0; j<coreServersMax; j++) {
		if (myNodeRoutingPaths[j].latency != INT64_MAX) {
			broadcastTreeAdd(t, myNodeRoutingPaths[j].path);
		}
	}

	ss = jsVarDstrCreateByPrintf("");
	broadcastTreePrint(t, ss);
	FREE(myNodeBroadcastPath);
	myNodeBroadcastPath = jsVarDstrGetStringAndReinit(ss);
	jsVarSetString(guiMyBroadcastPath, myNodeBroadcastPath);
	// PRINTF("my broadcast path == %s\n", myNodeBroadcastPath);
	jsVarDstrFree(&ss);
	broadcastTreeFree(t);
}

#define CANT_LOAD_SERVER_TOPOLOGY_FATAL() {								\
		PRINTF0("Error: Can't load server topology. Fatal. Exiting.\n"); \
		exit(1);														\
	}

static void loadServerTopology() {
	char					*ss;
	struct jsonnode 		*jj, *tt;
	struct jsonFieldList	*ee, *aa, *cc;
	int						i, ii;
	struct serverTopology	s;
	char					shahash[SHA512_DIGEST_LENGTH];

	// PRINTF("Loading server topology from %s/%s\n", PATH_TO_DATA_DIRECTORY, PATH_TO_SERVER_TOPOLOGY_FILE);
	memset(&coreServers, 0, sizeof(coreServers));

	// we do not have mynodeidstring at this time
	ss = fileLoad(1, "%s/%s", PATH_TO_DATA_DIRECTORY, PATH_TO_SERVER_TOPOLOGY_FILE);
	jsonDestructibleParseString(ss, &jj);

	if (jj->type != JSON_NODE_TYPE_ARRAY) {CANT_LOAD_SERVER_TOPOLOGY_FATAL();}

	for(i=0,ee=jj->u.fields; ee!=NULL; i++,ee=ee->next) {
		assert(i == ee->u.index);
		if (i >= SERVERS_MAX-1) {CANT_LOAD_SERVER_TOPOLOGY_FATAL();}
		memset(&s, 0, sizeof(s));
		if (ee->val == NULL || ee->val->type != JSON_NODE_TYPE_ARRAY) {CANT_LOAD_SERVER_TOPOLOGY_FATAL();}
		aa = ee->val->u.fields;
		s.index = jsonNextFieldDouble(&aa, -1);
		STRN_CPY_TMP(s.name, jsonNextFieldString(&aa, ""));
		STRN_CPY_TMP(s.guiname, jsonNextFieldString(&aa, ""));
		STRN_CPY_TMP(s.host, jsonNextFieldString(&aa, ""));
		s.bchainCoreServersListeningPort = jsonNextFieldDouble(&aa, -1);
		s.secondaryServersListeningPort = jsonNextFieldDouble(&aa, -1);
		s.guiListeningPort = jsonNextFieldDouble(&aa, -1);
		tt = aa->val; aa = aa->next;
		if (tt == NULL || tt->type != JSON_NODE_TYPE_ARRAY) {CANT_LOAD_SERVER_TOPOLOGY_FATAL();}
		for(ii=0,cc=tt->u.fields; cc!=NULL; ii++,cc=cc->next) {
			assert(ii == cc->u.index);
			if (ii >= ACTIVE_CONNECTIONS_MAX-1) {CANT_LOAD_SERVER_TOPOLOGY_FATAL();}
			if (cc->val->type != JSON_NODE_TYPE_NUMBER) {CANT_LOAD_SERVER_TOPOLOGY_FATAL();}
			s.connections[ii] = cc->val->u.n;
		}
		s.connections[ii] = -1;
		s.connectionsMax = ii;
		STRN_CPY_TMP(s.loginPasswordKey, jsonNextFieldString(&aa, ""));
		STRN_CPY_LONG_TMP(s.gdprInfo, jsonNextFieldString(&aa, ""));
		if (s.index < 0 || s.name[0] == 0 || s.host[0] == 0) {CANT_LOAD_SERVER_TOPOLOGY_FATAL();}
		coreServers[i] = s;
	}

	memset(&s, 0, sizeof(s));
	s.index = -1;
	s.connections[0] = -1;
	coreServers[i] = s;
	coreServersMax = i;

	// calculate the hash
	SHA512((unsigned char *)&coreServers, sizeof(coreServers), shahash);
	base64_encode(shahash, sizeof(shahash), coreServersHash, MAX_HASH64_SIZE);

	// can not free the string, its strings are used without copy
	// FREE(ss);
}

void mainInitialization() {
	// did you add something to "enum bookedOrderSide" and forget to update "sideNames" ?
	assert(strcmp(sideNames[SIDE_MAX], "SIDE_MAX") == 0);
	// did you add something to "enum bookedOrderType" and forget to update "orderTypeNames" ?
	assert(strcmp(orderTypeNames[ORDER_TYPE_MAX], "ORDER_TYPE_MAX") == 0);
}

void mainCoreServersTopologyInit() {
	int 					i, j, r;
	struct serverTopology 	*cc;
	struct sockaddr         saddr;
    int                     saddrlen;

	loadServerTopology();
	// later this shall contain the number of servers present (running) on the network
	coreServersActive = coreServersMax;
	for(i=0; i<coreServersMax; i++) {
		cc = &coreServers[i];
		for(j=0; cc->connections[j] >= 0 && j<ACTIVE_CONNECTIONS_MAX; j++) ;
		InternalCheck(j<ACTIVE_CONNECTIONS_MAX);
		cc->connectionsMax = j;
		// resolve server to IP address for further checking
		saddrlen = sizeof(saddr);
		r = baioGetSockAddr(coreServers[i].host, 0, &saddr, &saddrlen);
		if (r == 0) {
			memcpy(&cc->ipaddress, &((struct sockaddr_in *)&saddr)->sin_addr.s_addr, sizeof(cc->ipaddress));
			// printf("resolved %s to %s\n", coreServers[i].name, sprintfIpAddress_st(cc->ipaddress));
		} else {
			cc->ipaddress = 0;
			PRINTF("Error: Can't resolve host %s to IP address.\n", coreServers[i].host);
		}
	}
}

int mainServerOnWwwGetRequest(struct jsVaraio *jj, char *uri) {
    struct baio             *bb;
    struct wsaio            *ww;

    ww = &jj->w;
    bb = &ww->b;

    // printf("%s: Server: GET %s\n", JSVAR_PRINT_PREFIX(), uri);

	if (strcmp(uri, "/jquery.js") == 0) {
		baioWriteToBuffer(bb, jquery_js, jquery_js_len);
		wsaioHttpFinishAnswer(ww, "200 OK", "text/javascript", NULL);
		return(1);
	}
    return(0);
}

int mainServerOnWwwPostRequest(struct jsVaraio *jj, char *uri) {
    struct baio             	*bb;
    struct baio             	*cc;
    struct wsaio            	*ww;
	char						*name, *email;
	struct jsVarDynamicString	*ss;

    ww = &jj->w;
    bb = &ww->b;

    // printf("%s: Server: POST %s: query: %s\n", JSVAR_PRINT_PREFIX(), uri, jj->w.currentRequest.postQuery);

	if (strcmp(uri, "/mailfeedback.sh") == 0) {
		name = jsVarGetEnv_st(jj->w.currentRequest.postQuery, "name");
		email = jsVarGetEnv_st(jj->w.currentRequest.postQuery, "email");
		ss = jsVarGetEnvDstr(jj->w.currentRequest.postQuery, "message");
		// printf("Message Received from %s <%s> size: %d\r\n\r\n%.*s\n", name, email, ss->size, ss->size, ss->s);
		cc = baioNewPipedCommand("./mailfeedback.sh", BAIO_IO_DIRECTION_WRITE, 0);
		// cc = baioNewPipedCommand("/usr/sbin/ssmtp vittek@chello.sk", BAIO_IO_DIRECTION_WRITE, 0);
		baioPrintfMsg(cc, "Subject: BIX feedback\r\n\r\n");
		baioPrintfMsg(cc, "Message Received from %s <%s>\r\n\r\n%.*s\n", name, email, ss->size, ss->s);
		baioClose(cc);
		jsVarDstrFree(&ss);
        // wsaioHttpFinishAnswer(ww, "404 Not Found", "text/html", NULL);
		wsaioHttpFinishAnswer(ww, "200 OK", "text/html", NULL);
	} else {
        wsaioHttpFinishAnswer(ww, "404 Not Found", "text/html", NULL);
	}
    return(1);
}

void networkConnectionOnStatusChanged(int fromNode, int toNode) {
	int				i, j, status;
	int64_t			latency;

	if (fromNode >= 0 && fromNode < coreServersMax && toNode >= 0 && toNode < coreServersMax) {
		status = networkConnections[fromNode][toNode].status;
		latency = networkConnections[fromNode][toNode].latency;
		i = networkConnections[fromNode][toNode].guiEdgeIndex;
		if (guiEdgeStatus != NULL) {
			jsVarSetIntVector(guiEdgeStatus, i, status);
			jsVarSetIntVector(guiEdgeLatency, i, latency);
		}
		recalculateMyRoutingPaths(NULL);
		for(j=0; j<coreServersMax; j++) {
			if (myNodeRoutingPaths[j].latency == INT64_MAX) {
				networkNodes[j].status = NODE_STATE_UNREACHABLE;
				guiUpdateNodeStatus(j);
			} else if (networkNodes[j].status == NODE_STATE_UNREACHABLE) {
				networkNodes[j].status = NODE_STATE_UNKNOWN;
				guiUpdateNodeStatus(j);
			}
		}
		recalculateMyBroadcastPath(NULL);
	}
}

void networkConnectionStatusChange(int fromNode, int toNode, int newStatus) {
	struct jsVar	*v;

	if (fromNode >= 0 && fromNode < coreServersMax && toNode >= 0 && toNode < coreServersMax) {
		// PRINTF("reseting connection status %d %d -> %d\n", fromNode, toNode, newStatus);
		// do not check here if the previous statu was the same
		// sometimes we want that status is confirnmed in repeated messages
		networkConnections[fromNode][toNode].status = newStatus;
		networkConnectionOnStatusChanged(fromNode, toNode);
		bixBroadcastNetworkConnectionStatusMessage(fromNode, toNode);
	}
}

/////////////////////////////////////////////////////////////////////

void guiConnectionsInsertBaioMagic(int baioMagic) {
	struct baioMagicList *nn;

	ALLOC(nn, struct baioMagicList);
	nn->baioMagic = baioMagic;
    nn->next = guiConnections;
	guiConnections = nn;
}

void guiConnectionsRemoveDisconnected() {
	struct baioMagicList 	**nnn, *nn;
	struct baio				*bb;
	nnn= &guiConnections; 
	while (*nnn!=NULL) {
		bb = baioFromMagic((*nnn)->baioMagic);
		if (bb == NULL) {
			nn = *nnn;
			*nnn = (*nnn)->next;
			FREE(nn);
		} else {
			nnn = &(*nnn)->next;
		}
	}
}

int guiprintf(char *fmt, ...) {
	va_list 					arg_ptr;
	struct baioMagicList 		*nn;
	struct jsVarDynamicString	*ss;
	struct jsVaraio				*jj;
	int							res;

	va_start(arg_ptr, fmt);
	ss = jsVarDstrCreateByVPrintf(fmt, arg_ptr);
	res = ss->size;
	jsVarDstrReplace(ss, "'", "\\'", 1);
	jsVarDstrReplace(ss, "\n", "\\n", 1);

	guiConnectionsRemoveDisconnected();
	for(nn=guiConnections; nn!=NULL; nn=nn->next) {
		jj = (struct jsVaraio *) baioFromMagic(nn->baioMagic);
		if (jj != NULL) {
			jsVarSendEval(jj, "msgWrite('%s');", ss->s);
		}
	}

	jsVarDstrFree(&ss);
	va_end(arg_ptr);
	return(res);
}

int guisendEval(char *fmt, ...) {
	va_list 					arg_ptr;
	struct baioMagicList 		*nn;
	struct jsVarDynamicString	*ss;
	struct jsVaraio				*jj;
	int							res;

	va_start(arg_ptr, fmt);
	ss = jsVarDstrCreateByVPrintf(fmt, arg_ptr);
	res = ss->size;

	guiConnectionsRemoveDisconnected();
	for(nn=guiConnections; nn!=NULL; nn=nn->next) {
		jj = (struct jsVaraio *) baioFromMagic(nn->baioMagic);
		if (jj != NULL) {
			jsVarSendEval(jj, "%s", ss->s);
		}
	}

	jsVarDstrFree(&ss);
	va_end(arg_ptr);
	return(res);
}

void guiUpdateNodeStatus(int node) {
	int i;

	jsVarSetIntArray(guiNodeStateStatus, node, networkNodes[node].status);
	jsVarSetLlongArray(guiNodeStateUsedSpace, node, networkNodes[node].usedSpace);
	jsVarSetLlongArray(guiNodeStateAvailableSpace, node, networkNodes[node].totalAvailableSpace);
	jsVarSetLlongArray(guiNodeStateSeqn, node, networkNodes[node].blockSeqn);
}

void changeNodeStatus(int newStatus) {
	if (myNode < 0) {
		// this is normal for bchain explorer.
		PRINTF("Warning: can't change node status of %d.\n", myNode);
		return;
	}
	nodeState = networkNodes[myNode].status = newStatus;
	guiUpdateNodeStatus(myNode);
	bixBroadcastMyNodeStatusMessage(myNode);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////

static int secondaryServerConnectCallback(struct baio *bb) {
	struct bcaio			*cc;

	cc = (struct bcaio *) bb;
	if (debuglevel > 0) {PRINTF("Connected to node %d\n", cc->toNode); fflush(stdout);}
	secondaryToCoreServerConnectionMagic = cc->b.baioMagic;
	bixSendSecondaryServerLoginToConnection(cc);
	return(0);
}

static int secondaryServerNextCoreNodeToConnect(int n) {
	return((n + 1) % coreServersMax);
}

static int secondaryServerDeleteCallback(struct baio *bb) {
	struct bcaio			*cc;
	int						nextNode;

	cc = (struct bcaio *) bb;
	if (debuglevel > 0) {PRINTF("Connection to node %d closed\n", cc->toNode); fflush(stdout);}
	nextNode = secondaryServerNextCoreNodeToConnect(cc->toNode);

#if EDEBUG
	timeLineInsertEvent(UTIME_AFTER_SECONDS(3), secondaryServerConnectToCoreNode, (void*)nextNode);
#else
	timeLineInsertEvent(UTIME_AFTER_SECONDS(10), secondaryServerConnectToCoreNode, (void*)nextNode);
#endif
	return(0);
}

static int secondaryServerReadCallback(struct baio *bb, int fromj, int n) {
	struct bcaio			*cc;

	cc = (struct bcaio *) bb;
	// PRINTF("Got %d bytes. Nodes %d -> %d.\n", n, cc->fromNode, cc->toNode);
	// PRINTF("MSG: %.*s\n", n, bb->readBuffer.b + bb->readBuffer.i); 
	// fflush(stdout);
	bixParseAndProcessAvailableMessages(cc);
	return(1);
}

static int secondaryServerEofCallback(struct baio *bb) {
	// If EOF was read, process read buffer and dalete the connection
	secondaryServerReadCallback(bb, bb->readBuffer.j, 0);
	baioClose(bb);
	return(1);
}

void secondaryServerConnectToCoreNode(void *nn) {
	struct bcaio		*cc;
	struct baio 		*bb;
	struct sockaddr_in 	serv_addr;
	int					n;

	n = (int) nn;

	assert(n >= 0 && n < SERVERS_MAX);

	bb = baioNewTcpipClient(coreServers[n].host, coreServers[n].secondaryServersListeningPort, BAIO_SSL_YES, BAIO_ADDITIONAL_SIZE(struct bcaio));
	if (bb == NULL) {
		timeLineInsertEvent(UTIME_AFTER_SECONDS(10), secondaryServerConnectToCoreNode, (void*)secondaryServerNextCoreNodeToConnect(n));
	} else {
		bchainSetBufferSizes(bb);
		cc = (struct bcaio *) bb;
		cc->connectionType = CT_SECONDARY_TO_CORE_IN_SECONDARY_SERVER;
		cc->fromNode = -1;
		cc->toNode = n;
		cc->remoteNode = n;
		jsVarCallBackAddToHook(&bb->callBackOnRead, secondaryServerReadCallback);
		jsVarCallBackAddToHook(&bb->callBackOnConnect, secondaryServerConnectCallback);
		jsVarCallBackAddToHook(&bb->callBackOnEof, secondaryServerEofCallback);
		jsVarCallBackAddToHook(&bb->callBackOnDelete, secondaryServerDeleteCallback);
	}
}

////////////////////////////////////////////////////////////////////

int checkFileAndTokenName(char *name, int allowSlashFlag) {
	int i;
	if (name[0] == ' ') return(-1);
	for(i=0; name[i] != 0; i++) {
		if (name[i] < ' ') return(-1);
		if (! isprint(name[i])) return(-1);
		if (! allowSlashFlag) {
			if (name[i] == '/') return(-1);
		}
		if (name[i] == '.' && name[i+1] == '/') return(-1);
		if (name[i] == '&') return(-1);
	}
	return(0);
}

char *databaseKeyFromString_st(char *ss) {
	int		i;
	char	*res;

	res = staticStringRingGetTemporaryStringPtr_st();
	for(i=0; i<TMP_STRING_SIZE-1 && ss[i]!=0; i++) {
		if (isprint(ss[i]) && ss[i] != '/') res[i] = ss[i];
		else res[i] = '-';
	}
	res[i] = 0;
	return(res);
}

static char *databaseKeyFileName_st(char *database, char *key) {
	int		r;
	char	*res;

	if (checkFileAndTokenName(database, 0) < 0) return(NULL);
	if (checkFileAndTokenName(key, 0) < 0) return(NULL);
	res = staticStringRingGetTemporaryStringPtr_st();
	r = snprintf(res, TMP_STRING_SIZE, "%s/%s/%s/%s", PATH_TO_DATABASE_DIRECTORY, mynodeidstring, database, databaseKeyFromString_st(key));
	if (r < 0 || r >= TMP_STRING_SIZE) return(NULL);
	return(res);
}

int databasePut(char *database, char *key, char *value, int valueLength) {
	int 	res;
	char	*fname;

	fname = databaseKeyFileName_st(database, key);
	if (fname == NULL) return(-1);
	res = fileSaveWithLength(value, valueLength, 1, "%s", fname);
	return(res);
}

struct jsVarDynamicString *databaseGet(char *database, char *key) {
	struct jsVarDynamicString 	*res;
	char						*fname;

	fname = databaseKeyFileName_st(database, key);
	if (fname == NULL) return(NULL);
	res = jsVarDstrCreateByFileLoad(0, "%s", fname);
	return(res);
}

int databaseDelete(char *database, char *key) {
	int				res;
	char			*fname;
	struct stat		st;

	fname = databaseKeyFileName_st(database, key);
	if (fname == NULL) return(-1);

	if (stat(fname, &st) == -1) return(-1);
	res = unlink(fname);
	if (res != 0) {
		PRINTF("internall error: can't unlink %s: %s\n", fname, JSVAR_STR_ERRNO());
	}
	return(res);
}

int databaseMapOnKeys(char *database, void (*fun)(char *key, void *arg), void *arg) {
	int		r;
	char	ttt[TMP_STRING_SIZE];

	if (checkFileAndTokenName(database, 0) < 0) return(-1);
	r = snprintf(ttt, sizeof(ttt), "%s/%s/%s", PATH_TO_DATABASE_DIRECTORY, mynodeidstring, database);
	if (r <= 0 || r >= TMP_STRING_SIZE) return(-1);
	r = mapDirectoryFiles(ttt, fun, arg);
	return(r);
}

///////////////////////////////////////////////////////////////////////

struct pendingRequestAddToBixMsgArgStr {
	struct jsVarDynamicString 	*ss;
	int							prefixlen;
	char						*prefix;
};

static void pendingRequestAddToBixMsgMapFunction(char *key, void *arg) {
	struct pendingRequestAddToBixMsgArgStr	*aa;
	int										keylen;

	aa = (struct pendingRequestAddToBixMsgArgStr *) arg;

	keylen = strlen(key);
	if (keylen >= aa->prefixlen && strncmp(key, aa->prefix, aa->prefixlen) == 0) {
		jsVarDstrAppendPrintf(aa->ss, "fil:%d=%s;", keylen, key);
	}
}

void pendingRequestsAddToBixMsg(struct jsVarDynamicString *ss, char *database, char *prefix) {
	int										r;
	char									ttt[TMP_STRING_SIZE];
	struct pendingRequestAddToBixMsgArgStr	arg;

	arg.ss = ss;
	arg.prefix = prefix;
	arg.prefixlen = strlen(arg.prefix);
	databaseMapOnKeys(database, pendingRequestAddToBixMsgMapFunction, &arg);
}

///////////////////////////////

static void coreGuiPendingRequestAddMapFunctionForPrefix(char *key, void *arg) {
	static int	index = 0;
	
	char	*prefix;
	int		i, prefixlen, keylen;

	if (guiPendingRequestsVector == NULL) return;

	prefix = (char *) arg;
	if (key == NULL) {
		// initialization call
		index = 0;
		// to fix memory leak
		for(i=0; i<guiPendingRequestsVector->arrayDimension; i++) {
			FREE(jsVarGetStringVector(guiPendingRequestsVector, i));
		}
		// PRINTF("RESIZING guiPendingRequestsVector to 0\n", 0);
		jsVarResizeVector(guiPendingRequestsVector, 0);		
		// PRINTF("RESET pendingRequest...: %d <-> %d\n", guiPendingRequestsVector->arrayDimension, guiPendingRequestsVector->lastSentArrayDimension);
		return;
	}

	keylen = strlen(key);
	prefixlen = strlen(prefix);
	if (keylen >= prefixlen && strncmp(key, prefix, prefixlen) == 0) {
		// PRINTF("RESIZING guiPendingRequestsVector to %d\n", index+1);
		jsVarResizeVector(guiPendingRequestsVector, index + 1);
		// PRINTF("pendingRequest2...: %d <-> %d\n", guiPendingRequestsVector->arrayDimension, guiPendingRequestsVector->lastSentArrayDimension);
		// TODO: fix memory leak here, change jsvar.c !!!
		jsVarSetStringVector(guiPendingRequestsVector, index, strDuplicate(key));
		index ++;
	}
}

void coreGuiPendingRequestsRegularUpdateList(void *d) {
	int		r;
	char	ttt[TMP_STRING_SIZE];

	r = snprintf(ttt, TMP_STRING_SIZE, "%s-%s", PENDING_NEW_ACCOUNT_REQUEST, MESSAGE_DATATYPE_BYTECOIN);
	if (r >= TMP_STRING_SIZE || r < 0) {
		PRINTF0("Error: Problem forming update list request. Probably too long coin name.\n");
		return;
	}
	coreGuiPendingRequestAddMapFunctionForPrefix(NULL, NULL);
	databaseMapOnKeys(DATABASE_PENDING_REQUESTS, coreGuiPendingRequestAddMapFunctionForPrefix, ttt);
	timeLineInsertEvent(UTIME_AFTER_SECONDS(3), coreGuiPendingRequestsRegularUpdateList, NULL);
}

int pendingRequestApprove(char *request) {
	struct jsVarDynamicString 		*req;

	PRINTF("Approving request %s\n", request);
	req = databaseGet(DATABASE_PENDING_REQUESTS, request);
	if (req == NULL) {
		PRINTF("Error: Can't get request %s.\n", request);
		return(-1);
	}
	onAccountApproved(req->s, req->size);
	jsVarDstrFree(&req);
	onPendingGdprRequestDelete(request);
	return(0);
}

void pendingRequestDelete(char *request) {
	// TODO: Send some notification about account rejected.
	onPendingGdprRequestDelete(request);
}

static void pendingRequestSetScanData(struct jsVarDynamicString *ss, struct bixParsingContext *pbixc2, char *htmlselector, int fnameTag, int scanTag) {
	jsVarDstrAppendPrintf(ss, "var cc = '");
	jsVarDstrAppendBase64EncodedData(ss, pbixc2->tag[scanTag], pbixc2->tagLen[scanTag]);
	jsVarDstrAppendPrintf(ss, "';");
	jsVarDstrAppendPrintf(
		ss, 
		"jj.find('%s').html('<object style=\"width:99%;height:99%;\" type=\"%s\" data=\"data:%s;base64,'+cc+'\">Can not show file %s.</object>');", 
		// "jj.find('%s').html('<img style=\"width:99%;height:99%;\" type=\"%s\" src=\"data:%s;base64,'+cc+'\">Can not show file %s.</img>');", 
		htmlselector,
		wsaioGetFileMimeType(pbixc2->tag[fnameTag]),
		wsaioGetFileMimeType(pbixc2->tag[fnameTag]),
		pbixc2->tag[fnameTag]
		);
}

void pendingRequestShowNewAccountRequest(char *reqs, int reqsize, struct jsVaraio *jj) {
	struct jsVarDynamicString 		*ss;
	int								i, r, rlen, bblength;
	char							*bb;
	struct bixParsingContext		*pbixc, *pbixc2;

	// printf("A 1\n");
	// printf("A: %s\n", reqs);

	ss = jsVarDstrCreateByPrintf("var jj = $('#contentspan');");

	pbixc = bixParsingContextGet();
	bixParseString(pbixc, reqs, reqsize);
	bixMakeTagsNullTerminating(pbixc);

	jsVarDstrAppendPrintf(ss, "jj.find('.view-requestid').html('%s');", pbixc->tag[BIX_TAG_pgr]);

	if (pbixc->tagLen[BIX_TAG_kyc] != 0) {
		pbixc2 = bixParsingContextGet();
		bixParseString(pbixc2, pbixc->tag[BIX_TAG_kyc], pbixc->tagLen[BIX_TAG_kyc]);
		bixMakeTagsNullTerminating(pbixc2);
		jsVarDstrAppendPrintf(ss, "jj.find('.view-firstName').html('%s');", pbixc2->tag[BIX_TAG_fnm]);
		jsVarDstrAppendPrintf(ss, "jj.find('.view-lastName').html('%s');", pbixc2->tag[BIX_TAG_lnm]);
		jsVarDstrAppendPrintf(ss, "jj.find('.view-email').html('%s');", pbixc2->tag[BIX_TAG_eml]);
		jsVarDstrAppendPrintf(ss, "jj.find('.view-telephone').html('%s');", pbixc2->tag[BIX_TAG_tel]);
		jsVarDstrAppendPrintf(ss, "jj.find('.view-address').html('%s');", pbixc2->tag[BIX_TAG_adr]);
		jsVarDstrAppendPrintf(ss, "jj.find('.view-country').html('%s');", pbixc2->tag[BIX_TAG_cnt]);
		jsVarDstrAppendPrintf(ss, "jj.find('.view-idcardfname').html('%s');", pbixc2->tag[BIX_TAG_idf]);
		pendingRequestSetScanData(ss, pbixc2, ".view-idcard", BIX_TAG_idf, BIX_TAG_ids);
		jsVarDstrAppendPrintf(ss, "jj.find('.view-proofofaddressfname').html('%s');", pbixc2->tag[BIX_TAG_pdf]);
		pendingRequestSetScanData(ss, pbixc2, ".view-proofofaddress", BIX_TAG_pdf, BIX_TAG_pds);
		bixMakeTagsSemicolonTerminating(pbixc2);
		bixParsingContextFree(&pbixc2);
	}

	bixMakeTagsSemicolonTerminating(pbixc);
	bixParsingContextFree(&pbixc);

	// This is because address can contain newlines
	jsVarDstrReplace(ss, "\n", "\\n", 1);

	// PRINTF("Sending %s\n", ss->s);
	jsVarSendEval(jj, "%s", ss->s);

	jsVarDstrFree(&ss);
}

void pendingRequestExplore(char *request, struct jsVaraio *jj) {
	struct jsVarDynamicString 		*req;
	struct jsVarDynamicString 		*ss;
	int								i, r, rlen, bblength;
	char							*bb;

	PRINTF("View request %s\n", request);
	req = databaseGet(DATABASE_PENDING_REQUESTS, request);
	if (req == NULL) {
		PRINTF0("Error: Can't load pending request content\n");
		return;
	}
	pendingRequestShowNewAccountRequest(req->s, req->size, jj);
	jsVarDstrFree(&req);
}

/////////////////////

static void listAccountsMapFunction(char *key, void *arg) {
	struct jsVarDynamicString 	*ss;
	int 						keylen;

	ss = (struct jsVarDynamicString *)arg;
	keylen = strlen(key);
	jsVarDstrAppendPrintf(ss, "fil:%d=%s;", keylen, key);
}

void listAccountsToBixMsg(struct jsVarDynamicString *ss, char *coin) {
	int										r;
	char									ttt[TMP_STRING_SIZE];

	if (checkFileAndTokenName(coin, 0) < 0) return;
	r = snprintf(ttt, sizeof(ttt), "%s/%s/%s", PATH_TO_ACCOUNTS_PER_TOKEN_DIRECTORY, mynodeidstring, coin);
	if (r <= 0 || r >= TMP_STRING_SIZE) return;
	r = mapDirectoryFiles(ttt, listAccountsMapFunction, ss);
}

/////////////////////////////////////////////////////

void anotificationAddOrRefresh(char *account, int baioMagic, int connectionType) {
	int								i;
	struct anotificationsTreeStr 	nnn, *memb;

	if (account == NULL || account[0] == 0) return;

	// PRINTF("anotification: add binding %s --> %d\n", account, baioMagic);

	nnn.account = account;
	memb = sglib_anotifyTree_find_member(anotificationTree, &nnn);
	if (memb == NULL) {
		ALLOC(memb, struct anotificationsTreeStr);
		memset(memb, 0, sizeof(*memb));
		memb->account = strDuplicate(account);
		memb->notifyTab = NULL;
		memb->notifyTabi = 0;
		memb->notifyTabAllocatedSize = 0;
		sglib_anotifyTree_add(&anotificationTree, memb);
	}
	for(i=0; i<memb->notifyTabi; i++) {
		if (memb->notifyTab[i].baioMagic == baioMagic) {
			memb->notifyTab[i].connectionType = connectionType;
			memb->notifyTab[i].lastRefreshTime = currentTime.sec;
			return;
		}
	}
	if (memb->notifyTabi >= memb->notifyTabAllocatedSize) {
		memb->notifyTabAllocatedSize = memb->notifyTabAllocatedSize * 2 + 1;
		REALLOCC(memb->notifyTab, memb->notifyTabAllocatedSize, struct notificationInfo);
	}
	i = memb->notifyTabi;
	memb->notifyTab[i].baioMagic = baioMagic;
	memb->notifyTab[i].connectionType = connectionType;
	memb->notifyTab[i].lastRefreshTime = currentTime.sec;
	memb->notifyTabi ++;
}

void anotificationShowNotificationV(char *account, int connectionType, char *secondaryServerInfo, char *fmt, va_list arg_ptr) {
	int								i, j, r;
	struct anotificationsTreeStr 	nnn, *memb;
	struct jsVaraio					*jj;
	struct bcaio					*cc;
	struct jsVarDynamicString		*ss, *jss;

	// PRINTF("anotification: for %s\n", account);
	if (account == NULL || account[0] == 0) {
		PRINTF0("Internal error: No account in notification messages.\n");
		return;
	}

	if (secondaryServerInfo == NULL) secondaryServerInfo = "";

	nnn.account = account;
	memb = sglib_anotifyTree_find_member(anotificationTree, &nnn);
	if (memb == NULL) return;

	ss = jsVarDstrCreateByVPrintf(fmt, arg_ptr);
	jss = NULL;

	// PRINTF("anotification: send %s to %s\n", ss->s, account);

	for(i=0; i<memb->notifyTabi; i++) {
		memb->notifyTab[i].deleteFlag = 0;
		if (memb->notifyTab[i].connectionType == connectionType) {
			switch (connectionType) {
			case NCT_BIX:
				// PRINTF("anotification: sending to BIX %d\n", memb->notifyTab[i].baioMagic);
				cc = (struct bcaio *) baioFromMagic(memb->notifyTab[i].baioMagic);
				if (cc == NULL) {
					memb->notifyTab[i].deleteFlag = 1;
				} else {
					r = bixSendMsgToConnection(cc, "", "msg=msn;acc=%s;ssi=%s;dat:%d=%s;", account, secondaryServerInfo, ss->size, ss->s);
					if (r <= 0) {
						memb->notifyTab[i].deleteFlag = 1;
					}
				}
				break;
			case NCT_GUI:
				// PRINTF("anotification: sending to GUI %d\n", memb->notifyTab[i].baioMagic);
				jj = (struct jsVaraio *) baioFromMagic(memb->notifyTab[i].baioMagic);	
				if (jj == NULL)	{
					memb->notifyTab[i].deleteFlag = 1;
				} else {
					if (jss == NULL) {
						jss = jsVarDstrCreateCopy(ss);
						jsVarDstrAddCharacter(jss, '\n');
						jsVarDstrReplace(jss, "'", "\\'", 1);
						jsVarDstrReplace(jss, "\n", "\\n", 1);
					}
					jsVarSendEval(jj, "guiNotify('%s');", jss->s);
				}
				break;
			default:
				printf("Internal error: notification for unknown connection type %d\n", connectionType);
			}
		}
	}

	j = 0;
	for(i=0; i<memb->notifyTabi; i++) {
		if (memb->notifyTab[i].deleteFlag == 0) {
			memb->notifyTab[j] = memb->notifyTab[i];
			j++;
		}
	}
	memb->notifyTabi = j;

	jsVarDstrFree(&jss);
	jsVarDstrFree(&ss);
}

void anotificationShowNotification(char *account, int connectionType, char *fmt, ...) {
	va_list 						arg_ptr;

	va_start(arg_ptr, fmt);
	anotificationShowNotificationV(account, connectionType, NULL, fmt, arg_ptr);
	va_end(arg_ptr);
}

#define SPRINT_NOTIFICATION_FNAME(fname, dir, index) sprintf(fname, "%s/%05d.txt", dir, index)

static void anotificationGetNoticiationsDirectory(char dir[TMP_STRING_SIZE], char *token, char *account) {
	char 			*accToken;
	int 			dirlen;
	char 			*dd;

	// small hack, user file operations are counted to bytecoin account
	accToken = token;
	if (strcmp(token, MESSAGE_DATATYPE_USER_FILE) == 0) accToken = MESSAGE_DATATYPE_BYTECOIN;
	dd = bAccountNotesDirectory_st(accToken, account);
	dirlen = strlen(dd);

	if (dirlen >= TMP_STRING_SIZE-10) return;

	strcpy(dir, dd);
}

void anotificationSaveNotificationV(char *account, char *token, char *secondaryServerInfo, char *fmt, va_list arg_ptr) {
	int								i, j, r;
	char							*dd;
	char							dir[TMP_STRING_SIZE];
	char							fname[TMP_STRING_SIZE];
	char							fname2[TMP_STRING_SIZE];
	int								dirlen;
	struct stat						st;
	FILE							*ff;
	va_list							arg_ptr_copy;
	char							*accToken;

	// PRINTF("anotification: for %s\n", account);
	// first send the notification to on-line user
	if (account == NULL || account[0] == 0 || token == NULL || token[0] == 0) {
		PRINTF0("Internal error: No account or token in notification messages.\n");
		return;
	}

	va_copy(arg_ptr_copy, arg_ptr);
	anotificationShowNotificationV(account, NCT_BIX, secondaryServerInfo, fmt, arg_ptr_copy);
	va_copy_end(arg_ptr_copy);

	anotificationGetNoticiationsDirectory(dir, token, account);
	mkdirRecursivelyCreateDirectory(dir);
	SPRINT_NOTIFICATION_FNAME(fname, dir, 0);
	r = stat(dir, &st);
	if (r == 0 && st.st_size >= NOTIFICATION_FILE_THRESHOLD_SIZE) {
		// if there is more than 1/4 MB create a new file
		// and shift existing files
		for (i=NOTIFICATION_FILES_MAX-1; i>=0; i--) {
			SPRINT_NOTIFICATION_FNAME(fname, dir, i);
			r = stat(fname, &st);
			if (r == 0) {
				SPRINT_NOTIFICATION_FNAME(fname2, dir, i+1);
				rename(fname, fname2);
			}
		}
	}
	ff = fopen(fname, "a");
	if (ff == NULL) {
		PRINTF("Error: Can't open file %s for append\n", fname);
		return;
	}
	vfprintf(ff, fmt, arg_ptr);
	fprintf(ff, "\n");
	fclose(ff);
}

void anotificationSaveNotification(char *account, char *accountToken, char *fmt, ...) {
	va_list 						arg_ptr;

	va_start(arg_ptr, fmt);
	anotificationSaveNotificationV(account, accountToken, NULL, fmt, arg_ptr);
	va_end(arg_ptr);
}

void anotificationShowMsgNotification(struct bchainMessage *msg, char *fmt, ...) {
	char		notificationtext[LONG_LONG_TMP_STRING_SIZE];
	va_list 	arg_ptr, arg_ptr_copy;
	int			r;
	char		*ssi;

	if (msg == NULL) {
		PRINTF0("Internal error: bchainNotifySecondaryServerAboutMsg: missing notification message.\n");
		return;
	}

	va_start(arg_ptr, fmt);
	if (msg->account != NULL && msg->account[0] != 0) {
		va_copy(arg_ptr_copy, arg_ptr);
		anotificationShowNotificationV(msg->account, NCT_BIX, msg->secondaryServerInfo, fmt, arg_ptr_copy);
		va_copy_end(arg_ptr_copy);
	}
	if (msg->notifyAccount != NULL 
		&& msg->notifyAccountToken != NULL 
		&& msg->notifyAccount[0] != 0 
		&& (msg->account == NULL || strcmp(msg->account, msg->notifyAccount) != 0)
		) {
		anotificationShowNotificationV(msg->notifyAccount, NCT_BIX, msg->secondaryServerInfo, fmt, arg_ptr);
	}
	va_end(arg_ptr);
}

void anotificationSaveMsgNotification(struct bchainMessage *msg, char *fmt, ...) {
	char		notificationtext[LONG_LONG_TMP_STRING_SIZE];
	va_list 	arg_ptr, arg_ptr_copy;
	int			r;
	char		*ssi;

	if (msg == NULL) {
		PRINTF0("Internal error: bchainNotifySecondaryServerAboutMsg: missing notification message.\n");
		return;
	}

	va_start(arg_ptr, fmt);
	if (msg->account != NULL && msg->account[0] != 0) {
		va_copy(arg_ptr_copy, arg_ptr);
		anotificationSaveNotificationV(msg->account, msg->dataType, msg->secondaryServerInfo, fmt, arg_ptr_copy);
		va_copy_end(arg_ptr_copy);
	}
	if (msg->notifyAccount != NULL 
		&& msg->notifyAccountToken != NULL 
		&& msg->notifyAccount[0] != 0 
		&& (msg->account == NULL || strcmp(msg->account, msg->notifyAccount) != 0)
		) {
		anotificationSaveNotificationV(msg->notifyAccount, msg->notifyAccountToken, msg->secondaryServerInfo, fmt, arg_ptr);
	}
	va_end(arg_ptr);
}

void anotificationAppendSavedNotifications(struct jsVarDynamicString *ss, char *token, char *account, int historyLevel) {
	char						dir[TMP_STRING_SIZE];
	char						fname[TMP_STRING_SIZE];
	FILE						*ff;

	anotificationGetNoticiationsDirectory(dir, token, account);
	SPRINT_NOTIFICATION_FNAME(fname, dir, historyLevel);
	ff = fopen(fname, "r");
	if (ff == NULL) return;
	jsVarDstrAppendFile(ss, ff);
	fclose(ff);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// exlorer

static struct blockContentDataList *explorerCurrentBlock;

void mainExplorerExploreBlock(struct jsVaraio *jj, int64_t seq) {
	struct jsVarDynamicString	*ss, *bb;
	struct bixParsingContext 	*bix;
	time_t						tstamp;

	jsVarSendEval(jj, "$('.msgdiv').html('');");

	if (explorerCurrentBlock != NULL) bchainBlockContentFree(&explorerCurrentBlock);
    explorerCurrentBlock = bchainCreateBlockContentLoadedFromFile(seq);
    if (explorerCurrentBlock == NULL) {
        jsVarSendEval(jj, "$('.blockdiv').html('Block not found.');");
        return;
    }
	ss = jsVarDstrCreate();
	// jsVarDstrAppendPrintf(ss, "Messages:<br>");
	jsVarDstrAppendPrintf(ss, "<table style='width:90%;margin-left:5%;'>");
	jsVarDstrAppendPrintf(ss, "<tr><th>Message Id</th><th>Message Type</th><th>Owner Account</th><th>Message Length</th><th>Submission Time</th></tr>");
	bix = bixParsingContextGet();
	BIX_PARSE_AND_MAP_STRING(explorerCurrentBlock->blockContent, explorerCurrentBlock->blockContentLength, bix, 1, {
			if (strncmp(tagName, "dat", 3) == 0) {
				tstamp = atoll(bix->tag[BIX_TAG_tsu]) / 1000000LL;
				jsVarDstrAppendPrintf(
					ss, 
					"<tr><td><button style='background:none;border:none;cursor:pointer;color:blue;text-decoration:underline;' onclick='onExploreMessagePressed(&quot;%s&quot;);'>%s</button></td><td>%s</td><td>%s</td><td>%d</td><td>%.24s</td></tr>", 
					bix->tag[BIX_TAG_mid],
					bix->tag[BIX_TAG_mid],
					bix->tag[BIX_TAG_mdt],
					bix->tag[BIX_TAG_acc],
					//bix->tag[BIX_TAG_sig],
					bix->tagLen[BIX_TAG_dat],
					ctime(&tstamp)
					);
			}
		},
		{
			PRINTF0("Error while parsing block content");
		}
		);
	jsVarDstrAppendPrintf(ss, "</table>");
	// create global block data info into another string

	bb = jsVarDstrCreate();
	jsVarDstrAppendPrintf(bb, "<div class=breakwords >");
	jsVarDstrAppendPrintf(bb, "<h3>Block %s</h3>", bix->tag[BIX_TAG_seq]);
	jsVarDstrAppendPrintf(bb, "<b>Block Hash</b>: %s<br>", explorerCurrentBlock->blockContentHash);
	jsVarDstrAppendPrintf(bb, "<b>Previous Block Hash</b>:%s<br>", bix->tag[BIX_TAG_pbh]);
	jsVarDstrAppendPrintf(bb, "</div>");
	bixMakeTagsSemicolonTerminating(bix);
	bixParsingContextFree(&bix);
	jsVarSendEval(jj, "$('.blockdiv').html(\"%s %s\");", bb->s, ss->s);
	jsVarDstrFree(&bb);
	jsVarDstrFree(&ss);
}

void mainExplorerExploreMessage(struct jsVaraio *jj, char *msgid) {
	struct jsVarDynamicString	*ss;
	struct bixParsingContext 	*bix, *ebixi;
	time_t						tstamp;
	int							r, msgidlen;

    if (explorerCurrentBlock == NULL) {
        jsVarSendEval(jj, "$('.blockdiv').html('Block not found.');");
        return;
    }
	msgidlen = strlen(msgid);
	bix = bixParsingContextGet();
	BIX_PARSE_AND_MAP_STRING(explorerCurrentBlock->blockContent, explorerCurrentBlock->blockContentLength, bix, 1, {
			if (strncmp(tagName, "dat", 3) == 0 && bix->tagLen[BIX_TAG_mid] == msgidlen && strncmp(bix->tag[BIX_TAG_mid], msgid, msgidlen) == 0) {
				tstamp = atoll(bix->tag[BIX_TAG_tsu]) / 1000000LL;
				ss = jsVarDstrCreate();
				jsVarDstrAppendPrintf(ss, "$('.msgdiv').html(\"");
				jsVarDstrAppendPrintf(ss, "<h3>Message %s</h3>", msgid);
				jsVarDstrAppendPrintf(
					ss, 
					"message type: %s<br>account: %s<br>date: %.24s<br>length: %d<br>"
					"<div id=contentinfo></div>"
					"Content: <div id=msgcontent style='height:30%; border:1px solid black; padding:2%; overflow:auto;'></div>"
					, 
					bix->tag[BIX_TAG_mdt],
					bix->tag[BIX_TAG_acc],
					ctime(&tstamp),
					bix->tagLen[BIX_TAG_dat]
					);
				if (0 && strcmp(bix->tag[BIX_TAG_mdt], MESSAGE_DATATYPE_USER_FILE) == 0) {
						jsVarDstrAppendPrintf(
							ss, 
							"<input type=button onclick='onSaveMsgContent();' value='SaveContent' />"
							, 
							bix->tag[BIX_TAG_mdt],
							bix->tag[BIX_TAG_acc],
							ctime(&tstamp),
							bix->tagLen[BIX_TAG_dat]
							);
				}
				jsVarDstrAppendPrintf(ss, "\");");
				if (0 && strcmp(bix->tag[BIX_TAG_mdt], MESSAGE_DATATYPE_USER_FILE) == 0) {
					ebixi = bixParsingContextGet();
					bixParseString(ebixi, bix->tag[BIX_TAG_dat], bix->tagLen[BIX_TAG_dat]);
					bixMakeTagsNullTerminating(ebixi);
					jsVarDstrAppendPrintf(ss, "messageContentFilename=\"%s\";", ebixi->tag[BIX_TAG_fnm]);
					jsVarDstrAppendPrintf(ss, "document.getElementById('contentinfo').innerText = \"filename: \" + messageContentFilename;", ebixi->tag[BIX_TAG_fnm]);
					jsVarDstrAppendPrintf(ss, "messageContentBase64=\"");
					jsVarDstrAppendBase64EncodedData(ss, ebixi->tag[BIX_TAG_dat], ebixi->tagLen[BIX_TAG_dat]);
					jsVarDstrAppendPrintf(ss, "\";");
					bixMakeTagsSemicolonTerminating(ebixi);
					bixParsingContextFree(&ebixi);
				} else {
					jsVarDstrAppendPrintf(ss, "messageContentBase64=\"");
					jsVarDstrAppendBase64EncodedData(ss, bix->tag[BIX_TAG_dat], (bix->tagLen[BIX_TAG_dat] > 1000 ? 1000 : bix->tagLen[BIX_TAG_dat]));
					jsVarDstrAppendPrintf(ss, "\";");
				}
				jsVarDstrAppendPrintf(ss, "var content = atob(messageContentBase64);");
				if (bix->tagLen[BIX_TAG_dat] > 1000) jsVarDstrAppendPrintf(ss, "content += ' ...';");
				jsVarDstrAppendPrintf(ss, "document.getElementById('msgcontent').innerText = content;");
				r = jsVarSendEval(jj, "%s", ss->s);
				jsVarDstrFree(&ss);
			}
		},
		{
			PRINTF0("Error while parsing block content");
		}
		);
	bixMakeTagsSemicolonTerminating(bix);
	bixParsingContextFree(&bix);
}

