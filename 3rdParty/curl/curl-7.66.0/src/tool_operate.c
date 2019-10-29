/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) 1998 - 2019, Daniel Stenberg, <daniel@haxx.se>, et al.
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
#include "tool_setup.h"

#ifdef HAVE_FCNTL_H
#  include <fcntl.h>
#endif

#ifdef HAVE_LOCALE_H
#  include <locale.h>
#endif

#ifdef HAVE_SYS_SELECT_H
#  include <sys/select.h>
#endif

#ifdef __VMS
#  include <fabdef.h>
#endif

#ifdef __AMIGA__
#  include <proto/dos.h>
#endif

#include "strcase.h"

#define ENABLE_CURLX_PRINTF
/* use our own printf() functions */
#include "curlx.h"

#include "tool_binmode.h"
#include "tool_cfgable.h"
#include "tool_cb_dbg.h"
#include "tool_cb_hdr.h"
#include "tool_cb_prg.h"
#include "tool_cb_rea.h"
#include "tool_cb_see.h"
#include "tool_cb_wrt.h"
#include "tool_dirhie.h"
#include "tool_doswin.h"
#include "tool_easysrc.h"
#include "tool_filetime.h"
#include "tool_getparam.h"
#include "tool_helpers.h"
#include "tool_homedir.h"
#include "tool_libinfo.h"
#include "tool_main.h"
#include "tool_metalink.h"
#include "tool_msgs.h"
#include "tool_operate.h"
#include "tool_operhlp.h"
#include "tool_paramhlp.h"
#include "tool_parsecfg.h"
#include "tool_setopt.h"
#include "tool_sleep.h"
#include "tool_urlglob.h"
#include "tool_util.h"
#include "tool_writeout.h"
#include "tool_xattr.h"
#include "tool_vms.h"
#include "tool_help.h"
#include "tool_hugehelp.h"
#include "tool_progress.h"

#include "memdebug.h" /* keep this as LAST include */

#ifdef CURLDEBUG
/* libcurl's debug builds provide an extra function */
CURLcode curl_easy_perform_ev(CURL *easy);
#endif

#define CURLseparator  "--_curl_--"

#ifndef O_BINARY
/* since O_BINARY as used in bitmasks, setting it to zero makes it usable in
   source code but yet it doesn't ruin anything */
#  define O_BINARY 0
#endif

#define CURL_CA_CERT_ERRORMSG                                               \
  "More details here: https://curl.haxx.se/docs/sslcerts.html\n\n"          \
  "curl failed to verify the legitimacy of the server and therefore "       \
  "could not\nestablish a secure connection to it. To learn more about "    \
  "this situation and\nhow to fix it, please visit the web page mentioned " \
  "above.\n"

static CURLcode create_transfers(struct GlobalConfig *global,
                                 struct OperationConfig *config,
                                 CURLSH *share,
                                 bool capath_from_env);

static bool is_fatal_error(CURLcode code)
{
  switch(code) {
  case CURLE_FAILED_INIT:
  case CURLE_OUT_OF_MEMORY:
  case CURLE_UNKNOWN_OPTION:
  case CURLE_FUNCTION_NOT_FOUND:
  case CURLE_BAD_FUNCTION_ARGUMENT:
    /* critical error */
    return TRUE;
  default:
    break;
  }

  /* no error or not critical */
  return FALSE;
}

/*
 * Check if a given string is a PKCS#11 URI
 */
static bool is_pkcs11_uri(const char *string)
{
  if(curl_strnequal(string, "pkcs11:", 7)) {
    return TRUE;
  }
  else {
    return FALSE;
  }
}

#ifdef __VMS
/*
 * get_vms_file_size does what it takes to get the real size of the file
 *
 * For fixed files, find out the size of the EOF block and adjust.
 *
 * For all others, have to read the entire file in, discarding the contents.
 * Most posted text files will be small, and binary files like zlib archives
 * and CD/DVD images should be either a STREAM_LF format or a fixed format.
 *
 */
static curl_off_t vms_realfilesize(const char *name,
                                   const struct_stat *stat_buf)
{
  char buffer[8192];
  curl_off_t count;
  int ret_stat;
  FILE * file;

  /* !checksrc! disable FOPENMODE 1 */
  file = fopen(name, "r"); /* VMS */
  if(file == NULL) {
    return 0;
  }
  count = 0;
  ret_stat = 1;
  while(ret_stat > 0) {
    ret_stat = fread(buffer, 1, sizeof(buffer), file);
    if(ret_stat != 0)
      count += ret_stat;
  }
  fclose(file);

  return count;
}

/*
 *
 *  VmsSpecialSize checks to see if the stat st_size can be trusted and
 *  if not to call a routine to get the correct size.
 *
 */
static curl_off_t VmsSpecialSize(const char *name,
                                 const struct_stat *stat_buf)
{
  switch(stat_buf->st_fab_rfm) {
  case FAB$C_VAR:
  case FAB$C_VFC:
    return vms_realfilesize(name, stat_buf);
    break;
  default:
    return stat_buf->st_size;
  }
}
#endif /* __VMS */

#define BUFFER_SIZE (100*1024)

struct per_transfer *transfers; /* first node */
static struct per_transfer *transfersl; /* last node */

static CURLcode add_transfer(struct per_transfer **per)
{
  struct per_transfer *p;
  p = calloc(sizeof(struct per_transfer), 1);
  if(!p)
    return CURLE_OUT_OF_MEMORY;
  if(!transfers)
    /* first entry */
    transfersl = transfers = p;
  else {
    /* make the last node point to the new node */
    transfersl->next = p;
    /* make the new node point back to the formerly last node */
    p->prev = transfersl;
    /* move the last node pointer to the new entry */
    transfersl = p;
  }
  *per = p;
  all_xfers++; /* count total number of transfers added */
  return CURLE_OK;
}

/* Remove the specified transfer from the list (and free it), return the next
   in line */
static struct per_transfer *del_transfer(struct per_transfer *per)
{
  struct per_transfer *n;
  struct per_transfer *p;
  DEBUGASSERT(transfers);
  DEBUGASSERT(transfersl);
  DEBUGASSERT(per);

  n = per->next;
  p = per->prev;

  if(p)
    p->next = n;
  else
    transfers = n;

  if(n)
    n->prev = p;
  else
    transfersl = p;

  free(per);

  return n;
}

static CURLcode pre_transfer(struct GlobalConfig *global,
                             struct per_transfer *per)
{
  curl_off_t uploadfilesize = -1;
  struct_stat fileinfo;
  CURLcode result = CURLE_OK;

  if(per->separator_err)
    fprintf(global->errors, "%s\n", per->separator_err);
  if(per->separator)
    printf("%s\n", per->separator);

  if(per->uploadfile && !stdin_upload(per->uploadfile)) {
    /* VMS Note:
     *
     * Reading binary from files can be a problem...  Only FIXED, VAR
     * etc WITHOUT implied CC will work Others need a \n appended to a
     * line
     *
     * - Stat gives a size but this is UNRELIABLE in VMS As a f.e. a
     * fixed file with implied CC needs to have a byte added for every
     * record processed, this can by derived from Filesize & recordsize
     * for VARiable record files the records need to be counted!  for
     * every record add 1 for linefeed and subtract 2 for the record
     * header for VARIABLE header files only the bare record data needs
     * to be considered with one appended if implied CC
     */
#ifdef __VMS
    /* Calculate the real upload size for VMS */
    per->infd = -1;
    if(stat(per->uploadfile, &fileinfo) == 0) {
      fileinfo.st_size = VmsSpecialSize(uploadfile, &fileinfo);
      switch(fileinfo.st_fab_rfm) {
      case FAB$C_VAR:
      case FAB$C_VFC:
      case FAB$C_STMCR:
        per->infd = open(per->uploadfile, O_RDONLY | O_BINARY);
        break;
      default:
        per->infd = open(per->uploadfile, O_RDONLY | O_BINARY,
                        "rfm=stmlf", "ctx=stm");
      }
    }
    if(per->infd == -1)
#else
      per->infd = open(per->uploadfile, O_RDONLY | O_BINARY);
    if((per->infd == -1) || fstat(per->infd, &fileinfo))
#endif
    {
      helpf(global->errors, "Can't open '%s'!\n", per->uploadfile);
      if(per->infd != -1) {
        close(per->infd);
        per->infd = STDIN_FILENO;
      }
      return CURLE_READ_ERROR;
    }
    per->infdopen = TRUE;

    /* we ignore file size for char/block devices, sockets, etc. */
    if(S_ISREG(fileinfo.st_mode))
      uploadfilesize = fileinfo.st_size;

    if(uploadfilesize != -1)
      my_setopt(per->curl, CURLOPT_INFILESIZE_LARGE, uploadfilesize);
    per->input.fd = per->infd;
  }
  show_error:
  return result;
}

/*
 * Call this after a transfer has completed.
 */
static CURLcode post_transfer(struct GlobalConfig *global,
                              CURLSH *share,
                              struct per_transfer *per,
                              CURLcode result,
                              bool *retryp)
{
  struct OutStruct *outs = &per->outs;
  CURL *curl = per->curl;
  struct OperationConfig *config = per->config;

  *retryp = FALSE;

  if(per->infdopen)
    close(per->infd);

#ifdef __VMS
  if(is_vms_shell()) {
    /* VMS DCL shell behavior */
    if(!global->showerror)
      vms_show = VMSSTS_HIDE;
  }
  else
#endif
    if(config->synthetic_error) {
      ;
    }
    else if(result && global->showerror) {
      fprintf(global->errors, "curl: (%d) %s\n", result,
              (per->errorbuffer[0]) ? per->errorbuffer :
              curl_easy_strerror(result));
      if(result == CURLE_PEER_FAILED_VERIFICATION)
        fputs(CURL_CA_CERT_ERRORMSG, global->errors);
    }

  /* Set file extended attributes */
  if(!result && config->xattr && outs->fopened && outs->stream) {
    int rc = fwrite_xattr(curl, fileno(outs->stream));
    if(rc)
      warnf(config->global, "Error setting extended attributes: %s\n",
            strerror(errno));
  }

  if(!result && !outs->stream && !outs->bytes) {
    /* we have received no data despite the transfer was successful
       ==> force cration of an empty output file (if an output file
       was specified) */
    long cond_unmet = 0L;
    /* do not create (or even overwrite) the file in case we get no
       data because of unmet condition */
    curl_easy_getinfo(curl, CURLINFO_CONDITION_UNMET, &cond_unmet);
    if(!cond_unmet && !tool_create_output_file(outs))
      result = CURLE_WRITE_ERROR;
  }

  if(!outs->s_isreg && outs->stream) {
    /* Dump standard stream buffered data */
    int rc = fflush(outs->stream);
    if(!result && rc) {
      /* something went wrong in the writing process */
      result = CURLE_WRITE_ERROR;
      fprintf(global->errors, "(%d) Failed writing body\n", result);
    }
  }

#ifdef USE_METALINK
  if(per->metalink && !per->metalink_next_res)
    fprintf(global->errors, "Metalink: fetching (%s) from (%s) OK\n",
            per->mlfile->filename, per->this_url);

  if(!per->metalink && config->use_metalink && result == CURLE_OK) {
    int rv = parse_metalink(config, outs, per->this_url);
    if(!rv) {
      fprintf(config->global->errors, "Metalink: parsing (%s) OK\n",
              per->this_url);
    }
    else if(rv == -1)
      fprintf(config->global->errors, "Metalink: parsing (%s) FAILED\n",
              per->this_url);
    result = create_transfers(global, config, share, FALSE);
  }
  else if(per->metalink && result == CURLE_OK && !per->metalink_next_res) {
    int rv;
    (void)fflush(outs->stream);
    rv = metalink_check_hash(global, per->mlfile, outs->filename);
    if(!rv)
      per->metalink_next_res = 1;
  }
#else
  (void)share;
#endif /* USE_METALINK */

#ifdef USE_METALINK
  if(outs->metalink_parser)
    metalink_parser_context_delete(outs->metalink_parser);
#endif /* USE_METALINK */

  if(outs->is_cd_filename && outs->stream && !global->mute &&
     outs->filename)
    printf("curl: Saved to filename '%s'\n", outs->filename);

  /* if retry-max-time is non-zero, make sure we haven't exceeded the
     time */
  if(per->retry_numretries &&
     (!config->retry_maxtime ||
      (tvdiff(tvnow(), per->retrystart) <
       config->retry_maxtime*1000L)) ) {
    enum {
      RETRY_NO,
      RETRY_TIMEOUT,
      RETRY_CONNREFUSED,
      RETRY_HTTP,
      RETRY_FTP,
      RETRY_LAST /* not used */
    } retry = RETRY_NO;
    long response;
    if((CURLE_OPERATION_TIMEDOUT == result) ||
       (CURLE_COULDNT_RESOLVE_HOST == result) ||
       (CURLE_COULDNT_RESOLVE_PROXY == result) ||
       (CURLE_FTP_ACCEPT_TIMEOUT == result))
      /* retry timeout always */
      retry = RETRY_TIMEOUT;
    else if(config->retry_connrefused &&
            (CURLE_COULDNT_CONNECT == result)) {
      long oserrno;
      curl_easy_getinfo(curl, CURLINFO_OS_ERRNO, &oserrno);
      if(ECONNREFUSED == oserrno)
        retry = RETRY_CONNREFUSED;
    }
    else if((CURLE_OK == result) ||
            (config->failonerror &&
             (CURLE_HTTP_RETURNED_ERROR == result))) {
      /* If it returned OK. _or_ failonerror was enabled and it
         returned due to such an error, check for HTTP transient
         errors to retry on. */
      long protocol;
      curl_easy_getinfo(curl, CURLINFO_PROTOCOL, &protocol);
      if((protocol == CURLPROTO_HTTP) || (protocol == CURLPROTO_HTTPS)) {
        /* This was HTTP(S) */
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response);

        switch(response) {
        case 500: /* Internal Server Error */
        case 502: /* Bad Gateway */
        case 503: /* Service Unavailable */
        case 504: /* Gateway Timeout */
          retry = RETRY_HTTP;
          /*
           * At this point, we have already written data to the output
           * file (or terminal). If we write to a file, we must rewind
           * or close/re-open the file so that the next attempt starts
           * over from the beginning.
           *
           * TODO: similar action for the upload case. We might need
           * to start over reading from a previous point if we have
           * uploaded something when this was returned.
           */
          break;
        }
      }
    } /* if CURLE_OK */
    else if(result) {
      long protocol;

      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response);
      curl_easy_getinfo(curl, CURLINFO_PROTOCOL, &protocol);

      if((protocol == CURLPROTO_FTP || protocol == CURLPROTO_FTPS) &&
         response / 100 == 4)
        /*
         * This is typically when the FTP server only allows a certain
         * amount of users and we are not one of them.  All 4xx codes
         * are transient.
         */
        retry = RETRY_FTP;
    }

    if(retry) {
      long sleeptime = 0;
      curl_off_t retry_after = 0;
      static const char * const m[]={
        NULL,
        "timeout",
        "connection refused",
        "HTTP error",
        "FTP error"
      };

      sleeptime = per->retry_sleep;
      if(RETRY_HTTP == retry) {
        curl_easy_getinfo(curl, CURLINFO_RETRY_AFTER, &retry_after);
        if(retry_after) {
          /* store in a 'long', make sure it doesn't overflow */
          if(retry_after > LONG_MAX/1000)
            sleeptime = LONG_MAX;
          else
            sleeptime = (long)retry_after * 1000; /* milliseconds */
        }
      }
      warnf(config->global, "Transient problem: %s "
            "Will retry in %ld seconds. "
            "%ld retries left.\n",
            m[retry], per->retry_sleep/1000L, per->retry_numretries);

      per->retry_numretries--;
      tool_go_sleep(sleeptime);
      if(!config->retry_delay) {
        per->retry_sleep *= 2;
        if(per->retry_sleep > RETRY_SLEEP_MAX)
          per->retry_sleep = RETRY_SLEEP_MAX;
      }
      if(outs->bytes && outs->filename && outs->stream) {
        int rc;
        /* We have written data to a output file, we truncate file
         */
        if(!global->mute)
          fprintf(global->errors, "Throwing away %"
                  CURL_FORMAT_CURL_OFF_T " bytes\n",
                  outs->bytes);
        fflush(outs->stream);
        /* truncate file at the position where we started appending */
#ifdef HAVE_FTRUNCATE
        if(ftruncate(fileno(outs->stream), outs->init)) {
          /* when truncate fails, we can't just append as then we'll
             create something strange, bail out */
          if(!global->mute)
            fprintf(global->errors,
                    "failed to truncate, exiting\n");
          return CURLE_WRITE_ERROR;
        }
        /* now seek to the end of the file, the position where we
           just truncated the file in a large file-safe way */
        rc = fseek(outs->stream, 0, SEEK_END);
#else
        /* ftruncate is not available, so just reposition the file
           to the location we would have truncated it. This won't
           work properly with large files on 32-bit systems, but
           most of those will have ftruncate. */
        rc = fseek(outs->stream, (long)outs->init, SEEK_SET);
#endif
        if(rc) {
          if(!global->mute)
            fprintf(global->errors,
                    "failed seeking to end of file, exiting\n");
          return CURLE_WRITE_ERROR;
        }
        outs->bytes = 0; /* clear for next round */
      }
      *retryp = TRUE; /* curl_easy_perform loop */
      return CURLE_OK;
    }
  } /* if retry_numretries */
  else if(per->metalink) {
    /* Metalink: Decide to try the next resource or not. Try the next resource
       if download was not successful. */
    long response;
    if(CURLE_OK == result) {
      /* TODO We want to try next resource when download was
         not successful. How to know that? */
      char *effective_url = NULL;
      curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective_url);
      if(effective_url &&
         curl_strnequal(effective_url, "http", 4)) {
        /* This was HTTP(S) */
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response);
        if(response != 200 && response != 206) {
          per->metalink_next_res = 1;
          fprintf(global->errors,
                  "Metalink: fetching (%s) from (%s) FAILED "
                  "(HTTP status code %ld)\n",
                  per->mlfile->filename, per->this_url, response);
        }
      }
    }
    else {
      per->metalink_next_res = 1;
      fprintf(global->errors,
              "Metalink: fetching (%s) from (%s) FAILED (%s)\n",
              per->mlfile->filename, per->this_url,
              curl_easy_strerror(result));
    }
  }

  if((global->progressmode == CURL_PROGRESS_BAR) &&
     per->progressbar.calls)
    /* if the custom progress bar has been displayed, we output a
       newline here */
    fputs("\n", per->progressbar.out);

  if(config->writeout)
    ourWriteOut(per->curl, &per->outs, config->writeout);

  /* Close the outs file */
  if(outs->fopened && outs->stream) {
    int rc = fclose(outs->stream);
    if(!result && rc) {
      /* something went wrong in the writing process */
      result = CURLE_WRITE_ERROR;
      fprintf(global->errors, "(%d) Failed writing body\n", result);
    }
  }

  /* File time can only be set _after_ the file has been closed */
  if(!result && config->remote_time && outs->s_isreg && outs->filename) {
    /* Ask libcurl if we got a remote file time */
    curl_off_t filetime = -1;
    curl_easy_getinfo(curl, CURLINFO_FILETIME_T, &filetime);
    setfiletime(filetime, outs->filename, config->global->errors);
  }

  /* Close function-local opened file descriptors */
  if(per->heads.fopened && per->heads.stream)
    fclose(per->heads.stream);

  if(per->heads.alloc_filename)
    Curl_safefree(per->heads.filename);

  curl_easy_cleanup(per->curl);
  if(outs->alloc_filename)
    free(outs->filename);
  free(per->this_url);
  free(per->separator_err);
  free(per->separator);
  free(per->outfile);
  free(per->uploadfile);

  return CURLE_OK;
}

/* go through the list of URLs and configs and add transfers */

static CURLcode create_transfers(struct GlobalConfig *global,
                                 struct OperationConfig *config,
                                 CURLSH *share,
                                 bool capath_from_env)
{
  CURLcode result = CURLE_OK;
  struct getout *urlnode;
  metalinkfile *mlfile_last = NULL;
  bool orig_noprogress = global->noprogress;
  bool orig_isatty = global->isatty;
  char *httpgetfields = NULL;

  if(config->postfields) {
    if(config->use_httpget) {
      /* Use the postfields data for a http get */
      httpgetfields = strdup(config->postfields);
      Curl_safefree(config->postfields);
      if(!httpgetfields) {
        helpf(global->errors, "out of memory\n");
        result = CURLE_OUT_OF_MEMORY;
        goto quit_curl;
      }
      if(SetHTTPrequest(config,
                        (config->no_body?HTTPREQ_HEAD:HTTPREQ_GET),
                        &config->httpreq)) {
        result = CURLE_FAILED_INIT;
        goto quit_curl;
      }
    }
    else {
      if(SetHTTPrequest(config, HTTPREQ_SIMPLEPOST, &config->httpreq)) {
        result = CURLE_FAILED_INIT;
        goto quit_curl;
      }
    }
  }

  for(urlnode = config->url_list; urlnode; urlnode = urlnode->next) {
    unsigned long li;
    unsigned long up; /* upload file counter within a single upload glob */
    char *infiles; /* might be a glob pattern */
    char *outfiles;
    unsigned long infilenum;
    URLGlob *inglob;
    bool metalink = FALSE; /* metalink download? */
    metalinkfile *mlfile;
    metalink_resource *mlres;

    outfiles = NULL;
    infilenum = 1;
    inglob = NULL;

    if(urlnode->flags & GETOUT_METALINK) {
      metalink = 1;
      if(mlfile_last == NULL) {
        mlfile_last = config->metalinkfile_list;
      }
      mlfile = mlfile_last;
      mlfile_last = mlfile_last->next;
      mlres = mlfile->resource;
    }
    else {
      mlfile = NULL;
      mlres = NULL;
    }

    /* urlnode->url is the full URL (it might be NULL) */

    if(!urlnode->url) {
      /* This node has no URL. Free node data without destroying the
         node itself nor modifying next pointer and continue to next */
      Curl_safefree(urlnode->outfile);
      Curl_safefree(urlnode->infile);
      urlnode->flags = 0;
      continue; /* next URL please */
    }

    /* save outfile pattern before expansion */
    if(urlnode->outfile) {
      outfiles = strdup(urlnode->outfile);
      if(!outfiles) {
        helpf(global->errors, "out of memory\n");
        result = CURLE_OUT_OF_MEMORY;
        break;
      }
    }

    infiles = urlnode->infile;

    if(!config->globoff && infiles) {
      /* Unless explicitly shut off */
      result = glob_url(&inglob, infiles, &infilenum,
                        global->showerror?global->errors:NULL);
      if(result) {
        Curl_safefree(outfiles);
        break;
      }
    }

    /* Here's the loop for uploading multiple files within the same
       single globbed string. If no upload, we enter the loop once anyway. */
    for(up = 0 ; up < infilenum; up++) {

      char *uploadfile; /* a single file, never a glob */
      int separator;
      URLGlob *urls;
      unsigned long urlnum;

      uploadfile = NULL;
      urls = NULL;
      urlnum = 0;

      if(!up && !infiles)
        Curl_nop_stmt;
      else {
        if(inglob) {
          result = glob_next_url(&uploadfile, inglob);
          if(result == CURLE_OUT_OF_MEMORY)
            helpf(global->errors, "out of memory\n");
        }
        else if(!up) {
          uploadfile = strdup(infiles);
          if(!uploadfile) {
            helpf(global->errors, "out of memory\n");
            result = CURLE_OUT_OF_MEMORY;
          }
        }
        if(!uploadfile)
          break;
      }

      if(metalink) {
        /* For Metalink download, we don't use glob. Instead we use
           the number of resources as urlnum. */
        urlnum = count_next_metalink_resource(mlfile);
      }
      else if(!config->globoff) {
        /* Unless explicitly shut off, we expand '{...}' and '[...]'
           expressions and return total number of URLs in pattern set */
        result = glob_url(&urls, urlnode->url, &urlnum,
                          global->showerror?global->errors:NULL);
        if(result) {
          Curl_safefree(uploadfile);
          break;
        }
      }
      else
        urlnum = 1; /* without globbing, this is a single URL */

      /* if multiple files extracted to stdout, insert separators! */
      separator = ((!outfiles || !strcmp(outfiles, "-")) && urlnum > 1);

      /* Here's looping around each globbed URL */
      for(li = 0 ; li < urlnum; li++) {
        struct per_transfer *per;
        struct OutStruct *outs;
        struct InStruct *input;
        struct OutStruct *heads;
        struct HdrCbData *hdrcbdata = NULL;
        CURL *curl = curl_easy_init();

        result = add_transfer(&per);
        if(result || !curl) {
          free(uploadfile);
          curl_easy_cleanup(curl);
          result = CURLE_OUT_OF_MEMORY;
          goto show_error;
        }
        per->config = config;
        per->curl = curl;
        per->uploadfile = uploadfile;

        /* default headers output stream is stdout */
        heads = &per->heads;
        heads->stream = stdout;
        heads->config = config;

        /* Single header file for all URLs */
        if(config->headerfile) {
          /* open file for output: */
          if(strcmp(config->headerfile, "-")) {
            FILE *newfile = fopen(config->headerfile, "wb");
            if(!newfile) {
              warnf(config->global, "Failed to open %s\n", config->headerfile);
              result = CURLE_WRITE_ERROR;
              goto quit_curl;
            }
            else {
              heads->filename = config->headerfile;
              heads->s_isreg = TRUE;
              heads->fopened = TRUE;
              heads->stream = newfile;
            }
          }
          else {
            /* always use binary mode for protocol header output */
            set_binmode(heads->stream);
          }
        }


        hdrcbdata = &per->hdrcbdata;

        outs = &per->outs;
        input = &per->input;

        per->outfile = NULL;
        per->infdopen = FALSE;
        per->infd = STDIN_FILENO;

        /* default output stream is stdout */
        outs->stream = stdout;
        outs->config = config;

        if(metalink) {
          /* For Metalink download, use name in Metalink file as
             filename. */
          per->outfile = strdup(mlfile->filename);
          if(!per->outfile) {
            result = CURLE_OUT_OF_MEMORY;
            goto show_error;
          }
          per->this_url = strdup(mlres->url);
          if(!per->this_url) {
            result = CURLE_OUT_OF_MEMORY;
            goto show_error;
          }
          per->mlfile = mlfile;
        }
        else {
          if(urls) {
            result = glob_next_url(&per->this_url, urls);
            if(result)
              goto show_error;
          }
          else if(!li) {
            per->this_url = strdup(urlnode->url);
            if(!per->this_url) {
              result = CURLE_OUT_OF_MEMORY;
              goto show_error;
            }
          }
          else
            per->this_url = NULL;
          if(!per->this_url)
            break;

          if(outfiles) {
            per->outfile = strdup(outfiles);
            if(!per->outfile) {
              result = CURLE_OUT_OF_MEMORY;
              goto show_error;
            }
          }
        }

        if(((urlnode->flags&GETOUT_USEREMOTE) ||
            (per->outfile && strcmp("-", per->outfile))) &&
           (metalink || !config->use_metalink)) {

          /*
           * We have specified a file name to store the result in, or we have
           * decided we want to use the remote file name.
           */

          if(!per->outfile) {
            /* extract the file name from the URL */
            result = get_url_file_name(&per->outfile, per->this_url);
            if(result)
              goto show_error;
            if(!*per->outfile && !config->content_disposition) {
              helpf(global->errors, "Remote file name has no length!\n");
              result = CURLE_WRITE_ERROR;
              goto quit_urls;
            }
          }
          else if(urls) {
            /* fill '#1' ... '#9' terms from URL pattern */
            char *storefile = per->outfile;
            result = glob_match_url(&per->outfile, storefile, urls);
            Curl_safefree(storefile);
            if(result) {
              /* bad globbing */
              warnf(config->global, "bad output glob!\n");
              goto quit_urls;
            }
          }

          /* Create the directory hierarchy, if not pre-existent to a multiple
             file output call */

          if(config->create_dirs || metalink) {
            result = create_dir_hierarchy(per->outfile, global->errors);
            /* create_dir_hierarchy shows error upon CURLE_WRITE_ERROR */
            if(result == CURLE_WRITE_ERROR)
              goto quit_urls;
            if(result) {
              goto show_error;
            }
          }

          if((urlnode->flags & GETOUT_USEREMOTE)
             && config->content_disposition) {
            /* Our header callback MIGHT set the filename */
            DEBUGASSERT(!outs->filename);
          }

          if(config->resume_from_current) {
            /* We're told to continue from where we are now. Get the size
               of the file as it is now and open it for append instead */
            struct_stat fileinfo;
            /* VMS -- Danger, the filesize is only valid for stream files */
            if(0 == stat(per->outfile, &fileinfo))
              /* set offset to current file size: */
              config->resume_from = fileinfo.st_size;
            else
              /* let offset be 0 */
              config->resume_from = 0;
          }

          if(config->resume_from) {
#ifdef __VMS
            /* open file for output, forcing VMS output format into stream
               mode which is needed for stat() call above to always work. */
            FILE *file = fopen(outfile, config->resume_from?"ab":"wb",
                               "ctx=stm", "rfm=stmlf", "rat=cr", "mrs=0");
#else
            /* open file for output: */
            FILE *file = fopen(per->outfile, config->resume_from?"ab":"wb");
#endif
            if(!file) {
              helpf(global->errors, "Can't open '%s'!\n", per->outfile);
              result = CURLE_WRITE_ERROR;
              goto quit_urls;
            }
            outs->fopened = TRUE;
            outs->stream = file;
            outs->init = config->resume_from;
          }
          else {
            outs->stream = NULL; /* open when needed */
          }
          outs->filename = per->outfile;
          outs->s_isreg = TRUE;
        }

        if(per->uploadfile && !stdin_upload(per->uploadfile)) {
          /*
           * We have specified a file to upload and it isn't "-".
           */
          char *nurl = add_file_name_to_url(per->this_url, per->uploadfile);
          if(!nurl) {
            result = CURLE_OUT_OF_MEMORY;
            goto show_error;
          }
          per->this_url = nurl;
        }
        else if(per->uploadfile && stdin_upload(per->uploadfile)) {
          /* count to see if there are more than one auth bit set
             in the authtype field */
          int authbits = 0;
          int bitcheck = 0;
          while(bitcheck < 32) {
            if(config->authtype & (1UL << bitcheck++)) {
              authbits++;
              if(authbits > 1) {
                /* more than one, we're done! */
                break;
              }
            }
          }

          /*
           * If the user has also selected --anyauth or --proxy-anyauth
           * we should warn him/her.
           */
          if(config->proxyanyauth || (authbits>1)) {
            warnf(config->global,
                  "Using --anyauth or --proxy-anyauth with upload from stdin"
                  " involves a big risk of it not working. Use a temporary"
                  " file or a fixed auth type instead!\n");
          }

          DEBUGASSERT(per->infdopen == FALSE);
          DEBUGASSERT(per->infd == STDIN_FILENO);

          set_binmode(stdin);
          if(!strcmp(per->uploadfile, ".")) {
            if(curlx_nonblock((curl_socket_t)per->infd, TRUE) < 0)
              warnf(config->global,
                    "fcntl failed on fd=%d: %s\n", per->infd, strerror(errno));
          }
        }

        if(per->uploadfile && config->resume_from_current)
          config->resume_from = -1; /* -1 will then force get-it-yourself */

        if(output_expected(per->this_url, per->uploadfile) && outs->stream &&
           isatty(fileno(outs->stream)))
          /* we send the output to a tty, therefore we switch off the progress
             meter */
          global->noprogress = global->isatty = TRUE;
        else {
          /* progress meter is per download, so restore config
             values */
          global->noprogress = orig_noprogress;
          global->isatty = orig_isatty;
        }

        if(urlnum > 1 && !global->mute) {
          per->separator_err =
            aprintf("\n[%lu/%lu]: %s --> %s",
                    li + 1, urlnum, per->this_url,
                    per->outfile ? per->outfile : "<stdout>");
          if(separator)
            per->separator = aprintf("%s%s", CURLseparator, per->this_url);
        }
        if(httpgetfields) {
          char *urlbuffer;
          /* Find out whether the url contains a file name */
          const char *pc = strstr(per->this_url, "://");
          char sep = '?';
          if(pc)
            pc += 3;
          else
            pc = per->this_url;

          pc = strrchr(pc, '/'); /* check for a slash */

          if(pc) {
            /* there is a slash present in the URL */

            if(strchr(pc, '?'))
              /* Ouch, there's already a question mark in the URL string, we
                 then append the data with an ampersand separator instead! */
              sep = '&';
          }
          /*
           * Then append ? followed by the get fields to the url.
           */
          if(pc)
            urlbuffer = aprintf("%s%c%s", per->this_url, sep, httpgetfields);
          else
            /* Append  / before the ? to create a well-formed url
               if the url contains a hostname only
            */
            urlbuffer = aprintf("%s/?%s", per->this_url, httpgetfields);

          if(!urlbuffer) {
            result = CURLE_OUT_OF_MEMORY;
            goto show_error;
          }

          Curl_safefree(per->this_url); /* free previous URL */
          per->this_url = urlbuffer; /* use our new URL instead! */
        }

        if(!global->errors)
          global->errors = stderr;

        if((!per->outfile || !strcmp(per->outfile, "-")) &&
           !config->use_ascii) {
          /* We get the output to stdout and we have not got the ASCII/text
             flag, then set stdout to be binary */
          set_binmode(stdout);
        }

        /* explicitly passed to stdout means okaying binary gunk */
        config->terminal_binary_ok =
          (per->outfile && !strcmp(per->outfile, "-"));

        /* avoid having this setopt added to the --libcurl source
           output */
        result = curl_easy_setopt(curl, CURLOPT_SHARE, share);
        if(result)
          goto show_error;

        if(!config->tcp_nodelay)
          my_setopt(curl, CURLOPT_TCP_NODELAY, 0L);

        if(config->tcp_fastopen)
          my_setopt(curl, CURLOPT_TCP_FASTOPEN, 1L);

        /* where to store */
        my_setopt(curl, CURLOPT_WRITEDATA, per);
        my_setopt(curl, CURLOPT_INTERLEAVEDATA, per);

        if(metalink || !config->use_metalink)
          /* what call to write */
          my_setopt(curl, CURLOPT_WRITEFUNCTION, tool_write_cb);
#ifdef USE_METALINK
        else
          /* Set Metalink specific write callback function to parse
             XML data progressively. */
          my_setopt(curl, CURLOPT_WRITEFUNCTION, metalink_write_cb);
#endif /* USE_METALINK */

        /* for uploads */
        input->config = config;
        /* Note that if CURLOPT_READFUNCTION is fread (the default), then
         * lib/telnet.c will Curl_poll() on the input file descriptor
         * rather then calling the READFUNCTION at regular intervals.
         * The circumstances in which it is preferable to enable this
         * behaviour, by omitting to set the READFUNCTION & READDATA options,
         * have not been determined.
         */
        my_setopt(curl, CURLOPT_READDATA, input);
        /* what call to read */
        my_setopt(curl, CURLOPT_READFUNCTION, tool_read_cb);

        /* in 7.18.0, the CURLOPT_SEEKFUNCTION/DATA pair is taking over what
           CURLOPT_IOCTLFUNCTION/DATA pair previously provided for seeking */
        my_setopt(curl, CURLOPT_SEEKDATA, input);
        my_setopt(curl, CURLOPT_SEEKFUNCTION, tool_seek_cb);

        if(config->recvpersecond &&
           (config->recvpersecond < BUFFER_SIZE))
          /* use a smaller sized buffer for better sleeps */
          my_setopt(curl, CURLOPT_BUFFERSIZE, (long)config->recvpersecond);
        else
          my_setopt(curl, CURLOPT_BUFFERSIZE, (long)BUFFER_SIZE);

        my_setopt_str(curl, CURLOPT_URL, per->this_url);
        my_setopt(curl, CURLOPT_NOPROGRESS, global->noprogress?1L:0L);
        if(config->no_body)
          my_setopt(curl, CURLOPT_NOBODY, 1L);

        if(config->oauth_bearer)
          my_setopt_str(curl, CURLOPT_XOAUTH2_BEARER, config->oauth_bearer);

        {
          my_setopt_str(curl, CURLOPT_PROXY, config->proxy);
          /* new in libcurl 7.5 */
          if(config->proxy)
            my_setopt_enum(curl, CURLOPT_PROXYTYPE, config->proxyver);

          my_setopt_str(curl, CURLOPT_PROXYUSERPWD, config->proxyuserpwd);

          /* new in libcurl 7.3 */
          my_setopt(curl, CURLOPT_HTTPPROXYTUNNEL, config->proxytunnel?1L:0L);

          /* new in libcurl 7.52.0 */
          if(config->preproxy)
            my_setopt_str(curl, CURLOPT_PRE_PROXY, config->preproxy);

          /* new in libcurl 7.10.6 */
          if(config->proxyanyauth)
            my_setopt_bitmask(curl, CURLOPT_PROXYAUTH,
                              (long)CURLAUTH_ANY);
          else if(config->proxynegotiate)
            my_setopt_bitmask(curl, CURLOPT_PROXYAUTH,
                              (long)CURLAUTH_GSSNEGOTIATE);
          else if(config->proxyntlm)
            my_setopt_bitmask(curl, CURLOPT_PROXYAUTH,
                              (long)CURLAUTH_NTLM);
          else if(config->proxydigest)
            my_setopt_bitmask(curl, CURLOPT_PROXYAUTH,
                              (long)CURLAUTH_DIGEST);
          else if(config->proxybasic)
            my_setopt_bitmask(curl, CURLOPT_PROXYAUTH,
                              (long)CURLAUTH_BASIC);

          /* new in libcurl 7.19.4 */
          my_setopt_str(curl, CURLOPT_NOPROXY, config->noproxy);

          my_setopt(curl, CURLOPT_SUPPRESS_CONNECT_HEADERS,
                    config->suppress_connect_headers?1L:0L);
        }

        my_setopt(curl, CURLOPT_FAILONERROR, config->failonerror?1L:0L);
        my_setopt(curl, CURLOPT_REQUEST_TARGET, config->request_target);
        my_setopt(curl, CURLOPT_UPLOAD, per->uploadfile?1L:0L);
        my_setopt(curl, CURLOPT_DIRLISTONLY, config->dirlistonly?1L:0L);
        my_setopt(curl, CURLOPT_APPEND, config->ftp_append?1L:0L);

        if(config->netrc_opt)
          my_setopt_enum(curl, CURLOPT_NETRC, (long)CURL_NETRC_OPTIONAL);
        else if(config->netrc || config->netrc_file)
          my_setopt_enum(curl, CURLOPT_NETRC, (long)CURL_NETRC_REQUIRED);
        else
          my_setopt_enum(curl, CURLOPT_NETRC, (long)CURL_NETRC_IGNORED);

        if(config->netrc_file)
          my_setopt_str(curl, CURLOPT_NETRC_FILE, config->netrc_file);

        my_setopt(curl, CURLOPT_TRANSFERTEXT, config->use_ascii?1L:0L);
        if(config->login_options)
          my_setopt_str(curl, CURLOPT_LOGIN_OPTIONS, config->login_options);
        my_setopt_str(curl, CURLOPT_USERPWD, config->userpwd);
        my_setopt_str(curl, CURLOPT_RANGE, config->range);
        my_setopt(curl, CURLOPT_ERRORBUFFER, per->errorbuffer);
        my_setopt(curl, CURLOPT_TIMEOUT_MS, (long)(config->timeout * 1000));

        switch(config->httpreq) {
        case HTTPREQ_SIMPLEPOST:
          my_setopt_str(curl, CURLOPT_POSTFIELDS,
                        config->postfields);
          my_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE,
                    config->postfieldsize);
          break;
        case HTTPREQ_MIMEPOST:
          result = tool2curlmime(curl, config->mimeroot, &config->mimepost);
          if(result)
            goto show_error;
          my_setopt_mimepost(curl, CURLOPT_MIMEPOST, config->mimepost);
          break;
        default:
          break;
        }

        /* new in libcurl 7.10.6 (default is Basic) */
        if(config->authtype)
          my_setopt_bitmask(curl, CURLOPT_HTTPAUTH, (long)config->authtype);

        my_setopt_slist(curl, CURLOPT_HTTPHEADER, config->headers);

        if(built_in_protos & (CURLPROTO_HTTP | CURLPROTO_RTSP)) {
          my_setopt_str(curl, CURLOPT_REFERER, config->referer);
          my_setopt_str(curl, CURLOPT_USERAGENT, config->useragent);
        }

        if(built_in_protos & CURLPROTO_HTTP) {

          long postRedir = 0;

          my_setopt(curl, CURLOPT_FOLLOWLOCATION,
                    config->followlocation?1L:0L);
          my_setopt(curl, CURLOPT_UNRESTRICTED_AUTH,
                    config->unrestricted_auth?1L:0L);

          my_setopt(curl, CURLOPT_AUTOREFERER, config->autoreferer?1L:0L);

          /* new in libcurl 7.36.0 */
          if(config->proxyheaders) {
            my_setopt_slist(curl, CURLOPT_PROXYHEADER, config->proxyheaders);
            my_setopt(curl, CURLOPT_HEADEROPT, CURLHEADER_SEPARATE);
          }

          /* new in libcurl 7.5 */
          my_setopt(curl, CURLOPT_MAXREDIRS, config->maxredirs);

          if(config->httpversion)
            my_setopt_enum(curl, CURLOPT_HTTP_VERSION, config->httpversion);
          else if(curlinfo->features & CURL_VERSION_HTTP2) {
            my_setopt_enum(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);
          }

          /* curl 7.19.1 (the 301 version existed in 7.18.2),
             303 was added in 7.26.0 */
          if(config->post301)
            postRedir |= CURL_REDIR_POST_301;
          if(config->post302)
            postRedir |= CURL_REDIR_POST_302;
          if(config->post303)
            postRedir |= CURL_REDIR_POST_303;
          my_setopt(curl, CURLOPT_POSTREDIR, postRedir);

          /* new in libcurl 7.21.6 */
          if(config->encoding)
            my_setopt_str(curl, CURLOPT_ACCEPT_ENCODING, "");

          /* new in libcurl 7.21.6 */
          if(config->tr_encoding)
            my_setopt(curl, CURLOPT_TRANSFER_ENCODING, 1L);
          /* new in libcurl 7.64.0 */
          my_setopt(curl, CURLOPT_HTTP09_ALLOWED,
                    config->http09_allowed ? 1L : 0L);

        } /* (built_in_protos & CURLPROTO_HTTP) */

        my_setopt_str(curl, CURLOPT_FTPPORT, config->ftpport);
        my_setopt(curl, CURLOPT_LOW_SPEED_LIMIT,
                  config->low_speed_limit);
        my_setopt(curl, CURLOPT_LOW_SPEED_TIME, config->low_speed_time);
        my_setopt(curl, CURLOPT_MAX_SEND_SPEED_LARGE,
                  config->sendpersecond);
        my_setopt(curl, CURLOPT_MAX_RECV_SPEED_LARGE,
                  config->recvpersecond);

        if(config->use_resume)
          my_setopt(curl, CURLOPT_RESUME_FROM_LARGE, config->resume_from);
        else
          my_setopt(curl, CURLOPT_RESUME_FROM_LARGE, CURL_OFF_T_C(0));

        my_setopt_str(curl, CURLOPT_KEYPASSWD, config->key_passwd);
        my_setopt_str(curl, CURLOPT_PROXY_KEYPASSWD, config->proxy_key_passwd);

        if(built_in_protos & (CURLPROTO_SCP|CURLPROTO_SFTP)) {

          /* SSH and SSL private key uses same command-line option */
          /* new in libcurl 7.16.1 */
          my_setopt_str(curl, CURLOPT_SSH_PRIVATE_KEYFILE, config->key);
          /* new in libcurl 7.16.1 */
          my_setopt_str(curl, CURLOPT_SSH_PUBLIC_KEYFILE, config->pubkey);

          /* new in libcurl 7.17.1: SSH host key md5 checking allows us
             to fail if we are not talking to who we think we should */
          my_setopt_str(curl, CURLOPT_SSH_HOST_PUBLIC_KEY_MD5,
                        config->hostpubmd5);

          /* new in libcurl 7.56.0 */
          if(config->ssh_compression)
            my_setopt(curl, CURLOPT_SSH_COMPRESSION, 1L);
        }

        if(config->cacert)
          my_setopt_str(curl, CURLOPT_CAINFO, config->cacert);
        if(config->proxy_cacert)
          my_setopt_str(curl, CURLOPT_PROXY_CAINFO, config->proxy_cacert);

        if(config->capath) {
          result = res_setopt_str(curl, CURLOPT_CAPATH, config->capath);
          if(result == CURLE_NOT_BUILT_IN) {
            warnf(config->global, "ignoring %s, not supported by libcurl\n",
                  capath_from_env?
                  "SSL_CERT_DIR environment variable":"--capath");
          }
          else if(result)
            goto show_error;
        }
        /* For the time being if --proxy-capath is not set then we use the
           --capath value for it, if any. See #1257 */
        if((config->proxy_capath || config->capath) &&
           !tool_setopt_skip(CURLOPT_PROXY_CAPATH)) {
          result = res_setopt_str(curl, CURLOPT_PROXY_CAPATH,
                                  (config->proxy_capath ?
                                   config->proxy_capath :
                                   config->capath));
          if(result == CURLE_NOT_BUILT_IN) {
            if(config->proxy_capath) {
              warnf(config->global,
                    "ignoring --proxy-capath, not supported by libcurl\n");
            }
          }
          else if(result)
            goto show_error;
        }

        if(config->crlfile)
          my_setopt_str(curl, CURLOPT_CRLFILE, config->crlfile);
        if(config->proxy_crlfile)
          my_setopt_str(curl, CURLOPT_PROXY_CRLFILE, config->proxy_crlfile);
        else if(config->crlfile) /* CURLOPT_PROXY_CRLFILE default is crlfile */
          my_setopt_str(curl, CURLOPT_PROXY_CRLFILE, config->crlfile);

        if(config->pinnedpubkey)
          my_setopt_str(curl, CURLOPT_PINNEDPUBLICKEY, config->pinnedpubkey);

        if(curlinfo->features & CURL_VERSION_SSL) {
          /* Check if config->cert is a PKCS#11 URI and set the
           * config->cert_type if necessary */
          if(config->cert) {
            if(!config->cert_type) {
              if(is_pkcs11_uri(config->cert)) {
                config->cert_type = strdup("ENG");
              }
            }
          }

          /* Check if config->key is a PKCS#11 URI and set the
           * config->key_type if necessary */
          if(config->key) {
            if(!config->key_type) {
              if(is_pkcs11_uri(config->key)) {
                config->key_type = strdup("ENG");
              }
            }
          }

          /* Check if config->proxy_cert is a PKCS#11 URI and set the
           * config->proxy_type if necessary */
          if(config->proxy_cert) {
            if(!config->proxy_cert_type) {
              if(is_pkcs11_uri(config->proxy_cert)) {
                config->proxy_cert_type = strdup("ENG");
              }
            }
          }

          /* Check if config->proxy_key is a PKCS#11 URI and set the
           * config->proxy_key_type if necessary */
          if(config->proxy_key) {
            if(!config->proxy_key_type) {
              if(is_pkcs11_uri(config->proxy_key)) {
                config->proxy_key_type = strdup("ENG");
              }
            }
          }

          my_setopt_str(curl, CURLOPT_SSLCERT, config->cert);
          my_setopt_str(curl, CURLOPT_PROXY_SSLCERT, config->proxy_cert);
          my_setopt_str(curl, CURLOPT_SSLCERTTYPE, config->cert_type);
          my_setopt_str(curl, CURLOPT_PROXY_SSLCERTTYPE,
                        config->proxy_cert_type);
          my_setopt_str(curl, CURLOPT_SSLKEY, config->key);
          my_setopt_str(curl, CURLOPT_PROXY_SSLKEY, config->proxy_key);
          my_setopt_str(curl, CURLOPT_SSLKEYTYPE, config->key_type);
          my_setopt_str(curl, CURLOPT_PROXY_SSLKEYTYPE,
                        config->proxy_key_type);

          if(config->insecure_ok) {
            my_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            my_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
          }
          else {
            my_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
            /* libcurl default is strict verifyhost -> 2L   */
            /* my_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L); */
          }
          if(config->proxy_insecure_ok) {
            my_setopt(curl, CURLOPT_PROXY_SSL_VERIFYPEER, 0L);
            my_setopt(curl, CURLOPT_PROXY_SSL_VERIFYHOST, 0L);
          }
          else {
            my_setopt(curl, CURLOPT_PROXY_SSL_VERIFYPEER, 1L);
          }

          if(config->verifystatus)
            my_setopt(curl, CURLOPT_SSL_VERIFYSTATUS, 1L);

          if(config->falsestart)
            my_setopt(curl, CURLOPT_SSL_FALSESTART, 1L);

          my_setopt_enum(curl, CURLOPT_SSLVERSION,
                         config->ssl_version | config->ssl_version_max);
          my_setopt_enum(curl, CURLOPT_PROXY_SSLVERSION,
                         config->proxy_ssl_version);
        }
        if(config->path_as_is)
          my_setopt(curl, CURLOPT_PATH_AS_IS, 1L);

        if(built_in_protos & (CURLPROTO_SCP|CURLPROTO_SFTP)) {
          if(!config->insecure_ok) {
            char *home;
            char *file;
            result = CURLE_OUT_OF_MEMORY;
            home = homedir();
            if(home) {
              file = aprintf("%s/.ssh/known_hosts", home);
              if(file) {
                /* new in curl 7.19.6 */
                result = res_setopt_str(curl, CURLOPT_SSH_KNOWNHOSTS, file);
                curl_free(file);
                if(result == CURLE_UNKNOWN_OPTION)
                  /* libssh2 version older than 1.1.1 */
                  result = CURLE_OK;
              }
              Curl_safefree(home);
            }
            if(result)
              goto show_error;
          }
        }

        if(config->no_body || config->remote_time) {
          /* no body or use remote time */
          my_setopt(curl, CURLOPT_FILETIME, 1L);
        }

        my_setopt(curl, CURLOPT_CRLF, config->crlf?1L:0L);
        my_setopt_slist(curl, CURLOPT_QUOTE, config->quote);
        my_setopt_slist(curl, CURLOPT_POSTQUOTE, config->postquote);
        my_setopt_slist(curl, CURLOPT_PREQUOTE, config->prequote);

        if(config->cookie)
          my_setopt_str(curl, CURLOPT_COOKIE, config->cookie);

        if(config->cookiefile)
          my_setopt_str(curl, CURLOPT_COOKIEFILE, config->cookiefile);

        /* new in libcurl 7.9 */
        if(config->cookiejar)
          my_setopt_str(curl, CURLOPT_COOKIEJAR, config->cookiejar);

        /* new in libcurl 7.9.7 */
        my_setopt(curl, CURLOPT_COOKIESESSION, config->cookiesession?1L:0L);

        my_setopt_enum(curl, CURLOPT_TIMECONDITION, (long)config->timecond);
        my_setopt(curl, CURLOPT_TIMEVALUE_LARGE, config->condtime);
        my_setopt_str(curl, CURLOPT_CUSTOMREQUEST, config->customrequest);
        customrequest_helper(config, config->httpreq, config->customrequest);
        my_setopt(curl, CURLOPT_STDERR, global->errors);

        /* three new ones in libcurl 7.3: */
        my_setopt_str(curl, CURLOPT_INTERFACE, config->iface);
        my_setopt_str(curl, CURLOPT_KRBLEVEL, config->krblevel);
        progressbarinit(&per->progressbar, config);

        if((global->progressmode == CURL_PROGRESS_BAR) &&
           !global->noprogress && !global->mute) {
          /* we want the alternative style, then we have to implement it
             ourselves! */
          my_setopt(curl, CURLOPT_XFERINFOFUNCTION, tool_progress_cb);
          my_setopt(curl, CURLOPT_XFERINFODATA, &per->progressbar);
        }

        /* new in libcurl 7.24.0: */
        if(config->dns_servers)
          my_setopt_str(curl, CURLOPT_DNS_SERVERS, config->dns_servers);

        /* new in libcurl 7.33.0: */
        if(config->dns_interface)
          my_setopt_str(curl, CURLOPT_DNS_INTERFACE, config->dns_interface);
        if(config->dns_ipv4_addr)
          my_setopt_str(curl, CURLOPT_DNS_LOCAL_IP4, config->dns_ipv4_addr);
        if(config->dns_ipv6_addr)
        my_setopt_str(curl, CURLOPT_DNS_LOCAL_IP6, config->dns_ipv6_addr);

        /* new in libcurl 7.6.2: */
        my_setopt_slist(curl, CURLOPT_TELNETOPTIONS, config->telnet_options);

        /* new in libcurl 7.7: */
        my_setopt_str(curl, CURLOPT_RANDOM_FILE, config->random_file);
        my_setopt_str(curl, CURLOPT_EGDSOCKET, config->egd_file);
        my_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS,
                  (long)(config->connecttimeout * 1000));

        if(config->doh_url)
          my_setopt_str(curl, CURLOPT_DOH_URL, config->doh_url);

        if(config->cipher_list)
          my_setopt_str(curl, CURLOPT_SSL_CIPHER_LIST, config->cipher_list);

        if(config->proxy_cipher_list)
          my_setopt_str(curl, CURLOPT_PROXY_SSL_CIPHER_LIST,
                        config->proxy_cipher_list);

        if(config->cipher13_list)
          my_setopt_str(curl, CURLOPT_TLS13_CIPHERS, config->cipher13_list);

        if(config->proxy_cipher13_list)
          my_setopt_str(curl, CURLOPT_PROXY_TLS13_CIPHERS,
                        config->proxy_cipher13_list);

        /* new in libcurl 7.9.2: */
        if(config->disable_epsv)
          /* disable it */
          my_setopt(curl, CURLOPT_FTP_USE_EPSV, 0L);

        /* new in libcurl 7.10.5 */
        if(config->disable_eprt)
          /* disable it */
          my_setopt(curl, CURLOPT_FTP_USE_EPRT, 0L);

        if(global->tracetype != TRACE_NONE) {
          my_setopt(curl, CURLOPT_DEBUGFUNCTION, tool_debug_cb);
          my_setopt(curl, CURLOPT_DEBUGDATA, config);
          my_setopt(curl, CURLOPT_VERBOSE, 1L);
        }

        /* new in curl 7.9.3 */
        if(config->engine) {
          result = res_setopt_str(curl, CURLOPT_SSLENGINE, config->engine);
          if(result)
            goto show_error;
        }

        /* new in curl 7.10.7, extended in 7.19.4. Modified to use
           CREATE_DIR_RETRY in 7.49.0 */
        my_setopt(curl, CURLOPT_FTP_CREATE_MISSING_DIRS,
                  (long)(config->ftp_create_dirs?
                         CURLFTP_CREATE_DIR_RETRY:
                         CURLFTP_CREATE_DIR_NONE));

        /* new in curl 7.10.8 */
        if(config->max_filesize)
          my_setopt(curl, CURLOPT_MAXFILESIZE_LARGE,
                    config->max_filesize);

        if(4 == config->ip_version)
          my_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
        else if(6 == config->ip_version)
          my_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V6);
        else
          my_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_WHATEVER);

        /* new in curl 7.15.5 */
        if(config->ftp_ssl_reqd)
          my_setopt_enum(curl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);

        /* new in curl 7.11.0 */
        else if(config->ftp_ssl)
          my_setopt_enum(curl, CURLOPT_USE_SSL, (long)CURLUSESSL_TRY);

        /* new in curl 7.16.0 */
        else if(config->ftp_ssl_control)
          my_setopt_enum(curl, CURLOPT_USE_SSL, (long)CURLUSESSL_CONTROL);

        /* new in curl 7.16.1 */
        if(config->ftp_ssl_ccc)
          my_setopt_enum(curl, CURLOPT_FTP_SSL_CCC,
                         (long)config->ftp_ssl_ccc_mode);

        /* new in curl 7.19.4 */
        if(config->socks5_gssapi_nec)
          my_setopt_str(curl, CURLOPT_SOCKS5_GSSAPI_NEC,
                        config->socks5_gssapi_nec);

        /* new in curl 7.55.0 */
        if(config->socks5_auth)
          my_setopt_bitmask(curl, CURLOPT_SOCKS5_AUTH,
                            (long)config->socks5_auth);

        /* new in curl 7.43.0 */
        if(config->proxy_service_name)
          my_setopt_str(curl, CURLOPT_PROXY_SERVICE_NAME,
                        config->proxy_service_name);

        /* new in curl 7.43.0 */
        if(config->service_name)
          my_setopt_str(curl, CURLOPT_SERVICE_NAME,
                        config->service_name);

        /* curl 7.13.0 */
        my_setopt_str(curl, CURLOPT_FTP_ACCOUNT, config->ftp_account);
        my_setopt(curl, CURLOPT_IGNORE_CONTENT_LENGTH, config->ignorecl?1L:0L);

        /* curl 7.14.2 */
        my_setopt(curl, CURLOPT_FTP_SKIP_PASV_IP, config->ftp_skip_ip?1L:0L);

        /* curl 7.15.1 */
        my_setopt(curl, CURLOPT_FTP_FILEMETHOD, (long)config->ftp_filemethod);

        /* curl 7.15.2 */
        if(config->localport) {
          my_setopt(curl, CURLOPT_LOCALPORT, config->localport);
          my_setopt_str(curl, CURLOPT_LOCALPORTRANGE, config->localportrange);
        }

        /* curl 7.15.5 */
        my_setopt_str(curl, CURLOPT_FTP_ALTERNATIVE_TO_USER,
                      config->ftp_alternative_to_user);

        /* curl 7.16.0 */
        if(config->disable_sessionid)
          /* disable it */
          my_setopt(curl, CURLOPT_SSL_SESSIONID_CACHE, 0L);

        /* curl 7.16.2 */
        if(config->raw) {
          my_setopt(curl, CURLOPT_HTTP_CONTENT_DECODING, 0L);
          my_setopt(curl, CURLOPT_HTTP_TRANSFER_DECODING, 0L);
        }

        /* curl 7.17.1 */
        if(!config->nokeepalive) {
          my_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
          if(config->alivetime != 0) {
            my_setopt(curl, CURLOPT_TCP_KEEPIDLE, config->alivetime);
            my_setopt(curl, CURLOPT_TCP_KEEPINTVL, config->alivetime);
          }
        }
        else
          my_setopt(curl, CURLOPT_TCP_KEEPALIVE, 0L);

        /* curl 7.20.0 */
        if(config->tftp_blksize)
          my_setopt(curl, CURLOPT_TFTP_BLKSIZE, config->tftp_blksize);

        if(config->mail_from)
          my_setopt_str(curl, CURLOPT_MAIL_FROM, config->mail_from);

        if(config->mail_rcpt)
          my_setopt_slist(curl, CURLOPT_MAIL_RCPT, config->mail_rcpt);

        /* curl 7.20.x */
        if(config->ftp_pret)
          my_setopt(curl, CURLOPT_FTP_USE_PRET, 1L);

        if(config->proto_present)
          my_setopt_flags(curl, CURLOPT_PROTOCOLS, config->proto);
        if(config->proto_redir_present)
          my_setopt_flags(curl, CURLOPT_REDIR_PROTOCOLS, config->proto_redir);

        if(config->content_disposition
           && (urlnode->flags & GETOUT_USEREMOTE))
          hdrcbdata->honor_cd_filename = TRUE;
        else
          hdrcbdata->honor_cd_filename = FALSE;

        hdrcbdata->outs = outs;
        hdrcbdata->heads = heads;
        hdrcbdata->global = global;
        hdrcbdata->config = config;

        my_setopt(curl, CURLOPT_HEADERFUNCTION, tool_header_cb);
        my_setopt(curl, CURLOPT_HEADERDATA, per);

        if(config->resolve)
          /* new in 7.21.3 */
          my_setopt_slist(curl, CURLOPT_RESOLVE, config->resolve);

        if(config->connect_to)
          /* new in 7.49.0 */
          my_setopt_slist(curl, CURLOPT_CONNECT_TO, config->connect_to);

        /* new in 7.21.4 */
        if(curlinfo->features & CURL_VERSION_TLSAUTH_SRP) {
          if(config->tls_username)
            my_setopt_str(curl, CURLOPT_TLSAUTH_USERNAME,
                          config->tls_username);
          if(config->tls_password)
            my_setopt_str(curl, CURLOPT_TLSAUTH_PASSWORD,
                          config->tls_password);
          if(config->tls_authtype)
            my_setopt_str(curl, CURLOPT_TLSAUTH_TYPE,
                          config->tls_authtype);
          if(config->proxy_tls_username)
            my_setopt_str(curl, CURLOPT_PROXY_TLSAUTH_USERNAME,
                          config->proxy_tls_username);
          if(config->proxy_tls_password)
            my_setopt_str(curl, CURLOPT_PROXY_TLSAUTH_PASSWORD,
                          config->proxy_tls_password);
          if(config->proxy_tls_authtype)
            my_setopt_str(curl, CURLOPT_PROXY_TLSAUTH_TYPE,
                          config->proxy_tls_authtype);
        }

        /* new in 7.22.0 */
        if(config->gssapi_delegation)
          my_setopt_str(curl, CURLOPT_GSSAPI_DELEGATION,
                        config->gssapi_delegation);

        /* new in 7.25.0 and 7.44.0 */
        {
          long mask = (config->ssl_allow_beast ? CURLSSLOPT_ALLOW_BEAST : 0) |
                      (config->ssl_no_revoke ? CURLSSLOPT_NO_REVOKE : 0);
          if(mask)
            my_setopt_bitmask(curl, CURLOPT_SSL_OPTIONS, mask);
        }

        if(config->proxy_ssl_allow_beast)
          my_setopt(curl, CURLOPT_PROXY_SSL_OPTIONS,
                    (long)CURLSSLOPT_ALLOW_BEAST);

        if(config->mail_auth)
          my_setopt_str(curl, CURLOPT_MAIL_AUTH, config->mail_auth);

        /* new in 7.66.0 */
        if(config->sasl_authzid)
          my_setopt_str(curl, CURLOPT_SASL_AUTHZID, config->sasl_authzid);

        /* new in 7.31.0 */
        if(config->sasl_ir)
          my_setopt(curl, CURLOPT_SASL_IR, 1L);

        if(config->nonpn) {
          my_setopt(curl, CURLOPT_SSL_ENABLE_NPN, 0L);
        }

        if(config->noalpn) {
          my_setopt(curl, CURLOPT_SSL_ENABLE_ALPN, 0L);
        }

        /* new in 7.40.0, abstract support added in 7.53.0 */
        if(config->unix_socket_path) {
          if(config->abstract_unix_socket) {
            my_setopt_str(curl, CURLOPT_ABSTRACT_UNIX_SOCKET,
                          config->unix_socket_path);
          }
          else {
            my_setopt_str(curl, CURLOPT_UNIX_SOCKET_PATH,
                          config->unix_socket_path);
          }
        }

        /* new in 7.45.0 */
        if(config->proto_default)
          my_setopt_str(curl, CURLOPT_DEFAULT_PROTOCOL, config->proto_default);

        /* new in 7.47.0 */
        if(config->expect100timeout > 0)
          my_setopt_str(curl, CURLOPT_EXPECT_100_TIMEOUT_MS,
                        (long)(config->expect100timeout*1000));

        /* new in 7.48.0 */
        if(config->tftp_no_options)
          my_setopt(curl, CURLOPT_TFTP_NO_OPTIONS, 1L);

        /* new in 7.59.0 */
        if(config->happy_eyeballs_timeout_ms != CURL_HET_DEFAULT)
          my_setopt(curl, CURLOPT_HAPPY_EYEBALLS_TIMEOUT_MS,
                    config->happy_eyeballs_timeout_ms);

        /* new in 7.60.0 */
        if(config->haproxy_protocol)
          my_setopt(curl, CURLOPT_HAPROXYPROTOCOL, 1L);

        if(config->disallow_username_in_url)
          my_setopt(curl, CURLOPT_DISALLOW_USERNAME_IN_URL, 1L);

#ifdef USE_ALTSVC
        /* only if explicitly enabled in configure */
        if(config->altsvc)
          my_setopt_str(curl, CURLOPT_ALTSVC, config->altsvc);
#endif

#ifdef USE_METALINK
        if(!metalink && config->use_metalink) {
          outs->metalink_parser = metalink_parser_context_new();
          if(outs->metalink_parser == NULL) {
            result = CURLE_OUT_OF_MEMORY;
            goto show_error;
          }
          fprintf(config->global->errors,
                  "Metalink: parsing (%s) metalink/XML...\n", per->this_url);
        }
        else if(metalink)
          fprintf(config->global->errors,
                  "Metalink: fetching (%s) from (%s)...\n",
                  mlfile->filename, per->this_url);
#endif /* USE_METALINK */

        per->metalink = metalink;
        /* initialize retry vars for loop below */
        per->retry_sleep_default = (config->retry_delay) ?
          config->retry_delay*1000L : RETRY_SLEEP_DEFAULT; /* ms */
        per->retry_numretries = config->req_retry;
        per->retry_sleep = per->retry_sleep_default; /* ms */
        per->retrystart = tvnow();

      } /* loop to the next URL */

      show_error:
      quit_urls:

      if(urls) {
        /* Free list of remaining URLs */
        glob_cleanup(urls);
        urls = NULL;
      }

      if(infilenum > 1) {
        /* when file globbing, exit loop upon critical error */
        if(is_fatal_error(result))
          break;
      }
      else if(result)
        /* when not file globbing, exit loop upon any error */
        break;

    } /* loop to the next globbed upload file */

    /* Free loop-local allocated memory */

    Curl_safefree(outfiles);

    if(inglob) {
      /* Free list of globbed upload files */
      glob_cleanup(inglob);
      inglob = NULL;
    }

    /* Free this URL node data without destroying the
       the node itself nor modifying next pointer. */
    Curl_safefree(urlnode->url);
    Curl_safefree(urlnode->outfile);
    Curl_safefree(urlnode->infile);
    urlnode->flags = 0;

  } /* for-loop through all URLs */
  quit_curl:

  /* Free function-local referenced allocated memory */
  Curl_safefree(httpgetfields);

  return result;
}

static long all_added; /* number of easy handles currently added */

/*
 * add_parallel_transfers() sets 'morep' to TRUE if there are more transfers
 * to add even after this call returns. sets 'addedp' to TRUE if one or more
 * transfers were added.
 */
static int add_parallel_transfers(struct GlobalConfig *global,
                                  CURLM *multi,
                                  bool *morep,
                                  bool *addedp)
{
  struct per_transfer *per;
  CURLcode result;
  CURLMcode mcode;
  *addedp = FALSE;
  *morep = FALSE;
  for(per = transfers; per && (all_added < global->parallel_max);
      per = per->next) {
    if(per->added)
      /* already added */
      continue;

    result = pre_transfer(global, per);
    if(result)
      break;

    (void)curl_easy_setopt(per->curl, CURLOPT_PRIVATE, per);
    (void)curl_easy_setopt(per->curl, CURLOPT_XFERINFOFUNCTION, xferinfo_cb);
    (void)curl_easy_setopt(per->curl, CURLOPT_XFERINFODATA, per);

    mcode = curl_multi_add_handle(multi, per->curl);
    if(mcode)
      return CURLE_OUT_OF_MEMORY;
    per->added = TRUE;
    all_added++;
    *addedp = TRUE;
  }
  *morep = per ? TRUE : FALSE;
  return CURLE_OK;
}

static CURLcode parallel_transfers(struct GlobalConfig *global,
                                   CURLSH *share)
{
  CURLM *multi;
  bool done = FALSE;
  CURLMcode mcode = CURLM_OK;
  CURLcode result = CURLE_OK;
  int still_running = 1;
  struct timeval start = tvnow();
  bool more_transfers;
  bool added_transfers;

  multi = curl_multi_init();
  if(!multi)
    return CURLE_OUT_OF_MEMORY;

  result = add_parallel_transfers(global, multi,
                                  &more_transfers, &added_transfers);
  if(result)
    return result;

  while(!done && !mcode && (still_running || more_transfers)) {
    mcode = curl_multi_poll(multi, NULL, 0, 1000, NULL);
    if(!mcode)
      mcode = curl_multi_perform(multi, &still_running);

    progress_meter(global, &start, FALSE);

    if(!mcode) {
      int rc;
      CURLMsg *msg;
      bool removed = FALSE;
      do {
        msg = curl_multi_info_read(multi, &rc);
        if(msg) {
          bool retry;
          struct per_transfer *ended;
          CURL *easy = msg->easy_handle;
          result = msg->data.result;
          curl_easy_getinfo(easy, CURLINFO_PRIVATE, (void *)&ended);
          curl_multi_remove_handle(multi, easy);

          result = post_transfer(global, share, ended, result, &retry);
          if(retry)
            continue;
          progress_finalize(ended); /* before it goes away */
          all_added--; /* one fewer added */
          removed = TRUE;
          (void)del_transfer(ended);
        }
      } while(msg);
      if(removed) {
        /* one or more transfers completed, add more! */
        (void)add_parallel_transfers(global, multi, &more_transfers,
                                     &added_transfers);
        if(added_transfers)
          /* we added new ones, make sure the loop doesn't exit yet */
          still_running = 1;
      }
    }
  }

  (void)progress_meter(global, &start, TRUE);

  /* Make sure to return some kind of error if there was a multi problem */
  if(mcode) {
    result = (mcode == CURLM_OUT_OF_MEMORY) ? CURLE_OUT_OF_MEMORY :
      /* The other multi errors should never happen, so return
         something suitably generic */
      CURLE_BAD_FUNCTION_ARGUMENT;
  }

  curl_multi_cleanup(multi);

  return result;
}

static CURLcode serial_transfers(struct GlobalConfig *global,
                                 CURLSH *share)
{
  CURLcode returncode = CURLE_OK;
  CURLcode result = CURLE_OK;
  struct per_transfer *per;
  for(per = transfers; per;) {
    bool retry;
    result = pre_transfer(global, per);
    if(result)
      break;

#ifndef CURL_DISABLE_LIBCURL_OPTION
    if(global->libcurl) {
      result = easysrc_perform();
      if(result)
        break;
    }
#endif
#ifdef CURLDEBUG
    if(global->test_event_based)
      result = curl_easy_perform_ev(per->curl);
    else
#endif
      result = curl_easy_perform(per->curl);

    /* store the result of the actual transfer */
    returncode = result;

    result = post_transfer(global, share, per, result, &retry);
    if(retry)
      continue;
    per = del_transfer(per);

    /* Bail out upon critical errors or --fail-early */
    if(result || is_fatal_error(returncode) ||
       (returncode && global->fail_early))
      break;
  }
  if(returncode)
    /* returncode errors have priority */
    result = returncode;
  return result;
}

static CURLcode operate_do(struct GlobalConfig *global,
                           struct OperationConfig *config,
                           CURLSH *share)
{
  CURLcode result = CURLE_OK;
  bool capath_from_env;

  /* Check we have a url */
  if(!config->url_list || !config->url_list->url) {
    helpf(global->errors, "no URL specified!\n");
    return CURLE_FAILED_INIT;
  }

  /* On WIN32 we can't set the path to curl-ca-bundle.crt
   * at compile time. So we look here for the file in two ways:
   * 1: look at the environment variable CURL_CA_BUNDLE for a path
   * 2: if #1 isn't found, use the windows API function SearchPath()
   *    to find it along the app's path (includes app's dir and CWD)
   *
   * We support the environment variable thing for non-Windows platforms
   * too. Just for the sake of it.
   */
  capath_from_env = false;
  if(!config->cacert &&
     !config->capath &&
     !config->insecure_ok) {
    CURL *curltls = curl_easy_init();
    struct curl_tlssessioninfo *tls_backend_info = NULL;

    /* With the addition of CAINFO support for Schannel, this search could find
     * a certificate bundle that was previously ignored. To maintain backward
     * compatibility, only perform this search if not using Schannel.
     */
    result = curl_easy_getinfo(curltls, CURLINFO_TLS_SSL_PTR,
                               &tls_backend_info);
    if(result)
      return result;

    /* Set the CA cert locations specified in the environment. For Windows if
     * no environment-specified filename is found then check for CA bundle
     * default filename curl-ca-bundle.crt in the user's PATH.
     *
     * If Schannel is the selected SSL backend then these locations are
     * ignored. We allow setting CA location for schannel only when explicitly
     * specified by the user via CURLOPT_CAINFO / --cacert.
     */
    if(tls_backend_info->backend != CURLSSLBACKEND_SCHANNEL) {
      char *env;
      env = curlx_getenv("CURL_CA_BUNDLE");
      if(env) {
        config->cacert = strdup(env);
        if(!config->cacert) {
          curl_free(env);
          helpf(global->errors, "out of memory\n");
          return CURLE_OUT_OF_MEMORY;
        }
      }
      else {
        env = curlx_getenv("SSL_CERT_DIR");
        if(env) {
          config->capath = strdup(env);
          if(!config->capath) {
            curl_free(env);
            helpf(global->errors, "out of memory\n");
            return CURLE_OUT_OF_MEMORY;
          }
          capath_from_env = true;
        }
        else {
          env = curlx_getenv("SSL_CERT_FILE");
          if(env) {
            config->cacert = strdup(env);
            if(!config->cacert) {
              curl_free(env);
              helpf(global->errors, "out of memory\n");
              return CURLE_OUT_OF_MEMORY;
            }
          }
        }
      }

      if(env)
        curl_free(env);
#ifdef WIN32
      else {
        result = FindWin32CACert(config, tls_backend_info->backend,
                                 "curl-ca-bundle.crt");
      }
#endif
    }
    curl_easy_cleanup(curltls);
  }

  if(!result)
    /* loop through the list of given URLs */
    result = create_transfers(global, config, share, capath_from_env);

  return result;
}

static CURLcode operate_transfers(struct GlobalConfig *global,
                                  CURLSH *share,
                                  CURLcode result)
{
  /* Save the values of noprogress and isatty to restore them later on */
  bool orig_noprogress = global->noprogress;
  bool orig_isatty = global->isatty;
  struct per_transfer *per;

  /* Time to actually do the transfers */
  if(!result) {
    if(global->parallel)
      result = parallel_transfers(global, share);
    else
      result = serial_transfers(global, share);
  }

  /* cleanup if there are any left */
  for(per = transfers; per;) {
    bool retry;
    (void)post_transfer(global, share, per, result, &retry);
    /* Free list of given URLs */
    clean_getout(per->config);

    /* Release metalink related resources here */
    clean_metalink(per->config);
    per = del_transfer(per);
  }

  /* Reset the global config variables */
  global->noprogress = orig_noprogress;
  global->isatty = orig_isatty;


  return result;
}

CURLcode operate(struct GlobalConfig *config, int argc, argv_item_t argv[])
{
  CURLcode result = CURLE_OK;

  /* Setup proper locale from environment */
#ifdef HAVE_SETLOCALE
  setlocale(LC_ALL, "");
#endif

  /* Parse .curlrc if necessary */
  if((argc == 1) ||
     (!curl_strequal(argv[1], "-q") &&
      !curl_strequal(argv[1], "--disable"))) {
    parseconfig(NULL, config); /* ignore possible failure */

    /* If we had no arguments then make sure a url was specified in .curlrc */
    if((argc < 2) && (!config->first->url_list)) {
      helpf(config->errors, NULL);
      result = CURLE_FAILED_INIT;
    }
  }

  if(!result) {
    /* Parse the command line arguments */
    ParameterError res = parse_args(config, argc, argv);
    if(res) {
      result = CURLE_OK;

      /* Check if we were asked for the help */
      if(res == PARAM_HELP_REQUESTED)
        tool_help();
      /* Check if we were asked for the manual */
      else if(res == PARAM_MANUAL_REQUESTED)
        hugehelp();
      /* Check if we were asked for the version information */
      else if(res == PARAM_VERSION_INFO_REQUESTED)
        tool_version_info();
      /* Check if we were asked to list the SSL engines */
      else if(res == PARAM_ENGINES_REQUESTED)
        tool_list_engines();
      else if(res == PARAM_LIBCURL_UNSUPPORTED_PROTOCOL)
        result = CURLE_UNSUPPORTED_PROTOCOL;
      else
        result = CURLE_FAILED_INIT;
    }
    else {
#ifndef CURL_DISABLE_LIBCURL_OPTION
      if(config->libcurl) {
        /* Initialise the libcurl source output */
        result = easysrc_init();
      }
#endif

      /* Perform the main operations */
      if(!result) {
        size_t count = 0;
        struct OperationConfig *operation = config->first;
        CURLSH *share = curl_share_init();
        if(!share) {
#ifndef CURL_DISABLE_LIBCURL_OPTION
          if(config->libcurl) {
            /* Cleanup the libcurl source output */
            easysrc_cleanup();
          }
#endif
          return CURLE_OUT_OF_MEMORY;
        }

        curl_share_setopt(share, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE);
        curl_share_setopt(share, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
        curl_share_setopt(share, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
        curl_share_setopt(share, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);
        curl_share_setopt(share, CURLSHOPT_SHARE, CURL_LOCK_DATA_PSL);

        /* Get the required arguments for each operation */
        do {
          result = get_args(operation, count++);

          operation = operation->next;
        } while(!result && operation);

        /* Set the current operation pointer */
        config->current = config->first;

        /* Setup all transfers */
        while(!result && config->current) {
          result = operate_do(config, config->current, share);
          config->current = config->current->next;
        }

        /* now run! */
        result = operate_transfers(config, share, result);

        curl_share_cleanup(share);
#ifndef CURL_DISABLE_LIBCURL_OPTION
        if(config->libcurl) {
          /* Cleanup the libcurl source output */
          easysrc_cleanup();

          /* Dump the libcurl code if previously enabled */
          dumpeasysrc(config);
        }
#endif
      }
      else
        helpf(config->errors, "out of memory\n");
    }
  }

  return result;
}
