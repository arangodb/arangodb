/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) 1998 - 2017, Daniel Stenberg, <daniel@haxx.se>, et al.
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

#include "curl_setup.h"

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef __VMS
#include <in.h>
#include <inet.h>
#endif

#ifdef HAVE_SETJMP_H
#include <setjmp.h>
#endif
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

#ifdef HAVE_PROCESS_H
#include <process.h>
#endif

#include "urldata.h"
#include "sendf.h"
#include "hostip.h"
#include "hash.h"
#include "share.h"
#include "strerror.h"
#include "url.h"
#include "inet_ntop.h"
#include "warnless.h"
/* The last 3 #include files should be in this order */
#include "curl_printf.h"
#include "curl_memory.h"
#include "memdebug.h"

#if defined(CURLRES_SYNCH) && \
    defined(HAVE_ALARM) && defined(SIGALRM) && defined(HAVE_SIGSETJMP)
/* alarm-based timeouts can only be used with all the dependencies satisfied */
#define USE_ALARM_TIMEOUT
#endif

/*
 * hostip.c explained
 * ==================
 *
 * The main COMPILE-TIME DEFINES to keep in mind when reading the host*.c
 * source file are these:
 *
 * CURLRES_IPV6 - this host has getaddrinfo() and family, and thus we use
 * that. The host may not be able to resolve IPv6, but we don't really have to
 * take that into account. Hosts that aren't IPv6-enabled have CURLRES_IPV4
 * defined.
 *
 * CURLRES_ARES - is defined if libcurl is built to use c-ares for
 * asynchronous name resolves. This can be Windows or *nix.
 *
 * CURLRES_THREADED - is defined if libcurl is built to run under (native)
 * Windows, and then the name resolve will be done in a new thread, and the
 * supported API will be the same as for ares-builds.
 *
 * If any of the two previous are defined, CURLRES_ASYNCH is defined too. If
 * libcurl is not built to use an asynchronous resolver, CURLRES_SYNCH is
 * defined.
 *
 * The host*.c sources files are split up like this:
 *
 * hostip.c   - method-independent resolver functions and utility functions
 * hostasyn.c - functions for asynchronous name resolves
 * hostsyn.c  - functions for synchronous name resolves
 * hostip4.c  - IPv4 specific functions
 * hostip6.c  - IPv6 specific functions
 *
 * The two asynchronous name resolver backends are implemented in:
 * asyn-ares.c   - functions for ares-using name resolves
 * asyn-thread.c - functions for threaded name resolves

 * The hostip.h is the united header file for all this. It defines the
 * CURLRES_* defines based on the config*.h and curl_setup.h defines.
 */

/* These two symbols are for the global DNS cache */
static struct curl_hash hostname_cache;
static int host_cache_initialized;

static void freednsentry(void *freethis);

/*
 * Curl_global_host_cache_init() initializes and sets up a global DNS cache.
 * Global DNS cache is general badness. Do not use. This will be removed in
 * a future version. Use the share interface instead!
 *
 * Returns a struct curl_hash pointer on success, NULL on failure.
 */
struct curl_hash *Curl_global_host_cache_init(void)
{
  int rc = 0;
  if(!host_cache_initialized) {
    rc = Curl_hash_init(&hostname_cache, 7, Curl_hash_str,
                        Curl_str_key_compare, freednsentry);
    if(!rc)
      host_cache_initialized = 1;
  }
  return rc?NULL:&hostname_cache;
}

/*
 * Destroy and cleanup the global DNS cache
 */
void Curl_global_host_cache_dtor(void)
{
  if(host_cache_initialized) {
    Curl_hash_destroy(&hostname_cache);
    host_cache_initialized = 0;
  }
}

/*
 * Return # of addresses in a Curl_addrinfo struct
 */
int Curl_num_addresses(const Curl_addrinfo *addr)
{
  int i = 0;
  while(addr) {
    addr = addr->ai_next;
    i++;
  }
  return i;
}

/*
 * Curl_printable_address() returns a printable version of the 1st address
 * given in the 'ai' argument. The result will be stored in the buf that is
 * bufsize bytes big.
 *
 * If the conversion fails, it returns NULL.
 */
const char *
Curl_printable_address(const Curl_addrinfo *ai, char *buf, size_t bufsize)
{
  const struct sockaddr_in *sa4;
  const struct in_addr *ipaddr4;
#ifdef ENABLE_IPV6
  const struct sockaddr_in6 *sa6;
  const struct in6_addr *ipaddr6;
#endif

  switch(ai->ai_family) {
    case AF_INET:
      sa4 = (const void *)ai->ai_addr;
      ipaddr4 = &sa4->sin_addr;
      return Curl_inet_ntop(ai->ai_family, (const void *)ipaddr4, buf,
                            bufsize);
#ifdef ENABLE_IPV6
    case AF_INET6:
      sa6 = (const void *)ai->ai_addr;
      ipaddr6 = &sa6->sin6_addr;
      return Curl_inet_ntop(ai->ai_family, (const void *)ipaddr6, buf,
                            bufsize);
#endif
    default:
      break;
  }
  return NULL;
}

/*
 * Return a hostcache id string for the provided host + port, to be used by
 * the DNS caching.
 */
static char *
create_hostcache_id(const char *name, int port)
{
  /* create and return the new allocated entry */
  char *id = aprintf("%s:%d", name, port);
  char *ptr = id;
  if(ptr) {
    /* lower case the name part */
    while(*ptr && (*ptr != ':')) {
      *ptr = (char)TOLOWER(*ptr);
      ptr++;
    }
  }
  return id;
}

struct hostcache_prune_data {
  long cache_timeout;
  time_t now;
};

/*
 * This function is set as a callback to be called for every entry in the DNS
 * cache when we want to prune old unused entries.
 *
 * Returning non-zero means remove the entry, return 0 to keep it in the
 * cache.
 */
static int
hostcache_timestamp_remove(void *datap, void *hc)
{
  struct hostcache_prune_data *data =
    (struct hostcache_prune_data *) datap;
  struct Curl_dns_entry *c = (struct Curl_dns_entry *) hc;

  return (0 != c->timestamp)
    && (data->now - c->timestamp >= data->cache_timeout);
}

/*
 * Prune the DNS cache. This assumes that a lock has already been taken.
 */
static void
hostcache_prune(struct curl_hash *hostcache, long cache_timeout, time_t now)
{
  struct hostcache_prune_data user;

  user.cache_timeout = cache_timeout;
  user.now = now;

  Curl_hash_clean_with_criterium(hostcache,
                                 (void *) &user,
                                 hostcache_timestamp_remove);
}

/*
 * Library-wide function for pruning the DNS cache. This function takes and
 * returns the appropriate locks.
 */
void Curl_hostcache_prune(struct Curl_easy *data)
{
  time_t now;

  if((data->set.dns_cache_timeout == -1) || !data->dns.hostcache)
    /* cache forever means never prune, and NULL hostcache means
       we can't do it */
    return;

  if(data->share)
    Curl_share_lock(data, CURL_LOCK_DATA_DNS, CURL_LOCK_ACCESS_SINGLE);

  time(&now);

  /* Remove outdated and unused entries from the hostcache */
  hostcache_prune(data->dns.hostcache,
                  data->set.dns_cache_timeout,
                  now);

  if(data->share)
    Curl_share_unlock(data, CURL_LOCK_DATA_DNS);
}

#ifdef HAVE_SIGSETJMP
/* Beware this is a global and unique instance. This is used to store the
   return address that we can jump back to from inside a signal handler. This
   is not thread-safe stuff. */
sigjmp_buf curl_jmpenv;
#endif

/* lookup address, returns entry if found and not stale */
static struct Curl_dns_entry *
fetch_addr(struct connectdata *conn,
                const char *hostname,
                int port)
{
  char *entry_id = NULL;
  struct Curl_dns_entry *dns = NULL;
  size_t entry_len;
  struct Curl_easy *data = conn->data;

  /* Create an entry id, based upon the hostname and port */
  entry_id = create_hostcache_id(hostname, port);
  /* If we can't create the entry id, fail */
  if(!entry_id)
    return dns;

  entry_len = strlen(entry_id);

  /* See if its already in our dns cache */
  dns = Curl_hash_pick(data->dns.hostcache, entry_id, entry_len + 1);

  if(dns && (data->set.dns_cache_timeout != -1)) {
    /* See whether the returned entry is stale. Done before we release lock */
    struct hostcache_prune_data user;

    time(&user.now);
    user.cache_timeout = data->set.dns_cache_timeout;

    if(hostcache_timestamp_remove(&user, dns)) {
      infof(data, "Hostname in DNS cache was stale, zapped\n");
      dns = NULL; /* the memory deallocation is being handled by the hash */
      Curl_hash_delete(data->dns.hostcache, entry_id, entry_len + 1);
    }
  }

  /* free the allocated entry_id again */
  free(entry_id);

  return dns;
}

/*
 * Curl_fetch_addr() fetches a 'Curl_dns_entry' already in the DNS cache.
 *
 * Curl_resolv() checks initially and multi_runsingle() checks each time
 * it discovers the handle in the state WAITRESOLVE whether the hostname
 * has already been resolved and the address has already been stored in
 * the DNS cache. This short circuits waiting for a lot of pending
 * lookups for the same hostname requested by different handles.
 *
 * Returns the Curl_dns_entry entry pointer or NULL if not in the cache.
 *
 * The returned data *MUST* be "unlocked" with Curl_resolv_unlock() after
 * use, or we'll leak memory!
 */
struct Curl_dns_entry *
Curl_fetch_addr(struct connectdata *conn,
                const char *hostname,
                int port)
{
  struct Curl_easy *data = conn->data;
  struct Curl_dns_entry *dns = NULL;

  if(data->share)
    Curl_share_lock(data, CURL_LOCK_DATA_DNS, CURL_LOCK_ACCESS_SINGLE);

  dns = fetch_addr(conn, hostname, port);

  if(dns)
    dns->inuse++; /* we use it! */

  if(data->share)
    Curl_share_unlock(data, CURL_LOCK_DATA_DNS);

  return dns;
}

/*
 * Curl_cache_addr() stores a 'Curl_addrinfo' struct in the DNS cache.
 *
 * When calling Curl_resolv() has resulted in a response with a returned
 * address, we call this function to store the information in the dns
 * cache etc
 *
 * Returns the Curl_dns_entry entry pointer or NULL if the storage failed.
 */
struct Curl_dns_entry *
Curl_cache_addr(struct Curl_easy *data,
                Curl_addrinfo *addr,
                const char *hostname,
                int port)
{
  char *entry_id;
  size_t entry_len;
  struct Curl_dns_entry *dns;
  struct Curl_dns_entry *dns2;

  /* Create an entry id, based upon the hostname and port */
  entry_id = create_hostcache_id(hostname, port);
  /* If we can't create the entry id, fail */
  if(!entry_id)
    return NULL;
  entry_len = strlen(entry_id);

  /* Create a new cache entry */
  dns = calloc(1, sizeof(struct Curl_dns_entry));
  if(!dns) {
    free(entry_id);
    return NULL;
  }

  dns->inuse = 1;   /* the cache has the first reference */
  dns->addr = addr; /* this is the address(es) */
  time(&dns->timestamp);
  if(dns->timestamp == 0)
    dns->timestamp = 1;   /* zero indicates CURLOPT_RESOLVE entry */

  /* Store the resolved data in our DNS cache. */
  dns2 = Curl_hash_add(data->dns.hostcache, entry_id, entry_len + 1,
                       (void *)dns);
  if(!dns2) {
    free(dns);
    free(entry_id);
    return NULL;
  }

  dns = dns2;
  dns->inuse++;         /* mark entry as in-use */

  /* free the allocated entry_id */
  free(entry_id);

  return dns;
}

/*
 * Curl_resolv() is the main name resolve function within libcurl. It resolves
 * a name and returns a pointer to the entry in the 'entry' argument (if one
 * is provided). This function might return immediately if we're using asynch
 * resolves. See the return codes.
 *
 * The cache entry we return will get its 'inuse' counter increased when this
 * function is used. You MUST call Curl_resolv_unlock() later (when you're
 * done using this struct) to decrease the counter again.
 *
 * In debug mode, we specifically test for an interface name "LocalHost"
 * and resolve "localhost" instead as a means to permit test cases
 * to connect to a local test server with any host name.
 *
 * Return codes:
 *
 * CURLRESOLV_ERROR   (-1) = error, no pointer
 * CURLRESOLV_RESOLVED (0) = OK, pointer provided
 * CURLRESOLV_PENDING  (1) = waiting for response, no pointer
 */

int Curl_resolv(struct connectdata *conn,
                const char *hostname,
                int port,
                struct Curl_dns_entry **entry)
{
  struct Curl_dns_entry *dns = NULL;
  struct Curl_easy *data = conn->data;
  CURLcode result;
  int rc = CURLRESOLV_ERROR; /* default to failure */

  *entry = NULL;

  if(data->share)
    Curl_share_lock(data, CURL_LOCK_DATA_DNS, CURL_LOCK_ACCESS_SINGLE);

  dns = fetch_addr(conn, hostname, port);

  if(dns) {
    infof(data, "Hostname %s was found in DNS cache\n", hostname);
    dns->inuse++; /* we use it! */
    rc = CURLRESOLV_RESOLVED;
  }

  if(data->share)
    Curl_share_unlock(data, CURL_LOCK_DATA_DNS);

  if(!dns) {
    /* The entry was not in the cache. Resolve it to IP address */

    Curl_addrinfo *addr;
    int respwait;

    /* Check what IP specifics the app has requested and if we can provide it.
     * If not, bail out. */
    if(!Curl_ipvalid(conn))
      return CURLRESOLV_ERROR;

    /* If Curl_getaddrinfo() returns NULL, 'respwait' might be set to a
       non-zero value indicating that we need to wait for the response to the
       resolve call */
    addr = Curl_getaddrinfo(conn,
#ifdef DEBUGBUILD
                            (data->set.str[STRING_DEVICE]
                             && !strcmp(data->set.str[STRING_DEVICE],
                                        "LocalHost"))?"localhost":
#endif
                            hostname, port, &respwait);

    if(!addr) {
      if(respwait) {
        /* the response to our resolve call will come asynchronously at
           a later time, good or bad */
        /* First, check that we haven't received the info by now */
        result = Curl_resolver_is_resolved(conn, &dns);
        if(result) /* error detected */
          return CURLRESOLV_ERROR;
        if(dns)
          rc = CURLRESOLV_RESOLVED; /* pointer provided */
        else
          rc = CURLRESOLV_PENDING; /* no info yet */
      }
    }
    else {
      if(data->share)
        Curl_share_lock(data, CURL_LOCK_DATA_DNS, CURL_LOCK_ACCESS_SINGLE);

      /* we got a response, store it in the cache */
      dns = Curl_cache_addr(data, addr, hostname, port);

      if(data->share)
        Curl_share_unlock(data, CURL_LOCK_DATA_DNS);

      if(!dns)
        /* returned failure, bail out nicely */
        Curl_freeaddrinfo(addr);
      else
        rc = CURLRESOLV_RESOLVED;
    }
  }

  *entry = dns;

  return rc;
}

#ifdef USE_ALARM_TIMEOUT
/*
 * This signal handler jumps back into the main libcurl code and continues
 * execution.  This effectively causes the remainder of the application to run
 * within a signal handler which is nonportable and could lead to problems.
 */
static
RETSIGTYPE alarmfunc(int sig)
{
  /* this is for "-ansi -Wall -pedantic" to stop complaining!   (rabe) */
  (void)sig;
  siglongjmp(curl_jmpenv, 1);
}
#endif /* USE_ALARM_TIMEOUT */

/*
 * Curl_resolv_timeout() is the same as Curl_resolv() but specifies a
 * timeout.  This function might return immediately if we're using asynch
 * resolves. See the return codes.
 *
 * The cache entry we return will get its 'inuse' counter increased when this
 * function is used. You MUST call Curl_resolv_unlock() later (when you're
 * done using this struct) to decrease the counter again.
 *
 * If built with a synchronous resolver and use of signals is not
 * disabled by the application, then a nonzero timeout will cause a
 * timeout after the specified number of milliseconds. Otherwise, timeout
 * is ignored.
 *
 * Return codes:
 *
 * CURLRESOLV_TIMEDOUT(-2) = warning, time too short or previous alarm expired
 * CURLRESOLV_ERROR   (-1) = error, no pointer
 * CURLRESOLV_RESOLVED (0) = OK, pointer provided
 * CURLRESOLV_PENDING  (1) = waiting for response, no pointer
 */

int Curl_resolv_timeout(struct connectdata *conn,
                        const char *hostname,
                        int port,
                        struct Curl_dns_entry **entry,
                        time_t timeoutms)
{
#ifdef USE_ALARM_TIMEOUT
#ifdef HAVE_SIGACTION
  struct sigaction keep_sigact;   /* store the old struct here */
  volatile bool keep_copysig = FALSE; /* whether old sigact has been saved */
  struct sigaction sigact;
#else
#ifdef HAVE_SIGNAL
  void (*keep_sigact)(int);       /* store the old handler here */
#endif /* HAVE_SIGNAL */
#endif /* HAVE_SIGACTION */
  volatile long timeout;
  volatile unsigned int prev_alarm = 0;
  struct Curl_easy *data = conn->data;
#endif /* USE_ALARM_TIMEOUT */
  int rc;

  *entry = NULL;

  if(timeoutms < 0)
    /* got an already expired timeout */
    return CURLRESOLV_TIMEDOUT;

#ifdef USE_ALARM_TIMEOUT
  if(data->set.no_signal)
    /* Ignore the timeout when signals are disabled */
    timeout = 0;
  else
    timeout = (timeoutms > LONG_MAX) ? LONG_MAX : (long)timeoutms;

  if(!timeout)
    /* USE_ALARM_TIMEOUT defined, but no timeout actually requested */
    return Curl_resolv(conn, hostname, port, entry);

  if(timeout < 1000) {
    /* The alarm() function only provides integer second resolution, so if
       we want to wait less than one second we must bail out already now. */
    failf(data,
        "remaining timeout of %ld too small to resolve via SIGALRM method",
        timeout);
    return CURLRESOLV_TIMEDOUT;
  }
  /* This allows us to time-out from the name resolver, as the timeout
     will generate a signal and we will siglongjmp() from that here.
     This technique has problems (see alarmfunc).
     This should be the last thing we do before calling Curl_resolv(),
     as otherwise we'd have to worry about variables that get modified
     before we invoke Curl_resolv() (and thus use "volatile"). */
  if(sigsetjmp(curl_jmpenv, 1)) {
    /* this is coming from a siglongjmp() after an alarm signal */
    failf(data, "name lookup timed out");
    rc = CURLRESOLV_ERROR;
    goto clean_up;
  }
  else {
    /*************************************************************
     * Set signal handler to catch SIGALRM
     * Store the old value to be able to set it back later!
     *************************************************************/
#ifdef HAVE_SIGACTION
    sigaction(SIGALRM, NULL, &sigact);
    keep_sigact = sigact;
    keep_copysig = TRUE; /* yes, we have a copy */
    sigact.sa_handler = alarmfunc;
#ifdef SA_RESTART
    /* HPUX doesn't have SA_RESTART but defaults to that behaviour! */
    sigact.sa_flags &= ~SA_RESTART;
#endif
    /* now set the new struct */
    sigaction(SIGALRM, &sigact, NULL);
#else /* HAVE_SIGACTION */
    /* no sigaction(), revert to the much lamer signal() */
#ifdef HAVE_SIGNAL
    keep_sigact = signal(SIGALRM, alarmfunc);
#endif
#endif /* HAVE_SIGACTION */

    /* alarm() makes a signal get sent when the timeout fires off, and that
       will abort system calls */
    prev_alarm = alarm(curlx_sltoui(timeout/1000L));
  }

#else
#ifndef CURLRES_ASYNCH
  if(timeoutms)
    infof(conn->data, "timeout on name lookup is not supported\n");
#else
  (void)timeoutms; /* timeoutms not used with an async resolver */
#endif
#endif /* USE_ALARM_TIMEOUT */

  /* Perform the actual name resolution. This might be interrupted by an
   * alarm if it takes too long.
   */
  rc = Curl_resolv(conn, hostname, port, entry);

#ifdef USE_ALARM_TIMEOUT
clean_up:

  if(!prev_alarm)
    /* deactivate a possibly active alarm before uninstalling the handler */
    alarm(0);

#ifdef HAVE_SIGACTION
  if(keep_copysig) {
    /* we got a struct as it looked before, now put that one back nice
       and clean */
    sigaction(SIGALRM, &keep_sigact, NULL); /* put it back */
  }
#else
#ifdef HAVE_SIGNAL
  /* restore the previous SIGALRM handler */
  signal(SIGALRM, keep_sigact);
#endif
#endif /* HAVE_SIGACTION */

  /* switch back the alarm() to either zero or to what it was before minus
     the time we spent until now! */
  if(prev_alarm) {
    /* there was an alarm() set before us, now put it back */
    timediff_t elapsed_secs = Curl_timediff(Curl_now(),
                                            conn->created) / 1000;

    /* the alarm period is counted in even number of seconds */
    unsigned long alarm_set = prev_alarm - elapsed_secs;

    if(!alarm_set ||
       ((alarm_set >= 0x80000000) && (prev_alarm < 0x80000000)) ) {
      /* if the alarm time-left reached zero or turned "negative" (counted
         with unsigned values), we should fire off a SIGALRM here, but we
         won't, and zero would be to switch it off so we never set it to
         less than 1! */
      alarm(1);
      rc = CURLRESOLV_TIMEDOUT;
      failf(data, "Previous alarm fired off!");
    }
    else
      alarm((unsigned int)alarm_set);
  }
#endif /* USE_ALARM_TIMEOUT */

  return rc;
}

/*
 * Curl_resolv_unlock() unlocks the given cached DNS entry. When this has been
 * made, the struct may be destroyed due to pruning. It is important that only
 * one unlock is made for each Curl_resolv() call.
 *
 * May be called with 'data' == NULL for global cache.
 */
void Curl_resolv_unlock(struct Curl_easy *data, struct Curl_dns_entry *dns)
{
  if(data && data->share)
    Curl_share_lock(data, CURL_LOCK_DATA_DNS, CURL_LOCK_ACCESS_SINGLE);

  freednsentry(dns);

  if(data && data->share)
    Curl_share_unlock(data, CURL_LOCK_DATA_DNS);
}

/*
 * File-internal: release cache dns entry reference, free if inuse drops to 0
 */
static void freednsentry(void *freethis)
{
  struct Curl_dns_entry *dns = (struct Curl_dns_entry *) freethis;
  DEBUGASSERT(dns && (dns->inuse>0));

  dns->inuse--;
  if(dns->inuse == 0) {
    Curl_freeaddrinfo(dns->addr);
    free(dns);
  }
}

/*
 * Curl_mk_dnscache() inits a new DNS cache and returns success/failure.
 */
int Curl_mk_dnscache(struct curl_hash *hash)
{
  return Curl_hash_init(hash, 7, Curl_hash_str, Curl_str_key_compare,
                        freednsentry);
}

/*
 * Curl_hostcache_clean()
 *
 * This _can_ be called with 'data' == NULL but then of course no locking
 * can be done!
 */

void Curl_hostcache_clean(struct Curl_easy *data,
                          struct curl_hash *hash)
{
  if(data && data->share)
    Curl_share_lock(data, CURL_LOCK_DATA_DNS, CURL_LOCK_ACCESS_SINGLE);

  Curl_hash_clean(hash);

  if(data && data->share)
    Curl_share_unlock(data, CURL_LOCK_DATA_DNS);
}


CURLcode Curl_loadhostpairs(struct Curl_easy *data)
{
  struct curl_slist *hostp;
  char hostname[256];
  int port;

  for(hostp = data->change.resolve; hostp; hostp = hostp->next) {
    if(!hostp->data)
      continue;
    if(hostp->data[0] == '-') {
      char *entry_id;
      size_t entry_len;

      if(2 != sscanf(hostp->data + 1, "%255[^:]:%d", hostname, &port)) {
        infof(data, "Couldn't parse CURLOPT_RESOLVE removal entry '%s'!\n",
              hostp->data);
        continue;
      }

      /* Create an entry id, based upon the hostname and port */
      entry_id = create_hostcache_id(hostname, port);
      /* If we can't create the entry id, fail */
      if(!entry_id) {
        return CURLE_OUT_OF_MEMORY;
      }

      entry_len = strlen(entry_id);

      if(data->share)
        Curl_share_lock(data, CURL_LOCK_DATA_DNS, CURL_LOCK_ACCESS_SINGLE);

      /* delete entry, ignore if it didn't exist */
      Curl_hash_delete(data->dns.hostcache, entry_id, entry_len + 1);

      if(data->share)
        Curl_share_unlock(data, CURL_LOCK_DATA_DNS);

      /* free the allocated entry_id again */
      free(entry_id);
    }
    else {
      struct Curl_dns_entry *dns;
      Curl_addrinfo *addr;
      char *entry_id;
      size_t entry_len;
      char buffer[256];
      char *address = &buffer[0];

      if(3 != sscanf(hostp->data, "%255[^:]:%d:%255s", hostname, &port,
                     address)) {
        infof(data, "Couldn't parse CURLOPT_RESOLVE entry '%s'!\n",
              hostp->data);
        continue;
      }

      /* allow IP(v6) address within [brackets] */
      if(address[0] == '[') {
        size_t alen = strlen(address);
        if(address[alen-1] != ']')
          /* it needs to also end with ] to be valid */
          continue;
        address[alen-1] = 0; /* zero terminate there */
        address++; /* pass the open bracket */
      }

      addr = Curl_str2addr(address, port);
      if(!addr) {
        infof(data, "Address in '%s' found illegal!\n", hostp->data);
        continue;
      }

      /* Create an entry id, based upon the hostname and port */
      entry_id = create_hostcache_id(hostname, port);
      /* If we can't create the entry id, fail */
      if(!entry_id) {
        Curl_freeaddrinfo(addr);
        return CURLE_OUT_OF_MEMORY;
      }

      entry_len = strlen(entry_id);

      if(data->share)
        Curl_share_lock(data, CURL_LOCK_DATA_DNS, CURL_LOCK_ACCESS_SINGLE);

      /* See if its already in our dns cache */
      dns = Curl_hash_pick(data->dns.hostcache, entry_id, entry_len + 1);

      /* free the allocated entry_id again */
      free(entry_id);

      if(!dns) {
        /* if not in the cache already, put this host in the cache */
        dns = Curl_cache_addr(data, addr, hostname, port);
        if(dns) {
          dns->timestamp = 0; /* mark as added by CURLOPT_RESOLVE */
          /* release the returned reference; the cache itself will keep the
           * entry alive: */
          dns->inuse--;
        }
      }
      else
        /* this is a duplicate, free it again */
        Curl_freeaddrinfo(addr);

      if(data->share)
        Curl_share_unlock(data, CURL_LOCK_DATA_DNS);

      if(!dns) {
        Curl_freeaddrinfo(addr);
        return CURLE_OUT_OF_MEMORY;
      }
      infof(data, "Added %s:%d:%s to DNS cache\n",
            hostname, port, address);
    }
  }
  data->change.resolve = NULL; /* dealt with now */

  return CURLE_OK;
}
