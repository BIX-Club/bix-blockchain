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

#ifndef __COMMON__H
#define __COMMON__H

/////////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
// #include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <limits.h>
#include <time.h>
#include <signal.h>
#include <stdarg.h>
#include <malloc.h>
#include <math.h>

#include <stdint.h>

#include <sys/stat.h>
#include <sys/types.h>

// mmap, lockq
//#include <sys/mman.h>

// threads are used with moderation
// #include <pthread.h>
// #include <sched.h>

// openssl
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/hmac.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/md5.h>
#include <openssl/ec.h>

#if _WIN32

#include <tchar.h>
#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include <malloc.h>
#include <new.h>
#include <Winsock2.h>
#include <ws2tcpip.h>
#include <errno.h>
#include <io.h>
#include <fcntl.h>
typedef __int8           	 int8_t;
typedef __int16          	 int16_t; 
typedef __int32          	 int32_t;
typedef __int64          	 int64_t;
typedef unsigned __int8  	 uint8_t;
typedef unsigned __int16 	 uint16_t;
typedef unsigned __int32 	 uint32_t;
typedef unsigned __int64 	 uint64_t;
#define snprintf 			_snprintf
#define atoll(x)			_atoi64(x)
#define va_copy(d,s)        ((d) = (s))
#define va_copy_end(a)      {}
#define CORE_DUMP()			{}
#if !defined(PRId64)
#define PRId64 				"I64d"
#define PRIu64 				"I64u"
#define PRIx64 				"I64x"
#endif

#else
// if !_WIN32

#include <unistd.h>
#include <dirent.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/wait.h>
#define va_copy_end(a)      va_end(a)
#define CORE_DUMP()			{if (fork()==0) abort();}

// endif _WIN32
#endif


#include "expmem.h"
#include "sglib.h"

#include "jsvarextended.h"

//////////////////////////////////////////////////////////////////////////////////////

#define PILOT_VERSION_FLAG								1
#if EDEBUG
#define DEVELOPMENT_VERSION_FLAG						1
#else
#define DEVELOPMENT_VERSION_FLAG						0
#endif

#define WALLET_GUI_HTTP_PORT							80
#define WALLET_GUI_HTTPS_PORT							443

// set this to zero after initial creation of the blockchain
#define ALLOW_GENERATION_OF_GENESIS_BLOCK				1

#define BIX_TRANSFER_FEES								0.0015
#define BIX_TRANSFER_FEES_OTHER_TOKENS					0.0015
#define BIX_NEW_TOKEN_CREATION_FEES						(500.0 - BIX_NEW_TOKEN_OPTION_UPDATE_FEES)
#define BIX_NEW_TOKEN_OPTION_UPDATE_FEES				(250.0)
#define NEW_TOKEN_CAPITALIZATION_FEE_PERCENTAGE			10.0

/////////////////////////////////////////////////////////////////////////////////////

#define WALLET_PUBLIC_KEY_FILE							"public.pem"
#define WALLET_PRIVATE_KEY_FILE							"private.pem"

#define PATH_TO_DATA_DIRECTORY							"../data"
#define PATH_TO_SERVER_TOPOLOGY_FILE					"servertopology.json"

#define PATH_TO_BLOCKCHAIN 								PATH_TO_DATA_DIRECTORY "/blocks"
#define PATH_TO_STATUS									PATH_TO_DATA_DIRECTORY "/status"
#define PATH_TO_ACCOUNTS_DIRECTORY 						PATH_TO_DATA_DIRECTORY "/accounts"
#define PATH_TO_ACCOUNTS_PER_TOKEN_DIRECTORY 			PATH_TO_DATA_DIRECTORY "/accountspertoken"
#define PATH_TO_SUPPORTED_MSGS_DIRECTORY 				PATH_TO_DATA_DIRECTORY "/supportedmsgs"
#define PATH_TO_DATABASE_DIRECTORY 						PATH_TO_DATA_DIRECTORY "/db"

#define TMP_STRING_SIZE									256
#define LONG_TMP_STRING_SIZE 							4096
#define LONG_LONG_TMP_STRING_SIZE 						65536
#define TMP_FILE_NAME_SIZE								4096
#define STATIC_STRING_RING_SIZE							128
#define STATIC_LLSTRING_RING_SIZE						8

#define MAX_HASH64_SIZE									(SHA512_DIGEST_LENGTH*2)

#define SERVERS_MAX										100
#define ACTIVE_CONNECTIONS_MAX							20

#define CONSENSUS_TICK_USEC 							(1000000LL)
#define BLOCK_NODE_AVERAGE_PROPOSAL_PERIOD_USEC			(1000000LL)
#define CONSENSUS_ENGAGEMENT_ROUND_MAX					6
#define BCHAIN_MAX_BLOCK_SIZE							(1<<23) /* 8 MB */
//#define BCHAIN_MAX_BLOCK_SIZE							(1<<10)


#define CONSENSUS_ENGAGEMENT_EXPIRATION_INFINITY		(0x7fffffffffffffffLL)
#define CONSENSUS_MAX_MEMORIZED_SEQUENCE_DIFF			20
#define CONSENSUS_EXPIRATION_TIMEOUT					(consensusRounds[CONSENSUS_ENGAGEMENT_ROUND_MAX-2].roundTimeout)
#define BLOCK_RESYNCHRONIZATION_TIMEOUT					(BLOCK_NODE_AVERAGE_PROPOSAL_PERIOD_USEC * 20 / 1000000LL)
//#define BLOCK_ACCEPTANCE_TIMEOUT						(BLOCK_NODE_AVERAGE_PROPOSAL_PERIOD_USEC * 200 / 1000000LL)
#define BLOCK_ACCEPTANCE_TIMEOUT						(BLOCK_NODE_AVERAGE_PROPOSAL_PERIOD_USEC * 60 / 1000000LL)

#define BCHAIN_STATUS_FILE_VERSION						0x01
#define BCHAIN_STATUS_FILE_MAGIC 						(0xbabacafe12345678LL)

#define MESSAGE_EXPIRATION_TIMEOUT_USEC					(BLOCK_NODE_AVERAGE_PROPOSAL_PERIOD_USEC * 50)

// Initial space for the network, le't start with 1 TB
#define INITIAL_BLOCKCHAIN_SPACE_AVAILABLE				((long long)1E12)
// #define INITIAL_BLOCKCHAIN_SPACE_AVAILABLE				3000LL

// #define BCHAIN_MINING_PERIOD_SEC	(24*60*60)
#if PILOT_VERSION_FLAG
#define BCHAIN_MINING_PERIOD_SEC						(60*60)
#else
#define BCHAIN_MINING_PERIOD_SEC						(12*60*60)
#endif
// The current price is 0.1 BIX per MB
#define BCHAIN_FEE_FOR_INSERTING_LENGTH(datLength) 		(datLength / 10000000.0)
#define DEFAULT_LATENCY_USEC							(100*1000)

#define THIS_CORE_SERVER_ACCOUNT_NAME_ST() 				getAccountName_st(0, myNode)

#define REQ_ID_NONE										(-1)
#define SSI_NONE										(-1234)

// maximal channels for notifications of the same account
#define ACCOUNT_NOTIFICATIOS_MAX						8
// maximal number of files keeping history of account notifications
#define NOTIFICATION_FILES_MAX 							100
// if notification file is larger than the value, start new file
#define NOTIFICATION_FILE_THRESHOLD_SIZE				(1<<18)


// pow mining

#define MINING_DELAY_NOW 								(1LL)
#if 0 && EDEBUG
#define MINING_DELAY_10MS 								(1000LL)
#define MINING_DELAY_100MS 								(1000LL)
#define MINING_DELAY_SECOND 							(1000LL)
#else
#define MINING_DELAY_10MS 								(10000LL)
#define MINING_DELAY_100MS 								(100000LL)
#define MINING_DELAY_SECOND 							(1000000LL)
#endif
#define MINING_DELAY_MINUTE 							(60*1000000LL)


/////////////////////////////////////////////////////////////////////////////////////

#ifdef EDEBUG
#define dabort() {assert(0);}
#else
#define dabort() {}
#endif

#undef assert
#ifdef EDEBUG
#define assert(x) {if (! (x)) {printf("Assertion(%s) failed at %s:%d.\n", #x, __FILE__, __LINE__); fflush(stdout); abort();}}
#else
#define assert(x) {if (! (x)) {printf("Assertion(%s) failed at %s:%d.\n", #x, __FILE__, __LINE__); fflush(stdout); CORE_DUMP();}}
#endif
#define InternalCheck(x) {if (! (x)) {printf("%s: Error: Internal check %s failed at %s:%d\n", SPRINT_CURRENT_TIME(), #x, __FILE__, __LINE__); fflush(stdout); }}

// this does not really guarantee that a is an array, but catch most of unvoluntary bugs
#define ASSERT_NOT_PTR(a)				assert(sizeof(&(a)) != sizeof(a))
#define ASSERT_ARRAY(a)					{ASSERT_NOT_PTR(a); assert((char *)&(a) == (char *)(a));}

#define MEMSET_SIZEOF(d, v)				{ASSERT_NOT_PTR(d); memset(&(d), (v), sizeof(d));}
// #define CHECKED_DIMENSION(a)			(ASSERT_ARRAY(d), sizeof(a) / sizeof((a)[0]))
#define DIMENSION(a)					(sizeof(a) / sizeof((a)[0]))

#define UTIME_AFTER_MINUTES(n)  		(currentTime.usec + 1000000LL*60*(n))
#define UTIME_AFTER_SECONDS(n)  		(currentTime.usec + 1000000LL*(n))
#define UTIME_AFTER_MSEC(n)   			(currentTime.usec + 1000LL*(n))
#define UTIME_AFTER_USEC(n)     		(currentTime.usec + (n))

#define SPRINT_CURRENT_TIME() 			currentLocalTime_st()

#define MIN(x,y)            			((x)<(y) ? (x) : (y))
#define MAX(x,y)            			((x)<(y) ? (y) : (x))
#define ROUND_INT(n, r)     			( ( ((n)>0)?((n) + (r)/2):((n) - (r)/2) ) / (r) * (r) )
#define TRUNCATE_INT(n, r)  			( ((n)>=0) ? ( (n) / (r) * (r) ) : ( - ( (-(n)) / (r) * (r) ) ) )
#define ROUNDED_DIV(n,d)    			((((n) < 0) ^ ((d) < 0)) ? (((n) - (d)/2)/(d)) : (((n) + (d)/2)/(d)))
#define MUL10(res, n) 					{res = n + n; res = res + res + n; res = res + res;}

#define STRINGIFY(...) 					#__VA_ARGS__
#define EXIT_WITH_FAIL(r, msg, ...)		{PRINTF(msg "Exiting.", __VA_ARGS__); exit(r);}

#define PRINTF(fmt, ...)				{								\
		printf("%s: %s: %s:%d: " fmt, mynodeidstring, SPRINT_CURRENT_TIME(), __FILE__, __LINE__, __VA_ARGS__); \
		guiprintf("%s: %s:%d: " fmt, SPRINT_CURRENT_TIME(), __FILE__, __LINE__, __VA_ARGS__); \
	}
#define PRINTF0(fmt)				{									\
		printf("%s: %s: %s:%d: " fmt, mynodeidstring, SPRINT_CURRENT_TIME(), __FILE__, __LINE__); \
		guiprintf("%s: %s:%d: " fmt, SPRINT_CURRENT_TIME(), __FILE__, __LINE__); \
	}
#define PRINTF_NOP(fmt, ...)				{	\
		printf(fmt, __VA_ARGS__);				\
		guiprintf(fmt, __VA_ARGS__);			\
	}
#define PRINTF_NOP0(fmt)				{		\
		printf(fmt);							\
		guiprintf(fmt);							\
	}

#define GOTO_WITH_MSG(label, ...)			{PRINTF(__VA_ARGS__); goto label;}

#define BIX_TAG_BASE(x)									((x>='a' && x<='z')?(x-'a'):99999999999999999LL)
#define BIX_TAG(a, b, c) 								((BIX_TAG_BASE(a)*26+BIX_TAG_BASE(b))*26+BIX_TAG_BASE(c))
#define BIX_TAG_MAX										(BIX_TAG('z','z','z')+1)

#if EDEBUG
#define BACKWARD_COMPATIBILITY 0
#else
#define BACKWARD_COMPATIBILITY 1
#endif

// Those special messages/tokens has to start with space to be at the begining of table of 
// supported messages (which is sorted lexicographicaly)
#define MESSAGE_DATATYPE_META_MSG 						" MetaMsg"
#define MESSAGE_DATATYPE_EXCHANGE 						" Exchange"
#define MESSAGE_DATATYPE_USER_FILE 						" user file"
// be carefull, datatypes ares orted lexicographically, so if adding something larger that ' user...' change this as well
#define MESSAGE_DATATYPE_LAST_NON_COIN					MESSAGE_DATATYPE_USER_FILE
#define MESSAGE_DATATYPE_BYTECOIN 						"BIX"

#define BYTECOIN_BASE_ACCOUNT							"10811"

#define MESSAGE_METATYPE_META_MSG 						"MetaMsgMeta"
#define MESSAGE_METATYPE_USER_FILE 						"UserFileMeta"
#define MESSAGE_METATYPE_EXCHANGE 						"Exchange"
// simple token type is used for BIX and not mined coins
#define MESSAGE_METATYPE_SIMPLE_TOKEN 					"simpletoken"
// proof of work mined tokens
#define MESSAGE_METATYPE_POW_TOKEN 						"powtoken"

#define MSG_STATUS_INSERTED_INTO_BLOCKCHAIN				"insertedIntoBlockchain"

#define DATABASE_PENDING_REQUESTS						"pending"
#define DATABASE_KYC									"kyc"
#define DATABASE_KYC_DATA								"kycdata"
#define PENDING_NEW_ACCOUNT_REQUEST						"PendingNewAccountRequest"

//#define BYTECOIN_CURRENCY_OPTIONS						"new_account_balance=1000&new_account_approval=auto"
//#define BYTECOIN_CURRENCY_OPTIONS						"base_account=1081&base_balance=1000000&new_account_balance=1000&new_account_approval=auto"
#define STRN_CPY_TMP(d, s)								{strncpy(d, s, TMP_STRING_SIZE); d[TMP_STRING_SIZE-1] = 0;}
#define STRN_CPY_LONG_TMP(d, s)							{strncpy(d, s, LONG_TMP_STRING_SIZE); d[LONG_TMP_STRING_SIZE-1] = 0;}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define BIX_PARSE_AND_MAP_STRING(ss, sslen, bix, keepMemorizedTagsNullTerminatingFlag, command, commandonerror) { \
		char 	*tagName, *tagValue;									\
		int		tagn, tagValueLength;									\
		char 	*_s_, *_send_;											\
		int		_r_, _err_;												\
		_err_ = 0;														\
		if (ss != NULL) {												\
			bixCleanBixTags(bix);										\
			_s_ = ss;													\
			_send_ = ss + sslen;										\
			tagName = NULL;												\
			while (_s_ < _send_ && _err_ == 0) {						\
				if (_send_ - _s_ < 4) { _err_ = 1; break; }				\
				tagName = _s_;											\
				_s_ += 3;												\
				if (*_s_ == '=') {										\
					_s_++;												\
					tagValue = _s_;										\
					while (_s_ < _send_ && *_s_ != ';') _s_++;			\
					tagValueLength = _s_ - tagValue;					\
				} else if (*_s_ == ':') {								\
					_s_ ++;												\
					tagValueLength = parseInt(&_s_);					\
					if (*_s_ != '=') { _err_ = 1; break; }				\
					_s_ ++;												\
					if (tagValueLength < 0 || _s_ + tagValueLength > _send_) { _err_ = 1; break; } \
					tagValue = _s_;										\
					_s_ += tagValueLength;								\
				} else {												\
					_err_ = 1; break;									\
				}														\
				if (*_s_ != ';') { _err_ = 1; break; }					\
				tagn = BIX_TAG(tagName[0], tagName[1], tagName[2]);		\
				if (tagn < 0 || tagn >= BIX_TAG_MAX) {					\
					_err_ = 1; break;									\
				} else {												\
					_r_ = bixAddToUsedTags(bix, tagn, keepMemorizedTagsNullTerminatingFlag); \
					if (_r_ < 0) {_err_ = 1; break;	}					\
					bix->tag[tagn] = tagValue;							\
					bix->tagLen[tagn] = tagValueLength;					\
					tagValue[tagValueLength] = 0;						\
					command;											\
					if (keepMemorizedTagsNullTerminatingFlag == 0) tagValue[tagValueLength] = ';'; \
				}														\
				_s_ ++;													\
			}															\
			if (_err_) {												\
				commandonerror;											\
			}															\
		}																\
	}


////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum blockContentErrorCodes {
	BCIE_INTERNAL_ERROR_MISSING_BLOCK,
	BCIE_INTERNAL_ERROR_MISSING_CONTENT,
	BCIE_ERROR_INVALID_MSG,
	BCIE_ERROR_INCONSISTENT_MSG,
	BCIE_INTERNAL_ERROR_HASH_MISMATCH,
};

enum newMessageInsertFlags {
	NMF_NONE,
	NMF_INSERT_MESSAGE,
	NMF_PROCESS_MESSAGE,
	NMF_MAX,
};

enum networkConnectionStatuses {
	NCST_NONE,
	NCST_OPEN,
	NCST_CLOSED,
	NCST_MAX,
};

// We maintain this information about (current) time:
struct globalTimeInfo {
    long long int   usec;       // micro seconds since epoch
    time_t          sec;        // seconds since epoch
    int             hour;       // hours since epoch
    int             msecPart;   // milli seconds since last second (0 .. 999) 
    int             usecPart;   // micro seconds since last second (0 .. 999999)

    struct tm       gmttm;      // GMT time in form year, month, day, etc.
    struct tm       lcltm;      // Local time in form year, month, day, etc.
};


// timeline is a sorted list of timeLineEvents which shall be executed at given time
struct timeLineEvent {
    long long int           usecond;                // event time: micro seconds since epoch
    void                    (*event)(void *arg);    // function to be called at given usecond
    void                    *arg;                   // argument to be used when calling event()
    struct timeLineEvent    *next;                  // next event in the timeline
};

typedef void (*eventFunType)(void *arg);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct bixParsingContext {
	char 	*tag[BIX_TAG_MAX];
	int 	tagLen[BIX_TAG_MAX];
	int 	usedTags[BIX_TAG_MAX];
	int 	usedTagsi;

	struct bixParsingContext	*nextInContextStack;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum connectionTypes {
	CT_CORE_TO_CORE_SERVER,
	CT_SECONDARY_TO_CORE_IN_CORE_SERVER,
	CT_SECONDARY_TO_CORE_IN_SECONDARY_SERVER,
};

// the main data structure for blockchain server connections
struct bcaio {
	struct baio	b;

	int			connectionType;
	int 		fromNode;
	int 		toNode;
	int 		remoteNode;

	uint64_t	lastPingRequestUtime;
	int			errorCounter;
};

struct networkConnectionsStr {
	int 			status;
	int64_t			latency;
	int				guiEdgeIndex;
};

struct networkNodeStr {
	int				index;
	char			*name;
	int				status;
	int64_t			blockSeqn;
	int64_t			usedSpace;
	int64_t			totalAvailableSpace;
};

struct routingPathInfo {
	int64_t			latency;
	char			*path;
};

// No pointers inside this data structure, because they would break the hash
struct serverTopology {
	int32_t			index;
	char 			name[TMP_STRING_SIZE];	// unique name (used in gdprs identification)
	char 			guiname[TMP_STRING_SIZE];
	char 			host[TMP_STRING_SIZE];
	int32_t			bchainCoreServersListeningPort;
	int32_t			secondaryServersListeningPort;
	int32_t			guiListeningPort;
	int32_t			connections[ACTIVE_CONNECTIONS_MAX];
	int32_t			connectionsMax;
	char			loginPasswordKey[TMP_STRING_SIZE];
	char			gdprInfo[LONG_TMP_STRING_SIZE];
	uint32_t		ipaddress;
};

struct connectionsStruct {
	int			node;
	int			activeConnectionBaioMagic;
	int			passiveConnectionBaioMagic;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// blockchain

struct consensusBlockEngagementInfo {
	int64_t							blockSequenceNumber;
	int64_t							serverMsgSequenceNumber;
	int64_t							blockCreationTimeUsec;
	int								msgIdsLength;	// Hmm. this seems to be useless now
	char							*blockId;
	int64_t							firstAnnonceTimeUsec;
	int 							engagedNode;
	int								engagementRound;
	int64_t							expirationUsec;
	char							*blocksignature;
};


// consensus
struct consensusRoundsStr {
	int64_t		roundTimeout;
	int			quorumPercentage;
	int			blockContentRequired;
	int			blockValidationRequired;
	int			blockSignatureRequired;
};


struct bchainMessage {
	char		*msgId;

	// account of request issuer (he is always notified about updates)
	char		*account;

	// additional notification account
	char		*notifyAccount;
	char		*notifyAccountToken;

	// for case if the secondary server is watching the request status
	// this is not propagated through blockchain
	char		*secondaryServerInfo;

	char		*signature;
	uint8_t		signatureVerifiedFlag;
	int64_t 	timeStampUsec;
	char		*dataType;
	char		*data;
	int			dataLength;
};

struct bchainMessageList {
	struct bchainMessage		msg;

	int8_t						markedAsMissingBySomeNode;
	int8_t						markedAsInvalid;

	struct bchainMessageList	*nextInHashTable;
	struct bchainMessageList	*nextInBlockProposal;	// temporary for sorting messages by time before inserted into block
};

struct bchainBlock {
	int64_t						blockSequenceNumber;
	int64_t						blockCreationTimeUsec;
	char						*blockHash;
	struct bchainMessageList	*messages;
	struct bchainBlock			*previousBlock;
};

struct blockContentDataList {
	int64_t						blockSequenceNumber;
	int64_t						serverMsgSequenceNumber;
	int64_t						blockCreationTimeUsec;
	char						*blockId;
	int							msgIdsLength;
	char						*msgIds;			// string containing msgIds one after another
	int							blockContentLength;
	char						*blockContent;		// string containing whole messages
	char						*blockContentHash;
	char						*signatures;
	struct blockContentDataList *next;
};

struct bchainResynchronizationRequestList {
	int											reqBaioMagic;
	int											reqNode;
	int64_t										fromSeqn;
	int64_t										untilSeqn;
	struct bchainResynchronizationRequestList 	*next;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum secondaryToCoreServerRequestTypesEnum {
	STC_NONE,

	STC_NEW_ACCOUNT_NUMBER,
	STC_NEW_ACCOUNT,
	STC_NEW_TOKEN,
	STC_LOGIN,	
	STC_MORE_NOTIFICATIONS,	
	STC_GET_BALANCE,	
	STC_GET_ALL_BALANCES,	
	STC_GET_MSGTYPES,	

	STC_PENDING_REQ_LIST,	
	STC_PENDING_REQ_EXPLORE,	
	STC_PENDING_REQ_APPROVE,	
	STC_PENDING_REQ_DELETE,	

	STC_GET_ACCOUNTS,	
	STC_ACCOUNT_EXPLORE,	

	STC_EXCHANGE_GET_ORDER_BOOK,	

	STC_MAX,
};

struct secondaryToCoreServerPendingRequestsStr {
	int 								reqid;
	int									requestType;
	char								*formname;			// to which HTML5 form write results

	// "univesral usage"
	char								*coin;
	char								*account;

	// new coin creation
	char								*newcoin;

	// new account creation request
	int									gdprs;
	int									guiBaioMagic;
	struct jsVarDynamicString			*kycBix;
	char								*publicKey;
	char								*privateKey;
	EVP_PKEY 							*pkey;

	// db map
	void 								(*fun)(char *key, void *arg);
	void 								*arg;


	// next request
	struct secondaryToCoreServerPendingRequestsStr	*next;
};

//////////////////////////////////////////////////////////////////////////////////////////
// accounts
struct bAccountData;

struct bAccountBalance {
	char					*coin;
	double					balance;
	struct bAccountsTree	*myAccount;

	// red black tree stuff
	uint8_t					color;
	struct bAccountBalance 	*leftSubtree, *rightSubtree;
};

struct bAccountData {
	char					*publicKey;
	time_t					lastUsedTimeSec;

	uint8_t					created;	/* account record can be pre-created during block check, but not created if block is rejected */
	uint8_t					approved;	

	// TODO: remove this and makes mining better
	int64_t					lastMiningTime;
};

struct bAccountsTree {
	char					*account;
	struct bAccountBalance	*balances;

	// main data of the account
	struct bAccountData		d;

	uint8_t					allBalancesLoadedFlag;

	// red black tree stuff
	uint8_t					color;
	struct bAccountsTree 	*leftSubtree, *rightSubtree;
};

struct bchainContext {
	struct bAccountsTree 	*accs;
};

////////////////////////////////////////////////////////
// messages waiting for proof of work block

struct powcoinBlockPendingMessagesListStr {
	struct bchainMessage						msg;
	uint64_t									fromBlockSequenceNumber;
	struct powcoinBlockPendingMessagesListStr	*prev, *next;
};


////////////////////////////////////////////////////////
// block undo or finalize related definition

// TODO: rename all those from Undo to Finalize
struct blockMsgUndoForAccountOp {
	struct bAccountsTree		*acc;
	struct bAccountData			recoveryData;
};

struct blockMsgUndoForAccountBalanceOp {
	struct bAccountBalance		*bal;
	double						previousBalance;
};

struct blockMsgUndoForCoinMetaMsg {
	struct supportedMessageStr	*previousTable;
	int							previousTableMax;
};

struct blockMsgUndoForTokenOptionsMsg {
	char						**place;
	char						*options;
};

struct blockMsgUndoForPendingReq {
	char						*reqName;
	char						*reqData;
	int							reqDataLen;
};
struct blockMsgUndoForPowMessage {
	struct powcoinBlockPendingMessagesListStr 	*powcoinMessage;
	uint64_t									miningBlockSequenceNumber;
	char										*miningMsg;
};
struct blockMsgUndoForExchangeOp {
	struct bookedOrder 			*o;
	double						price;
	double						quantity;
	double						executedQuantity;
	struct halfOrderBook		*halfbook;
};

enum blockMsgUndoForNotificationFlags {
	NF_NONE,
	NF_ON_INSERTED,
	NF_ON_REJECTED,
	NF_ON_BOTH,
	NF_MAX,
};
struct blockMsgUndoForNotification  {
	int							notificationFlag;
	char						*account;
	char						*coin;
	char						*message;
};

enum blockFinalActionTypeEnum {
	BFA_NONE,
	BFA_ACCOUNT_MODIFIED,
	BFA_ACCOUNT_BALANCE_MODIFIED,
	BFA_TOKEN_TAB_MODIFIED,
	BFA_TOKEN_OPTIONS_MODIFIED,
	BFA_PENDING_REQUEST_INSERT,
	BFA_PENDING_REQUEST_DELETE,
	BFA_POW_MESSAGE_ADD,
	BFA_POW_MESSAGE_DELETE,
	BFA_ACCOUNT_NOTIFICATION_ON_SUCCESS,
	BFA_ACCOUNT_NOTIFICATION_ON_FAIL,
	BFA_EXCHANGE_ORDER_ADDED,
	BFA_EXCHANGE_ORDER_DELETED,
	BFA_EXCHANGE_ORDER_EXECUTED,
	BFA_MAX,
};

// Because during application/verification of messages we do not know if the block in whole 
// is accepted or rejected, we keep lists of undo actions or non-undoable final actions
// to be done for both cases. Moreover f=some actions (like notification messages) must be
// executed in the same order as messages stored in the block and some actions (like undoing changes)
// must be done in reversed order. 
struct bFinalActionList {
	int												action;					// enum blockFinalActionTypeEnum
	union	{
		struct blockMsgUndoForAccountOp 			accountModified;
		struct blockMsgUndoForAccountBalanceOp		accountBalanceModified;
		struct blockMsgUndoForCoinMetaMsg			tokenTableModified;
		struct blockMsgUndoForPendingReq			pendingRequest;
		struct blockMsgUndoForPowMessage			powMessage;
		struct blockMsgUndoForNotification			notification;
		struct blockMsgUndoForTokenOptionsMsg 		tokenOptions;
		struct blockMsgUndoForExchangeOp			exchangeOrderModified;
	} u;
	struct bFinalActionList							*next;
};

/////
// old way
struct blockMsgUndoList {
	// This is the function which shall be applied to this undo/pending data
	int						(*undoOrConfirmFunction)(void *arg, int applyChangesFlag);
	void					*undoOrConfirmArg;
	struct blockMsgUndoList	*next;
};

////////

struct msgOptionalValues {
	double			fees;
	char			*feesAccount;
	double			mining_award;
	int				mining_difficulty;
};

struct supportedMessageStr {
	char						*messageType;			// coin name
	char						*messageMetaType;		// coin type
	char						*masterAccount;			// coin creator BIX account
	char						*opt;					// coin optional parameters, just to be saved / recovered from file
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct kycData {
	char						firstName[TMP_STRING_SIZE];
	char						lastName[TMP_STRING_SIZE];
	char						email[TMP_STRING_SIZE];
	char						telephone[TMP_STRING_SIZE];
	char						country[TMP_STRING_SIZE];
	char						address[TMP_STRING_SIZE];
	char						masterAccount[TMP_STRING_SIZE];	// this is used as kyc for new currency base accounts
	char						idscanfname[TMP_STRING_SIZE];
	struct jsVarDynamicString   *idscan;
	char						podscanfname[TMP_STRING_SIZE];
	struct jsVarDynamicString   *podscan;
};

/////////////////////////////////////////
// there is additional tree saying where to send information / notification if something regarding an account happens
// those informations are propagated from layout 0 servers to layout 1 servers and from there to GUIs.

enum notificationConnectionType {
	NCT_NONE,
	NCT_BIX,
	NCT_GUI,
	NCT_MAX,
};

struct notificationInfo {
	int			baioMagic;
	int			connectionType;

	uint8_t		deleteFlag;			// used temporarily to mark baios flagged for deletion
	time_t		lastRefreshTime;
};

struct anotificationsTreeStr {
	char							*account;

	// send notifications to those connections (up to 8 connections)
	struct notificationInfo			*notifyTab;
	int								notifyTabi;
	int								notifyTabAllocatedSize;

	// red black tree stuff
	uint8_t							color;
	struct anotificationsTreeStr 	*leftSubtree, *rightSubtree;	

	// temporary "deletion" list
	struct anotificationsTreeStr 	*nextInDeleteList;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum nodeStates {
	NODE_STATE_NONE,
	NODE_STATE_UNKNOWN,
	NODE_STATE_NORMAL,
	NODE_STATE_RESYNC,
	NODE_STATE_UNREACHABLE,
	NODE_STATE_MAX,
};

struct nodeStatusPersistentData {
	int32_t		statusFileVersion;

	int32_t		index;						// my node id

	// counter for pending request
	int32_t		currentpPendingRequestNumber;

	// this is global counter which shall be the same for all servers
	int64_t 	currentBlockSequenceNumber;
	// hash of the previous blockchain block
	char 		previousBlockHash[MAX_HASH64_SIZE];

	// each server has a counter for emitted messages, so that we know which message is emitted sooner and which later
	int64_t 	currentBixSequenceNumber;
	// each server has a counter for messages he is emmiting
	int64_t 	currentMsgSequenceNumber;
	// each server has a counter for blocks he is emmiting
	// I think, this is only for case I propose two different nodes within the same round (same block sequence numbers)
	// can be reste after each block
	uint64_t 	currentServerBlockSequenceNumber;

	// each server has a counter for emitting new account numbers
	uint64_t 	currentNewAccountNumber;

	// space statistics for mining formula
	uint64_t 	currentUsedSpaceBytes;
	uint64_t 	currentTotalSpaceAvailableBytes;

	// keep information about last bytecoin mining time, so that we remove redundant mining messages
	// TODO: move this somewhere else
	uint64_t	lastMiningTime;

	// information about last block included into pow mining
	uint64_t	powMiningBlockSequenceNumber;

	// information about last nonce used in my messages
	uint64_t	lastNonce;

	uint64_t	magic;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct baioMagicList {
	int 					baioMagic;
	struct baioMagicList	*next;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum jsonNodeTypeEnum {
	JSON_NODE_TYPE_BOOL,
	JSON_NODE_TYPE_NUMBER,
	JSON_NODE_TYPE_STRING,
	JSON_NODE_TYPE_STRUCT,
	JSON_NODE_TYPE_ARRAY,
};

struct jsonnode {
	int	type;
	union {
		int						b;
		double 					n;
		char 					*s;
		struct jsonFieldList {
			union {
				int						index;
				char					*name;
			} u;
			struct jsonnode			*val;
			struct jsonFieldList	*next;
		} *fields;
	} u;
};

////////////////////////////////////////////////////////////////////////////////////
// exchange order book

#define BOOKED_ACCOUNT_NAME_MAX_LENGTH	32
#define BOOK_HASHED_ORDERS_TABLE_SIZE	(1<<20)
#define SYMBOL_BOOKS_HASHED_TABLE_SIZE	(1<<16)

enum bookedOrderSide {
	SIDE_NONE,
	SIDE_BUY,
	SIDE_SELL,
	SIDE_MAX,
};

enum bookedOrderType {
	ORDER_TYPE_NONE,
	ORDER_TYPE_MARKET,
	ORDER_TYPE_LIMIT,
	ORDER_TYPE_STOP,
	ORDER_TYPE_MAX,
};

struct bookedOrder {

	uint64_t				orderId;
	double					quantity;
	double					executedQuantity;

	char					ownerAccount[BOOKED_ACCOUNT_NAME_MAX_LENGTH];

	struct bookedPriceLevel	*myPriceLevel;

	// Orders with the same price are stored in a double linked circular list
	struct bookedOrder		*previousInPriceList;	// this shall be lastlty inserted order
	struct bookedOrder		*nextInPriceList;		// the next in time

    // All orders are in a hash table linked with simple list
    struct bookedOrder		*nextInHash;

};

struct bookedPriceLevel {
	double					price;
	struct bookedOrder 		*priceLevelList;
	struct halfOrderBook 	*myHalfBook;

	uint8_t					side;		// bid / ask (it determines price ordering)
	// red-black tree of orders by price
	uint8_t					rbcolor;
	struct bookedPriceLevel	*rbleft;
	struct bookedPriceLevel	*rbright;
	struct bookedPriceLevel	*rbparent;
};

struct halfOrderBook {
	uint8_t					side;
	struct bookedPriceLevel *rbtree;
	struct bookedOrder		*bestOrder;
	struct symbolOrderBook	*mySymbolBook;
};

struct symbolOrderBook {
	// symbol is identified as pair of base/quote tokens
	char					*baseToken;
	char					*quoteToken;
	struct halfOrderBook	bids;
	struct halfOrderBook	asks;

	struct symbolOrderBook 	*nextInHash;
};

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////


// common.c
extern int myNode;
extern int coreServersActive;
extern int coreServersMax;
extern int debuglevel;
extern char mynodeidstring[TMP_STRING_SIZE];
extern char *myNodePrivateKey;

extern int32_t nodeState;
extern int32_t nodeResyncFromNode;
extern int64_t nodeResyncUntilSeqNum;

extern time_t lastResynchronizationActionTime;
extern time_t lastAcceptedBlockTime;

extern struct nodeStatusPersistentData	nodeStatus;
extern struct nodeStatusPersistentData	*node;
extern FILE						*nodeFile;

extern struct consensusBlockEngagementInfo	*currentProposals;
extern struct consensusBlockEngagementInfo	*currentEngagement;
extern struct consensusBlockEngagementInfo	*engagementsPerServer[CONSENSUS_MAX_MEMORIZED_SEQUENCE_DIFF][SERVERS_MAX];
extern int									nodeEngagementOutOfOrderFlag[SERVERS_MAX];

extern struct bchainResynchronizationRequestList	*bchainResynchronizationRequest;

extern struct blockContentDataList 			*receivedBlockContents[CONSENSUS_MAX_MEMORIZED_SEQUENCE_DIFF];

extern struct bchainMessageList 	*pendingMessagesList;
extern struct blockContentDataList	*currentProposedBlock;

extern struct globalTimeInfo 	currentTime;
extern struct timeLineEvent     *timeLine;

extern struct serverTopology coreServers[SERVERS_MAX] ;
extern char coreServersHash[MAX_HASH64_SIZE];
extern struct connectionsStruct coreServersConnections[SERVERS_MAX];

// overall informations about existing connections in the network
extern struct networkConnectionsStr networkConnections[SERVERS_MAX][SERVERS_MAX];
extern struct routingPathInfo		myNodeRoutingPaths[SERVERS_MAX];
extern char							*myNodeBroadcastPath;

// overall information about other network nodes
extern struct networkNodeStr			networkNodes[SERVERS_MAX];

extern struct bchainContext 						currentBchainContext;
extern struct powcoinBlockPendingMessagesListStr 	*powcoinBlockPendingMessagesList;

extern int secondaryToCoreServerConnectionMagic;
extern struct baioMagicList	*guiConnections;

extern struct supportedMessageStr supportedMessageStrNull;
extern struct supportedMessageStr supportedMessagesAllTypeTemplate[];
extern struct supportedMessageStr *supportedMessagesTab;
extern int supportedMessagesTabMaxi;
extern char specialValueNoCoin[];

extern struct secondaryToCoreServerPendingRequestsStr *secondaryToCoreServerPendingRequestsList;

extern struct jsVar *guiEdgeStatus;
extern struct jsVar *guiEdgeLatency;
extern struct jsVar *guiMyRoutingPathPath;
extern struct jsVar *guiMyRoutingPathLatency;
extern struct jsVar *guiMyBroadcastPath;
extern struct jsVar *guiNodeStateName;
extern struct jsVar *guiNodeStateStatus;
extern struct jsVar *guiNodeStateSeqn;
extern struct jsVar *guiNodeStateUsedSpace;
extern struct jsVar *guiNodeStateAvailableSpace;

extern struct jsVar	*guiSupportedMsgTypesVector;
extern struct jsVar	*guiSupportedTokenTypesVector;
extern struct jsVar	*guiSupportedTokenTransferFee;
extern struct jsVar	*guiPendingRequestsVector;
extern struct jsVar	*guiCoreServersGdpr;
extern struct jsVar	*guiMinerCurrentlyMinedBlocksVector;
extern struct jsVar	*guiSupportedTokenDifficultyVector;

extern EVP_MD_CTX *openSslMdctx;

extern char *orderTypeNames[];
extern char *sideNames[];

char *staticStringRingGetTemporaryStringPtr_st() ;
char *staticStringRingGetLongLongTemporaryStringPtr_st() ;
int enumFromName(char **names, char *nn);
char *strDuplicate(char *s) ;
char *memDuplicate(char *s, int slen) ;
int parseInt(char **pp) ;
int64_t parseLongLong(char **pp) ;
int parseHexi(char **ss) ;
int64_t parseHexll(char **ss) ;
int hexStringToBin(char *dst, int dstsize, char *src) ;
int hexStringFromBin(char *dst, char *src, int srcsize) ;
char *currentLocalTime_st() ;
void setCurrentTime() ;
char *sprintSecTime_st(long long int utime) ;
char *sprintUsecTime_st(long long int utime) ;
char *bchainPrintPrefix_st();
void coreServerSupportedMessageCreateCopyWithAdditionalMsgWithType(char *msgType, char *msgMetaType, char *masterAccount, char *msgopt) ;
void coreServerSupportedMessageAddAdditionalMsgWithType(char *msgType, char *msgMetaType, char *masterAccount, char *msgopt) ;
void coreServerSupportedMessageCreateCopyWithAdditionalMsgType(struct supportedMessageStr *smsg) ;
void guiCoreServersVectorUpdate() ;
void guiSupportedMsgTypesVectorUpdate() ;
int coreServerSupportedMessageFindIndex(char *messageType) ;
struct supportedMessageStr *coreServerSupportedMessageFind(char *messageType) ;
int coreServerSupportedMessageIsSupported(char *msgtype) ;
void coreServerSupportedMessagesTabCreateCopy() ;
void coreServerSupportedMessageInit() ;
void coreServerSupportedMessageSetToDefaultValues() ;
void coreServerSupportedMessageTableFree(struct supportedMessageStr *tt, int ttsize) ;
void coreServerSupportedMessageTableSave() ;
void coreServerSupportedMessageTableLoad() ;
struct supportedMessageStr *supportedMessagesFindMetaTypeTemplate(char *messageMetaType) ;
char *messageDataTypeToAccountCoin(char *datatype) ;
void timeLineInsertEvent(long long int usec, void (*event)(void *arg), void *arg) ;
struct timeLineEvent *timeLineFindEventAtUnknownTime(void (*event)(void *arg), void *arg) ;
void timeLineRemoveEvent(long long int usec, void (*event)(void *arg), void *arg) ;
int timeLineRemoveEventAtUnknownTime(void (*event)(void *arg), void *arg) ;
int timeLineRemoveEventAtUnknownTimeAndArg(void (*event)(void *arg)) ;
int timeLineRescheduleUniqueEvent(long long int usec, void (*event)(void *arg), void *arg) ;
int timeLineInsertUniqEventIfNotYetInserted(long long int usec, void (*event)(void *arg), void *arg) ;
void timeLineTimeToNextEvent(struct timeval *tv, int maxseconds) ;
void timeLineExecuteScheduledEvents() ;
void timeLineDump() ;
void mkdirRecursivelyCreateDirectoryForFile(const char *fname) ;
void mkdirRecursivelyCreateDirectory(const char *dirname) ;
void regularFflush(void *f) ;
void bchainSetBufferSizes(struct baio *bb) ;
void openSslCryptoInit() ;
char *fileLoad(int useCppFlag, char *fmt, ...) ;
int fileSaveWithLength(char *content, int len, int createDirectoryFlag, char *fmt, ...) ;
int fileSave(char *content, char *fmt, ...) ;
void mainInitialization() ;
void mainCoreServersTopologyInit() ;
int mainServerOnWwwGetRequest(struct jsVaraio *jj, char *uri) ;
int mainServerOnWwwPostRequest(struct jsVaraio *jj, char *uri) ;
void networkConnectionOnStatusChanged(int fromNode, int toNode) ;
void networkConnectionStatusChange(int fromNode, int toNode, int newStatus) ;
int mapDirectoryFiles(char *dirname, void (*fun)(char *fname, void *arg), void *arg) ;
void zombieHandler(int s) ;
void msSleep(int millis) ;
int guiprintf(char *fmt, ...) ;
int guisendEval(char *fmt, ...) ;
void guiUpdateNodeStatus(int node) ;
void changeNodeStatus(int newStatus) ;
void secondaryServerConnectToCoreNode(void *nn) ;
void guiConnectionsInsertBaioMagic(int baioMagic) ;
int checkFileAndTokenName(char *name, int allowSlashFlag) ;
char *databaseKeyFromString_st(char *ss) ;
int databasePut(char *database, char *key, char *value, int valueLength) ;
struct jsVarDynamicString *databaseGet(char *database, char *key) ;
int databaseDelete(char *database, char *key) ;
int databaseMapOnKeys(char *database, void (*fun)(char *key, void *arg), void *arg) ;
void coreGuiPendingRequestsRegularUpdateList(void *d) ;
void pendingRequestsAddToBixMsg(struct jsVarDynamicString *ss, char *database, char *prefix) ;
int pendingRequestApprove(char *request) ;
void pendingRequestShowNewAccountRequest(char *reqs, int reqsize, struct jsVaraio *jj) ;
void pendingRequestExplore(char *request, struct jsVaraio *jj) ;
void pendingRequestDelete(char *request) ;
void listAccountsToBixMsg(struct jsVarDynamicString *ss, char *coin) ;
void anotificationShowNotificationV(char *account, int connectionType, char *secondaryServerInfo, char *fmt, va_list arg_ptr) ;
void anotificationShowNotification(char *account, int connectionType, char *fmt, ...) ;
void anotificationShowMsgNotification(struct bchainMessage *msg, char *fmt, ...) ;
void anotificationSaveNotificationV(char *account, char *token, char *secondaryServerInfo, char *fmt, va_list arg_ptr) ;
void anotificationSaveNotification(char *account, char *accountToken, char *fmt, ...) ;
void anotificationSaveMsgNotification(struct bchainMessage *msg, char *fmt, ...) ;
void anotificationAddOrRefresh(char *account, int baioMagic, int connectionType) ;
void anotificationAppendSavedNotifications(struct jsVarDynamicString *ss, char *token, char *account, int historyLevel) ;
void mainExplorerExploreBlock(struct jsVaraio *jj, int64_t seq) ;
void mainExplorerExploreMessage(struct jsVaraio *jj, char *msgid) ;

// bix.c
int bixSendMsgToConnection(struct bcaio *cc, char *dstpath, char *fmt, ...) ;
void bixSendLoginToConnection(struct bcaio *cc) ;
void bixSendSecondaryServerLoginToConnection(struct bcaio *cc) ;
void bixSendPingToConnection(struct bcaio *cc) ;
void bixSendImmediatePingAndScheduleNext(struct bcaio *cc) ;
void bixSendResynchronizationRequestToConnection(struct bcaio *cc) ;
void bixSendBlockResynchronizationAnswerToConnection(struct bcaio *cc, struct blockContentDataList *bb) ;
int bixSendNewMessageInsertOrProcessRequest_s2c(char *dstpath, int insertProcessFlag, int baioMagic, int secondaryServerInfo, char *msg, int msgLen, char *messageDataType, char *ownerAccount, char *signature) ;
int bixSendMessageStatusUpdated_c2s(int baioMagic, char *secServerInfo, char *account, char *newstatus) ;
// int bixSendNewAccountRequest_s2c(int baioMagic, int guiBaioMagic, int reqid, char *publicKey) ;
int bixAddToUsedTags(struct bixParsingContext *bix, int tag, int makeTagsNullTerminatingFlag) ;
void bixCleanBixTags(struct bixParsingContext *bix) ;
void bixMakeTagsNullTerminating(struct bixParsingContext *bix) ;
void bixMakeTagsSemicolonTerminating(struct bixParsingContext *bix) ;
void bixFillParsedBchainMessage(struct bixParsingContext *bix, struct bchainMessage *msg) ;
struct bixParsingContext *bixParsingContextGet() ;
void bixParsingContextFree(struct bixParsingContext **bixa) ;
int bixParseString(struct bixParsingContext *bix, char *message, int messageLength) ;
void bixParseAndProcessAvailableMessages(struct bcaio *cc) ;
void bixBroadcastCurrentBlockEngagementInfo() ;
void bixBroadcastNewMessage(struct bchainMessageList *mm) ;
void bixBroadcastCurrentProposedBlockContent() ;
void bixBroadcastNetworkConnectionStatusMessage(int fromNode, int toNode) ;
void bixBroadcastMyNodeStatusMessage() ;
int bchainBlockContentAppendCurrentBlockHeader(struct jsVarDynamicString *bb) ;
int bchainBlockContentAppendSingleMessage(struct jsVarDynamicString *bb, struct bchainMessage *msg) ;
struct jsVarDynamicString *kycToBix(struct kycData *kk) ;
struct kycData *bixToKyc(char *msg, int msglen) ;
void kycFree(struct kycData **kyc) ;

// consensus.c
extern struct consensusRoundsStr consensusRounds[CONSENSUS_ENGAGEMENT_ROUND_MAX];

void consensusReconsiderMyEngagement() ;
struct blockContentDataList *consensusBlockContentCreateCopy(struct blockContentDataList *bc) ;
void bchainBlockContentFree(struct blockContentDataList **bc) ;
int consensusIsThereSomeFutureBlockProposal() ;
void consensusNewEngagementReceived(struct consensusBlockEngagementInfo *nn) ;
void consensusBlockContentReceived(struct blockContentDataList *bc) ;
struct blockContentDataList *consensusBlockContentFind(int blockSequenceNumber, char *hash) ;
int consensusBlockSignatureValidation(struct blockContentDataList *memb, int signode, char *signature) ;
void consensusMaybeProlongCurrentEngagement(int forceBroadcastEngagementFlag) ;
void consensusOnCurrentBlockProposalIsInconsistent() ;
void consensusOnBlockAccepted() ;

// blockchain.c
extern int msgNumberOfPendingMessages;

int bchainMsgIdCreate(char *msgId, int64_t msgSequenceNumber, int nodeNumber, int nonce) ;
void bchainMessageCopy(struct bchainMessage *dst, struct bchainMessage *src) ;
struct bchainMessageList *bchainEnlistPendingMessage(struct bchainMessage *msg) ;
void bchainFillMsgStructure(struct bchainMessage *msg, char *msgId, char *ownerAccount, char *notifyAccount, char *notifyAccountToken, long long timestamp, char *messageDataType, int datalen, char *data, char *signature, char *secondaryServerInfo) ;
struct bchainMessageList *bchainCreateNewMessage(char *data, int datalen, char *messageDataType, char *ownerAccount, char *notifyAccountToken, char *notifyAccount, char *signature, char *secondaryServerInfo) ;
void bchainFreeMessageContent(struct bchainMessage *msg) ;
void bchainNoteMissingMessage(char *msgId) ;
void bchainProposeNewBlock() ;
void bchainGetBlockContentHash64(struct blockContentDataList *bb, char hash64[MAX_HASH64_SIZE]) ;
char *bAccountDirectory_st(char *account) ;
char *bAccountNotesDirectory_st(char *coin, char *account) ;
int bchainVerifyBlockContent(struct blockContentDataList *data) ;
void bchainOnNewBlockAccepted();
void bchainOnResynchronizationBlockAccepted(struct blockContentDataList *data) ;
void bchainWriteBlockToFile(struct blockContentDataList *data) ;
void bchainWriteBlockSignaturesToFile(struct blockContentDataList *data) ;
void bchainOpenAndLoadGlobalStatusFile() ;
void bchainWriteGlobalStatus() ;
void bchainStartResynchronization(void *d) ;
void bchainServeResynchronizationRequests() ;
void bchainRegularStatusWrite(void *d) ;
void ssBchainRegularSendSynchronizationRequests(void *d) ;
void ssUpdateTokenList(void *d) ;
void ssRegularUpdateTokenList(void *d) ;
void bchainInitializeMessageTypes() ;
struct jsVarDynamicString *bchainImmediatelyProcessSingleMessage_st(struct bchainMessage *msg, char *messageDataType) ;
int bchainApplyBlockMessagesInBlockContent(struct blockContentDataList *data) ;
int bchainVerifyBlockMessagesInBlockContent(struct blockContentDataList *data) ;
struct blockContentDataList *bchainCreateBlockContentLoadedFromFile(int64_t blockSequenceNumber);
int msgSignatureCreate(char *msg, int msglen, char **sig, size_t *slen, EVP_PKEY* key) ;
int msgSignatureVerify(char *msg, int msglen, char *sig, int slen, EVP_PKEY* key) ;
EVP_PKEY *readPrivateKeyFromFile(char *fmt, ...) ;
EVP_PKEY *readPublicKeyFromFile(char *fmt, ...) ;
EVP_PKEY *generateNewEllipticKey() ;
int generatePrivateAndPublicKeysToFiles(char *publicKeyFile, char *privateKeyFile) ;
int encryptWithAesCtr256(unsigned char *plaintext, int plaintext_len, unsigned char *key, unsigned char *iv, unsigned char *ciphertext) ;
int decryptWithAesCtr256(unsigned char *ciphertext, int ciphertext_len, unsigned char *key, unsigned char *iv, unsigned char *plaintext) ;
EVP_PKEY *readPrivateKeyFromString(char *s) ;
EVP_PKEY *readPublicKeyFromString(char *s) ;
char *getPublicKeyAsString(EVP_PKEY *pkey) ;
char *getPrivateKeyAsString(EVP_PKEY *pkey) ;
char *signatureCreateBase64(char *msgtosign, int msgToSignLen, char *privateKey) ;
int secondaryServerSignAndSendMessageToCoreServer(char *dstpath, int insertProcessFlag, struct jsVaraio *jj, int secondaryServerInfo, char *msgtosign, int msgToSignLen, char *messageDataType, char *ownerAccount, char *privateKey) ;
int verifyMessageSignature(char *msg, int msgsize, char *coin, char *account, char *signature, int signatureLen, int quietFlag) ;
void setSignatureVerificationFlagInMessage(struct bchainMessage *msg) ;

// coins.c
char *bchainPrettyPrintMetaMsgContent(struct bixParsingContext *bix) ;
char *bchainPrettyPrintUserFileMsgContent(struct bixParsingContext *bix) ;
char *bchainPrettyPrintTokenMsgContent(struct bixParsingContext *bix) ;
char *bchainPrettyPrintMsgContent_st(struct bchainMessage *msg) ;
char *coinBaseAccount_st(char *coin) ;
int coinGdprs(char *coin) ;
char *coinType_st(char *coin) ;
struct bchainMessageList *submitAccountApprovedMessage(char *coin, char *account, char *publicKey, int publicKeyLength, char *secServerInfo, char *kychash, int kychashLength);
struct jsVarDynamicString *immediatelyExecuteCoinMessage(struct bchainMessage *msg) ;
char *coinBaseAccount_st(char *coin) ;
char *coinType_st(char *coin) ;
int onAccountApproved(char *reqMsg, int reqMsgLen) ;
int onPendingRequestDelete(char *request) ;
int onPendingGdprRequestDelete(char *request) ;
int applyCoinBlockchainMessage(struct bchainMessage *msg, struct blockContentDataList *fromBlock, struct bFinalActionList **finalActionList) ;
int applyPowcoinBlockchainMessage(struct bchainMessage *msg, struct blockContentDataList *fromBlock, struct bFinalActionList **finalActionList) ;
void powcoinParseReceivedBlocks() ;
long long powcoinMine() ;
void powcoinOnUserMinedBlock(struct jsVaraio *jj, char *coin, char *account, struct jsVarDynamicString *bb) ;
int coinUndoDataFree(struct blockMsgUndoList *undo) ;
int applyExchangeBlockchainMessage(struct bchainMessage *msg, struct blockContentDataList *fromBlock, struct bFinalActionList **finalActionList) ;
int applyUserfileBlockchainMessage(struct bchainMessage *msg, struct blockContentDataList *fromBlock, struct bFinalActionList **finalActionList) ;
void blockFinalizeNoteAccountBalanceModification(struct bFinalActionList **list, struct bAccountBalance *bal) ;
void blockFinalizeNoteExchangeOrderModified(int opcode, struct bFinalActionList **list, struct bookedOrder *o) ;
struct jsVarDynamicString *immediatelyExecuteMetamsgMessage(struct bchainMessage *msg) ;
struct jsVarDynamicString *immediatelyExecuteExchangeMessage(struct bchainMessage *msg) ;
int applyMetamsgBlockchainMessage(struct bchainMessage *msg, struct blockContentDataList *fromBlock, struct bFinalActionList **finalActionList) ;
int metacoinUndoDataFree(struct blockMsgUndoList *undo) ;
char *getAccountName_st(int64_t accnumber, int nodeNumber) ;
int isCoreServerAccount(char *account) ;
char *generateNewAccountName_st() ;
void walletNewTokenCreation(struct jsVaraio *jj, char *formname, char *newcoin, char *cointype, char *account, char *privateKey, int gdprs);
int walletNewTokenCreationStateUpdated(struct secondaryToCoreServerPendingRequestsStr *pp, char *account, char *data, int datalen) ;
void walletTokenOptions(struct jsVaraio *jj, char *newcoin, char *cointype, char *opt, char *account, char *privateKey) ;
void walletGotNewAccountNumber(struct secondaryToCoreServerPendingRequestsStr *pp, char *account, char *msg, int msglen) ;
void walletNewAccountCreated(struct secondaryToCoreServerPendingRequestsStr *pp, char *account, char *msg);
void walletShowAnswerInMessages(struct secondaryToCoreServerPendingRequestsStr *pp, char *account, char *msg, int msglen) ;
void walletShowAnswerInNotifications(struct secondaryToCoreServerPendingRequestsStr *pp, char *account, char *msg, int msglen) ;
void walletShowMoreNotifications(struct secondaryToCoreServerPendingRequestsStr *pp, char *account, char *msg, int msglen) ;
void walletGotBalance(struct secondaryToCoreServerPendingRequestsStr *pp, char *account, char *msg) ;
void walletGotAllBalances(struct secondaryToCoreServerPendingRequestsStr *pp, char *account, char *msg, int msgLen) ;
void walletGotOrdebook(struct secondaryToCoreServerPendingRequestsStr *pp, char *account, char *msg, int msgLen) ;
void bytecoinScheduleNextMining(void *dummy) ;
void bytecoinMineNewCoins(void *d) ;
void bytecoinInitialize() ;
struct bAccountBalance *bAccountBalanceLoadOrFind(char *coin, char *account, struct bchainContext *context) ;
struct bAccountsTree *bAccountLoadOrFind(char *account, struct bchainContext *context) ;
void bAccountRecoveryListApplyAndSaveChanges(struct blockMsgUndoForAccountOp *bb) ;
void bAccountRecoveryListRecover(struct blockMsgUndoForAccountOp *bb) ;
void bAccountBalanceRecoveryListApplyAndSaveChanges(struct blockMsgUndoForAccountBalanceOp *bb) ;
void bAccountBalanceRecoveryListRecover(struct blockMsgUndoForAccountBalanceOp *bb) ;

struct secondaryToCoreServerPendingRequestsStr **secondaryToCorePendingRequestFind(int secondaryServerInfo) ;
struct secondaryToCoreServerPendingRequestsStr *secondaryToCorePendingRequestFindAndUnlist(int secondaryServerInfo) ;
void secondaryToCorePendingRequestFree(struct secondaryToCoreServerPendingRequestsStr *pp) ;
struct secondaryToCoreServerPendingRequestsStr *secondaryToCorePendingRequestSimpleCreate(struct jsVaraio *jj, char *formname, int requestType) ;
char *createTokenOptionsMsgText_st(char *newcoin, char *cointype, char *opt) ;
void ssSendUpdateTokenListRequest(void *jj, char *formname) ;
void ssSendRegularUpdateCoinListRequest(void *d) ;
void ssGotSupportedMsgTypes(struct secondaryToCoreServerPendingRequestsStr *pp, char *account, char *msg, int msglen) ;
void ssGotPendingReqList(struct secondaryToCoreServerPendingRequestsStr *pp, char *account, char *msg, int msglen) ;
void ssGotPendingReqExplore(struct secondaryToCoreServerPendingRequestsStr *pp, char *account, char *msg, int msglen) ;
struct jsVarDynamicString *getNewAccountMessageBody(char *account, char *publicKey, struct jsVarDynamicString *kycBix) ;
void walletNewAccountCreation(struct jsVaraio *jj, char *formname, char *coin, struct kycData *kk, int gdprs) ;
void walletSendCoins(struct jsVaraio *jj, char *coin, double amount, char *fromAccount, char *toAccount, char *privateKey) ;
void walletLogin(struct jsVaraio *jj, char *formname, char *coin, char *account, char *privateKey) ;
void walletGetBalance(struct jsVaraio *jj, char *formname, char *coin, char *account, char *privateKey) ;
void walletGetAllBalances(struct jsVaraio *jj, char *formname, char *account, char *privateKey) ;
void walletInsertUserFile(struct jsVaraio *jj, char *account, char *filename, char *msg, int msgsize, char *encryptwithkey, char *privateKey) ;
void walletDownloadUserFile(struct jsVaraio *jj, uint64_t blocknum, char *msgid, char *encryptkey) ;
void walletPendingRequestListRequests(struct jsVaraio *jj, char *formname, char *coin, char *account, char *privateKey) ;
void walletListAccounts(struct jsVaraio *jj, char *formname, char *coin, char *account, char *privateKey) ;
void walletPendingRequestExplore(struct jsVaraio *jj, char *coin, char *account, char *request, char *requestType, char *privateKey) ;
void walletPendingRequestApprove(struct jsVaraio *jj, char *coin, char *account, char *request, char *requestType, char *privateKey) ;
void walletPendingRequestDelete(struct jsVaraio *jj, char *coin, char *account, char *request, char *requestType, char *privateKey) ;
void walletExchangeNewOrder(struct jsVaraio *jj, char *formname, char *account, char *baseToken, char *quoteToken, char *side, char *orderType, double quantity, double limitPrice, double stopPrice, char *privateKey) ;
void walletExchangeUpdateOrderbook(struct jsVaraio *jj, char *formname, char *account, char *baseToken, char *quoteToken, char *privateKey) ;


// json.c
char *jsonDestructibleParseString(char *b, struct jsonnode **res) ;
void jsonPrint(struct jsonnode *nn, FILE *ff) ;
struct jsonnode *jsonFindField(struct jsonnode	*nn, char *name) ;
double jsonFindFieldDouble(struct jsonnode	*nn, char *name, double defaultValue) ;
char *jsonFindFieldString(struct jsonnode	*nn, char *name, char *defaultValue) ;
double jsonNextFieldDouble(struct jsonFieldList **aa, double defaultValue) ;
char *jsonNextFieldString(struct jsonFieldList **aa, char *defaultValue) ;


// jquery.c
extern unsigned char jquery_js[];
extern unsigned int jquery_js_len;

// exchange.c
extern struct bookedOrder *allOrdersHashTab[BOOK_HASHED_ORDERS_TABLE_SIZE];
int exchangeAppendOrderbookAsString(struct jsVarDynamicString *ss, char *baseToken, char *quoteToken) ;
struct bookedOrder *exchangeGetOrder(uint64_t orderId) ;
int exchangeAddOrder(char *ownerAccount, char *baseToken, char *quoteToken, int side, double price, double quantity, double executedQuantity, struct bFinalActionList **finalActionList) ;
void exchangeDeleteBookedOrder(struct bookedOrder *memb, struct bFinalActionList **finalActionList) ;
int exchangeDeleteOrder(int orderid, char *ownerAccount, char *baseToken, char *quoteToken, int side, struct bFinalActionList **finalActionList) ;
struct bookedOrder *exchangeReplaceOrder(uint64_t orderId, char *ownerAccount, char *baseToken, char *quoteToken, int side, double price, double quantity) ;


// maincoreserver.c

// mainwalletserver.c


#endif
