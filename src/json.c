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

#define CONFIG_JSON_SKIP_BLANK(s, line) {						\
		while (isblank(*(s)) || *(s) == '\n' || *(s) == '\r' || *(s) == '\t') {	\
			if (*(s) == '\n') (line) ++;						\
			(s)++;												\
		}														\
	}

static char *jsonLiterals[] = {"false", "true", "null", NULL};
static int jsonCurrentLine = 1;

char *jsonDestructibleParseString(char *b, struct jsonnode **res) {
    char    				*s, *ss, *send, *key, *field, *ll, *slit, *dlit;
    int     				i, index, ec, fieldIdLen, fieldLen, llen;
    int     				fieldId, jclen;
	struct jsonnode 		*rrr, *eee;
	struct jsonFieldList	*ff, **fff;

    s = b;
    CONFIG_JSON_SKIP_BLANK(s, jsonCurrentLine);

    // printf("\nParsing '%s'\n", b);

    if (*s == '{') {
        // JSON Object
		ALLOC(*res, struct jsonnode);
		(*res)->type = JSON_NODE_TYPE_STRUCT;
		(*res)->u.fields = NULL;
		fff = &(*res)->u.fields;
        do {
            s++;
            CONFIG_JSON_SKIP_BLANK(s, jsonCurrentLine);
            if (*s == '}') break;				// this allows comma after the last record
            if (*s != '"') goto jsonError;
            s++;
            key = s;
            while (*s != '"' && *s!=0) s++;
            if (*s == 0) goto jsonError;
			*s = 0;
            s++;
            CONFIG_JSON_SKIP_BLANK(s, jsonCurrentLine);
            if (*s != ':') goto jsonError;
            s++;
            CONFIG_JSON_SKIP_BLANK(s, jsonCurrentLine);
            s = jsonDestructibleParseString(s, &rrr);
			if (s == NULL) return(NULL);
            CONFIG_JSON_SKIP_BLANK(s, jsonCurrentLine);
			ALLOC(*fff, struct jsonFieldList);
			(*fff)->u.name = key;
			(*fff)->val = rrr;
			(*fff)->next = NULL;
			fff = &(*fff)->next;
        } while (*s == ',');
        if (*s != '}') goto jsonError;
        s ++;
    } else if (*s == '[') {
        // JSON array
		ALLOC(*res, struct jsonnode);
		(*res)->type = JSON_NODE_TYPE_ARRAY;
		(*res)->u.fields = NULL;
		fff = &(*res)->u.fields;
		index = 0;
        do {
            s++;
            CONFIG_JSON_SKIP_BLANK(s, jsonCurrentLine);
			if (*s == ']') {
				eee = NULL;
			} else {
				s = jsonDestructibleParseString(s, &eee);
				if (s == NULL) return(NULL);
				CONFIG_JSON_SKIP_BLANK(s, jsonCurrentLine);
			}
			if (eee != NULL) {
				ALLOC(*fff, struct jsonFieldList);
				(*fff)->u.index = index;
				(*fff)->val = eee;
				(*fff)->next = NULL;
				fff = &(*fff)->next;
				index ++;
			}
        } while (*s == ',');
        if (*s != ']') goto jsonError;
        s++;
    } else if (*s == '"') {
        // JSON string (no unicode escapes accepted yet)
        s ++;
		slit = s;
        while (*s != '"' && *s != 0) {
            if (*s == '\\') s++;
            s++;
        }
        if (*s == 0) goto jsonError;
		*s = 0;
        s ++;
		ALLOC(*res, struct jsonnode);
		(*res)->type = JSON_NODE_TYPE_STRING;
		(*res)->u.s = slit;
    } else if (isdigit(*s) || *s == '.' || *s == '+' || *s == '-') {
		dlit = s;
        if (*s == '+' || *s == '-') s++;
        while (isdigit(*s) || *s == '.') s ++;
        if (*s == 'e' || *s == 'E') {
            s ++;
            if (*s == '+' || *s == '-') s++;
            while (isdigit(*s)) s ++;           
        }
		ALLOC(*res, struct jsonnode);
		(*res)->type = JSON_NODE_TYPE_NUMBER;
		ec = *s; *s = 0; (*res)->u.n = atof(dlit); *s = ec;
    } else {
        // Nothing of above, maybe it is a literal
        for(i=0; jsonLiterals[i]!=NULL; i++) {
            ll = jsonLiterals[i];
            llen = strlen(ll);		// can be precomputed
            if (strncmp(s, ll, llen) == 0 && ! isalpha(s[llen+1])) {
                s += llen;
                break;
            }
        }
        // If it is even not a literal, make error
        if (jsonLiterals[i]==NULL) goto jsonError;
		if (strcmp(jsonLiterals[i], "null") == 0) {
			*res = NULL;
		} else {
			ALLOC(*res, struct jsonnode);
			(*res)->type = JSON_NODE_TYPE_BOOL;
			(*res)->u.b = i;
		}
    }

    return(s);

jsonError:
    PRINTF("JSON: Syntax error at line %d before %30.30s\n", jsonCurrentLine, s);
    return(NULL);
}

void jsonPrint(struct jsonnode *nn, FILE *ff) {
	int						i;
	char					*ss;
	struct jsonFieldList	*ll;

	switch (nn->type) {
	case JSON_NODE_TYPE_BOOL:
		if (nn->u.b) fprintf(ff, "true");
		else fprintf(ff, "false");
		break;
	case JSON_NODE_TYPE_NUMBER:
		fprintf(ff, "%f", nn->u.n);
		break;
	case JSON_NODE_TYPE_STRING:
		fprintf(ff, "%s", nn->u.s);
		break;
	case JSON_NODE_TYPE_ARRAY:
		fprintf(ff, "[");
		for(ll=nn->u.fields; ll!=NULL; ll=ll->next) {
			jsonPrint(ll->val, ff);
			fprintf(ff, ",\n");
		}
		fprintf(ff, "]");
		break;
	case JSON_NODE_TYPE_STRUCT:
		fprintf(ff, "{");
		for(ll=nn->u.fields; ll!=NULL; ll=ll->next) {
			fprintf(ff, "\"%s\": ", ll->u.name);
			jsonPrint(ll->val, ff);
			fprintf(ff, ",\n");
		}
		fprintf(ff, "}");
		break;
	}
}

struct jsonnode *jsonFindField(struct jsonnode	*nn, char *name) {
	struct jsonFieldList	*ll;

	if (nn->type != JSON_NODE_TYPE_STRUCT) return(NULL);
	for(ll=nn->u.fields; ll!=NULL; ll=ll->next) {
		if (strcmp(ll->u.name, name) == 0) return(ll->val);
	}
	return(NULL);
}

double jsonFindFieldDouble(struct jsonnode	*nn, char *name, double defaultValue) {
	struct jsonnode 	*tt;

	tt = jsonFindField(nn, name);
	if (tt == NULL || tt->type != JSON_NODE_TYPE_NUMBER) return(defaultValue);
	return(tt->u.n);
}

char *jsonFindFieldString(struct jsonnode	*nn, char *name, char *defaultValue) {
	struct jsonnode 	*tt;

	tt = jsonFindField(nn, name);
	if (tt == NULL || tt->type != JSON_NODE_TYPE_STRING) return(defaultValue);
	return(tt->u.s);
}

double jsonNextFieldDouble(struct jsonFieldList **aa, double defaultValue) {
	struct jsonnode 	*tt;

	if (aa == NULL || *aa == NULL) return(defaultValue);
	tt = (*aa)->val;
	*aa = (*aa)->next;
	if (tt == NULL || tt->type != JSON_NODE_TYPE_NUMBER) return(defaultValue);

	return(tt->u.n);
}

char *jsonNextFieldString(struct jsonFieldList **aa, char *defaultValue) {
	struct jsonnode 	*tt;

	if (aa == NULL || *aa == NULL) return(defaultValue);
	tt = (*aa)->val;
	*aa = (*aa)->next;
	if (tt == NULL || tt->type != JSON_NODE_TYPE_STRING) return(defaultValue);

	return(tt->u.s);
}

