/*----------------------------------------------------------------------------*/
/* Big Brother network test tool.                                             */
/*                                                                            */
/* Copyright (C) 2004 Henrik Storner <henrik@hswn.dk>                         */
/*                                                                            */
/* This program is released under the GNU General Public License (GPL),       */
/* version 2. See the file "COPYING" for details.                             */
/*                                                                            */
/*----------------------------------------------------------------------------*/

static char rcsid[] = "$Id: dns2.c,v 1.2 2004-08-28 07:12:30 henrik Exp $";

/*
 * All of the code for parsing DNS responses and formatting these into
 * text were taken from the "adig.c" source-file included with the
 * C-ARES 1.2.0 library. This file carries the following copyright
 * notice, reproduced in full:
 *
 * --------------------------------------------------------------------
 * Copyright 1998 by the Massachusetts Institute of Technology.
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting
 * documentation, and that the name of M.I.T. not be used in
 * advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 * --------------------------------------------------------------------
 */

#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>

#include "bbgen.h"
#include "util.h"
#include "dns2.h"

#include "ares.h"
#include "ares_dns.h"

static int msgstatus;
static char *msgbuf;
static int msglen;
static char msg[1024];

char *dns_detail_response(int *status)
{
	char *result;
	
	result = malcop(msgbuf);
	free(msgbuf);
	msgbuf = NULL;
	msglen = 0;

	if (status != NULL) *status = msgstatus;
	return result;
}

static const unsigned char *display_question(const unsigned char *aptr,
					     const unsigned char *abuf,
					     int alen);
static const unsigned char *display_rr(const unsigned char *aptr,
				       const unsigned char *abuf, int alen);
static const char *type_name(int type);
static const char *class_name(int dnsclass);

struct nv {
  const char *name;
  int value;
};

static const struct nv flags[] = {
  { "usevc",		ARES_FLAG_USEVC },
  { "primary",		ARES_FLAG_PRIMARY },
  { "igntc",		ARES_FLAG_IGNTC },
  { "norecurse",	ARES_FLAG_NORECURSE },
  { "stayopen",		ARES_FLAG_STAYOPEN },
  { "noaliases",	ARES_FLAG_NOALIASES }
};
static const int nflags = sizeof(flags) / sizeof(flags[0]);

static const struct nv classes[] = {
  { "IN",	C_IN },
  { "CHAOS",	C_CHAOS },
  { "HS",	C_HS },
  { "ANY",	C_ANY }
};
static const int nclasses = sizeof(classes) / sizeof(classes[0]);

static const struct nv types[] = {
  { "A",	T_A },
  { "NS",	T_NS },
  { "MD",	T_MD },
  { "MF",	T_MF },
  { "CNAME",	T_CNAME },
  { "SOA",	T_SOA },
  { "MB",	T_MB },
  { "MG",	T_MG },
  { "MR",	T_MR },
  { "NULL",	T_NULL },
  { "WKS",	T_WKS },
  { "PTR",	T_PTR },
  { "HINFO",	T_HINFO },
  { "MINFO",	T_MINFO },
  { "MX",	T_MX },
  { "TXT",	T_TXT },
  { "RP",	T_RP },
  { "AFSDB",	T_AFSDB },
  { "X25",	T_X25 },
  { "ISDN",	T_ISDN },
  { "RT",	T_RT },
  { "NSAP",	T_NSAP },
  { "NSAP_PTR",	T_NSAP_PTR },
  { "SIG",	T_SIG },
  { "KEY",	T_KEY },
  { "PX",	T_PX },
  { "GPOS",	T_GPOS },
  { "AAAA",	T_AAAA },
  { "LOC",	T_LOC },
  { "SRV",	T_SRV },
  { "AXFR",	T_AXFR },
  { "MAILB",	T_MAILB },
  { "MAILA",	T_MAILA },
  { "ANY",	T_ANY }
};
static const int ntypes = sizeof(types) / sizeof(types[0]);

static const char *opcodes[] = {
  "QUERY", "IQUERY", "STATUS", "(reserved)", "NOTIFY",
  "(unknown)", "(unknown)", "(unknown)", "(unknown)",
  "UPDATEA", "UPDATED", "UPDATEDA", "UPDATEM", "UPDATEMA",
  "ZONEINIT", "ZONEREF"
};

static const char *rcodes[] = {
  "NOERROR", "FORMERR", "SERVFAIL", "NXDOMAIN", "NOTIMP", "REFUSED",
  "(unknown)", "(unknown)", "(unknown)", "(unknown)", "(unknown)",
  "(unknown)", "(unknown)", "(unknown)", "(unknown)", "NOCHANGE"
};

void dns_detail_callback(void *arg, int status, unsigned char *abuf, int alen)
{
	int id, qr, opcode, aa, tc, rd, ra, rcode;
	unsigned int qdcount, ancount, nscount, arcount, i;
	const unsigned char *aptr;
	msgbuf = NULL; msglen = 0;

	/*
	 * Display an error message if there was an error, but only stop if
	 * we actually didn't get an answer buffer.
	 */
	msgstatus = status;
	if (status != ARES_SUCCESS) {
		sprintf(msg, "%s\n", ares_strerror(status));
		addtobuffer(&msgbuf, &msglen, msg);
		if (!abuf) return;
	}

	/* Won't happen, but check anyway, for safety. */
	if (alen < HFIXEDSZ) return;

	/* Parse the answer header. */
	id = DNS_HEADER_QID(abuf);
	qr = DNS_HEADER_QR(abuf);
	opcode = DNS_HEADER_OPCODE(abuf);
	aa = DNS_HEADER_AA(abuf);
	tc = DNS_HEADER_TC(abuf);
	rd = DNS_HEADER_RD(abuf);
	ra = DNS_HEADER_RA(abuf);
	rcode = DNS_HEADER_RCODE(abuf);
	qdcount = DNS_HEADER_QDCOUNT(abuf);
	ancount = DNS_HEADER_ANCOUNT(abuf);
	nscount = DNS_HEADER_NSCOUNT(abuf);
	arcount = DNS_HEADER_ARCOUNT(abuf);

	/* Display the answer header. */
	sprintf(msg, "id: %d\n", id);
	addtobuffer(&msgbuf, &msglen, msg);
	sprintf(msg, "flags: %s%s%s%s%s\n",
		qr ? "qr " : "",
		aa ? "aa " : "",
		tc ? "tc " : "",
		rd ? "rd " : "",
		ra ? "ra " : "");
	addtobuffer(&msgbuf, &msglen, msg);
	sprintf(msg, "opcode: %s\n", opcodes[opcode]);
	addtobuffer(&msgbuf, &msglen, msg);
	sprintf(msg, "rcode: %s\n", rcodes[rcode]);
	addtobuffer(&msgbuf, &msglen, msg);

	/* Display the questions. */
	addtobuffer(&msgbuf, &msglen, "Questions:\n");
	aptr = abuf + HFIXEDSZ;
	for (i = 0; i < qdcount; i++) {
		aptr = display_question(aptr, abuf, alen);
		if (aptr == NULL) return;
	}

	/* Display the answers. */
	addtobuffer(&msgbuf, &msglen, "Answers:\n");
	for (i = 0; i < ancount; i++) {
		aptr = display_rr(aptr, abuf, alen);
		if (aptr == NULL) return;
	}

	/* Display the NS records. */
	addtobuffer(&msgbuf, &msglen, "NS records:\n");
	for (i = 0; i < nscount; i++) {
		aptr = display_rr(aptr, abuf, alen);
		if (aptr == NULL) return;
	}

	/* Display the additional records. */
	addtobuffer(&msgbuf, &msglen, "Additional records:\n");
	for (i = 0; i < arcount; i++) {
		aptr = display_rr(aptr, abuf, alen);
		if (aptr == NULL) return;
	}

	return;
}

static const unsigned char *display_question(const unsigned char *aptr,
					     const unsigned char *abuf,
					     int alen)
{
	char *name;
	int type, dnsclass, status;
	long len;

	/* Parse the question name. */
	status = ares_expand_name(aptr, abuf, alen, &name, &len);
	if (status != ARES_SUCCESS) return NULL;
	aptr += len;

	/* Make sure there's enough data after the name for the fixed part
	 * of the question.
	 */
	if (aptr + QFIXEDSZ > abuf + alen) {
		free(name);
		return NULL;
	}

	/* Parse the question type and class. */
	type = DNS_QUESTION_TYPE(aptr);
	dnsclass = DNS_QUESTION_CLASS(aptr);
	aptr += QFIXEDSZ;

	/*
	 * Display the question, in a format sort of similar to how we will
	 * display RRs.
	 */
	sprintf(msg, "\t%-15s.\t", name);
	addtobuffer(&msgbuf, &msglen, msg);
	if (dnsclass != C_IN) {
		sprintf(msg, "\t%s", class_name(dnsclass));
		addtobuffer(&msgbuf, &msglen, msg);
	}
	sprintf(msg, "\t%s\n", type_name(type));
	addtobuffer(&msgbuf, &msglen, msg);
	free(name);
	return aptr;
}

static const unsigned char *display_rr(const unsigned char *aptr,
				       const unsigned char *abuf, int alen)
{
	const unsigned char *p;
	char *name;
	int type, dnsclass, ttl, dlen, status;
	long len;
	struct in_addr addr;

	/* Parse the RR name. */
	status = ares_expand_name(aptr, abuf, alen, &name, &len);
	if (status != ARES_SUCCESS) return NULL;
	aptr += len;

	/* Make sure there is enough data after the RR name for the fixed
	* part of the RR.
	*/
	if (aptr + RRFIXEDSZ > abuf + alen) {
		free(name);
		return NULL;
	}

	/* Parse the fixed part of the RR, and advance to the RR data field. */
	type = DNS_RR_TYPE(aptr);
	dnsclass = DNS_RR_CLASS(aptr);
	ttl = DNS_RR_TTL(aptr);
	dlen = DNS_RR_LEN(aptr);
	aptr += RRFIXEDSZ;
	if (aptr + dlen > abuf + alen) {
		free(name);
		return NULL;
	}

	/* Display the RR name, class, and type. */
	sprintf(msg, "\t%-15s.\t%d", name, ttl);
	addtobuffer(&msgbuf, &msglen, msg);
	if (dnsclass != C_IN) {
		sprintf(msg, "\t%s", class_name(dnsclass));
		addtobuffer(&msgbuf, &msglen, msg);
	}
	sprintf(msg, "\t%s", type_name(type));
	addtobuffer(&msgbuf, &msglen, msg);
	free(name);

	/* Display the RR data.  Don't touch aptr. */
	switch (type) {
	  case T_CNAME:
	  case T_MB:
	  case T_MD:
	  case T_MF:
	  case T_MG:
	  case T_MR:
	  case T_NS:
	  case T_PTR:
		/* For these types, the RR data is just a domain name. */
		status = ares_expand_name(aptr, abuf, alen, &name, &len);
		if (status != ARES_SUCCESS) return NULL;
		sprintf(msg, "\t%s.", name);
		addtobuffer(&msgbuf, &msglen, msg);
		free(name);
		break;

	  case T_HINFO:
		/* The RR data is two length-counted character strings. */
		p = aptr;
		len = *p;
		if (p + len + 1 > aptr + dlen) return NULL;
		sprintf(msg, "\t%.*s", (int) len, p + 1);
		addtobuffer(&msgbuf, &msglen, msg);
		p += len + 1;
		len = *p;
		if (p + len + 1 > aptr + dlen) return NULL;
		sprintf(msg, "\t%.*s", (int) len, p + 1);
		addtobuffer(&msgbuf, &msglen, msg);
		break;

	  case T_MINFO:
		/* The RR data is two domain names. */
		p = aptr;
		status = ares_expand_name(p, abuf, alen, &name, &len);
		if (status != ARES_SUCCESS) return NULL;
		sprintf(msg, "\t%s.", name);
		addtobuffer(&msgbuf, &msglen, msg);
		free(name);
		p += len;
		status = ares_expand_name(p, abuf, alen, &name, &len);
		if (status != ARES_SUCCESS) return NULL;
		sprintf(msg, "\t%s.", name);
		addtobuffer(&msgbuf, &msglen, msg);
		free(name);
		break;

	  case T_MX:
		/* The RR data is two bytes giving a preference ordering, and then a domain name.  */
		if (dlen < 2) return NULL;
		sprintf(msg, "\t%d", (aptr[0] << 8) | aptr[1]);
		addtobuffer(&msgbuf, &msglen, msg);
		status = ares_expand_name(aptr + 2, abuf, alen, &name, &len);
		if (status != ARES_SUCCESS) return NULL;
		sprintf(msg, "\t%s.", name);
		addtobuffer(&msgbuf, &msglen, msg);
		free(name);
		break;

	  case T_SOA:
		/*
		 * The RR data is two domain names and then five four-byte
		 * numbers giving the serial number and some timeouts.
		 */
		p = aptr;
		status = ares_expand_name(p, abuf, alen, &name, &len);
		if (status != ARES_SUCCESS) return NULL;
		sprintf(msg, "\t%s.\n", name);
		addtobuffer(&msgbuf, &msglen, msg);
		free(name);
		p += len;
		status = ares_expand_name(p, abuf, alen, &name, &len);
		if (status != ARES_SUCCESS) return NULL;
		sprintf(msg, "\t\t\t\t\t\t%s.\n", name);
		addtobuffer(&msgbuf, &msglen, msg);
		free(name);
		p += len;
		if (p + 20 > aptr + dlen) return NULL;
		sprintf(msg, "\t\t\t\t\t\t( %d %d %d %d %d )",
			(p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3],
			(p[4] << 24) | (p[5] << 16) | (p[6] << 8) | p[7],
			(p[8] << 24) | (p[9] << 16) | (p[10] << 8) | p[11],
			(p[12] << 24) | (p[13] << 16) | (p[14] << 8) | p[15],
			(p[16] << 24) | (p[17] << 16) | (p[18] << 8) | p[19]);
		addtobuffer(&msgbuf, &msglen, msg);
		break;

	  case T_TXT:
		/* The RR data is one or more length-counted character strings. */
		p = aptr;
		while (p < aptr + dlen) {
			len = *p;
			if (p + len + 1 > aptr + dlen) return NULL;
			sprintf(msg, "\t%.*s", (int)len, p + 1);
			addtobuffer(&msgbuf, &msglen, msg);
			p += len + 1;
		}
		break;

	  case T_A:
		/* The RR data is a four-byte Internet address. */
		if (dlen != 4) return NULL;
		memcpy(&addr, aptr, sizeof(struct in_addr));
		sprintf(msg, "\t%s", inet_ntoa(addr));
		addtobuffer(&msgbuf, &msglen, msg);
		break;

	  case T_WKS:
		/* Not implemented yet */
		break;

	  case T_SRV:
		/*
		 * The RR data is three two-byte numbers representing the
		 * priority, weight, and port, followed by a domain name.
		 */
      
		sprintf(msg, "\t%d", DNS__16BIT(aptr));
		addtobuffer(&msgbuf, &msglen, msg);
		sprintf(msg, " %d", DNS__16BIT(aptr + 2));
		addtobuffer(&msgbuf, &msglen, msg);
		sprintf(msg, " %d", DNS__16BIT(aptr + 4));
		addtobuffer(&msgbuf, &msglen, msg);
      
		status = ares_expand_name(aptr + 6, abuf, alen, &name, &len);
		if (status != ARES_SUCCESS) return NULL;
		sprintf(msg, "\t%s.", name);
		addtobuffer(&msgbuf, &msglen, msg);
		free(name);
		break;
      
	  default:
		sprintf(msg, "\t[Unknown RR; cannot parse]");
		addtobuffer(&msgbuf, &msglen, msg);
	}
	sprintf(msg, "\n");
	addtobuffer(&msgbuf, &msglen, msg);

	return aptr + dlen;
}

static const char *type_name(int type)
{
	int i;

	for (i = 0; i < ntypes; i++) {
		if (types[i].value == type) return types[i].name;
	}
	return "(unknown)";
}

static const char *class_name(int dnsclass)
{
	int i;

	for (i = 0; i < nclasses; i++) {
		if (classes[i].value == dnsclass) return classes[i].name;
	}
	return "(unknown)";
}
