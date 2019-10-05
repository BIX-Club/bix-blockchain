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

#undef  SGLIB___RBTREE_SET_PARENT
#define SGLIB___RBTREE_SET_PARENT(x, v) {(x)->rbparent = (v);}

typedef struct bookedPriceLevel bookedPriceLevel;
typedef struct bookedOrder 		bookedOrder;
typedef struct symbolOrderBook	symbolOrderBook;

static int sbookSymbolCmp(struct symbolOrderBook *b1, struct symbolOrderBook *b2) {
	int		r;
	r = strcmp(b1->baseToken, b2->baseToken);
	if (r != 0) return(r);
	r = strcmp(b1->quoteToken, b2->quoteToken);
	if (r != 0) return(r);
	return(0);
}
static unsigned sbookHashFun(struct symbolOrderBook *b) {
	unsigned res1, res2;
	SGLIB_HASH_FUN_STR(b->baseToken, res1);
	SGLIB_HASH_FUN_STR(b->quoteToken, res2);
	return(res1+res2);
}

static int bookedOrderPriceCmp(struct bookedPriceLevel *o1, struct bookedPriceLevel *o2) {
	assert(o1->side == SIDE_BUY || o1->side == SIDE_SELL);
	assert(o2->side == SIDE_BUY || o2->side == SIDE_SELL);
	// This is importent o1 decides the ordering
	if (o1->side == SIDE_BUY) {
		if (o1->price < o2->price) return(1);
		if (o1->price > o2->price) return(-1);
	} else if (o1->side == SIDE_SELL) {
		if (o1->price < o2->price) return(-1);
		if (o1->price > o2->price) return(1);
	}
	return(0);
}

static int bookedOrderIdCmp(struct bookedOrder *o1, struct bookedOrder *o2) {
	if (o1->orderId < o2->orderId) return(-1);
	if (o1->orderId > o2->orderId) return(1);
	return(0);
}
static unsigned bookedOrderHashFun(struct bookedOrder *o) {
	unsigned res;
	SGLIB_HASH_FUN(&o->orderId, sizeof(o->orderId), res);
	return(res);
}

// rbtree ordered by prices
SGLIB_DEFINE_RBTREE_PROTOTYPES(bookedPriceLevel, rbleft, rbright, rbcolor, bookedOrderPriceCmp);
SGLIB_DEFINE_RBTREE_FUNCTIONS(bookedPriceLevel, rbleft, rbright, rbcolor, bookedOrderPriceCmp);

// lists for hash table by id
SGLIB_DEFINE_LIST_PROTOTYPES(bookedOrder, bookedOrderIdCmp, nextInHash);
SGLIB_DEFINE_LIST_FUNCTIONS(bookedOrder, bookedOrderIdCmp, nextInHash);
// hashed table of lists ordered by id
SGLIB_DEFINE_HASHED_CONTAINER_PROTOTYPES(bookedOrder, BOOK_HASHED_ORDERS_TABLE_SIZE, bookedOrderHashFun);
SGLIB_DEFINE_HASHED_CONTAINER_FUNCTIONS(bookedOrder, BOOK_HASHED_ORDERS_TABLE_SIZE, bookedOrderHashFun);

// lists for hash table by id
SGLIB_DEFINE_LIST_PROTOTYPES(symbolOrderBook, sbookSymbolCmp, nextInHash);
SGLIB_DEFINE_LIST_FUNCTIONS(symbolOrderBook, sbookSymbolCmp, nextInHash);
// hashed table of lists ordered by id
SGLIB_DEFINE_HASHED_CONTAINER_PROTOTYPES(symbolOrderBook, SYMBOL_BOOKS_HASHED_TABLE_SIZE, sbookHashFun);
SGLIB_DEFINE_HASHED_CONTAINER_FUNCTIONS(symbolOrderBook, SYMBOL_BOOKS_HASHED_TABLE_SIZE, sbookHashFun);

//////////////////////////////////////////////////////////////////////////////////////////

struct bookedOrder *allOrdersHashTab[BOOK_HASHED_ORDERS_TABLE_SIZE];
struct symbolOrderBook	*allOrderBooks[SYMBOL_BOOKS_HASHED_TABLE_SIZE];

//////////////////////////////////////////////////////////////////////////////////////////

// find the next order in price ordering
static struct bookedOrder *exchangeFindNextOrder(struct bookedOrder *o) {
	struct bookedPriceLevel *p;

	p = o->myPriceLevel;

	// if there is another order in the price list, return it
	if (o->nextInPriceList != NULL) return(o->nextInPriceList);

	// otherwise find the next in rbtree
	if (p->rbright != NULL) {
		p = p->rbright;
		while (p->rbleft != NULL) p = p->rbleft;
		return(p->priceLevelList);
	}
	while (p->rbparent != NULL && p->rbparent->rbright == p) p = p->rbparent;
	if (p->rbparent == NULL) return(NULL);
	p = p->rbparent;
	return(p->priceLevelList);
}

static char *exchangeSprintBookedOrder_st(struct bookedOrder *o) {
	char	*res;
	int		i,n;

	res = staticStringRingGetTemporaryStringPtr_st();
	i = 0;
	n = TMP_STRING_SIZE;
	if (o == NULL || o->myPriceLevel == NULL || o->myPriceLevel->myHalfBook == NULL || o->myPriceLevel->myHalfBook->mySymbolBook == NULL) {
		PRINTF0("Internal Error: exchangeDumpBookedOrder: incorrect booked order\n");
		return("???");
	}
	i += snprintf(res+i, n-i, "%s: %"PRIu64": ", o->ownerAccount, o->orderId);
	if (o->myPriceLevel->side == SIDE_BUY) {
		i += snprintf(res+i, n-i, "BUY ");
	} else if (o->myPriceLevel->side == SIDE_SELL) {
		i += snprintf(res+i, n-i, "SELL ");
	} else {
		PRINTF0("Internal Error: exchangeDumpBookedOrder: wrong side\n");
		return("???");		
	}
	i += snprintf(res+i, n-i, "(%f -%f) * %s/%s @ %f",
				  o->quantity,
				  o->executedQuantity,
				  o->myPriceLevel->myHalfBook->mySymbolBook->baseToken,
				  o->myPriceLevel->myHalfBook->mySymbolBook->quoteToken,
				  o->myPriceLevel->price
		);
	res[n-1] = 0;
	return(res);
}

static void exchangeDumpBook(struct symbolOrderBook *b) {
	struct bookedOrder 	*o;
	int					i;
	PRINTF("[exchangeDumpBook]: %s / %s \n", b->baseToken, b->quoteToken);
	PRINTF0("bids: \n");
	for(i=0,o=b->bids.bestOrder; o!=NULL; o=exchangeFindNextOrder(o),i++) {
		PRINTF("%d: %s\n", i, exchangeSprintBookedOrder_st(o));
	}
	PRINTF0("asks: \n");
	for(i=0,o=b->asks.bestOrder; o!=NULL; o=exchangeFindNextOrder(o),i++) {
		PRINTF("%d: %s\n", i, exchangeSprintBookedOrder_st(o));
	}
	PRINTF0("[exchangeDumpBook]: end of dump\n\n");
}

static int exchangeAppendOrderString(struct bookedOrder *o, struct jsVarDynamicString *ss) {

	if (o == NULL || o->myPriceLevel == NULL || o->myPriceLevel->myHalfBook == NULL || o->myPriceLevel->myHalfBook->mySymbolBook == NULL) {
		return(-1);
	}
	jsVarDstrAppendPrintf(ss, "acc=%s;oid=%"PRIu64";sid=%s;qty=%a;exq=%a;prc=%a;mrk=eoo;", 
						  o->ownerAccount, 
						  o->orderId, 
						  sideNames[o->myPriceLevel->side],
						  o->quantity,
						  o->executedQuantity,
						  o->myPriceLevel->price
		);
	return(0);
}

int exchangeAppendOrderbookAsString(struct jsVarDynamicString *ss, char *baseToken, char *quoteToken) {
	struct symbolOrderBook 	*sbook, sss;
	struct bookedOrder		*o;

	// PRINTF("Creating book snapshot %s %s\n", baseToken, quoteToken);

	jsVarDstrAppendPrintf(ss, "hdr=orderbook;btk=%s;qtk=%s;mrk=eoh;", baseToken, quoteToken);

	sss.baseToken = baseToken;
	sss.quoteToken = quoteToken;
	sbook = sglib_hashed_symbolOrderBook_find_member(allOrderBooks, &sss);
	if (sbook == NULL) return(-1);

	jsVarDstrAppendPrintf(ss, "mrk=bids;");
	for(o=sbook->bids.bestOrder; o!=NULL; o=exchangeFindNextOrder(o)) {
		exchangeAppendOrderString(o, ss);
	}
	jsVarDstrAppendPrintf(ss, "mrk=eobids;");
	jsVarDstrAppendPrintf(ss, "mrk=asks;");
	PRINTF0("asks: \n");
	for(o=sbook->asks.bestOrder; o!=NULL; o=exchangeFindNextOrder(o)) {
		exchangeAppendOrderString(o, ss);
	}
	jsVarDstrAppendPrintf(ss, "mrk=eoasks;");
	return(0);
}



///////////////////////////////


void exchangeDeleteBookedOrder(struct bookedOrder *memb, struct bFinalActionList **finalActionList) {
	struct halfOrderBook *halfbook;

	assert(memb != NULL);
	assert(memb->myPriceLevel != NULL);
	assert(memb->myPriceLevel->myHalfBook != NULL);

	if (finalActionList != NULL) blockFinalizeNoteExchangeOrderModified(BFA_EXCHANGE_ORDER_DELETED, finalActionList, memb);

	halfbook = memb->myPriceLevel->myHalfBook;
	if (halfbook->bestOrder == memb) {
		// find next best order
		halfbook->bestOrder = exchangeFindNextOrder(memb);
	}
	// delete the order from price tree
	assert(memb->myPriceLevel != NULL);
	assert(memb->myPriceLevel->priceLevelList != NULL);
	if (memb->previousInPriceList == NULL && memb->nextInPriceList == NULL) {
		// this was the only order at this price, delete price level from rbtree
		sglib_bookedPriceLevel_delete(&halfbook->rbtree, memb->myPriceLevel);
		FREE(memb->myPriceLevel);
	} else {
		//more then one element in price list
		if (memb == memb->myPriceLevel->priceLevelList) memb->myPriceLevel->priceLevelList = memb->nextInPriceList;
		SGLIB_DL_LIST_DELETE(struct bookedOrder, memb->myPriceLevel->priceLevelList, memb, previousInPriceList, nextInPriceList);
	}
	// DO not free now, order will be freed in finalization of an accepted block
	// FREE(memb);
}

static void exchangePerformExecution(struct bookedOrder *makerOrder, struct bookedOrder *takerOrder, double executedQuantity, struct bFinalActionList **finalActionList) {
	struct bAccountBalance	*bmbt, *btqt, *btbt, *bmqt;
	char					*token1, *token2;
	double					price;

	if (finalActionList != NULL) {
		blockFinalizeNoteExchangeOrderModified(BFA_EXCHANGE_ORDER_EXECUTED, finalActionList, makerOrder);
		blockFinalizeNoteExchangeOrderModified(BFA_EXCHANGE_ORDER_EXECUTED, finalActionList, takerOrder);
	}

	price = makerOrder->myPriceLevel->price;
	PRINTF("Execution: quantity %f executed at price %f\n", executedQuantity, price);
	PRINTF("Execution: matching orders %s <> %s\n", exchangeSprintBookedOrder_st(makerOrder), exchangeSprintBookedOrder_st(takerOrder));

	makerOrder->executedQuantity += executedQuantity;
	takerOrder->executedQuantity += executedQuantity;

	token1 = makerOrder->myPriceLevel->myHalfBook->mySymbolBook->baseToken;
	token2 = makerOrder->myPriceLevel->myHalfBook->mySymbolBook->quoteToken;
	bmbt = bAccountBalanceLoadOrFind(token1, makerOrder->ownerAccount, &currentBchainContext);
	bmqt = bAccountBalanceLoadOrFind(token2, makerOrder->ownerAccount, &currentBchainContext);
	btbt = bAccountBalanceLoadOrFind(token1, takerOrder->ownerAccount, &currentBchainContext);
	btqt = bAccountBalanceLoadOrFind(token2, takerOrder->ownerAccount, &currentBchainContext);
	blockFinalizeNoteAccountBalanceModification(finalActionList, bmbt);
	blockFinalizeNoteAccountBalanceModification(finalActionList, btqt);
	blockFinalizeNoteAccountBalanceModification(finalActionList, btbt);
	blockFinalizeNoteAccountBalanceModification(finalActionList, bmqt);

	if (makerOrder->myPriceLevel->myHalfBook->side == SIDE_BUY) {
		assert(takerOrder->myPriceLevel->myHalfBook->side == SIDE_SELL);
		bmbt->balance += executedQuantity;
		btbt->balance -= executedQuantity;
		bmqt->balance -= executedQuantity * price;
		btqt->balance += executedQuantity * price;
	} else if (makerOrder->myPriceLevel->myHalfBook->side == SIDE_SELL) {
		assert(takerOrder->myPriceLevel->myHalfBook->side == SIDE_BUY);
		bmbt->balance -= executedQuantity;
		btbt->balance += executedQuantity;
		bmqt->balance += executedQuantity * price;
		btqt->balance -= executedQuantity * price;
	}
}

static int halfbookAddOrder(struct halfOrderBook *halfbook, struct halfOrderBook *oppositeHalfBook, char *ownerAccount, int side, double price, double quantity, double executedQuantity, uint64_t orderId, struct bFinalActionList **finalActionList) {
	int							r;
	struct bookedPriceLevel		*memb, pp;
	struct bookedOrder			*o;
	double 						quantityToMatch;

	ALLOC(o, struct bookedOrder);
	memset(o, 0, sizeof(*o));
	o->quantity = quantity;
	o->executedQuantity = executedQuantity;
	o->orderId = orderId;
	strncpy(o->ownerAccount, ownerAccount, sizeof(o->ownerAccount)-1);
	o->previousInPriceList = o->nextInPriceList = NULL;		
	o->nextInHash = NULL;

	// First make possible order matching
	memset(&pp, 0, sizeof(pp));
	pp.side = side;
	pp.price = price;
	pp.myHalfBook = halfbook;
	o->myPriceLevel = &pp;
	while (oppositeHalfBook->bestOrder != NULL && bookedOrderPriceCmp(&pp, oppositeHalfBook->bestOrder->myPriceLevel) <= 0) {
		// execute the new order against the best order of the oppositeHalfBook
		if (oppositeHalfBook->bestOrder->quantity - oppositeHalfBook->bestOrder->executedQuantity < o->quantity - o->executedQuantity) {
			quantityToMatch = oppositeHalfBook->bestOrder->quantity - oppositeHalfBook->bestOrder->executedQuantity;
		} else {
			quantityToMatch = o->quantity - o->executedQuantity;
		}
		exchangePerformExecution(oppositeHalfBook->bestOrder, o, quantityToMatch, finalActionList);
		//exchangeDumpBook(oppositeHalfBook->mySymbolBook);
		// if best order is fully executed, delete it		
		if (oppositeHalfBook->bestOrder->quantity <= oppositeHalfBook->bestOrder->executedQuantity) exchangeDeleteBookedOrder(oppositeHalfBook->bestOrder, finalActionList);
		//exchangeDumpBook(oppositeHalfBook->mySymbolBook);
		// if new order is fully executed, do not continue
		if (o->quantity <= o->executedQuantity) return(1);
	}

	// insert to tree
	pp.price = price;
	memb = sglib_bookedPriceLevel_find_member(halfbook->rbtree, &pp);
	if (memb == NULL) {
		ALLOC(memb, struct bookedPriceLevel);
		memset(memb, 0, sizeof(*memb));
		memb->price = price;
		memb->side = side;
		memb->myHalfBook = halfbook;
		sglib_bookedPriceLevel_add(&halfbook->rbtree, memb);
		memb->priceLevelList = o;
	} else {
		// orders in the same price level are sorted accoring to orderId which must be monotone
		// this is because if reverting deletion of an order we have to insert it into the same place as it was before
		SGLIB_SORTED_DL_LIST_ADD(struct bookedOrder, memb->priceLevelList, o, bookedOrderIdCmp, previousInPriceList, nextInPriceList);
	}
	o->myPriceLevel = memb;

	// insert to hashtab
	sglib_hashed_bookedOrder_add(allOrdersHashTab, o);
	if (halfbook->bestOrder == NULL || bookedOrderPriceCmp(memb, halfbook->bestOrder->myPriceLevel) < 0) halfbook->bestOrder = o;
	if (finalActionList != NULL) blockFinalizeNoteExchangeOrderModified(BFA_EXCHANGE_ORDER_ADDED, finalActionList, o);
	return(0);
}

static int sbookAddOrder(struct symbolOrderBook *sbook, char *ownerAccount, int side, double price, double quantity, double executedQuantity, uint64_t orderId, struct bFinalActionList **finalActionList) {
	int res;

	if (side == SIDE_BUY) {
		res = halfbookAddOrder(&sbook->bids, &sbook->asks, ownerAccount, side, price, quantity, executedQuantity, orderId, finalActionList);
	} else if (side == SIDE_SELL) {
		res = halfbookAddOrder(&sbook->asks, &sbook->bids, ownerAccount, side, price, quantity, executedQuantity, orderId, finalActionList);
	} else {
		assert(0 && "wrong side");
		res = -1;
	}
	return(res);
}

/////////////////////////////////////

static uint64_t	currentOrderId = 0;

static uint64_t exchangeGetNewOrderId() {
	currentOrderId ++;
	return(currentOrderId);
}

struct bookedOrder *exchangeGetOrder(uint64_t orderId) {
	struct bookedOrder 		*memb, oo;

	oo.orderId = orderId;
	memb = sglib_hashed_bookedOrder_find_member(allOrdersHashTab, &oo);
	return(memb);
}

static int exchangeCheckCommonOrderValues(char *ownerAccount, char *baseToken, char *quoteToken, int side, double price, double quantity, double executedQuantity) {
	// a common sense checks
	if (ownerAccount == NULL || ownerAccount[0] == 0) {
		PRINTF("Wrong order account %s for book %s/%s\n", ownerAccount, baseToken, quoteToken);
		return(-1);
	}
	if (coreServerSupportedMessageFind(baseToken) == NULL) {
		PRINTF("Wrong order base token for book %s/%s\n", baseToken, quoteToken);
		return(-1);
	}
	if (coreServerSupportedMessageFind(quoteToken) == NULL) {
		PRINTF("Wrong order quote token for book %s/%s\n", baseToken, quoteToken);
		return(-1);
	}
#if 0
	if (strcmp(baseToken, quoteToken) >= 0) {
		PRINTF("Wrong order token pair (wrong token ordering) for book %s/%s\n", baseToken, quoteToken);
		return(-1);
	}
#endif
	if (side != SIDE_BUY && side != SIDE_SELL) {
		PRINTF("Wrong order side for book %s/%s\n", baseToken, quoteToken);
		return(-1);
	}
	if (quantity <= 0) {
		PRINTF("Wrong order quantity %f for book %s/%s\n", quantity, baseToken, quoteToken);
		return(-1);
	}
	if (quantity <= executedQuantity) {
		PRINTF("Wrong order (executed) quantity %f (%f) for book %s/%s\n", executedQuantity, quantity, baseToken, quoteToken);
		return(-1);
	}
	if (price <= 0) {
		printf("Wrong order price %f for book %s/%s\n", price, baseToken, quoteToken);
		return(-1);
	}
	return(0);
}

static int exchangeCheckModifyOrDeleteRights(struct bookedOrder *o, char *ownerAccount, char *baseToken, char *quoteToken, int side) {
	struct symbolOrderBook *sbook;
	struct halfOrderBook *h;
	struct bookedPriceLevel *p;

	if (strcmp(o->ownerAccount, ownerAccount) != 0) return(-1);

	p = o->myPriceLevel;
	assert(p!=NULL);
	h = p->myHalfBook;
	assert(h!=NULL);
	sbook = h->mySymbolBook;
	assert(sbook!=NULL);

	if (h->side != side) return(-2);
	if (strcmp(sbook->baseToken, baseToken) != 0) return(-3);
	if (strcmp(sbook->quoteToken, quoteToken) != 0) return(-3);
	return(0);
}

int exchangeAddOrder(char *ownerAccount, char *baseToken, char *quoteToken, int side, double price, double quantity, double executedQuantity, struct bFinalActionList **finalActionList) {
	uint64_t				orderid;
	struct symbolOrderBook 	*sbook, ss;
	int						r, res;

	PRINTF("Adding order %s %f * %s/%s @ %f\n", sideNames[side], quantity, baseToken, quoteToken, price);

	r = exchangeCheckCommonOrderValues(ownerAccount, baseToken, quoteToken, side, price, quantity, executedQuantity);
	if (r < 0) return(-1);

	ss.baseToken = baseToken;
	ss.quoteToken = quoteToken;
	sbook = sglib_hashed_symbolOrderBook_find_member(allOrderBooks, &ss);
	if (sbook == NULL) {
		ALLOC(sbook, struct symbolOrderBook);
		memset(sbook, 0, sizeof(*sbook));
		sbook->baseToken = strDuplicate(baseToken);
		sbook->quoteToken = strDuplicate(quoteToken);
		sbook->bids.mySymbolBook = sbook->asks.mySymbolBook = sbook;
		sbook->bids.side = SIDE_BUY;
		sbook->asks.side = SIDE_SELL;
		sglib_hashed_symbolOrderBook_add(allOrderBooks, sbook);
	}
	orderid = exchangeGetNewOrderId();
	res = sbookAddOrder(sbook, ownerAccount, side, price, quantity, executedQuantity, orderid, finalActionList);
	exchangeDumpBook(sbook);
	return(res);
}

int exchangeDeleteOrder(int orderid, char *ownerAccount, char *baseToken, char *quoteToken, int side, struct bFinalActionList **finalActionList) {
	struct bookedOrder 		*o;
	int						r;

	o = exchangeGetOrder(orderid);
	if (o == NULL) return(-1);
	r = exchangeCheckModifyOrDeleteRights(o, ownerAccount, baseToken, quoteToken, side);
	if (r != 0) return(r);
	exchangeDeleteBookedOrder(o, finalActionList);
	return(0);
}

#if 0
struct bookedOrder *exchangeReplaceOrder(uint64_t orderId, char *ownerAccount, char *baseToken, char *quoteToken, int side, double price, double quantity) {
	struct bookedOrder		oo, *o, *memb;
	int						r;
	double					executedQuantity;

	if (quantity == 0) {
		r = exchangeDeleteOrder(orderId, ownerAccount, baseToken, quoteToken, side);
		return(NULL);
	}

	r = exchangeCheckCommonOrderValues(ownerAccount, baseToken, quoteToken, side, price, quantity, 0);
	if (r < 0) return(NULL);

	memb = exchangeGetOrder(orderId);
	if (memb == NULL) return(NULL);
	r = exchangeCheckModifyOrDeleteRights(memb, ownerAccount, baseToken, quoteToken, side);
	if (r != 0) return(NULL);

	if (quantity <= memb->executedQuantity) {
		exchangeDeleteBookedOrder(memb);
		return(NULL);
	}

	assert(memb->myPriceLevel != NULL);
	if (memb->myPriceLevel->price == price) {
		// If orderid and price is equal, change only remaining fields, do not touch trees
		memb->quantity = quantity;
		assert(strcmp(memb->ownerAccount, ownerAccount) == 0);
		o = memb;
	} else {
		executedQuantity = memb->executedQuantity;
		exchangeDeleteBookedOrder(memb);
		o = exchangeAddOrder(ownerAccount, baseToken, quoteToken, side, price, quantity, executedQuantity);
	}
	return(o);
}
#endif

