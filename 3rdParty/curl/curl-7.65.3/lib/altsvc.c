/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) 2019, Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at https://curl.haxx.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ***************************************************************************/
/*
 * The Alt-Svc: header is defined in RFC 7838:
 * https://tools.ietf.org/html/rfc7838
 */
#include "curl_setup.h"

#if !defined(CURL_DISABLE_HTTP) && defined(USE_ALTSVC)
#include <curl/curl.h>
#include "urldata.h"
#include "altsvc.h"
#include "curl_get_line.h"
#include "strcase.h"
#include "parsedate.h"
#include "sendf.h"
#include "warnless.h"

/* The last 3 #include files should be in this order */
#include "curl_printf.h"
#include "curl_memory.h"
#include "memdebug.h"

#define MAX_ALTSVC_LINE 4095
#define MAX_ALTSVC_DATELENSTR "64"
#define MAX_ALTSVC_DATELEN 64
#define MAX_ALTSVC_HOSTLENSTR "512"
#define MAX_ALTSVC_HOSTLEN 512
#define MAX_ALTSVC_ALPNLENSTR "10"
#define MAX_ALTSVC_ALPNLEN 10

static enum alpnid alpn2alpnid(char *name)
{
  if(strcasecompare(name, "h1"))
    return ALPN_h1;
  if(strcasecompare(name, "h2"))
    return ALPN_h2;
  if(strcasecompare(name, "h2c"))
    return ALPN_h2c;
  if(strcasecompare(name, "h3"))
    return ALPN_h3;
  return ALPN_none; /* unknown, probably rubbish input */
}

/* Given the ALPN ID, return the name */
const char *Curl_alpnid2str(enum alpnid id)
{
  switch(id) {
  case ALPN_h1:
    return "h1";
  case ALPN_h2:
    return "h2";
  case ALPN_h2c:
    return "h2c";
  case ALPN_h3:
    return "h3";
  default:
    return ""; /* bad */
  }
}


static void altsvc_free(struct altsvc *as)
{
  free(as->srchost);
  free(as->dsthost);
  free(as);
}

static struct altsvc *altsvc_createid(const char *srchost,
                                      const char *dsthost,
                                      enum alpnid srcalpnid,
                                      enum alpnid dstalpnid,
                                      unsigned int srcport,
                                      unsigned int dstport)
{
  struct altsvc *as = calloc(sizeof(struct altsvc), 1);
  if(!as)
    return NULL;

  as->srchost = strdup(srchost);
  if(!as->srchost)
    goto error;
  as->dsthost = strdup(dsthost);
  if(!as->dsthost)
    goto error;

  as->srcalpnid = srcalpnid;
  as->dstalpnid = dstalpnid;
  as->srcport = curlx_ultous(srcport);
  as->dstport = curlx_ultous(dstport);

  return as;
  error:
  altsvc_free(as);
  return NULL;
}

static struct altsvc *altsvc_create(char *srchost,
                                    char *dsthost,
                                    char *srcalpn,
                                    char *dstalpn,
                                    unsigned int srcport,
                                    unsigned int dstport)
{
  enum alpnid dstalpnid = alpn2alpnid(dstalpn);
  enum alpnid srcalpnid = alpn2alpnid(srcalpn);
  if(!srcalpnid || !dstalpnid)
    return NULL;
  return altsvc_createid(srchost, dsthost, srcalpnid, dstalpnid,
                         srcport, dstport);
}

/* only returns SERIOUS errors */
static CURLcode altsvc_add(struct altsvcinfo *asi, char *line)
{
  /* Example line:
     h2 example.com 443 h3 shiny.example.com 8443 "20191231 10:00:00" 1
   */
  char srchost[MAX_ALTSVC_HOSTLEN + 1];
  char dsthost[MAX_ALTSVC_HOSTLEN + 1];
  char srcalpn[MAX_ALTSVC_ALPNLEN + 1];
  char dstalpn[MAX_ALTSVC_ALPNLEN + 1];
  char date[MAX_ALTSVC_DATELEN + 1];
  unsigned int srcport;
  unsigned int dstport;
  unsigned int prio;
  unsigned int persist;
  int rc;

  rc = sscanf(line,
              "%" MAX_ALTSVC_ALPNLENSTR "s %" MAX_ALTSVC_HOSTLENSTR "s %u "
              "%" MAX_ALTSVC_ALPNLENSTR "s %" MAX_ALTSVC_HOSTLENSTR "s %u "
              "\"%" MAX_ALTSVC_DATELENSTR "[^\"]\" %u %u",
              srcalpn, srchost, &srcport,
              dstalpn, dsthost, &dstport,
              date, &persist, &prio);
  if(9 == rc) {
    struct altsvc *as;
    time_t expires = curl_getdate(date, NULL);
    as = altsvc_create(srchost, dsthost, srcalpn, dstalpn, srcport, dstport);
    if(as) {
      as->expires = expires;
      as->prio = prio;
      as->persist = persist ? 1 : 0;
      Curl_llist_insert_next(&asi->list, asi->list.tail, as, &as->node);
      asi->num++; /* one more entry */
    }
  }

  return CURLE_OK;
}

/*
 * Load alt-svc entries from the given file. The text based line-oriented file
 * format is documented here:
 * https://github.com/curl/curl/wiki/QUIC-implementation
 *
 * This function only returns error on major problems that prevents alt-svc
 * handling to work completely. It will ignore individual syntactical errors
 * etc.
 */
static CURLcode altsvc_load(struct altsvcinfo *asi, const char *file)
{
  CURLcode result = CURLE_OK;
  char *line = NULL;
  FILE *fp = fopen(file, FOPEN_READTEXT);
  if(fp) {
    line = malloc(MAX_ALTSVC_LINE);
    if(!line)
      goto fail;
    while(Curl_get_line(line, MAX_ALTSVC_LINE, fp)) {
      char *lineptr = line;
      while(*lineptr && ISBLANK(*lineptr))
        lineptr++;
      if(*lineptr == '#')
        /* skip commented lines */
        continue;

      altsvc_add(asi, lineptr);
    }
    free(line); /* free the line buffer */
    fclose(fp);
  }
  return result;

  fail:
  free(line);
  fclose(fp);
  return CURLE_OUT_OF_MEMORY;
}

/*
 * Write this single altsvc entry to a single output line
 */

static CURLcode altsvc_out(struct altsvc *as, FILE *fp)
{
  struct tm stamp;
  CURLcode result = Curl_gmtime(as->expires, &stamp);
  if(result)
    return result;

  fprintf(fp,
          "%s %s %u "
          "%s %s %u "
          "\"%d%02d%02d "
          "%02d:%02d:%02d\" "
          "%u %d\n",
          Curl_alpnid2str(as->srcalpnid), as->srchost, as->srcport,
          Curl_alpnid2str(as->dstalpnid), as->dsthost, as->dstport,
          stamp.tm_year + 1900, stamp.tm_mon + 1, stamp.tm_mday,
          stamp.tm_hour, stamp.tm_min, stamp.tm_sec,
          as->persist, as->prio);
  return CURLE_OK;
}

/* ---- library-wide functions below ---- */

/*
 * Curl_altsvc_init() creates a new altsvc cache.
 * It returns the new instance or NULL if something goes wrong.
 */
struct altsvcinfo *Curl_altsvc_init(void)
{
  struct altsvcinfo *asi = calloc(sizeof(struct altsvcinfo), 1);
  if(!asi)
    return NULL;
  Curl_llist_init(&asi->list, NULL);

  /* set default behavior */
  asi->flags = CURLALTSVC_H1
#ifdef USE_NGHTTP2
    | CURLALTSVC_H2
#endif
#ifdef USE_HTTP3
    | CURLALTSVC_H3
#endif
    ;
  return asi;
}

/*
 * Curl_altsvc_load() loads alt-svc from file.
 */
CURLcode Curl_altsvc_load(struct altsvcinfo *asi, const char *file)
{
  CURLcode result;
  DEBUGASSERT(asi);
  result = altsvc_load(asi, file);
  return result;
}

/*
 * Curl_altsvc_ctrl() passes on the external bitmask.
 */
CURLcode Curl_altsvc_ctrl(struct altsvcinfo *asi, const long ctrl)
{
  DEBUGASSERT(asi);
  if(!ctrl)
    /* unexpected */
    return CURLE_BAD_FUNCTION_ARGUMENT;
  asi->flags = ctrl;
  return CURLE_OK;
}

/*
 * Curl_altsvc_cleanup() frees an altsvc cache instance and all associated
 * resources.
 */
void Curl_altsvc_cleanup(struct altsvcinfo *altsvc)
{
  struct curl_llist_element *e;
  struct curl_llist_element *n;
  if(altsvc) {
    for(e = altsvc->list.head; e; e = n) {
      struct altsvc *as = e->ptr;
      n = e->next;
      altsvc_free(as);
    }
    free(altsvc);
  }
}

/*
 * Curl_altsvc_save() writes the altsvc cache to a file.
 */
CURLcode Curl_altsvc_save(struct altsvcinfo *altsvc, const char *file)
{
  struct curl_llist_element *e;
  struct curl_llist_element *n;
  CURLcode result = CURLE_OK;
  FILE *out;

  if(!altsvc)
    /* no cache activated */
    return CURLE_OK;

  if((altsvc->flags & CURLALTSVC_READONLYFILE) || !file[0])
    /* marked as read-only or zero length file name */
    return CURLE_OK;
  out = fopen(file, FOPEN_WRITETEXT);
  if(!out)
    return CURLE_WRITE_ERROR;
  fputs("# Your alt-svc cache. https://curl.haxx.se/docs/alt-svc.html\n"
        "# This file was generated by libcurl! Edit at your own risk.\n",
        out);
  for(e = altsvc->list.head; e; e = n) {
    struct altsvc *as = e->ptr;
    n = e->next;
    result = altsvc_out(as, out);
    if(result)
      break;
  }
  fclose(out);
  return result;
}

static CURLcode getalnum(const char **ptr, char *alpnbuf, size_t buflen)
{
  size_t len;
  const char *protop;
  const char *p = *ptr;
  while(*p && ISBLANK(*p))
    p++;
  protop = p;
  while(*p && ISALNUM(*p))
    p++;
  len = p - protop;

  if(!len || (len >= buflen))
    return CURLE_BAD_FUNCTION_ARGUMENT;
  memcpy(alpnbuf, protop, len);
  alpnbuf[len] = 0;
  *ptr = p;
  return CURLE_OK;
}

/* altsvc_flush() removes all alternatives for this source origin from the
   list */
static void altsvc_flush(struct altsvcinfo *asi, enum alpnid srcalpnid,
                         const char *srchost, unsigned short srcport)
{
  struct curl_llist_element *e;
  struct curl_llist_element *n;
  for(e = asi->list.head; e; e = n) {
    struct altsvc *as = e->ptr;
    n = e->next;
    if((srcalpnid == as->srcalpnid) &&
       (srcport == as->srcport) &&
       strcasecompare(srchost, as->srchost)) {
      Curl_llist_remove(&asi->list, e, NULL);
      altsvc_free(as);
      asi->num--;
    }
  }
}

#ifdef DEBUGBUILD
/* to play well with debug builds, we can *set* a fixed time this will
   return */
static time_t debugtime(void *unused)
{
  char *timestr = getenv("CURL_TIME");
  (void)unused;
  if(timestr) {
    unsigned long val = strtol(timestr, NULL, 10);
    return (time_t)val;
  }
  return time(NULL);
}
#define time(x) debugtime(x)
#endif

/*
 * Curl_altsvc_parse() takes an incoming alt-svc response header and stores
 * the data correctly in the cache.
 *
 * 'value' points to the header *value*. That's contents to the right of the
 * header name.
 */
CURLcode Curl_altsvc_parse(struct Curl_easy *data,
                           struct altsvcinfo *asi, const char *value,
                           enum alpnid srcalpnid, const char *srchost,
                           unsigned short srcport)
{
  const char *p = value;
  size_t len;
  enum alpnid dstalpnid = srcalpnid; /* the same by default */
  char namebuf[MAX_ALTSVC_HOSTLEN] = "";
  char alpnbuf[MAX_ALTSVC_ALPNLEN] = "";
  struct altsvc *as;
  unsigned short dstport = srcport; /* the same by default */
  const char *semip;
  time_t maxage = 24 * 3600; /* default is 24 hours */
  bool persist = FALSE;
  CURLcode result = getalnum(&p, alpnbuf, sizeof(alpnbuf));
  if(result)
    return result;

  DEBUGASSERT(asi);

  /* Flush all cached alternatives for this source origin, if any */
  altsvc_flush(asi, srcalpnid, srchost, srcport);

  /* "clear" is a magic keyword */
  if(strcasecompare(alpnbuf, "clear")) {
    return CURLE_OK;
  }

  /* The 'ma' and 'persist' flags are annoyingly meant for all alternatives
     but are set after the list on the line. Scan for the semicolons and get
     those fields first! */
  semip = p;
  do {
    semip = strchr(semip, ';');
    if(semip) {
      char option[32];
      unsigned long num;
      char *end_ptr;
      semip++; /* pass the semicolon */
      result = getalnum(&semip, option, sizeof(option));
      if(result)
        break;
      while(*semip && ISBLANK(*semip))
        semip++;
      if(*semip != '=')
        continue;
      semip++;
      num = strtoul(semip, &end_ptr, 10);
      if(num < ULONG_MAX) {
        if(strcasecompare("ma", option))
          maxage = num;
        else if(strcasecompare("persist", option) && (num == 1))
          persist = TRUE;
      }
      semip = end_ptr;
    }
  } while(semip);

  do {
    if(*p == '=') {
      /* [protocol]="[host][:port]" */
      dstalpnid = alpn2alpnid(alpnbuf);
      if(!dstalpnid) {
        infof(data, "Unknown alt-svc protocol \"%s\", ignoring...\n", alpnbuf);
        return CURLE_OK;
      }
      p++;
      if(*p == '\"') {
        const char *dsthost;
        p++;
        if(*p != ':') {
          /* host name starts here */
          const char *hostp = p;
          while(*p && (ISALNUM(*p) || (*p == '.') || (*p == '-')))
            p++;
          len = p - hostp;
          if(!len || (len >= MAX_ALTSVC_HOSTLEN))
            return CURLE_BAD_FUNCTION_ARGUMENT;
          memcpy(namebuf, hostp, len);
          namebuf[len] = 0;
          dsthost = namebuf;
        }
        else {
          /* no destination name, use source host */
          dsthost = srchost;
        }
        if(*p == ':') {
          /* a port number */
          char *end_ptr;
          unsigned long port = strtoul(++p, &end_ptr, 10);
          if(port > USHRT_MAX || end_ptr == p || *end_ptr != '\"') {
            infof(data, "Unknown alt-svc port number, ignoring...\n");
            return CURLE_OK;
          }
          p = end_ptr;
          dstport = curlx_ultous(port);
        }
        if(*p++ != '\"')
          return CURLE_BAD_FUNCTION_ARGUMENT;
        as = altsvc_createid(srchost, dsthost,
                             srcalpnid, dstalpnid,
                             srcport, dstport);
        if(as) {
          /* The expires time also needs to take the Age: value (if any) into
             account. [See RFC 7838 section 3.1] */
          as->expires = maxage + time(NULL);
          as->persist = persist;
          Curl_llist_insert_next(&asi->list, asi->list.tail, as, &as->node);
          asi->num++; /* one more entry */
          infof(data, "Added alt-svc: %s:%d over %s\n", dsthost, dstport,
                Curl_alpnid2str(dstalpnid));
        }
      }
      /* after the double quote there can be a comma if there's another
         string or a semicolon if no more */
      if(*p == ',') {
        /* comma means another alternative is presented */
        p++;
        result = getalnum(&p, alpnbuf, sizeof(alpnbuf));
        if(result)
          /* failed to parse, but since we already did at least one host we
             return OK */
          return CURLE_OK;
      }
    }
  } while(*p && (*p != ';') && (*p != '\n') && (*p != '\r'));

  return CURLE_OK;
}

/*
 * Return TRUE on a match
 */
bool Curl_altsvc_lookup(struct altsvcinfo *asi,
                        enum alpnid srcalpnid, const char *srchost,
                        int srcport,
                        enum alpnid *dstalpnid, const char **dsthost,
                        int *dstport)
{
  struct curl_llist_element *e;
  struct curl_llist_element *n;
  time_t now = time(NULL);
  DEBUGASSERT(asi);
  DEBUGASSERT(srchost);
  DEBUGASSERT(dsthost);

  for(e = asi->list.head; e; e = n) {
    struct altsvc *as = e->ptr;
    n = e->next;
    if(as->expires < now) {
      /* an expired entry, remove */
      altsvc_free(as);
      continue;
    }
    if((as->srcalpnid == srcalpnid) &&
       strcasecompare(as->srchost, srchost) &&
       as->srcport == srcport) {
      /* match */
      *dstalpnid = as->dstalpnid;
      *dsthost = as->dsthost;
      *dstport = as->dstport;
      return TRUE;
    }
  }
  return FALSE;
}

#endif /* CURL_DISABLE_HTTP || USE_ALTSVC */
