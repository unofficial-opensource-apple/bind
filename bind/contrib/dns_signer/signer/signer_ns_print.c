/* $Header: /cvs/Darwin/src/live/bind/bind/contrib/dns_signer/signer/signer_ns_print.c,v 1.1.1.3 2001/01/31 03:58:42 zarzycki Exp $ */
/* Altered for the new signer */
/*
 * Copyright (c) 1996,1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/* Import. */

#include "port_before.h"

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>

#include <assert.h>
#include <errno.h>
#include <resolv.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "port_after.h"

#include "dns_signer.h"
#include <dns_support.h>
#include <bind_support.h>

static int	charstr(const u_char *rdata, const u_char *edata,
			char **buf, size_t *buflen);
static void	addlen(size_t len, char **buf, size_t *buflen);
static int	addstr(const char *src, size_t len,
			   char **buf, size_t *buflen);
static int	addtab(size_t len, size_t target, int spaced,
			   char **buf, size_t *buflen);
static int	addttl(struct signing_options *options, u_int32_t ttl,
			   char *buf, size_t buflen);

#ifdef SPRINTF_CHAR
# define SPRINTF(x) strlen(sprintf/**/x)
#else
# define SPRINTF(x) ((size_t)sprintf x)
#endif

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

#ifndef MAX
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif

/* Forward. */

static int	addname_addr(u_char **p, char **buf, size_t *buflen);

/* Macros. */

#define	T(x) if ((x) < 0) return (-1); else (void)NULL

/* Public. */

/*
 * int
 * ns_sprintrrf_rdata(options, type, rdata, rdlen, buf, buflen)
 *	Convert the fields of an RR into presentation format.
 * return:
 *	Number of characters written to buf, or -1 (check errno).
 */
int
ns_sprintrrf_rdata(struct signing_options *options, u_int16_t type, u_char *rdata, size_t rdlen, char *buf, size_t buflen)
{
	char *obuf = buf;
	u_char *edata = rdata + rdlen;
	int spaced = 0;

	char *comment;
	char tmp[100];
	int len;

	/*
	 * RData.
	 */
	switch (type) {
	case ns_t_a:
		if (rdlen != NS_INADDRSZ)
			goto formerr;
		(void) inet_ntop(AF_INET, rdata, buf, buflen);
		addlen(strlen(buf), &buf, &buflen);
		break;

	case ns_t_cname:
	case ns_t_mb:
	case ns_t_mg:
	case ns_t_mr:
	case ns_t_ns:
	case ns_t_ptr:
		T(addname_addr(&rdata, &buf, &buflen));
		break;

	case ns_t_hinfo:
	case ns_t_isdn:
		/* First word. */
		T(len = charstr(rdata, edata, &buf, &buflen));
		if (len == 0)
			goto formerr;
		rdata += len;
		T(addstr(" ", 1, &buf, &buflen));

		/* Second word. */
		T(len = charstr(rdata, edata, &buf, &buflen));
		if (len == 0)
			goto formerr;
		rdata += len;
		break;

	case ns_t_soa: {
		u_long t;

		/* Server name. */
		T(addname_addr(&rdata, &buf, &buflen));
		T(addstr(" ", 1, &buf, &buflen));

		/* Administrator name. */
		T(addname_addr(&rdata, &buf, &buflen));
		T(addstr(" (\n", 3, &buf, &buflen));
		spaced = 0;

		if ((edata - rdata) != 5*NS_INT32SZ)
			goto formerr;

		/* Serial number. */
		t = ns_get32(rdata);  rdata += NS_INT32SZ;
		T(addstr("                                        ",31, &buf, &buflen));
		len = SPRINTF((tmp, "%lu", t));
		T(addstr(tmp, len, &buf, &buflen));
		T(spaced = addtab(len, 16, spaced, &buf, &buflen));
		T(addstr("; serial\n", 9, &buf, &buflen));
		spaced = 0;

		/* Refresh interval. */
		t = ns_get32(rdata);  rdata += NS_INT32SZ;
		T(addstr("                                        ",31, &buf, &buflen));
		T(len = addttl(options, t, buf, buflen));
		addlen(len, &buf, &buflen);
		T(spaced = addtab(len, 16, spaced, &buf, &buflen));
		T(addstr("; refresh\n", 10, &buf, &buflen));
		spaced = 0;

		/* Retry interval. */
		t = ns_get32(rdata);  rdata += NS_INT32SZ;
		T(addstr("                                        ",31, &buf, &buflen));
		T(len = addttl(options, t, buf, buflen));
		addlen(len, &buf, &buflen);
		T(spaced = addtab(len, 16, spaced, &buf, &buflen));
		T(addstr("; retry\n", 8, &buf, &buflen));
		spaced = 0;

		/* Expiry. */
		t = ns_get32(rdata);  rdata += NS_INT32SZ;
		T(addstr("                                        ",31, &buf, &buflen));
		T(len = addttl(options, t, buf, buflen));
		addlen(len, &buf, &buflen);
		T(spaced = addtab(len, 16, spaced, &buf, &buflen));
		T(addstr("; expiry\n", 9, &buf, &buflen));
		spaced = 0;

		/* Minimum TTL. */
		t = ns_get32(rdata);  rdata += NS_INT32SZ;
		T(addstr("                                        ",31, &buf, &buflen));
		T(len = addttl(options, t, buf, buflen));
		addlen(len, &buf, &buflen);
		T(addstr(" )", 2, &buf, &buflen));
		T(spaced = addtab(len, 16, spaced, &buf, &buflen));
		T(addstr("; minimum", 10, &buf, &buflen));

		break;
		}

	case ns_t_mx:
	case ns_t_afsdb:
	case ns_t_rt: {
		u_int t;

		if (rdlen < NS_INT16SZ)
			goto formerr;

		/* Priority. */
		t = ns_get16(rdata);
		rdata += NS_INT16SZ;
		len = SPRINTF((tmp, "%u ", t));
		T(addstr(tmp, len, &buf, &buflen));

		/* Target. */
		T(addname_addr(&rdata, &buf, &buflen));

		break;
		}

	case ns_t_px: {
		u_int t;

		if (rdlen < NS_INT16SZ)
			goto formerr;

		/* Priority. */
		t = ns_get16(rdata);
		rdata += NS_INT16SZ;
		len = SPRINTF((tmp, "%u ", t));
		T(addstr(tmp, len, &buf, &buflen));

		/* Name1. */
		T(addname_addr(&rdata, &buf, &buflen));
		T(addstr(" ", 1, &buf, &buflen));

		/* Name2. */
		T(addname_addr(&rdata, &buf, &buflen));

		break;
		}

	case ns_t_x25:
		T(len = charstr(rdata, edata, &buf, &buflen));
		if (len == 0)
			goto formerr;
		rdata += len;
		break;

	case ns_t_txt:
		while (rdata < edata) {
			T(len = charstr(rdata, edata, &buf, &buflen));
			if (len == 0)
				goto formerr;
			rdata += len;
			if (rdata < edata)
				T(addstr(" ", 1, &buf, &buflen));
		}
		break;

	case ns_t_nsap: {
		char t[255*3];

		(void) inet_nsap_ntoa(rdlen, rdata, t);
		T(addstr(t, strlen(t), &buf, &buflen));
		break;
		}

	case ns_t_aaaa:
		if (rdlen != NS_IN6ADDRSZ)
			goto formerr;
		(void) inet_ntop(AF_INET6, rdata, buf, buflen);
		addlen(strlen(buf), &buf, &buflen);
		break;

	case ns_t_loc: {
		char t[255];

		/* XXX protocol format checking? */
		(void) loc_ntoa(rdata, t);
		T(addstr(t, strlen(t), &buf, &buflen));
		break;
		}

	case ns_t_naptr: {
		u_int order, preference;
		char t[50];

		if (rdlen < 2*NS_INT16SZ)
			goto formerr;

		/* Order, Precedence. */
		order = ns_get16(rdata);	rdata += NS_INT16SZ;
		preference = ns_get16(rdata);	rdata += NS_INT16SZ;
		len = SPRINTF((t, "%u %u ", order, preference));
		T(addstr(t, len, &buf, &buflen));

		/* Flags. */
		T(len = charstr(rdata, edata, &buf, &buflen));
		if (len == 0)
			goto formerr;
		rdata += len;
		T(addstr(" ", 1, &buf, &buflen));

		/* Service. */
		T(len = charstr(rdata, edata, &buf, &buflen));
		if (len == 0)
			goto formerr;
		rdata += len;
		T(addstr(" ", 1, &buf, &buflen));

		/* Regexp. */
		T(len = charstr(rdata, edata, &buf, &buflen));
		if (len < 0)
			return (-1);
		if (len == 0)
			goto formerr;
		rdata += len;
		T(addstr(" ", 1, &buf, &buflen));

		/* Server. */
		T(addname_addr(&rdata, &buf, &buflen));
		break;
		}

	case ns_t_srv: {
		u_int priority, weight, port;
		char t[50];

		if (rdlen < NS_INT16SZ*3)
			goto formerr;

		/* Priority, Weight, Port. */
		priority = ns_get16(rdata);  rdata += NS_INT16SZ;
		weight   = ns_get16(rdata);  rdata += NS_INT16SZ;
		port     = ns_get16(rdata);  rdata += NS_INT16SZ;
		len = SPRINTF((t, "%u %u %u ", priority, weight, port));
		T(addstr(t, len, &buf, &buflen));

		/* Server. */
		T(addname_addr(&rdata, &buf, &buflen));
		break;
		}

	case ns_t_minfo:
	case ns_t_rp:
		/* Name1. */
		T(addname_addr(&rdata, &buf, &buflen));
		T(addstr(" ", 1, &buf, &buflen));

		/* Name2. */
		T(addname_addr(&rdata, &buf, &buflen));

		break;

	case ns_t_wks: {
		int n, lcnt;

		if (rdlen < NS_INT32SZ + 1)
			goto formerr;

		/* Address. */
		(void) inet_ntop(AF_INET, rdata, buf, buflen);
		addlen(strlen(buf), &buf, &buflen);
		rdata += NS_INADDRSZ;

		/* Protocol. */
		len = SPRINTF((tmp, " %u ( ", *rdata));
		T(addstr(tmp, len, &buf, &buflen));
		rdata += NS_INT8SZ;

		/* Bit map. */
		n = 0;
		lcnt = 0;
		while (rdata < edata) {
			u_int c = *rdata++;
			do {
				if (c & 0200) {
					if (lcnt == 0) {
						T(addstr("\n                                ",31,
							 &buf, &buflen));
						lcnt = 10;
						spaced = 0;
					}
					len = SPRINTF((tmp, "%d ", n));
					T(addstr(tmp, len, &buf, &buflen));
					lcnt--;
				}
				c <<= 1;
			} while (++n & 07);
		}
		T(addstr(")", 1, &buf, &buflen));

		break;
		}

	case ns_t_key: {
		char base64_key[NS_MD5RSA_MAX_BASE64];
		u_int keyflags, protocol, algorithm;
		char *leader;
		int n;

		if (rdlen < NS_INT16SZ + NS_INT8SZ + NS_INT8SZ)
			goto formerr;

		/* Key flags */
		keyflags = ns_get16(rdata);  rdata += NS_INT16SZ;
		if (options->so_bind_extensions)
			len = SPRINTF((tmp, "0x%04x", keyflags));
		else
			len = SPRINTF((tmp, "%u", keyflags));
		T(addstr(tmp, len, &buf, &buflen));

		/* Protocol, Algorithm */
		protocol = *rdata++;
		algorithm = *rdata++;
		len = SPRINTF((tmp, " %u %u", protocol, algorithm));
		T(addstr(tmp, len, &buf, &buflen));

		/* Public key data. */
		len = b64_ntop(rdata, edata - rdata,
				   base64_key, sizeof base64_key);
		if (len < 0)
			goto formerr;
		if (len > 15) {
			T(addstr(" (", 2, &buf, &buflen));
			leader = "\n                   ";
			spaced = 0;
		} else
			leader = " ";
		for (n = 0; n < len; n += 48) {
			T(addstr(leader, strlen(leader), &buf, &buflen));
			T(addstr(base64_key + n, MIN(len - n, 48),
				 &buf, &buflen));
		}
		if (len > 15)
			T(addstr(" )", 2, &buf, &buflen));

		break;
		}

	case ns_t_sig: {
		char base64_key[NS_MD5RSA_MAX_BASE64];
		u_int type, algorithm, labels, footprint;
		char *leader;
		u_long t;
		int n;

		if (rdlen < 22)
			goto formerr;

		/* Type covered, Algorithm, Label count, Original TTL. */
			type = ns_get16(rdata);  rdata += NS_INT16SZ;
		algorithm = *rdata++;
		labels = *rdata++;
		t = ns_get32(rdata);  rdata += NS_INT32SZ;
		len = SPRINTF((tmp, "%s %d %d %lu ",
				   p_type(type), algorithm, labels, t));
		T(addstr(tmp, len, &buf, &buflen));

		/* Signature expiry. */
		t = ns_get32(rdata);  rdata += NS_INT32SZ;
		len = SPRINTF((tmp, "%s ", p_secstodate(t)));
		T(addstr(tmp, len, &buf, &buflen));

		/* Time signed. */
		t = ns_get32(rdata);  rdata += NS_INT32SZ;
		len = SPRINTF((tmp, "%s ", p_secstodate(t)));
		T(addstr(tmp, len, &buf, &buflen));

		/* Signature Footprint. */
		footprint = ns_get16(rdata);  rdata += NS_INT16SZ;
		len = SPRINTF((tmp, "%u ", footprint));
		T(addstr(tmp, len, &buf, &buflen));

		/* Signer's name. */
		T(addname_addr(&rdata, &buf, &buflen));

		/* Signature. */
		len = b64_ntop(rdata, edata - rdata,
				   base64_key, sizeof base64_key);
		if (len > 15) {
			T(addstr(" (", 2, &buf, &buflen));
			leader = "\n                   ";
			spaced = 0;
		} else
			leader = " ";
		if (len < 0)
			goto formerr;
		for (n = 0; n < len; n += 48) {
			T(addstr(leader, strlen(leader), &buf, &buflen));
			T(addstr(base64_key + n, MIN(len - n, 48),
				 &buf, &buflen));
		}
		if (len > 15)
			T(addstr(" )", 2, &buf, &buflen));

		break;
		}

	case ns_t_nxt: {
		int n, c;

		/* Next domain name. */
		T(addname_addr(&rdata, &buf, &buflen));

		/* Type bit map. */
		n = edata - rdata;
		for (c = 0; c < n*8; c++)
			if (NS_NXT_BIT_ISSET(c, rdata)) {
				len = SPRINTF((tmp, " %s", p_type(c)));
				T(addstr(tmp, len, &buf, &buflen));
			}
		break;
		}

	case  ns_t_cert: { 
		u_int c_type, key_tag, alg;
		int n, siz;
		char *base64_cert, tmp[40], *leader;

		c_type  = ns_get16(rdata); rdata += NS_INT16SZ;
		key_tag = ns_get16(rdata); rdata += NS_INT16SZ;
		alg     = *rdata++;

		len = SPRINTF((tmp, "%d %d %d ", c_type, key_tag, alg));
		T(addstr(tmp, len, &buf, &buflen));
		siz = (edata-rdata)*4/3 + 4;
		base64_cert = (char *) malloc( siz);
		len = b64_ntop( rdata, edata-rdata, base64_cert, siz);

		if (len < 0) {
			free( base64_cert);
			goto formerr;
		}
		if (len > 15) {
			T(addstr(" (", 2, &buf, &buflen));
			leader = "\n                   ";
			spaced = 0;
		} else
			leader = " ";

		for (n = 0; n < len; n += 48) {
			T(addstr(leader, strlen(leader), &buf, &buflen));
			T(addstr(base64_cert + n, MIN(len - n, 48),
				 &buf, &buflen));
		}
		if (len > 15)
			T(addstr(" )", 2, &buf, &buflen));

		free( base64_cert);
		break;
	}

	default:
		comment = "unknown RR type";
		goto hexify;
	}
	return (buf - obuf);
 formerr:
	comment = "RR format error";
 hexify: {
	int n, m;
	char *p;

	len = SPRINTF((tmp, "\\#(                ; %s", comment));
	T(addstr(tmp, len, &buf, &buflen));
	while (rdata < edata) {
		p = tmp;
		p += SPRINTF((p, "\n        "));
		spaced = 0;
		n = MIN(16, edata - rdata);
		for (m = 0; m < n; m++)
			p += SPRINTF((p, "%02x ", rdata[m]));
		T(addstr(tmp, p - tmp, &buf, &buflen));
		if (n < 16) {
			T(addstr(")", 1, &buf, &buflen));
			T(addtab(p - tmp + 1, 48, spaced, &buf, &buflen));
		}
		p = tmp;
		p += SPRINTF((p, "; "));
		for (m = 0; m < n; m++)
			*p++ = (isascii(rdata[m]) && isprint(rdata[m]))
				? rdata[m]
				: '.';
		T(addstr(tmp, p - tmp, &buf, &buflen));
		rdata += n;
	}
	return (buf - obuf);
	}
}

/* Private. */

static int
addname_addr(u_char **pp, char **buf, size_t *buflen)
{
	size_t save_buflen = *buflen;
	char *save_buf = *buf;
	int newlen;

	newlen = wire_to_ascii_name (*buf, *pp, *buflen);

	if (newlen == -1)
		goto enospc;	/* Guess. */

	*pp += wire_name_length (*pp);
	addlen(newlen, buf, buflen);
	**buf = '\0';
	return (newlen);
 enospc:
	errno = ENOSPC;
	*buf = save_buf;
	*buflen = save_buflen;
	return (-1);
}

static int	charstr(const u_char *rdata, const u_char *edata,
			char **buf, size_t *buflen);
static void	addlen(size_t len, char **buf, size_t *buflen);
static int	addstr(const char *src, size_t len,
			   char **buf, size_t *buflen);
static int	addtab(size_t len, size_t target, int spaced,
			   char **buf, size_t *buflen);

/*
 * int
 * charstr(rdata, edata, buf, buflen)
 *	Format a <character-string> into the presentation buffer.
 * return:
 *	Number of rdata octets consumed
 *	0 for protocol format error
 *	-1 for output buffer error
 * side effects:
 *	buffer is advanced on success.
 */
static int
charstr(const u_char *rdata, const u_char *edata, char **buf, size_t *buflen) {
	const u_char *odata = rdata;
	size_t save_buflen = *buflen;
	char *save_buf = *buf;

	if (addstr("\"", 1, buf, buflen) < 0)
		goto enospc;
	if (rdata < edata) {
		int n = *rdata;

		if (rdata + 1 + n <= edata) {
			rdata++;
			while (n-- > 0) {
				if (strchr("\n\"\\", *rdata) != NULL)
					if (addstr("\\", 1, buf, buflen) < 0)
						goto enospc;
				if (addstr((const char *)rdata, 1,
					   buf, buflen) < 0)
					goto enospc;
				rdata++;
			}
		}
	}
	if (addstr("\"", 1, buf, buflen) < 0)
		goto enospc;
	return (rdata - odata);
 enospc:
	errno = ENOSPC;
	*buf = save_buf;
	*buflen = save_buflen;
	return (-1);
}

static void
addlen(size_t len, char **buf, size_t *buflen) {
	assert(len <= *buflen);
	*buf += len;
	*buflen -= len;
}

static int
addstr(const char *src, size_t len, char **buf, size_t *buflen) {
	if (len > *buflen) {
		errno = ENOSPC;
		return (-1);
	}
	memcpy(*buf, src, len);
	addlen(len, buf, buflen);
	**buf = '\0';
	return (0);
}

static int
addtab(size_t len, size_t target, int spaced, char **buf, size_t *buflen) {
	size_t save_buflen = *buflen;
	char *save_buf = *buf;
	int t;

	if (spaced || len >= target - 1) {
		T(addstr("  ", 2, buf, buflen));
		spaced = 1;
	} else {
		for (t = (target - len - 1) / 8; t >= 0; t--)
			if (addstr("        ", 1, buf, buflen) < 0) {
				*buflen = save_buflen;
				*buf = save_buf;
				return (-1);
			}
		spaced = 0;
	}
	return (spaced);
}

static int
addttl(struct signing_options *options, u_int32_t ttl, char *buf, size_t buflen) {
	char tbuf[16];
	int tlen;
	if (options->so_bind_extensions)
		return (ns_format_ttl(ttl, buf, buflen));
	sprintf(tbuf, "%d", ttl);
	tlen = strlen(tbuf);
	if (tlen > buflen)
		return (-1);
	strcpy(buf, tbuf);
	buf += tlen;
	return tlen;
}


/* Portions Copyright (c) 1995,1996,1997,1998 by Trusted Information Systems, Inc.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND TRUSTED INFORMATION SYSTEMS DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL TRUSTED INFORMATION 
 * SYSTEMS BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 *
 *
 * Trusted Information Systems, Inc. has received approval from the
 * United States Government for export and reexport of TIS/DNSSEC
 * software from the United States of America under the provisions of
 * the Export Administration Regulations (EAR) General Software Note
 * (GSN) license exception for mass market software.  Under the
 * provisions of this license, this software may be exported or
 * reexported to all destinations except for the embargoed countries of
 * Cuba, Iran, Iraq, Libya, North Korea, Sudan and Syria.  Any export
 * or reexport of TIS/DNSSEC software to the embargoed countries
 * requires additional, specific licensing approval from the United
 * States Government. 
 */



