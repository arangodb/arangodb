/**********************************************************************

  transcode.c -

  $Author: usa $
  created at: Tue Oct 30 16:10:22 JST 2007

  Copyright (C) 2007 Martin Duerst

**********************************************************************/

#include "mruby.h"
#ifdef INCLUDE_ENCODING
#include "encoding.h"
#include <sys/types.h> /* for ssize_t */
#include "transcode_data.h"
#include <ctype.h>
#include "st.h"
#include "variable.h"
#include <string.h>
#include "mruby/string.h"
#include "mruby/array.h"
#include "mruby/hash.h"
#include "error.h"
#include "mruby/numeric.h"
//#include "mio.h"
#include <stdio.h>


#define TYPE(o)     (o).tt//mrb_type(o)

#define E_CONVERTERNOTFOUND_ERROR   (mrb_class_obj_get(mrb, "ConverterNotFoundError"))
#define E_INVALIDBYTESEQUENCE_ERROR (mrb_class_obj_get(mrb, "InvalidByteSequenceError"))
#define E_UNDEFINEDCONVERSION_ERROR (mrb_class_obj_get(mrb, "UndefinedConversionError"))

/* mrb_value mrb_cEncoding = rb_define_class("Encoding", rb_cObject); */
mrb_value rb_eUndefinedConversionError;
mrb_value mrb_eInvalidByteSequenceError;
mrb_value rb_eConverterNotFoundError;

mrb_value mrb_cEncodingConverter;

static mrb_value sym_invalid, sym_undef, sym_replace, sym_fallback;
static mrb_value sym_xml, sym_text, sym_attr;
static mrb_value sym_universal_newline;
static mrb_value sym_crlf_newline;
static mrb_value sym_cr_newline;
static mrb_value sym_partial_input;

static mrb_value sym_invalid_byte_sequence;
static mrb_value sym_undefined_conversion;
static mrb_value sym_destination_buffer_full;
static mrb_value sym_source_buffer_empty;
static mrb_value sym_finished;
static mrb_value sym_after_output;
static mrb_value sym_incomplete_input;

static unsigned char *
allocate_converted_string(mrb_state *mrb,
        const char *sname, const char *dname,
        const unsigned char *str, size_t len,
        unsigned char *caller_dst_buf, size_t caller_dst_bufsize,
        size_t *dst_len_ptr);

/* dynamic structure, one per conversion (similar to iconv_t) */
/* may carry conversion state (e.g. for iso-2022-jp) */
typedef struct mrb_transcoding {
    const mrb_transcoder *transcoder;

    int flags;

    int resume_position;
    unsigned int next_table;
    mrb_value next_info;
    unsigned char next_byte;
    unsigned int output_index;

    ssize_t recognized_len; /* already interpreted */
    ssize_t readagain_len; /* not yet interpreted */
    union {
        unsigned char ary[8]; /* max_input <= sizeof(ary) */
        unsigned char *ptr; /* length: max_input */
    } readbuf; /* recognized_len + readagain_len used */

    ssize_t writebuf_off;
    ssize_t writebuf_len;
    union {
        unsigned char ary[8]; /* max_output <= sizeof(ary) */
        unsigned char *ptr; /* length: max_output */
    } writebuf;

    union mrb_transcoding_state_t { /* opaque data for stateful encoding */
        void *ptr;
        char ary[sizeof(double) > sizeof(void*) ? sizeof(double) : sizeof(void*)];
        double dummy_for_alignment;
    } state;
} mrb_transcoding;
#define TRANSCODING_READBUF(tc) \
    ((tc)->transcoder->max_input <= (int)sizeof((tc)->readbuf.ary) ? \
     (tc)->readbuf.ary : \
     (tc)->readbuf.ptr)
#define TRANSCODING_WRITEBUF(tc) \
    ((tc)->transcoder->max_output <= (int)sizeof((tc)->writebuf.ary) ? \
     (tc)->writebuf.ary : \
     (tc)->writebuf.ptr)
#define TRANSCODING_WRITEBUF_SIZE(tc) \
    ((tc)->transcoder->max_output <= (int)sizeof((tc)->writebuf.ary) ? \
     sizeof((tc)->writebuf.ary) : \
     (size_t)(tc)->transcoder->max_output)
#define TRANSCODING_STATE_EMBED_MAX ((int)sizeof(union mrb_transcoding_state_t))
#define TRANSCODING_STATE(tc) \
    ((tc)->transcoder->state_size <= (int)sizeof((tc)->state) ? \
     (tc)->state.ary : \
     (tc)->state.ptr)

typedef struct {
    struct mrb_transcoding *tc;
    unsigned char *out_buf_start;
    unsigned char *out_data_start;
    unsigned char *out_data_end;
    unsigned char *out_buf_end;
    mrb_econv_result_t last_result;
} mrb_econv_elem_t;

struct mrb_econv_t {
    int flags;
    const char *source_encoding_name;
    const char *destination_encoding_name;

    int started;

    const unsigned char *replacement_str;
    size_t replacement_len;
    const char *replacement_enc;
    int replacement_allocated;

    unsigned char *in_buf_start;
    unsigned char *in_data_start;
    unsigned char *in_data_end;
    unsigned char *in_buf_end;
    mrb_econv_elem_t *elems;
    int num_allocated;
    int num_trans;
    int num_finished;
    struct mrb_transcoding *last_tc;

    /* last error */
    struct {
        mrb_econv_result_t result;
        struct mrb_transcoding *error_tc;
        const char *source_encoding;
        const char *destination_encoding;
        const unsigned char *error_bytes_start;
        size_t error_bytes_len;
        size_t readagain_len;
    } last_error;

    /* The following fields are only for Encoding::Converter.
     * mrb_econv_open set them NULL. */
    mrb_encoding *source_encoding;
    mrb_encoding *destination_encoding;
};

/*
 *  Dispatch data and logic
 */

#define DECORATOR_P(sname, dname) (*(sname) == '\0')

typedef struct {
    const char *sname;
    const char *dname;
    const char *lib; /* null means means no need to load a library */
    const mrb_transcoder *transcoder;
} transcoder_entry_t;

static st_table *transcoder_table;

static transcoder_entry_t *
make_transcoder_entry(const char *sname, const char *dname)
{
    st_data_t val;
    st_table *table2;

    if (!st_lookup(transcoder_table, (st_data_t)sname, &val)) {
        val = (st_data_t)st_init_strcasetable();
        st_add_direct(transcoder_table, (st_data_t)sname, val);
    }
    table2 = (st_table *)val;
    if (!st_lookup(table2, (st_data_t)dname, &val)) {
        transcoder_entry_t *entry = malloc(sizeof(transcoder_entry_t));
        entry->sname = sname;
        entry->dname = dname;
        entry->lib = NULL;
        entry->transcoder = NULL;
        val = (st_data_t)entry;
        st_add_direct(table2, (st_data_t)dname, val);
    }
    return (transcoder_entry_t *)val;
}

static transcoder_entry_t *
get_transcoder_entry(const char *sname, const char *dname)
{
    st_data_t val;
    st_table *table2;

    if (!st_lookup(transcoder_table, (st_data_t)sname, &val)) {
        return NULL;
    }
    table2 = (st_table *)val;
    if (!st_lookup(table2, (st_data_t)dname, &val)) {
        return NULL;
    }
    return (transcoder_entry_t *)val;
}

void
mrb_register_transcoder(mrb_state *mrb, const mrb_transcoder *tr)
{
    const char *const sname = tr->src_encoding;
    const char *const dname = tr->dst_encoding;

    transcoder_entry_t *entry;

    entry = make_transcoder_entry(sname, dname);
    if (entry->transcoder) {
      mrb_raise(mrb, E_ARGUMENT_ERROR, "transcoder from %s to %s has been already registered",
             sname, dname);
    }

    entry->transcoder = tr;
}

static void
declare_transcoder(const char *sname, const char *dname, const char *lib)
{
    transcoder_entry_t *entry;

    entry = make_transcoder_entry(sname, dname);
    entry->lib = lib;
}

#define MAX_TRANSCODER_LIBNAME_LEN 64
static const char transcoder_lib_prefix[] = "enc/trans/";

void
mrb_declare_transcoder(mrb_state *mrb, const char *enc1, const char *enc2, const char *lib)
{
    if (!lib || strlen(lib) > MAX_TRANSCODER_LIBNAME_LEN) {
      mrb_raise(mrb, E_ARGUMENT_ERROR, "invalid library name - %s",
             lib ? lib : "(null)");
    }
    declare_transcoder(enc1, enc2, lib);
}

#define encoding_equal(enc1, enc2) (STRCASECMP(enc1, enc2) == 0)

typedef struct search_path_queue_tag {
    struct search_path_queue_tag *next;
    const char *enc;
} search_path_queue_t;

typedef struct {
    st_table *visited;
    search_path_queue_t *queue;
    search_path_queue_t **queue_last_ptr;
    const char *base_enc;
} search_path_bfs_t;

static int
transcode_search_path_i(st_data_t key, st_data_t val, st_data_t arg)
{
    const char *dname = (const char *)key;
    search_path_bfs_t *bfs = (search_path_bfs_t *)arg;
    search_path_queue_t *q;

    if (st_lookup(bfs->visited, (st_data_t)dname, &val)) {
        return ST_CONTINUE;
    }

    q = malloc(sizeof(search_path_queue_t));
    q->enc = dname;
    q->next = NULL;
    *bfs->queue_last_ptr = q;
    bfs->queue_last_ptr = &q->next;

    st_add_direct(bfs->visited, (st_data_t)dname, (st_data_t)bfs->base_enc);
    return ST_CONTINUE;
}

static int
transcode_search_path(mrb_state *mrb, const char *sname, const char *dname,
    void (*callback)(mrb_state *mrb, const char *sname, const char *dname, int depth, void *arg),
    void *arg)
{
    search_path_bfs_t bfs;
    search_path_queue_t *q;
    st_data_t val;
    st_table *table2;
    int found;
    int pathlen = -1;

    if (encoding_equal(sname, dname))
        return -1;

    q = malloc(sizeof(search_path_queue_t));//ALLOC(search_path_queue_t);
    q->enc = sname;
    q->next = NULL;
    bfs.queue_last_ptr = &q->next;
    bfs.queue = q;

    bfs.visited = st_init_strcasetable();
    st_add_direct(bfs.visited, (st_data_t)sname, (st_data_t)NULL);

    while (bfs.queue) {
        q = bfs.queue;
        bfs.queue = q->next;
        if (!bfs.queue)
            bfs.queue_last_ptr = &bfs.queue;

        if (!st_lookup(transcoder_table, (st_data_t)q->enc, &val)) {
            xfree(q);
            continue;
        }
        table2 = (st_table *)val;

        if (st_lookup(table2, (st_data_t)dname, &val)) {
            st_add_direct(bfs.visited, (st_data_t)dname, (st_data_t)q->enc);
            xfree(q);
            found = 1;
            goto cleanup;
        }

        bfs.base_enc = q->enc;
        st_foreach(table2, transcode_search_path_i, (st_data_t)&bfs);
        bfs.base_enc = NULL;

        xfree(q);
    }
    found = 0;

  cleanup:
    while (bfs.queue) {
        q = bfs.queue;
        bfs.queue = q->next;
        xfree(q);
    }

    if (found) {
        const char *enc = dname;
        int depth;
        pathlen = 0;
        while (1) {
            st_lookup(bfs.visited, (st_data_t)enc, &val);
            if (!val)
                break;
            pathlen++;
            enc = (const char *)val;
        }
        depth = pathlen;
        enc = dname;
        while (1) {
            st_lookup(bfs.visited, (st_data_t)enc, &val);
            if (!val)
                break;
            callback(mrb, (const char *)val, enc, --depth, arg);
            enc = (const char *)val;
        }
    }

    st_free_table(bfs.visited);

    return pathlen; /* is -1 if not found */
}

int
mrb_require(mrb_state *mrb, const char *fname)
{
    //mrb_value fn = mrb_str_new2(mrb, fname);
    //OBJ_FREEZE(fn);
    //return mrb_require_safe(fn, mrb_safe_level());
    mrb_str_new2(mrb, fname);
    return 1/* OK */;
}

static const mrb_transcoder *
load_transcoder_entry(mrb_state *mrb, transcoder_entry_t *entry)
{
    if (entry->transcoder)
        return entry->transcoder;

    if (entry->lib) {
        const char *lib = entry->lib;
        size_t len = strlen(lib);
        char path[sizeof(transcoder_lib_prefix) + MAX_TRANSCODER_LIBNAME_LEN];

        entry->lib = NULL;

        if (len > MAX_TRANSCODER_LIBNAME_LEN)
            return NULL;
        memcpy(path, transcoder_lib_prefix, sizeof(transcoder_lib_prefix) - 1);
        memcpy(path + sizeof(transcoder_lib_prefix) - 1, lib, len + 1);
        if (!mrb_require(mrb, path))
            return NULL;
    }

    if (entry->transcoder)
        return entry->transcoder;

    return NULL;
}

static const char*
get_replacement_character(const char *encname, size_t *len_ret, const char **repl_encname_ptr)
{
    if (encoding_equal(encname, "UTF-8")) {
        *len_ret = 3;
        *repl_encname_ptr = "UTF-8";
        return "\xEF\xBF\xBD";
    }
    else {
        *len_ret = 1;
        *repl_encname_ptr = "US-ASCII";
        return "?";
    }
}

/*
 *  Transcoding engine logic
 */

static const unsigned char *
transcode_char_start(mrb_transcoding *tc,
                         const unsigned char *in_start,
                         const unsigned char *inchar_start,
                         const unsigned char *in_p,
                         size_t *char_len_ptr)
{
    const unsigned char *ptr;
    if (inchar_start - in_start < tc->recognized_len) {
        memcpy(TRANSCODING_READBUF(tc) + tc->recognized_len,
               inchar_start, in_p - inchar_start);
        ptr = TRANSCODING_READBUF(tc);
    }
    else {
        ptr = inchar_start - tc->recognized_len;
    }
    *char_len_ptr = tc->recognized_len + (in_p - inchar_start);
    return ptr;
}

static mrb_econv_result_t
transcode_restartable0(mrb_state *mrb,
                      const unsigned char **in_pos, unsigned char **out_pos,
                      const unsigned char *in_stop, unsigned char *out_stop,
                      mrb_transcoding *tc,
                      const int opt)
{
    const mrb_transcoder *tr = tc->transcoder;
    int unitlen = tr->input_unit_length;
    ssize_t readagain_len = 0;

    const unsigned char *inchar_start;
    const unsigned char *in_p;

    unsigned char *out_p;

    in_p = inchar_start = *in_pos;

    out_p = *out_pos;

#define SUSPEND(ret, num) \
    do { \
        tc->resume_position = (num); \
        if (0 < in_p - inchar_start) \
            memmove(TRANSCODING_READBUF(tc)+tc->recognized_len, \
                   inchar_start, in_p - inchar_start); \
        *in_pos = in_p; \
        *out_pos = out_p; \
        tc->recognized_len += in_p - inchar_start; \
        if (readagain_len) { \
            tc->recognized_len -= readagain_len; \
            tc->readagain_len = readagain_len; \
        } \
        return ret; \
        resume_label ## num:; \
    } while (0)
#define SUSPEND_OBUF(num) \
    do { \
        while (out_stop - out_p < 1) { SUSPEND(econv_destination_buffer_full, num); } \
    } while (0)

#define SUSPEND_AFTER_OUTPUT(num) \
    if ((opt & ECONV_AFTER_OUTPUT) && *out_pos != out_p) { \
        SUSPEND(econv_after_output, num); \
    }

#define next_table (tc->next_table)
#define next_info (tc->next_info)
#define next_byte (tc->next_byte)
#define writebuf_len (tc->writebuf_len)
#define writebuf_off (tc->writebuf_off)

    switch (tc->resume_position) {
      case 0: break;
      case 1: goto resume_label1;
      case 2: goto resume_label2;
      case 3: goto resume_label3;
      case 4: goto resume_label4;
      case 5: goto resume_label5;
      case 6: goto resume_label6;
      case 7: goto resume_label7;
      case 8: goto resume_label8;
      case 9: goto resume_label9;
      case 10: goto resume_label10;
      case 11: goto resume_label11;
      case 12: goto resume_label12;
      case 13: goto resume_label13;
      case 14: goto resume_label14;
      case 15: goto resume_label15;
      case 16: goto resume_label16;
      case 17: goto resume_label17;
      case 18: goto resume_label18;
      case 19: goto resume_label19;
      case 20: goto resume_label20;
      case 21: goto resume_label21;
      case 22: goto resume_label22;
      case 23: goto resume_label23;
      case 24: goto resume_label24;
      case 25: goto resume_label25;
      case 26: goto resume_label26;
      case 27: goto resume_label27;
      case 28: goto resume_label28;
      case 29: goto resume_label29;
      case 30: goto resume_label30;
      case 31: goto resume_label31;
      case 32: goto resume_label32;
      case 33: goto resume_label33;
      case 34: goto resume_label34;
    }

    while (1) {
        inchar_start = in_p;
        tc->recognized_len = 0;
      next_table = tr->conv_tree_start;

        SUSPEND_AFTER_OUTPUT(24);

        if (in_stop <= in_p) {
            if (!(opt & ECONV_PARTIAL_INPUT))
                break;
            SUSPEND(econv_source_buffer_empty, 7);
            continue;
        }

#define BYTE_ADDR(index) (tr->byte_array + (index))
#define WORD_ADDR(index) (tr->word_array + INFO2WORDINDEX(index))
#define BL_BASE BYTE_ADDR(BYTE_LOOKUP_BASE(WORD_ADDR(next_table)))
#define BL_INFO WORD_ADDR(BYTE_LOOKUP_INFO(WORD_ADDR(next_table)))
#define BL_MIN_BYTE     (BL_BASE[0])
#define BL_MAX_BYTE     (BL_BASE[1])
#define BL_OFFSET(byte) (BL_BASE[2+(byte)-BL_MIN_BYTE])
#define BL_ACTION(byte) (BL_INFO[BL_OFFSET((byte))])

      next_byte = (unsigned char)*in_p++;
      follow_byte:
        if (next_byte < BL_MIN_BYTE || BL_MAX_BYTE < next_byte)
            next_info = mrb_fixnum_value(INVALID);
        else {
            next_info = mrb_fixnum_value(BL_ACTION(next_byte));
        }
  follow_info:
      switch (mrb_fixnum(next_info) & 0x1F) {
        case NOMAP:
            {
                const unsigned char *p = inchar_start;
                writebuf_off = 0;
                while (p < in_p) {
                    TRANSCODING_WRITEBUF(tc)[writebuf_off++] = (unsigned char)*p++;
                }
                writebuf_len = writebuf_off;
                writebuf_off = 0;
                while (writebuf_off < writebuf_len) {
                    SUSPEND_OBUF(3);
                    *out_p++ = TRANSCODING_WRITEBUF(tc)[writebuf_off++];
                }
            }
            continue;
        case 0x00: case 0x04: case 0x08: case 0x0C:
        case 0x10: case 0x14: case 0x18: case 0x1C:
            SUSPEND_AFTER_OUTPUT(25);
          while (in_p >= in_stop) {
                if (!(opt & ECONV_PARTIAL_INPUT))
                    goto incomplete;
                SUSPEND(econv_source_buffer_empty, 5);
          }
          next_byte = (unsigned char)*in_p++;
          next_table = (unsigned int)mrb_fixnum(next_info);
          goto follow_byte;
        case ZERObt: /* drop input */
          continue;
        case ONEbt:
            SUSPEND_OBUF(9); *out_p++ = getBT1(mrb_fixnum(next_info));
          continue;
        case TWObt:
            SUSPEND_OBUF(10); *out_p++ = getBT1(mrb_fixnum(next_info));
            SUSPEND_OBUF(21); *out_p++ = getBT2(mrb_fixnum(next_info));
          continue;
        case THREEbt:
            SUSPEND_OBUF(11); *out_p++ = getBT1(mrb_fixnum(next_info));
            SUSPEND_OBUF(15); *out_p++ = getBT2(mrb_fixnum(next_info));
            SUSPEND_OBUF(16); *out_p++ = getBT3(mrb_fixnum(next_info));
          continue;
        case FOURbt:
            SUSPEND_OBUF(12); *out_p++ = getBT0(mrb_fixnum(next_info));
            SUSPEND_OBUF(17); *out_p++ = getBT1(mrb_fixnum(next_info));
            SUSPEND_OBUF(18); *out_p++ = getBT2(mrb_fixnum(next_info));
            SUSPEND_OBUF(19); *out_p++ = getBT3(mrb_fixnum(next_info));
          continue;
        case GB4bt:
            SUSPEND_OBUF(29); *out_p++ = getGB4bt0((unsigned char)mrb_fixnum(next_info));
            SUSPEND_OBUF(30); *out_p++ = getGB4bt1((mrb_fixnum(next_info)));
            SUSPEND_OBUF(31); *out_p++ = getGB4bt2((unsigned char)mrb_fixnum(next_info));
            SUSPEND_OBUF(32); *out_p++ = getGB4bt3(mrb_fixnum(next_info));
          continue;
          case STR1:
            tc->output_index = 0;
            while (tc->output_index < STR1_LENGTH(BYTE_ADDR(STR1_BYTEINDEX(mrb_fixnum(next_info))))) {
                SUSPEND_OBUF(28); *out_p++ = BYTE_ADDR(STR1_BYTEINDEX(mrb_fixnum(next_info)))[1+tc->output_index];
                tc->output_index++;
            }
            continue;
        case FUNii:
          next_info = (*tr->func_ii)(TRANSCODING_STATE(tc), next_info);
          goto follow_info;
        case FUNsi:
            {
                const unsigned char *char_start;
                size_t char_len;
                char_start = transcode_char_start(tc, *in_pos, inchar_start, in_p, &char_len);
                next_info = (*tr->func_si)(TRANSCODING_STATE(tc), char_start, (size_t)char_len);
                goto follow_info;
            }
        case FUNio:
            SUSPEND_OBUF(13);
            if (tr->max_output <= out_stop - out_p)
                out_p += tr->func_io(TRANSCODING_STATE(tc),
                    next_info, out_p, out_stop - out_p);
            else {
                writebuf_len = tr->func_io(TRANSCODING_STATE(tc),
                    next_info,
                    TRANSCODING_WRITEBUF(tc), TRANSCODING_WRITEBUF_SIZE(tc));
                writebuf_off = 0;
                while (writebuf_off < writebuf_len) {
                    SUSPEND_OBUF(20);
                    *out_p++ = TRANSCODING_WRITEBUF(tc)[writebuf_off++];
                }
            }
          break;
        case FUNso:
            {
                const unsigned char *char_start;
                size_t char_len;
                SUSPEND_OBUF(14);
                if (tr->max_output <= out_stop - out_p) {
                    char_start = transcode_char_start(tc, *in_pos, inchar_start, in_p, &char_len);
                    out_p += tr->func_so(TRANSCODING_STATE(tc),
                        char_start, (size_t)char_len,
                        out_p, out_stop - out_p);
                }
                else {
                    char_start = transcode_char_start(tc, *in_pos, inchar_start, in_p, &char_len);
                    writebuf_len = tr->func_so(TRANSCODING_STATE(tc),
                        char_start, (size_t)char_len,
                        TRANSCODING_WRITEBUF(tc), TRANSCODING_WRITEBUF_SIZE(tc));
                    writebuf_off = 0;
                    while (writebuf_off < writebuf_len) {
                        SUSPEND_OBUF(22);
                        *out_p++ = TRANSCODING_WRITEBUF(tc)[writebuf_off++];
                    }
                }
                break;
            }
      case FUNsio:
            {
                const unsigned char *char_start;
                size_t char_len;
                SUSPEND_OBUF(33);
                if (tr->max_output <= out_stop - out_p) {
                    char_start = transcode_char_start(tc, *in_pos, inchar_start, in_p, &char_len);
                    out_p += tr->func_sio(TRANSCODING_STATE(tc),
                        char_start, (size_t)char_len, next_info,
                        out_p, out_stop - out_p);
                }
                else {
                    char_start = transcode_char_start(tc, *in_pos, inchar_start, in_p, &char_len);
                    writebuf_len = tr->func_sio(TRANSCODING_STATE(tc),
                        char_start, (size_t)char_len, next_info,
                        TRANSCODING_WRITEBUF(tc), TRANSCODING_WRITEBUF_SIZE(tc));
                    writebuf_off = 0;
                    while (writebuf_off < writebuf_len) {
                        SUSPEND_OBUF(34);
                        *out_p++ = TRANSCODING_WRITEBUF(tc)[writebuf_off++];
                    }
                }
                break;
            }
        case INVALID:
            if (tc->recognized_len + (in_p - inchar_start) <= unitlen) {
                if (tc->recognized_len + (in_p - inchar_start) < unitlen)
                    SUSPEND_AFTER_OUTPUT(26);
                while ((opt & ECONV_PARTIAL_INPUT) && tc->recognized_len + (in_stop - inchar_start) < unitlen) {
                    in_p = in_stop;
                    SUSPEND(econv_source_buffer_empty, 8);
                }
                if (tc->recognized_len + (in_stop - inchar_start) <= unitlen) {
                    in_p = in_stop;
                }
                else {
                    in_p = inchar_start + (unitlen - tc->recognized_len);
                }
            }
            else {
                ssize_t invalid_len; /* including the last byte which causes invalid */
                ssize_t discard_len;
                invalid_len = tc->recognized_len + (in_p - inchar_start);
                discard_len = ((invalid_len - 1) / unitlen) * unitlen;
                readagain_len = invalid_len - discard_len;
            }
            goto invalid;
        case UNDEF:
          goto undef;
        default:
          mrb_raise(mrb, mrb->eRuntimeError_class, "unknown transcoding instruction");
      }
      continue;

      invalid:
        SUSPEND(econv_invalid_byte_sequence, 1);
        continue;

      incomplete:
        SUSPEND(econv_incomplete_input, 27);
        continue;

      undef:
        SUSPEND(econv_undefined_conversion, 2);
        continue;
    }

    /* cleanup */
    if (tr->finish_func) {
        SUSPEND_OBUF(4);
        if (tr->max_output <= out_stop - out_p) {
            out_p += tr->finish_func(TRANSCODING_STATE(tc),
                out_p, out_stop - out_p);
        }
        else {
            writebuf_len = tr->finish_func(TRANSCODING_STATE(tc),
                TRANSCODING_WRITEBUF(tc), TRANSCODING_WRITEBUF_SIZE(tc));
            writebuf_off = 0;
            while (writebuf_off < writebuf_len) {
                SUSPEND_OBUF(23);
                *out_p++ = TRANSCODING_WRITEBUF(tc)[writebuf_off++];
            }
        }
    }
    while (1)
        SUSPEND(econv_finished, 6);
#undef SUSPEND
#undef next_table
#undef next_info
#undef next_byte
#undef writebuf_len
#undef writebuf_off
}

static mrb_econv_result_t
transcode_restartable(mrb_state *mrb,
                      const unsigned char **in_pos, unsigned char **out_pos,
                      const unsigned char *in_stop, unsigned char *out_stop,
                      mrb_transcoding *tc,
                      const int opt)
{
    if (tc->readagain_len) {
        unsigned char *readagain_buf = malloc(tc->readagain_len);//ALLOCA_N(unsigned char, tc->readagain_len);
        const unsigned char *readagain_pos = readagain_buf;
        const unsigned char *readagain_stop = readagain_buf + tc->readagain_len;
        mrb_econv_result_t res;

        memcpy(readagain_buf, TRANSCODING_READBUF(tc) + tc->recognized_len,
               tc->readagain_len);
        tc->readagain_len = 0;
        res = transcode_restartable0(mrb, &readagain_pos, out_pos, readagain_stop, out_stop, tc, opt|ECONV_PARTIAL_INPUT);
        if (res != econv_source_buffer_empty) {
            memcpy(TRANSCODING_READBUF(tc) + tc->recognized_len + tc->readagain_len,
                   readagain_pos, readagain_stop - readagain_pos);
            tc->readagain_len += readagain_stop - readagain_pos;
            return res;
        }
    }
    return transcode_restartable0(mrb, in_pos, out_pos, in_stop, out_stop, tc, opt);
}

static mrb_transcoding *
mrb_transcoding_open_by_transcoder(const mrb_transcoder *tr, int flags)
{
    mrb_transcoding *tc;

    tc = malloc(sizeof(mrb_transcoding));
    tc->transcoder = tr;
    tc->flags = flags;
    if (TRANSCODING_STATE_EMBED_MAX < tr->state_size)
        tc->state.ptr = xmalloc(tr->state_size);
    if (tr->state_init_func) {
        (tr->state_init_func)(TRANSCODING_STATE(tc)); /* xxx: check return value */
    }
    tc->resume_position = 0;
    tc->recognized_len = 0;
    tc->readagain_len = 0;
    tc->writebuf_len = 0;
    tc->writebuf_off = 0;
    if ((int)sizeof(tc->readbuf.ary) < tr->max_input) {
        tc->readbuf.ptr = xmalloc(tr->max_input);
    }
    if ((int)sizeof(tc->writebuf.ary) < tr->max_output) {
        tc->writebuf.ptr = xmalloc(tr->max_output);
    }
    return tc;
}

static mrb_econv_result_t
mrb_transcoding_convert(mrb_state *mrb, mrb_transcoding *tc,
  const unsigned char **input_ptr, const unsigned char *input_stop,
  unsigned char **output_ptr, unsigned char *output_stop,
  int flags)
{
    return transcode_restartable(mrb,
                input_ptr, output_ptr,
                input_stop, output_stop,
                tc, flags);
}

static void
mrb_transcoding_close(mrb_transcoding *tc)
{
    const mrb_transcoder *tr = tc->transcoder;
    if (tr->state_fini_func) {
        (tr->state_fini_func)(TRANSCODING_STATE(tc)); /* check return value? */
    }
    if (TRANSCODING_STATE_EMBED_MAX < tr->state_size)
        xfree(tc->state.ptr);
    if ((int)sizeof(tc->readbuf.ary) < tr->max_input)
        xfree(tc->readbuf.ptr);
    if ((int)sizeof(tc->writebuf.ary) < tr->max_output)
        xfree(tc->writebuf.ptr);
    xfree(tc);
}

static size_t
mrb_transcoding_memsize(mrb_transcoding *tc)
{
    size_t size = sizeof(mrb_transcoding);
    const mrb_transcoder *tr = tc->transcoder;

    if (TRANSCODING_STATE_EMBED_MAX < tr->state_size) {
      size += tr->state_size;
    }
    if ((int)sizeof(tc->readbuf.ary) < tr->max_input) {
      size += tr->max_input;
    }
    if ((int)sizeof(tc->writebuf.ary) < tr->max_output) {
      size += tr->max_output;
    }
    return size;
}

static mrb_econv_t *
mrb_econv_alloc(int n_hint)
{
    mrb_econv_t *ec;

    if (n_hint <= 0)
        n_hint = 1;

    ec = malloc(sizeof(mrb_econv_t));//ALLOC(mrb_econv_t);
    ec->flags = 0;
    ec->source_encoding_name = NULL;
    ec->destination_encoding_name = NULL;
    ec->started = 0;
    ec->replacement_str = NULL;
    ec->replacement_len = 0;
    ec->replacement_enc = NULL;
    ec->replacement_allocated = 0;
    ec->in_buf_start = NULL;
    ec->in_data_start = NULL;
    ec->in_data_end = NULL;
    ec->in_buf_end = NULL;
    ec->num_allocated = n_hint;
    ec->num_trans = 0;
    ec->elems = malloc(sizeof(mrb_econv_elem_t)*ec->num_allocated);//ALLOC_N(mrb_econv_elem_t, ec->num_allocated);
    ec->num_finished = 0;
    ec->last_tc = NULL;
    ec->last_error.result = econv_source_buffer_empty;
    ec->last_error.error_tc = NULL;
    ec->last_error.source_encoding = NULL;
    ec->last_error.destination_encoding = NULL;
    ec->last_error.error_bytes_start = NULL;
    ec->last_error.error_bytes_len = 0;
    ec->last_error.readagain_len = 0;
    ec->source_encoding = NULL;
    ec->destination_encoding = NULL;
    return ec;
}

static int
mrb_econv_add_transcoder_at(mrb_state *mrb, mrb_econv_t *ec, const mrb_transcoder *tr, int i)
{
    int n, j;
    int bufsize = 4096;
    unsigned char *p;

    if (ec->num_trans == ec->num_allocated) {
        n = ec->num_allocated * 2;
        mrb_realloc(mrb, ec->elems, sizeof(mrb_econv_elem_t)*n);//REALLOC_N(ec->elems, mrb_econv_elem_t, n);
        ec->num_allocated = n;
    }

    p = xmalloc(bufsize);

    memmove(ec->elems+i+1, ec->elems+i, sizeof(mrb_econv_elem_t)*(ec->num_trans-i));

    ec->elems[i].tc = mrb_transcoding_open_by_transcoder(tr, 0);
    ec->elems[i].out_buf_start = p;
    ec->elems[i].out_buf_end = p + bufsize;
    ec->elems[i].out_data_start = p;
    ec->elems[i].out_data_end = p;
    ec->elems[i].last_result = econv_source_buffer_empty;

    ec->num_trans++;

    if (!DECORATOR_P(tr->src_encoding, tr->dst_encoding))
        for (j = ec->num_trans-1; i <= j; j--) {
            mrb_transcoding *tc = ec->elems[j].tc;
            const mrb_transcoder *tr2 = tc->transcoder;
            if (!DECORATOR_P(tr2->src_encoding, tr2->dst_encoding)) {
                ec->last_tc = tc;
                break;
            }
        }

    return 0;
}

static mrb_econv_t *
mrb_econv_open_by_transcoder_entries(mrb_state *mrb, int n, transcoder_entry_t **entries)
{
    mrb_econv_t *ec;
    int i, ret;

    for (i = 0; i < n; i++) {
        const mrb_transcoder *tr;
        tr = load_transcoder_entry(mrb, entries[i]);
        if (!tr)
            return NULL;
    }

    ec = mrb_econv_alloc(n);

    for (i = 0; i < n; i++) {
        const mrb_transcoder *tr = load_transcoder_entry(mrb, entries[i]);
        ret = mrb_econv_add_transcoder_at(mrb, ec, tr, ec->num_trans);
        if (ret == -1) {
            mrb_econv_close(ec);
            return NULL;
        }
    }

    return ec;
}

struct trans_open_t {
    transcoder_entry_t **entries;
    int num_additional;
};

static void
trans_open_i(mrb_state *mrb, const char *sname, const char *dname, int depth, void *arg)
{
    struct trans_open_t *toarg = arg;

    if (!toarg->entries) {
        toarg->entries = malloc(sizeof(transcoder_entry_t*)*depth+1+toarg->num_additional);//ALLOC_N(transcoder_entry_t *, depth+1+toarg->num_additional);
    }
    toarg->entries[depth] = get_transcoder_entry(sname, dname);
}

static mrb_econv_t *
mrb_econv_open0(mrb_state *mrb, const char *sname, const char *dname, int ecflags)
{
    transcoder_entry_t **entries = NULL;
    int num_trans;
    mrb_econv_t *ec;

    mrb_encoding *senc, *denc;
    int sidx, didx;

    senc = NULL;
    if (*sname) {
        sidx = mrb_enc_find_index(mrb, sname);
        if (0 <= sidx) {
            senc = mrb_enc_from_index(mrb, sidx);
        }
    }

    denc = NULL;
    if (*dname) {
        didx = mrb_enc_find_index(mrb, dname);
        if (0 <= didx) {
            denc = mrb_enc_from_index(mrb, didx);
        }
    }

    if (*sname == '\0' && *dname == '\0') {
        num_trans = 0;
        entries = NULL;
    }
    else {
        struct trans_open_t toarg;
        toarg.entries = NULL;
        toarg.num_additional = 0;
        num_trans = transcode_search_path(mrb, sname, dname, trans_open_i, (void *)&toarg);
        entries = toarg.entries;
        if (num_trans < 0) {
            xfree(entries);
            return NULL;
        }
    }

    ec = mrb_econv_open_by_transcoder_entries(mrb, num_trans, entries);
    xfree(entries);
    if (!ec)
        return NULL;

    ec->flags = ecflags;
    ec->source_encoding_name = sname;
    ec->destination_encoding_name = dname;

    return ec;
}

#define MAX_ECFLAGS_DECORATORS 32

static int
decorator_names(int ecflags, const char **decorators_ret)
{
    int num_decorators;

    if ((ecflags & ECONV_CRLF_NEWLINE_DECORATOR) &&
        (ecflags & ECONV_CR_NEWLINE_DECORATOR))
        return -1;

    if ((ecflags & (ECONV_CRLF_NEWLINE_DECORATOR|ECONV_CR_NEWLINE_DECORATOR)) &&
        (ecflags & ECONV_UNIVERSAL_NEWLINE_DECORATOR))
        return -1;

    if ((ecflags & ECONV_XML_TEXT_DECORATOR) &&
        (ecflags & ECONV_XML_ATTR_CONTENT_DECORATOR))
        return -1;

    num_decorators = 0;

    if (ecflags & ECONV_XML_TEXT_DECORATOR)
        decorators_ret[num_decorators++] = "xml_text_escape";
    if (ecflags & ECONV_XML_ATTR_CONTENT_DECORATOR)
        decorators_ret[num_decorators++] = "xml_attr_content_escape";
    if (ecflags & ECONV_XML_ATTR_QUOTE_DECORATOR)
        decorators_ret[num_decorators++] = "xml_attr_quote";

    if (ecflags & ECONV_CRLF_NEWLINE_DECORATOR)
        decorators_ret[num_decorators++] = "crlf_newline";
    if (ecflags & ECONV_CR_NEWLINE_DECORATOR)
        decorators_ret[num_decorators++] = "cr_newline";
    if (ecflags & ECONV_UNIVERSAL_NEWLINE_DECORATOR)
        decorators_ret[num_decorators++] = "universal_newline";

    return num_decorators;
}

mrb_econv_t *
mrb_econv_open(mrb_state *mrb, const char *sname, const char *dname, int ecflags)
{
    mrb_econv_t *ec;
    int num_decorators;
    const char *decorators[MAX_ECFLAGS_DECORATORS];
    int i;

    num_decorators = decorator_names(ecflags, decorators);
    if (num_decorators == -1)
        return NULL;

    ec = mrb_econv_open0(mrb, sname, dname, ecflags & ECONV_ERROR_HANDLER_MASK);
    if (!ec)
        return NULL;

    for (i = 0; i < num_decorators; i++)
        if (mrb_econv_decorate_at_last(mrb, ec, decorators[i]) == -1) {
            mrb_econv_close(ec);
            return NULL;
        }

    ec->flags |= ecflags & ~ECONV_ERROR_HANDLER_MASK;

    return ec;
}

static int
trans_sweep(mrb_state *mrb, mrb_econv_t *ec,
    const unsigned char **input_ptr, const unsigned char *input_stop,
    unsigned char **output_ptr, unsigned char *output_stop,
    int flags,
    int start)
{
    int try;
    int i, f;

    const unsigned char **ipp, *is, *iold;
    unsigned char **opp, *os, *oold;
    mrb_econv_result_t res;

    try = 1;
    while (try) {
        try = 0;
        for (i = start; i < ec->num_trans; i++) {
            mrb_econv_elem_t *te = &ec->elems[i];

            if (i == 0) {
                ipp = input_ptr;
                is = input_stop;
            }
            else {
                mrb_econv_elem_t *prev_te = &ec->elems[i-1];
                ipp = (const unsigned char **)&prev_te->out_data_start;
                is = prev_te->out_data_end;
            }

            if (i == ec->num_trans-1) {
                opp = output_ptr;
                os = output_stop;
            }
            else {
                if (te->out_buf_start != te->out_data_start) {
                    ssize_t len = te->out_data_end - te->out_data_start;
                    ssize_t off = te->out_data_start - te->out_buf_start;
                    memmove(te->out_buf_start, te->out_data_start, len);
                    te->out_data_start = te->out_buf_start;
                    te->out_data_end -= off;
                }
                opp = &te->out_data_end;
                os = te->out_buf_end;
            }

            f = flags;
            if (ec->num_finished != i)
                f |= ECONV_PARTIAL_INPUT;
            if (i == 0 && (flags & ECONV_AFTER_OUTPUT)) {
                start = 1;
                flags &= ~ECONV_AFTER_OUTPUT;
            }
            if (i != 0)
                f &= ~ECONV_AFTER_OUTPUT;
            iold = *ipp;
            oold = *opp;
            te->last_result = res = mrb_transcoding_convert(mrb, te->tc, ipp, is, opp, os, f);
            if (iold != *ipp || oold != *opp)
                try = 1;

            switch (res) {
              case econv_invalid_byte_sequence:
              case econv_incomplete_input:
              case econv_undefined_conversion:
              case econv_after_output:
                return i;

              case econv_destination_buffer_full:
              case econv_source_buffer_empty:
                break;

              case econv_finished:
                ec->num_finished = i+1;
                break;
            }
        }
    }
    return -1;
}

static mrb_econv_result_t
mrb_trans_conv(mrb_state *mrb, mrb_econv_t *ec,
    const unsigned char **input_ptr, const unsigned char *input_stop,
    unsigned char **output_ptr, unsigned char *output_stop,
    int flags,
    int *result_position_ptr)
{
    int i;
    int needreport_index;
    int sweep_start;

    unsigned char empty_buf;
    unsigned char *empty_ptr = &empty_buf;

    if (!input_ptr) {
        input_ptr = (const unsigned char **)&empty_ptr;
        input_stop = empty_ptr;
    }

    if (!output_ptr) {
        output_ptr = &empty_ptr;
        output_stop = empty_ptr;
    }

    if (ec->elems[0].last_result == econv_after_output)
        ec->elems[0].last_result = econv_source_buffer_empty;

    needreport_index = -1;
    for (i = ec->num_trans-1; 0 <= i; i--) {
        switch (ec->elems[i].last_result) {
          case econv_invalid_byte_sequence:
          case econv_incomplete_input:
          case econv_undefined_conversion:
          case econv_after_output:
          case econv_finished:
            sweep_start = i+1;
            needreport_index = i;
            goto found_needreport;

          case econv_destination_buffer_full:
          case econv_source_buffer_empty:
            break;

          default:
            mrb_bug("unexpected transcode last result");
        }
    }

    /* /^[sd]+$/ is confirmed.  but actually /^s*d*$/. */

    if (ec->elems[ec->num_trans-1].last_result == econv_destination_buffer_full &&
        (flags & ECONV_AFTER_OUTPUT)) {
        mrb_econv_result_t res;

        res = mrb_trans_conv(mrb, ec, NULL, NULL, output_ptr, output_stop,
                (flags & ~ECONV_AFTER_OUTPUT)|ECONV_PARTIAL_INPUT,
                result_position_ptr);

        if (res == econv_source_buffer_empty)
            return econv_after_output;
        return res;
    }

    sweep_start = 0;

  found_needreport:

    do {
        needreport_index = trans_sweep(mrb, ec, input_ptr, input_stop, output_ptr, output_stop, flags, sweep_start);
        sweep_start = needreport_index + 1;
    } while (needreport_index != -1 && needreport_index != ec->num_trans-1);

    for (i = ec->num_trans-1; 0 <= i; i--) {
        if (ec->elems[i].last_result != econv_source_buffer_empty) {
            mrb_econv_result_t res = ec->elems[i].last_result;
            if (res == econv_invalid_byte_sequence ||
                res == econv_incomplete_input ||
                res == econv_undefined_conversion ||
                res == econv_after_output) {
                ec->elems[i].last_result = econv_source_buffer_empty;
            }
            if (result_position_ptr)
                *result_position_ptr = i;
            return res;
        }
    }
    if (result_position_ptr)
        *result_position_ptr = -1;
    return econv_source_buffer_empty;
}

static mrb_econv_result_t
mrb_econv_convert0(mrb_state *mrb, mrb_econv_t *ec,
    const unsigned char **input_ptr, const unsigned char *input_stop,
    unsigned char **output_ptr, unsigned char *output_stop,
    int flags)
{
    mrb_econv_result_t res;
    int result_position;
    int has_output = 0;

    memset(&ec->last_error, 0, sizeof(ec->last_error));

    if (ec->num_trans == 0) {
        size_t len;
        if (ec->in_buf_start && ec->in_data_start != ec->in_data_end) {
            if (output_stop - *output_ptr < ec->in_data_end - ec->in_data_start) {
                len = output_stop - *output_ptr;
                memcpy(*output_ptr, ec->in_data_start, len);
                *output_ptr = output_stop;
                ec->in_data_start += len;
                res = econv_destination_buffer_full;
                goto gotresult;
            }
            len = ec->in_data_end - ec->in_data_start;
            memcpy(*output_ptr, ec->in_data_start, len);
            *output_ptr += len;
            ec->in_data_start = ec->in_data_end = ec->in_buf_start;
            if (flags & ECONV_AFTER_OUTPUT) {
                res = econv_after_output;
                goto gotresult;
            }
        }
        if (output_stop - *output_ptr < input_stop - *input_ptr) {
            len = output_stop - *output_ptr;
        }
        else {
            len = input_stop - *input_ptr;
        }
        if (0 < len && (flags & ECONV_AFTER_OUTPUT)) {
            *(*output_ptr)++ = *(*input_ptr)++;
            res = econv_after_output;
            goto gotresult;
        }
        memcpy(*output_ptr, *input_ptr, len);
        *output_ptr += len;
        *input_ptr += len;
        if (*input_ptr != input_stop)
            res = econv_destination_buffer_full;
        else if (flags & ECONV_PARTIAL_INPUT)
            res = econv_source_buffer_empty;
        else
            res = econv_finished;
        goto gotresult;
    }

    if (ec->elems[ec->num_trans-1].out_data_start) {
        unsigned char *data_start = ec->elems[ec->num_trans-1].out_data_start;
        unsigned char *data_end = ec->elems[ec->num_trans-1].out_data_end;
        if (data_start != data_end) {
            size_t len;
            if (output_stop - *output_ptr < data_end - data_start) {
                len = output_stop - *output_ptr;
                memcpy(*output_ptr, data_start, len);
                *output_ptr = output_stop;
                ec->elems[ec->num_trans-1].out_data_start += len;
                res = econv_destination_buffer_full;
                goto gotresult;
            }
            len = data_end - data_start;
            memcpy(*output_ptr, data_start, len);
            *output_ptr += len;
            ec->elems[ec->num_trans-1].out_data_start =
                ec->elems[ec->num_trans-1].out_data_end =
                ec->elems[ec->num_trans-1].out_buf_start;
            has_output = 1;
        }
    }

    if (ec->in_buf_start &&
        ec->in_data_start != ec->in_data_end) {
        res = mrb_trans_conv(mrb, ec, (const unsigned char **)&ec->in_data_start, ec->in_data_end, output_ptr, output_stop,
                (flags&~ECONV_AFTER_OUTPUT)|ECONV_PARTIAL_INPUT, &result_position);
        if (res != econv_source_buffer_empty)
            goto gotresult;
    }

    if (has_output &&
        (flags & ECONV_AFTER_OUTPUT) &&
        *input_ptr != input_stop) {
        input_stop = *input_ptr;
        res = mrb_trans_conv(mrb, ec, input_ptr, input_stop, output_ptr, output_stop, flags, &result_position);
        if (res == econv_source_buffer_empty)
            res = econv_after_output;
    }
    else if ((flags & ECONV_AFTER_OUTPUT) ||
        ec->num_trans == 1) {
        res = mrb_trans_conv(mrb, ec, input_ptr, input_stop, output_ptr, output_stop, flags, &result_position);
    }
    else {
        flags |= ECONV_AFTER_OUTPUT;
        do {
            res = mrb_trans_conv(mrb, ec, input_ptr, input_stop, output_ptr, output_stop, flags, &result_position);
        } while (res == econv_after_output);
    }

  gotresult:
    ec->last_error.result = res;
    if (res == econv_invalid_byte_sequence ||
        res == econv_incomplete_input ||
        res == econv_undefined_conversion) {
        mrb_transcoding *error_tc = ec->elems[result_position].tc;
        ec->last_error.error_tc = error_tc;
        ec->last_error.source_encoding = error_tc->transcoder->src_encoding;
        ec->last_error.destination_encoding = error_tc->transcoder->dst_encoding;
        ec->last_error.error_bytes_start = TRANSCODING_READBUF(error_tc);
        ec->last_error.error_bytes_len = error_tc->recognized_len;
        ec->last_error.readagain_len = error_tc->readagain_len;
    }

    return res;
}

static int output_replacement_character(mrb_state *mrb, mrb_econv_t *ec);

static int
output_hex_charref(mrb_state *mrb, mrb_econv_t *ec)
{
    int ret;
    unsigned char utfbuf[1024];
    const unsigned char *utf;
    size_t utf_len;
    int utf_allocated = 0;
    char charef_buf[16];
    const unsigned char *p;

    if (encoding_equal(ec->last_error.source_encoding, "UTF-32BE")) {
        utf = ec->last_error.error_bytes_start;
        utf_len = ec->last_error.error_bytes_len;
    }
    else {
        utf = allocate_converted_string(mrb,
                ec->last_error.source_encoding, "UTF-32BE",
                ec->last_error.error_bytes_start, ec->last_error.error_bytes_len,
                utfbuf, sizeof(utfbuf),
                &utf_len);
        if (!utf)
            return -1;
        if (utf != utfbuf && utf != ec->last_error.error_bytes_start)
            utf_allocated = 1;
    }

    if (utf_len % 4 != 0)
        goto fail;

    p = utf;
    while (4 <= utf_len) {
        unsigned int u = 0;
        u += p[0] << 24;
        u += p[1] << 16;
        u += p[2] << 8;
        u += p[3];
        snprintf(charef_buf, sizeof(charef_buf), "&#x%X;", u);

        ret = mrb_econv_insert_output(mrb, ec, (unsigned char *)charef_buf, strlen(charef_buf), "US-ASCII");
        if (ret == -1)
            goto fail;

        p += 4;
        utf_len -= 4;
    }

    if (utf_allocated)
        xfree((void *)utf);
    return 0;

  fail:
    if (utf_allocated)
        xfree((void *)utf);
    return -1;
}

mrb_econv_result_t
mrb_econv_convert(mrb_state *mrb, mrb_econv_t *ec,
    const unsigned char **input_ptr, const unsigned char *input_stop,
    unsigned char **output_ptr, unsigned char *output_stop,
    int flags)
{
    mrb_econv_result_t ret;

    unsigned char empty_buf;
    unsigned char *empty_ptr = &empty_buf;

    ec->started = 1;

    if (!input_ptr) {
        input_ptr = (const unsigned char **)&empty_ptr;
        input_stop = empty_ptr;
    }

    if (!output_ptr) {
        output_ptr = &empty_ptr;
        output_stop = empty_ptr;
    }

  resume:
    ret = mrb_econv_convert0(mrb, ec, input_ptr, input_stop, output_ptr, output_stop, flags);

    if (ret == econv_invalid_byte_sequence ||
        ret == econv_incomplete_input) {
      /* deal with invalid byte sequence */
      /* todo: add more alternative behaviors */
        switch (ec->flags & ECONV_INVALID_MASK) {
          case ECONV_INVALID_REPLACE:
          if (output_replacement_character(mrb, ec) == 0)
                goto resume;
      }
    }

    if (ret == econv_undefined_conversion) {
      /* valid character in source encoding
       * but no related character(s) in destination encoding */
      /* todo: add more alternative behaviors */
        switch (ec->flags & ECONV_UNDEF_MASK) {
          case ECONV_UNDEF_REPLACE:
          if (output_replacement_character(mrb, ec) == 0)
                goto resume;
            break;

          case ECONV_UNDEF_HEX_CHARREF:
            if (output_hex_charref(mrb, ec) == 0)
                goto resume;
            break;
        }
    }

    return ret;
}

const char *
mrb_econv_encoding_to_insert_output(mrb_econv_t *ec)
{
    mrb_transcoding *tc = ec->last_tc;
    const mrb_transcoder *tr;

    if (tc == NULL)
        return "";

    tr = tc->transcoder;

    if (tr->asciicompat_type == asciicompat_encoder)
        return tr->src_encoding;
    return tr->dst_encoding;
}

static unsigned char *
allocate_converted_string(mrb_state *mrb,
        const char *sname, const char *dname,
        const unsigned char *str, size_t len,
        unsigned char *caller_dst_buf, size_t caller_dst_bufsize,
        size_t *dst_len_ptr)
{
    unsigned char *dst_str;
    size_t dst_len;
    size_t dst_bufsize;

    mrb_econv_t *ec;
    mrb_econv_result_t res;

    const unsigned char *sp;
    unsigned char *dp;

    if (caller_dst_buf)
        dst_bufsize = caller_dst_bufsize;
    else if (len == 0)
        dst_bufsize = 1;
    else
        dst_bufsize = len;

    ec = mrb_econv_open(mrb, sname, dname, 0);
    if (ec == NULL)
        return NULL;
    if (caller_dst_buf)
        dst_str = caller_dst_buf;
    else
        dst_str = xmalloc(dst_bufsize);
    dst_len = 0;
    sp = str;
    dp = dst_str+dst_len;
    res = mrb_econv_convert(mrb, ec, &sp, str+len, &dp, dst_str+dst_bufsize, 0);
    dst_len = dp - dst_str;
    while (res == econv_destination_buffer_full) {
        if (SIZE_MAX/2 < dst_bufsize) {
            goto fail;
        }
        dst_bufsize *= 2;
        if (dst_str == caller_dst_buf) {
            unsigned char *tmp;
            tmp = xmalloc(dst_bufsize);
            memcpy(tmp, dst_str, dst_bufsize/2);
            dst_str = tmp;
        }
        else {
            dst_str = xrealloc(dst_str, dst_bufsize);
        }
        dp = dst_str+dst_len;
        res = mrb_econv_convert(mrb, ec, &sp, str+len, &dp, dst_str+dst_bufsize, 0);
        dst_len = dp - dst_str;
    }
    if (res != econv_finished) {
        goto fail;
    }
    mrb_econv_close(ec);
    *dst_len_ptr = dst_len;
    return dst_str;

  fail:
    if (dst_str != caller_dst_buf)
        xfree(dst_str);
    mrb_econv_close(ec);
    return NULL;
}

/* result: 0:success -1:failure */
int
mrb_econv_insert_output(mrb_state *mrb, mrb_econv_t *ec,
    const unsigned char *str, size_t len, const char *str_encoding)
{
    const char *insert_encoding = mrb_econv_encoding_to_insert_output(ec);
    unsigned char insert_buf[4096];
    const unsigned char *insert_str = NULL;
    size_t insert_len;

    int last_trans_index;
    mrb_transcoding *tc;

    unsigned char **buf_start_p;
    unsigned char **data_start_p;
    unsigned char **data_end_p;
    unsigned char **buf_end_p;

    size_t need;

    ec->started = 1;

    if (len == 0)
        return 0;

    if (encoding_equal(insert_encoding, str_encoding)) {
        insert_str = str;
        insert_len = len;
    }
    else {
        insert_str = allocate_converted_string(mrb, str_encoding, insert_encoding,
                str, len, insert_buf, sizeof(insert_buf), &insert_len);
        if (insert_str == NULL)
            return -1;
    }

    need = insert_len;

    last_trans_index = ec->num_trans-1;
    if (ec->num_trans == 0) {
        tc = NULL;
        buf_start_p = &ec->in_buf_start;
        data_start_p = &ec->in_data_start;
        data_end_p = &ec->in_data_end;
        buf_end_p = &ec->in_buf_end;
    }
    else if (ec->elems[last_trans_index].tc->transcoder->asciicompat_type == asciicompat_encoder) {
        tc = ec->elems[last_trans_index].tc;
        need += tc->readagain_len;
        if (need < insert_len)
            goto fail;
        if (last_trans_index == 0) {
            buf_start_p = &ec->in_buf_start;
            data_start_p = &ec->in_data_start;
            data_end_p = &ec->in_data_end;
            buf_end_p = &ec->in_buf_end;
        }
        else {
            mrb_econv_elem_t *ee = &ec->elems[last_trans_index-1];
            buf_start_p = &ee->out_buf_start;
            data_start_p = &ee->out_data_start;
            data_end_p = &ee->out_data_end;
            buf_end_p = &ee->out_buf_end;
        }
    }
    else {
        mrb_econv_elem_t *ee = &ec->elems[last_trans_index];
        buf_start_p = &ee->out_buf_start;
        data_start_p = &ee->out_data_start;
        data_end_p = &ee->out_data_end;
        buf_end_p = &ee->out_buf_end;
        tc = ec->elems[last_trans_index].tc;
    }

    if (*buf_start_p == NULL) {
        unsigned char *buf = xmalloc(need);
        *buf_start_p = buf;
        *data_start_p = buf;
        *data_end_p = buf;
        *buf_end_p = buf+need;
    }
    else if ((size_t)(*buf_end_p - *data_end_p) < need) {
        memmove(*buf_start_p, *data_start_p, *data_end_p - *data_start_p);
        *data_end_p = *buf_start_p + (*data_end_p - *data_start_p);
        *data_start_p = *buf_start_p;
        if ((size_t)(*buf_end_p - *data_end_p) < need) {
            unsigned char *buf;
            size_t s = (*data_end_p - *buf_start_p) + need;
            if (s < need)
                goto fail;
            buf = xrealloc(*buf_start_p, s);
            *data_start_p = buf;
            *data_end_p = buf + (*data_end_p - *buf_start_p);
            *buf_start_p = buf;
            *buf_end_p = buf + s;
        }
    }

    memcpy(*data_end_p, insert_str, insert_len);
    *data_end_p += insert_len;
    if (tc && tc->transcoder->asciicompat_type == asciicompat_encoder) {
        memcpy(*data_end_p, TRANSCODING_READBUF(tc)+tc->recognized_len, tc->readagain_len);
        *data_end_p += tc->readagain_len;
        tc->readagain_len = 0;
    }

    if (insert_str != str && insert_str != insert_buf)
        xfree((void*)insert_str);
    return 0;

  fail:
    if (insert_str != str && insert_str != insert_buf)
        xfree((void*)insert_str);
    return -1;
}

void
mrb_econv_close(mrb_econv_t *ec)
{
    int i;

    if (ec->replacement_allocated) {
        xfree((void *)ec->replacement_str);
    }
    for (i = 0; i < ec->num_trans; i++) {
        mrb_transcoding_close(ec->elems[i].tc);
        if (ec->elems[i].out_buf_start)
            xfree(ec->elems[i].out_buf_start);
    }
    xfree(ec->in_buf_start);
    xfree(ec->elems);
    xfree(ec);
}

size_t
mrb_econv_memsize(mrb_econv_t *ec)
{
    size_t size = sizeof(mrb_econv_t);
    int i;

    if (ec->replacement_allocated) {
      size += ec->replacement_len;
    }
    for (i = 0; i < ec->num_trans; i++) {
      size += mrb_transcoding_memsize(ec->elems[i].tc);

      if (ec->elems[i].out_buf_start) {
            size += ec->elems[i].out_buf_end - ec->elems[i].out_buf_start;
      }
    }
    size += ec->in_buf_end - ec->in_buf_start;
    size += sizeof(mrb_econv_elem_t) * ec->num_allocated;

    return size;
}

int
mrb_econv_putbackable(mrb_econv_t *ec)
{
    if (ec->num_trans == 0)
        return 0;
#if SIZEOF_SIZE_T > SIZEOF_INT
    if (ec->elems[0].tc->readagain_len > INT_MAX) return INT_MAX;
#endif
    return (int)ec->elems[0].tc->readagain_len;
}

void
mrb_econv_putback(mrb_econv_t *ec, unsigned char *p, int n)
{
    mrb_transcoding *tc;
    if (ec->num_trans == 0 || n == 0)
        return;
    tc = ec->elems[0].tc;
    memcpy(p, TRANSCODING_READBUF(tc) + tc->recognized_len + tc->readagain_len - n, n);
    tc->readagain_len -= n;
}

struct asciicompat_encoding_t {
    const char *ascii_compat_name;
    const char *ascii_incompat_name;
};

static int
asciicompat_encoding_i(mrb_state *mrb, st_data_t key, st_data_t val, st_data_t arg)
{
    struct asciicompat_encoding_t *data = (struct asciicompat_encoding_t *)arg;
    transcoder_entry_t *entry = (transcoder_entry_t *)val;
    const mrb_transcoder *tr;

    if (DECORATOR_P(entry->sname, entry->dname))
        return ST_CONTINUE;
    tr = load_transcoder_entry(mrb, entry);
    if (tr && tr->asciicompat_type == asciicompat_decoder) {
        data->ascii_compat_name = tr->dst_encoding;
        return ST_STOP;
    }
    return ST_CONTINUE;
}

const char *
mrb_econv_asciicompat_encoding(const char *ascii_incompat_name)
{
    st_data_t v;
    st_table *table2;
    struct asciicompat_encoding_t data;

    if (!st_lookup(transcoder_table, (st_data_t)ascii_incompat_name, &v))
        return NULL;
    table2 = (st_table *)v;

    /*
     * Assumption:
     * There is at most one transcoder for
     * converting from ASCII incompatible encoding.
     *
     * For ISO-2022-JP, there is ISO-2022-JP -> stateless-ISO-2022-JP and no others.
     */
    if (table2->num_entries != 1)
        return NULL;

    data.ascii_incompat_name = ascii_incompat_name;
    data.ascii_compat_name = NULL;
    st_foreach(table2, asciicompat_encoding_i, (st_data_t)&data);
    return data.ascii_compat_name;
}

mrb_value
mrb_econv_substr_append(mrb_state *mrb, mrb_econv_t *ec, mrb_value src, long off, long len, mrb_value dst, int flags)
{
    unsigned const char *ss, *sp, *se;
    unsigned char *ds, *dp, *de;
    mrb_econv_result_t res;
    int max_output;

    if (mrb_nil_p(dst)) {
        dst = mrb_str_buf_new(mrb, len);
        if (ec->destination_encoding)
            mrb_enc_associate(mrb, dst, ec->destination_encoding);
    }

    if (ec->last_tc)
        max_output = ec->last_tc->transcoder->max_output;
    else
        max_output = 1;

    res = econv_destination_buffer_full;
    while (res == econv_destination_buffer_full) {
        long dlen = RSTRING_LEN(dst);
        if (mrb_str_capacity(dst) - dlen < (size_t)len + max_output) {
            unsigned long new_capa = (unsigned long)dlen + len + max_output;
            if (LONG_MAX < new_capa)
                mrb_raise(mrb, E_ARGUMENT_ERROR, "too long string");
            mrb_str_resize(mrb, dst, new_capa);
            mrb_str_set_len(mrb, dst, dlen);
        }
        ss = sp = (const unsigned char *)RSTRING_PTR(src) + off;
        se = ss + len;
        ds = (unsigned char *)RSTRING_PTR(dst);
        de = ds + mrb_str_capacity(dst);
        dp = ds += dlen;
        res = mrb_econv_convert(mrb, ec, &sp, se, &dp, de, flags);
        off += sp - ss;
        len -= sp - ss;
        mrb_str_set_len(mrb, dst, dlen + (dp - ds));
        mrb_econv_check_error(mrb, ec);
    }

    return dst;
}

mrb_value
mrb_econv_str_append(mrb_state *mrb, mrb_econv_t *ec, mrb_value src, mrb_value dst, int flags)
{
    return mrb_econv_substr_append(mrb, ec, src, 0, RSTRING_LEN(src), dst, flags);
}

mrb_value
mrb_econv_substr_convert(mrb_state *mrb, mrb_econv_t *ec, mrb_value src, long byteoff, long bytesize, int flags)
{
    return mrb_econv_substr_append(mrb, ec, src, byteoff, bytesize, mrb_nil_value(), flags);
}

mrb_value
mrb_econv_str_convert(mrb_state *mrb, mrb_econv_t *ec, mrb_value src, int flags)
{
    return mrb_econv_substr_append(mrb, ec, src, 0, RSTRING_LEN(src), mrb_nil_value(), flags);
}

static int
mrb_econv_add_converter(mrb_state *mrb, mrb_econv_t *ec, const char *sname, const char *dname, int n)
{
    transcoder_entry_t *entry;
    const mrb_transcoder *tr;

    if (ec->started != 0)
        return -1;

    entry = get_transcoder_entry(sname, dname);
    if (!entry)
        return -1;

    tr = load_transcoder_entry(mrb, entry);

    return mrb_econv_add_transcoder_at(mrb, ec, tr, n);
}

static int
mrb_econv_decorate_at(mrb_state *mrb, mrb_econv_t *ec, const char *decorator_name, int n)
{
    return mrb_econv_add_converter(mrb, ec, "", decorator_name, n);
}

int
mrb_econv_decorate_at_first(mrb_state *mrb, mrb_econv_t *ec, const char *decorator_name)
{
    const mrb_transcoder *tr;

    if (ec->num_trans == 0)
        return mrb_econv_decorate_at(mrb, ec, decorator_name, 0);

    tr = ec->elems[0].tc->transcoder;

    if (!DECORATOR_P(tr->src_encoding, tr->dst_encoding) &&
        tr->asciicompat_type == asciicompat_decoder)
        return mrb_econv_decorate_at(mrb, ec, decorator_name, 1);

    return mrb_econv_decorate_at(mrb, ec, decorator_name, 0);
}

int
mrb_econv_decorate_at_last(mrb_state *mrb, mrb_econv_t *ec, const char *decorator_name)
{
    const mrb_transcoder *tr;

    if (ec->num_trans == 0)
        return mrb_econv_decorate_at(mrb, ec, decorator_name, 0);

    tr = ec->elems[ec->num_trans-1].tc->transcoder;

    if (!DECORATOR_P(tr->src_encoding, tr->dst_encoding) &&
        tr->asciicompat_type == asciicompat_encoder)
        return mrb_econv_decorate_at(mrb, ec, decorator_name, ec->num_trans-1);

    return mrb_econv_decorate_at(mrb, ec, decorator_name, ec->num_trans);
}

void
mrb_econv_binmode(mrb_econv_t *ec)
{
    const mrb_transcoder *trs[3];
    int n, i, j;
    transcoder_entry_t *entry;
    int num_trans;

    n = 0;
    if (ec->flags & ECONV_UNIVERSAL_NEWLINE_DECORATOR) {
        entry = get_transcoder_entry("", "universal_newline");
        if (entry->transcoder)
            trs[n++] = entry->transcoder;
    }
    if (ec->flags & ECONV_CRLF_NEWLINE_DECORATOR) {
        entry = get_transcoder_entry("", "crlf_newline");
        if (entry->transcoder)
            trs[n++] = entry->transcoder;
    }
    if (ec->flags & ECONV_CR_NEWLINE_DECORATOR) {
        entry = get_transcoder_entry("", "cr_newline");
        if (entry->transcoder)
            trs[n++] = entry->transcoder;
    }

    num_trans = ec->num_trans;
    j = 0;
    for (i = 0; i < num_trans; i++) {
        int k;
        for (k = 0; k < n; k++)
            if (trs[k] == ec->elems[i].tc->transcoder)
                break;
        if (k == n) {
            ec->elems[j] = ec->elems[i];
            j++;
        }
        else {
            mrb_transcoding_close(ec->elems[i].tc);
            xfree(ec->elems[i].out_buf_start);
            ec->num_trans--;
        }
    }

    ec->flags &= ~(ECONV_UNIVERSAL_NEWLINE_DECORATOR|ECONV_CRLF_NEWLINE_DECORATOR|ECONV_CR_NEWLINE_DECORATOR);

}

static mrb_value
econv_description(mrb_state *mrb, const char *sname, const char *dname, int ecflags, mrb_value mesg)
{
    int has_description = 0;

    if (mrb_nil_p(mesg))
        mesg = mrb_str_new(mrb, NULL, 0);

    if (*sname != '\0' || *dname != '\0') {
        if (*sname == '\0')
            mrb_str_cat2(mrb, mesg, dname);
        else if (*dname == '\0')
            mrb_str_cat2(mrb, mesg, sname);
        else
            mrb_str_catf(mrb, mesg, "%s to %s", sname, dname);
        has_description = 1;
    }

    if (ecflags & (ECONV_UNIVERSAL_NEWLINE_DECORATOR|
                   ECONV_CRLF_NEWLINE_DECORATOR|
                   ECONV_CR_NEWLINE_DECORATOR|
                   ECONV_XML_TEXT_DECORATOR|
                   ECONV_XML_ATTR_CONTENT_DECORATOR|
                   ECONV_XML_ATTR_QUOTE_DECORATOR)) {
        const char *pre = "";
        if (has_description)
            mrb_str_cat2(mrb, mesg, " with ");
        if (ecflags & ECONV_UNIVERSAL_NEWLINE_DECORATOR)  {
            mrb_str_cat2(mrb, mesg, pre); pre = ",";
            mrb_str_cat2(mrb, mesg, "universal_newline");
        }
        if (ecflags & ECONV_CRLF_NEWLINE_DECORATOR) {
            mrb_str_cat2(mrb, mesg, pre); pre = ",";
            mrb_str_cat2(mrb, mesg, "crlf_newline");
        }
        if (ecflags & ECONV_CR_NEWLINE_DECORATOR) {
            mrb_str_cat2(mrb, mesg, pre); pre = ",";
            mrb_str_cat2(mrb, mesg, "cr_newline");
        }
        if (ecflags & ECONV_XML_TEXT_DECORATOR) {
            mrb_str_cat2(mrb, mesg, pre); pre = ",";
            mrb_str_cat2(mrb, mesg, "xml_text");
        }
        if (ecflags & ECONV_XML_ATTR_CONTENT_DECORATOR) {
            mrb_str_cat2(mrb, mesg, pre); pre = ",";
            mrb_str_cat2(mrb, mesg, "xml_attr_content");
        }
        if (ecflags & ECONV_XML_ATTR_QUOTE_DECORATOR) {
            mrb_str_cat2(mrb, mesg, pre); pre = ",";
            mrb_str_cat2(mrb, mesg, "xml_attr_quote");
        }
        has_description = 1;
    }
    if (!has_description) {
        mrb_str_cat2(mrb, mesg, "no-conversion");
    }

    return mesg;
}

mrb_value
mrb_econv_open_exc(mrb_state *mrb, const char *sname, const char *dname, int ecflags)
{
    mrb_value mesg, exc;
    mesg = mrb_str_new_cstr(mrb, "code converter not found (");
    econv_description(mrb, sname, dname, ecflags, mesg);
    mrb_str_cat2(mrb, mesg, ")");
    exc = mrb_exc_new3(mrb, E_CONVERTERNOTFOUND_ERROR, mesg);
    return exc;
}

static mrb_value
make_econv_exception(mrb_state *mrb, mrb_econv_t *ec)
{
    mrb_value mesg, exc;
    if (ec->last_error.result == econv_invalid_byte_sequence ||
        ec->last_error.result == econv_incomplete_input) {
        const char *err = (const char *)ec->last_error.error_bytes_start;
        size_t error_len = ec->last_error.error_bytes_len;
        mrb_value bytes = mrb_str_new(mrb, err, error_len);
        mrb_value dumped = mrb_str_dump(mrb, bytes);
        size_t readagain_len = ec->last_error.readagain_len;
        mrb_value bytes2 = mrb_nil_value();
        mrb_value dumped2;
        int idx;
        if (ec->last_error.result == econv_incomplete_input) {
            mesg = mrb_sprintf(mrb, "incomplete %s on %s",
                    //StringValueCStr(dumped),
                    mrb_string_value_cstr(mrb, &dumped),
                    ec->last_error.source_encoding);
        }
        else if (readagain_len) {
            bytes2 = mrb_str_new(mrb, err+error_len, readagain_len);
            dumped2 = mrb_str_dump(mrb, bytes2);
            mesg = mrb_sprintf(mrb, "%s followed by %s on %s",
                    //StringValueCStr(dumped),
                    mrb_string_value_cstr(mrb, &dumped),
                    //StringValueCStr(dumped2),
                    mrb_string_value_cstr(mrb, &dumped2),
                    ec->last_error.source_encoding);
        }
        else {
            mesg = mrb_sprintf(mrb, "%s on %s",
                    //StringValueCStr(dumped),
                    mrb_string_value_cstr(mrb, &dumped),
                    ec->last_error.source_encoding);
        }

        exc = mrb_exc_new3(mrb, E_INVALIDBYTESEQUENCE_ERROR, mesg);
        mrb_iv_set(mrb, exc, mrb_intern(mrb, "error_bytes"), bytes);
        mrb_iv_set(mrb, exc, mrb_intern(mrb, "readagain_bytes"), bytes2);
        mrb_iv_set(mrb, exc, mrb_intern(mrb, "incomplete_input"), ec->last_error.result == econv_incomplete_input ? mrb_true_value() : mrb_false_value());

set_encs:
        mrb_iv_set(mrb, exc, mrb_intern(mrb, "source_encoding_name"), mrb_str_new2(mrb, ec->last_error.source_encoding));
        mrb_iv_set(mrb, exc, mrb_intern(mrb, "destination_encoding_name"), mrb_str_new2(mrb, ec->last_error.destination_encoding));
        idx = mrb_enc_find_index(mrb, ec->last_error.source_encoding);
        if (0 <= idx)
            mrb_iv_set(mrb, exc, mrb_intern(mrb, "source_encoding"), mrb_enc_from_encoding(mrb, mrb_enc_from_index(mrb, idx)));
        idx = mrb_enc_find_index(mrb, ec->last_error.destination_encoding);
        if (0 <= idx)
            mrb_iv_set(mrb, exc, mrb_intern(mrb, "destination_encoding"), mrb_enc_from_encoding(mrb, mrb_enc_from_index(mrb, idx)));
        return exc;
    }
    if (ec->last_error.result == econv_undefined_conversion) {
        mrb_value bytes = mrb_str_new(mrb, (const char *)ec->last_error.error_bytes_start,
                                 ec->last_error.error_bytes_len);
        mrb_value dumped = mrb_nil_value();
        int idx;
        if (strcmp(ec->last_error.source_encoding, "UTF-8") == 0) {
            mrb_encoding *utf8 = mrb_utf8_encoding(mrb);
            const char *start, *end;
            int n;
            start = (const char *)ec->last_error.error_bytes_start;
            end = start + ec->last_error.error_bytes_len;
            n = mrb_enc_precise_mbclen(start, end, utf8);
            if (MBCLEN_CHARFOUND_P(n) &&
                (size_t)MBCLEN_CHARFOUND_LEN(n) == ec->last_error.error_bytes_len) {
                unsigned int cc = mrb_enc_mbc_to_codepoint(start, end, utf8);
                dumped = mrb_sprintf(mrb, "U+%04X", cc);
            }
        }
        if (mrb_obj_equal(mrb, dumped, mrb_nil_value()))
            dumped = mrb_str_dump(mrb, bytes);
        if (strcmp(ec->last_error.source_encoding,
                   ec->source_encoding_name) == 0 &&
            strcmp(ec->last_error.destination_encoding,
                   ec->destination_encoding_name) == 0) {
            mesg = mrb_sprintf(mrb, "%s from %s to %s",
                    //StringValueCStr(dumped),
                    mrb_string_value_cstr(mrb, &dumped),
                    ec->last_error.source_encoding,
                    ec->last_error.destination_encoding);
        }
        else {
            int i;
            mesg = mrb_sprintf(mrb, "%s to %s in conversion from %s",
                    //StringValueCStr(dumped),
                    mrb_string_value_cstr(mrb, &dumped),
                    ec->last_error.destination_encoding,
                    ec->source_encoding_name);
            for (i = 0; i < ec->num_trans; i++) {
                const mrb_transcoder *tr = ec->elems[i].tc->transcoder;
                if (!DECORATOR_P(tr->src_encoding, tr->dst_encoding))
                    mrb_str_catf(mrb, mesg, " to %s",
                                ec->elems[i].tc->transcoder->dst_encoding);
            }
        }
        exc = mrb_exc_new3(mrb, E_UNDEFINEDCONVERSION_ERROR, mesg);
        idx = mrb_enc_find_index(mrb, ec->last_error.source_encoding);
        if (0 <= idx)
            mrb_enc_associate_index(mrb, bytes, idx);
        mrb_iv_set(mrb, exc, mrb_intern(mrb, "error_char"), bytes);
        goto set_encs;
    }
    return mrb_nil_value();
}

static void
more_output_buffer(mrb_state *mrb,
        mrb_value destination,
        unsigned char *(*resize_destination)(mrb_state *, mrb_value, size_t, size_t),
        int max_output,
        unsigned char **out_start_ptr,
        unsigned char **out_pos,
        unsigned char **out_stop_ptr)
{
    size_t len = (*out_pos - *out_start_ptr);
    size_t new_len = (len + max_output) * 2;
    *out_start_ptr = resize_destination(mrb, destination, len, new_len);
    *out_pos = *out_start_ptr + len;
    *out_stop_ptr = *out_start_ptr + new_len;
}

static int
make_replacement(mrb_state *mrb, mrb_econv_t *ec)
{
    mrb_transcoding *tc;
    const mrb_transcoder *tr;
    mrb_encoding *enc;
    const unsigned char *replacement;
    const char *repl_enc;
    const char *ins_enc;
    size_t len;

    if (ec->replacement_str)
        return 0;

    ins_enc = mrb_econv_encoding_to_insert_output(ec);

    tc = ec->last_tc;
    if (*ins_enc) {
        tr = tc->transcoder;
        enc = mrb_enc_find(mrb, tr->dst_encoding);
        replacement = (const unsigned char *)get_replacement_character(ins_enc, &len, &repl_enc);
    }
    else {
        replacement = (unsigned char *)"?";
        len = 1;
        repl_enc = "";
    }

    ec->replacement_str = replacement;
    ec->replacement_len = len;
    ec->replacement_enc = repl_enc;
    ec->replacement_allocated = 0;
    return 0;
}

int
mrb_econv_set_replacement(mrb_state *mrb, mrb_econv_t *ec,
    const unsigned char *str, size_t len, const char *encname)
{
    unsigned char *str2;
    size_t len2;
    const char *encname2;

    encname2 = mrb_econv_encoding_to_insert_output(ec);

    if (encoding_equal(encname, encname2)) {
        str2 = xmalloc(len);
        memcpy(str2, str, len); /* xxx: str may be invalid */
        len2 = len;
        encname2 = encname;
    }
    else {
        str2 = allocate_converted_string(mrb, encname, encname2, str, len, NULL, 0, &len2);
        if (!str2)
            return -1;
    }

    if (ec->replacement_allocated) {
        xfree((void *)ec->replacement_str);
    }
    ec->replacement_allocated = 1;
    ec->replacement_str = str2;
    ec->replacement_len = len2;
    ec->replacement_enc = encname2;
    return 0;
}

static int
output_replacement_character(mrb_state *mrb, mrb_econv_t *ec)
{
    int ret;

    if (make_replacement(mrb, ec) == -1)
        return -1;

    ret = mrb_econv_insert_output(mrb, ec, ec->replacement_str, ec->replacement_len, ec->replacement_enc);
    if (ret == -1)
        return -1;

    return 0;
}

static void
transcode_loop(mrb_state *mrb,
             const unsigned char **in_pos, unsigned char **out_pos,
             const unsigned char *in_stop, unsigned char *out_stop,
               mrb_value destination,
               unsigned char *(*resize_destination)(mrb_state *, mrb_value, size_t, size_t),
               const char *src_encoding,
               const char *dst_encoding,
               int ecflags,
               mrb_value ecopts)
{
  mrb_econv_t *ec;
  mrb_transcoding *last_tc;
  mrb_econv_result_t ret;
  unsigned char *out_start = *out_pos;
  int max_output;
  mrb_value exc;
  mrb_value fallback = mrb_nil_value();
  mrb_value Qundef;
  Qundef.tt = 0;

    ec = mrb_econv_open_opts(mrb, src_encoding, dst_encoding, ecflags, ecopts);
    if (!ec)
        mrb_exc_raise(mrb, mrb_econv_open_exc(mrb, src_encoding, dst_encoding, ecflags));

    if (!mrb_nil_p(ecopts) && TYPE(ecopts) == MRB_TT_HASH)
      fallback = mrb_hash_get(mrb, ecopts, sym_fallback);
    last_tc = ec->last_tc;
    max_output = last_tc ? last_tc->transcoder->max_output : 1;

  resume:
    ret = mrb_econv_convert(mrb, ec, in_pos, in_stop, out_pos, out_stop, 0);

    if (!mrb_nil_p(fallback) && ret == econv_undefined_conversion) {
      mrb_value rep = mrb_enc_str_new(mrb,
            (const char *)ec->last_error.error_bytes_start,
            ec->last_error.error_bytes_len,
            mrb_enc_find(mrb, ec->last_error.source_encoding));
      rep = mrb_hash_getWithDef(mrb, fallback, rep, Qundef);//mrb_hash_lookup2(fallback, rep, Qundef);
      if (!mrb_obj_equal(mrb, rep, Qundef)) {
          //StringValue(rep);
          mrb_string_value(mrb, &rep);
          ret = mrb_econv_insert_output(mrb, ec, (const unsigned char *)RSTRING_PTR(rep),
                RSTRING_LEN(rep), mrb_enc_name(mrb_enc_get(mrb, rep)));
          if ((int)ret == -1) {
            mrb_raise(mrb, E_ARGUMENT_ERROR, "too big fallback string");
          }
          goto resume;
      }
    }

    if (ret == econv_invalid_byte_sequence ||
        ret == econv_incomplete_input ||
        ret == econv_undefined_conversion) {
        exc = make_econv_exception(mrb, ec);
        mrb_econv_close(ec);
      mrb_exc_raise(mrb, exc);
    }

    if (ret == econv_destination_buffer_full) {
        more_output_buffer(mrb, destination, resize_destination, max_output, &out_start, out_pos, &out_stop);
        goto resume;
    }

    mrb_econv_close(ec);
    return;
}

/*
 *  String-specific code
 */

static unsigned char *
str_transcoding_resize(mrb_state *mrb, mrb_value destination, size_t len, size_t new_len)
{
    mrb_str_resize(mrb, destination, new_len);
    return (unsigned char *)RSTRING_PTR(destination);
}

static int
econv_opts(mrb_state *mrb, mrb_value opt)
{
    mrb_value v;
    int ecflags = 0;

    v = mrb_hash_get(mrb, opt, sym_invalid);
    if (mrb_nil_p(v)) {
    }
    else if (mrb_obj_equal(mrb, v, sym_replace)) {
        ecflags |= ECONV_INVALID_REPLACE;
    }
    else {
        mrb_raise(mrb, E_ARGUMENT_ERROR, "unknown value for invalid character option");
    }

    v = mrb_hash_get(mrb, opt, sym_undef);
    if (mrb_nil_p(v)) {
    }
    else if (mrb_obj_equal(mrb, v, sym_replace)) {
        ecflags |= ECONV_UNDEF_REPLACE;
    }
    else {
        mrb_raise(mrb, E_ARGUMENT_ERROR, "unknown value for undefined character option");
    }

    v = mrb_hash_get(mrb, opt, sym_replace);
    if (!mrb_nil_p(v) && !(ecflags & ECONV_INVALID_REPLACE)) {
        ecflags |= ECONV_UNDEF_REPLACE;
    }

    v = mrb_hash_get(mrb, opt, sym_xml);
    if (!mrb_nil_p(v)) {
        if (mrb_obj_equal(mrb, v, sym_text)) {
            ecflags |= ECONV_XML_TEXT_DECORATOR|ECONV_UNDEF_HEX_CHARREF;
        }
        else if (mrb_obj_equal(mrb, v, sym_attr)) {
            ecflags |= ECONV_XML_ATTR_CONTENT_DECORATOR|ECONV_XML_ATTR_QUOTE_DECORATOR|ECONV_UNDEF_HEX_CHARREF;
        }
        else if (TYPE(v) == MRB_TT_SYMBOL) {
            mrb_raise(mrb, E_ARGUMENT_ERROR, "unexpected value for xml option: %s", mrb_sym2name(mrb, SYM2ID(v)));
        }
        else {
            mrb_raise(mrb, E_ARGUMENT_ERROR, "unexpected value for xml option");
        }
    }

    v = mrb_hash_get(mrb, opt, sym_universal_newline);
    if (RTEST(v))
        ecflags |= ECONV_UNIVERSAL_NEWLINE_DECORATOR;

    v = mrb_hash_get(mrb, opt, sym_crlf_newline);
    if (RTEST(v))
        ecflags |= ECONV_CRLF_NEWLINE_DECORATOR;

    v = mrb_hash_get(mrb, opt, sym_cr_newline);
    if (RTEST(v))
        ecflags |= ECONV_CR_NEWLINE_DECORATOR;

    return ecflags;
}

int
mrb_econv_prepare_opts(mrb_state *mrb, mrb_value opthash, mrb_value *opts)
{
    int ecflags;
    mrb_value newhash = mrb_nil_value();
    mrb_value v;

    if (mrb_nil_p(opthash)) {
        *opts = mrb_nil_value();
        return 0;
    }
    ecflags = econv_opts(mrb, opthash);

    v = mrb_hash_get(mrb, opthash, sym_replace);
    if (!mrb_nil_p(v)) {
      //StringValue(v);
      mrb_string_value(mrb, &v);
      if (mrb_enc_str_coderange(mrb, v) == ENC_CODERANGE_BROKEN) {
          mrb_value dumped = mrb_str_dump(mrb, v);
          mrb_raise(mrb, E_ARGUMENT_ERROR, "replacement string is broken: %s as %s",
                 //StringValueCStr(dumped),
                 mrb_string_value_cstr(mrb, &dumped),
                 mrb_enc_name(mrb_enc_get(mrb, v)));
      }
      v = mrb_str_new_frozen(mrb, v);
      newhash = mrb_hash_new_capa(mrb, 0);
      mrb_hash_set(mrb, newhash, sym_replace, v);
    }

    v = mrb_hash_get(mrb, opthash, sym_fallback);
    if (!mrb_nil_p(v)) {
      v = mrb_convert_type(mrb, v, MRB_TT_HASH, "Hash", "to_hash");
      if (!mrb_nil_p(v)) {
          if (mrb_nil_p(newhash))
            newhash = mrb_hash_new_capa(mrb, 0);
          mrb_hash_set(mrb, newhash, sym_fallback, v);
      }
    }

    //if (!mrb_nil_p(newhash))
    //    mrb_hash_freeze(newhash);
    *opts = newhash;

    return ecflags;
}

mrb_econv_t *
mrb_econv_open_opts(mrb_state *mrb, const char *source_encoding, const char *destination_encoding, int ecflags, mrb_value opthash)
{
    mrb_econv_t *ec;
    mrb_value replacement;

    if (mrb_nil_p(opthash)) {
        replacement = mrb_nil_value();
    }
    else {
        if (TYPE(opthash) != MRB_TT_HASH /*|| !OBJ_FROZEN(opthash)*/)
            mrb_bug("mrb_econv_open_opts called with invalid opthash");
        replacement = mrb_hash_get(mrb, opthash, sym_replace);
    }

    ec = mrb_econv_open(mrb, source_encoding, destination_encoding, ecflags);
    if (!ec)
        return ec;

    if (!mrb_nil_p(replacement)) {
        int ret;
        mrb_encoding *enc = mrb_enc_get(mrb, replacement);

        ret = mrb_econv_set_replacement(mrb, ec,
                (const unsigned char *)RSTRING_PTR(replacement),
                RSTRING_LEN(replacement),
                mrb_enc_name(enc));
        if (ret == -1) {
            mrb_econv_close(ec);
            return NULL;
        }
    }
    return ec;
}

static int
enc_arg(mrb_state *mrb, mrb_value *arg, const char **name_p, mrb_encoding **enc_p)
{
    mrb_encoding *enc;
    const char *n;
    int encidx;
    mrb_value encval;

    if (((encidx = mrb_to_encoding_index(mrb, encval = *arg)) < 0) ||
      !(enc = mrb_enc_from_index(mrb, encidx))) {
      enc = NULL;
      encidx = 0;
      //n = StringValueCStr(*arg);
      n = mrb_string_value_cstr(mrb, arg);
    }
    else {
      n = mrb_enc_name(enc);
    }

    *name_p = n;
    *enc_p = enc;

    return encidx;
}

static int
str_transcode_enc_args(mrb_state *mrb,
        mrb_value str, mrb_value *arg1, mrb_value *arg2,
        const char **sname_p, mrb_encoding **senc_p,
        const char **dname_p, mrb_encoding **denc_p)
{
    mrb_encoding *senc, *denc;
    const char *sname, *dname;
    int sencidx, dencidx;

    dencidx = enc_arg(mrb, arg1, &dname, &denc);

    if (mrb_nil_p(*arg2)) {
      sencidx = mrb_enc_get_index(mrb, str);
      senc = mrb_enc_from_index(mrb, sencidx);
      sname = mrb_enc_name(senc);
    }
    else {
        sencidx = enc_arg(mrb, arg2, &sname, &senc);
    }

    *sname_p = sname;
    *senc_p = senc;
    *dname_p = dname;
    *denc_p = denc;
    return dencidx;
}

mrb_value
mrb_str_tmp_new(mrb_state *mrb, long len)
{
    return mrb_str_new(mrb, 0, len);
}

static int
str_transcode0(mrb_state *mrb, int argc, mrb_value *argv, mrb_value *self, int ecflags, mrb_value ecopts)
{

    mrb_value dest;
    mrb_value str = *self;
    mrb_value arg1, arg2;
    long blen, slen;
    unsigned char *buf, *bp, *sp;
    const unsigned char *fromp;
    mrb_encoding *senc, *denc;
    const char *sname, *dname;
    int dencidx;

    if (argc <0 || argc > 2) {
      mrb_raise(mrb, E_ARGUMENT_ERROR, "wrong number of arguments (%d for 0..2)", argc);
    }

    if (argc == 0) {
      arg1 = mrb_enc_default_internal(mrb);
      if (mrb_nil_p(arg1)) {
          if (!ecflags) return -1;
          arg1 = mrb_obj_encoding(mrb, str);
      }
      ecflags |= ECONV_INVALID_REPLACE | ECONV_UNDEF_REPLACE;
    }
    else {
      arg1 = argv[0];
    }
    arg2 = argc<=1 ? mrb_nil_value() : argv[1];
    dencidx = str_transcode_enc_args(mrb, str, &arg1, &arg2, &sname, &senc, &dname, &denc);

    if ((ecflags & (ECONV_UNIVERSAL_NEWLINE_DECORATOR|
                    ECONV_CRLF_NEWLINE_DECORATOR|
                    ECONV_CR_NEWLINE_DECORATOR|
                    ECONV_XML_TEXT_DECORATOR|
                    ECONV_XML_ATTR_CONTENT_DECORATOR|
                    ECONV_XML_ATTR_QUOTE_DECORATOR)) == 0) {
        if (senc && senc == denc) {
            return mrb_nil_p(arg2) ? -1 : dencidx;
        }
        if (senc && denc && mrb_enc_asciicompat(mrb, senc) && mrb_enc_asciicompat(mrb, denc)) {
            if (mrb_enc_str_coderange(mrb, str) == ENC_CODERANGE_7BIT) {
                return dencidx;
            }
        }
        if (encoding_equal(sname, dname)) {
            return mrb_nil_p(arg2) ? -1 : dencidx;
        }
    }
    else {
        if (encoding_equal(sname, dname)) {
            sname = "";
            dname = "";
        }
    }

    fromp = sp = (unsigned char *)RSTRING_PTR(str);
    slen = RSTRING_LEN(str);
    blen = slen + 30; /* len + margin */
    dest = mrb_str_tmp_new(mrb, blen);
    bp = (unsigned char *)RSTRING_PTR(dest);

    transcode_loop(mrb, &fromp, &bp, (sp+slen), (bp+blen), dest, str_transcoding_resize, sname, dname, ecflags, ecopts);
    if (fromp != sp+slen) {
        mrb_raise(mrb, E_ARGUMENT_ERROR, "not fully converted, %"PRIdPTRDIFF" bytes left", sp+slen-fromp);
    }
    buf = (unsigned char *)RSTRING_PTR(dest);
    *bp = '\0';
    mrb_str_set_len(mrb, dest, bp - buf);

    /* set encoding */
    if (!denc) {
      dencidx = mrb_define_dummy_encoding(mrb, dname);
    }
    *self = dest;

    return dencidx;
}

static int
str_transcode(mrb_state *mrb, int argc, mrb_value *argv, mrb_value *self)
{
    mrb_value opt;
    int ecflags = 0;
    mrb_value ecopts = mrb_nil_value();

    if (0 < argc) {
        opt = mrb_check_convert_type(mrb, argv[argc-1], MRB_TT_HASH, "Hash", "to_hash");
        if (!mrb_nil_p(opt)) {
            argc--;
            ecflags = mrb_econv_prepare_opts(mrb, opt, &ecopts);
        }
    }
    return str_transcode0(mrb, argc, argv, self, ecflags, ecopts);
}

static inline mrb_value
str_encode_associate(mrb_state *mrb, mrb_value str, int encidx)
{
    int cr = 0;

    mrb_enc_associate_index(mrb, str, encidx);

    /* transcoded string never be broken. */
    if (mrb_enc_asciicompat(mrb, mrb_enc_from_index(mrb, encidx))) {
      mrb_str_coderange_scan_restartable(RSTRING_PTR(str), RSTRING_END(str), 0, &cr);
    }
    else {
      cr = ENC_CODERANGE_VALID;
    }
    ENC_CODERANGE_SET(str, cr);
    return str;
}

/*
 *  call-seq:
 *     str.encode!(encoding [, options] )   -> str
 *     str.encode!(dst_encoding, src_encoding [, options] )   -> str
 *
 *  The first form transcodes the contents of <i>str</i> from
 *  str.encoding to +encoding+.
 *  The second form transcodes the contents of <i>str</i> from
 *  src_encoding to dst_encoding.
 *  The options Hash gives details for conversion. See String#encode
 *  for details.
 *  Returns the string even if no changes were made.
 */

static mrb_value
str_encode_bang(mrb_state *mrb, /*int argc, mrb_value *argv,*/ mrb_value str)
{
  mrb_value argv[16];
  int argc;

  mrb_get_args(mrb, "*", &argv, &argc);
  mrb_value newstr;
  int encidx;

  //if (OBJ_FROZEN(str)) { /* in future, may use str_frozen_check from string.c, but that's currently static */
  //  mrb_raise(mrb, mrb->eRuntimeError_class, "string frozen");
  //}

  newstr = str;
  encidx = str_transcode(mrb, argc, argv, &newstr);

  if (encidx < 0) return str;
  mrb_str_shared_replace(mrb, str, newstr);
  return str_encode_associate(mrb, str, encidx);
}

/*
 *  call-seq:
 *     str.encode(encoding [, options] )   -> str
 *     str.encode(dst_encoding, src_encoding [, options] )   -> str
 *     str.encode([options])   -> str
 *
 *  The first form returns a copy of <i>str</i> transcoded
 *  to encoding +encoding+.
 *  The second form returns a copy of <i>str</i> transcoded
 *  from src_encoding to dst_encoding.
 *  The last form returns a copy of <i>str</i> transcoded to
 *  <code>Encoding.default_internal</code>.
 *  By default, the first and second form raise
 *  Encoding::UndefinedConversionError for characters that are
 *  undefined in the destination encoding, and
 *  Encoding::InvalidByteSequenceError for invalid byte sequences
 *  in the source encoding. The last form by default does not raise
 *  exceptions but uses replacement strings.
 *  The <code>options</code> Hash gives details for conversion.
 *
 *  === options
 *  The hash <code>options</code> can have the following keys:
 *  :invalid ::
 *    If the value is <code>:replace</code>, <code>#encode</code> replaces
 *    invalid byte sequences in <code>str</code> with the replacement character.
 *    The default is to raise the exception
 *  :undef ::
 *    If the value is <code>:replace</code>, <code>#encode</code> replaces
 *    characters which are undefined in the destination encoding with
 *    the replacement character.
 *  :replace ::
 *    Sets the replacement string to the value. The default replacement
 *    string is "\uFFFD" for Unicode encoding forms, and "?" otherwise.
 *  :fallback ::
 *    Sets the replacement string by the hash for undefined character.
 *    Its key is a such undefined character encoded in source encoding
 *    of current transcoder. Its value can be any encoding until it
 *    can be converted into the destination encoding of the transcoder.
 *  :xml ::
 *    The value must be <code>:text</code> or <code>:attr</code>.
 *    If the value is <code>:text</code> <code>#encode</code> replaces
 *    undefined characters with their (upper-case hexadecimal) numeric
 *    character references. '&', '<', and '>' are converted to "&amp;",
 *    "&lt;", and "&gt;", respectively.
 *    If the value is <code>:attr</code>, <code>#encode</code> also quotes
 *    the replacement result (using '"'), and replaces '"' with "&quot;".
 *  :cr_newline ::
 *    Replaces LF ("\n") with CR ("\r") if value is true.
 *  :crlf_newline ::
 *    Replaces LF ("\n") with CRLF ("\r\n") if value is true.
 *  :universal_newline ::
 *    Replaces CRLF ("\r\n") and CR ("\r") with LF ("\n") if value is true.
 */

static mrb_value
str_encode(mrb_state *mrb, /*int argc, mrb_value *argv,*/ mrb_value str)
{
  mrb_value argv[16];
  int argc;

  mrb_get_args(mrb, "*", &argv, &argc);
  mrb_value newstr = str;
  int encidx = str_transcode(mrb, argc, argv, &newstr);

  if (encidx < 0) return mrb_str_dup(mrb, str);
  if (mrb_obj_equal(mrb, newstr, str)) {
    newstr = mrb_str_dup(mrb, str);
  }
  else {
    RBASIC(newstr)->c = mrb_obj_class(mrb, str);
  }
  return str_encode_associate(mrb, newstr, encidx);
}

mrb_value
mrb_str_encode(mrb_state *mrb, mrb_value str, mrb_value to, int ecflags, mrb_value ecopts)
{
    int argc = 1;
    mrb_value *argv = &to;
    mrb_value newstr = str;
    int encidx = str_transcode0(mrb, argc, argv, &newstr, ecflags, ecopts);

    if (encidx < 0) return mrb_str_dup(mrb, str);
    if (mrb_obj_equal(mrb, newstr, str)) {
      newstr = mrb_str_dup(mrb, str);
    }
    else {
      RBASIC(newstr)->c = mrb_obj_class(mrb, str);
    }
    return str_encode_associate(mrb, newstr, encidx);
}

static void
econv_free(mrb_state *mrb, void *ptr)
{
    mrb_econv_t *ec = ptr;
    mrb_econv_close(ec);
}

static const struct mrb_data_type econv_data_type = {
    "econv", econv_free,
};

static mrb_encoding *
make_dummy_encoding(mrb_state *mrb, const char *name)
{
    mrb_encoding *enc;
    int idx;
    idx = mrb_define_dummy_encoding(mrb, name);
    enc = mrb_enc_from_index(mrb, idx);
    return enc;
}

static mrb_encoding *
make_encoding(mrb_state *mrb, const char *name)
{
    mrb_encoding *enc;
    enc = mrb_enc_find(mrb, name);
    if (!enc)
        enc = make_dummy_encoding(mrb, name);
    return enc;
}

static mrb_value
make_encobj(mrb_state *mrb, const char *name)
{
    return mrb_enc_from_encoding(mrb, make_encoding(mrb, name));
}

/*
 * call-seq:
 *   Encoding::Converter.asciicompat_encoding(string) -> encoding or nil
 *   Encoding::Converter.asciicompat_encoding(encoding) -> encoding or nil
 *
 * Returns the corresponding ASCII compatible encoding.
 *
 * Returns nil if the argument is an ASCII compatible encoding.
 *
 * "corresponding ASCII compatible encoding" is a ASCII compatible encoding which
 * can represents exactly the same characters as the given ASCII incompatible encoding.
 * So, no conversion undefined error occurs when converting between the two encodings.
 *
 *   Encoding::Converter.asciicompat_encoding("ISO-2022-JP") #=> #<Encoding:stateless-ISO-2022-JP>
 *   Encoding::Converter.asciicompat_encoding("UTF-16BE") #=> #<Encoding:UTF-8>
 *   Encoding::Converter.asciicompat_encoding("UTF-8") #=> nil
 *
 */
static mrb_value
econv_s_asciicompat_encoding(mrb_state *mrb, mrb_value klass)
{
  mrb_value arg;
  const char *arg_name, *result_name;
  mrb_encoding *arg_enc, *result_enc;

  mrb_get_args(mrb, "o", &arg);
  enc_arg(mrb, &arg, &arg_name, &arg_enc);

  result_name = mrb_econv_asciicompat_encoding(arg_name);

  if (result_name == NULL)
      return mrb_nil_value();

  result_enc = make_encoding(mrb, result_name);

  return mrb_enc_from_encoding(mrb, result_enc);
}

static void
econv_args(mrb_state *mrb,
    int argc, mrb_value *argv,
    mrb_value *snamev_p, mrb_value *dnamev_p,
    const char **sname_p, const char **dname_p,
    mrb_encoding **senc_p, mrb_encoding **denc_p,
    int *ecflags_p,
    mrb_value *ecopts_p)
{
    mrb_value opt, opthash, flags_v, ecopts;
    int sidx, didx;
    const char *sname, *dname;
    mrb_encoding *senc, *denc;
    int ecflags;

    //mrb_scan_args(argc, argv, "21", snamev_p, dnamev_p, &opt);
    *snamev_p  = argv[0];
    *dnamev_p  = argv[1];
    opt = argv[2];

    if (argc < 3) {//mrb_nil_p(opt)) {
        ecflags = 0;
        ecopts  = mrb_nil_value();
    }
    else if (!mrb_nil_p(flags_v = mrb_check_to_integer(mrb, opt, "to_int"))) {
        ecflags = mrb_fixnum(flags_v);
        ecopts  = mrb_nil_value();
    }
    else {
        opthash = mrb_convert_type(mrb, opt, MRB_TT_HASH, "Hash", "to_hash");
        ecflags = mrb_econv_prepare_opts(mrb, opthash, &ecopts);
    }

    senc = NULL;
    sidx = mrb_to_encoding_index(mrb, *snamev_p);
    if (0 <= sidx) {
        senc = mrb_enc_from_index(mrb, sidx);
    }
    else {
        //StringValue(*snamev_p);
        mrb_string_value(mrb, snamev_p);
    }

    denc = NULL;
    didx = mrb_to_encoding_index(mrb, *dnamev_p);
    if (0 <= didx) {
        denc = mrb_enc_from_index(mrb, didx);
    }
    else {
        //StringValue(*dnamev_p);
        mrb_string_value(mrb, dnamev_p);
    }

    //sname = senc ? mrb_enc_name(senc) : StringValueCStr(*snamev_p);
    sname = senc ? mrb_enc_name(senc) : mrb_string_value_cstr(mrb, snamev_p);
    //dname = denc ? mrb_enc_name(denc) : StringValueCStr(*dnamev_p);
    dname = denc ? mrb_enc_name(denc) : mrb_string_value_cstr(mrb, dnamev_p);

    *sname_p = sname;
    *dname_p = dname;
    *senc_p = senc;
    *denc_p = denc;
    *ecflags_p = ecflags;
    *ecopts_p = ecopts;
}

static int
decorate_convpath(mrb_state *mrb, mrb_value convpath, int ecflags)
{
    int num_decorators;
    const char *decorators[MAX_ECFLAGS_DECORATORS];
    int i;
    int n, len;

    num_decorators = decorator_names(ecflags, decorators);
    if (num_decorators == -1)
        return -1;

    len = n = RARRAY_LEN(convpath);//RARRAY_LENINT(convpath);
    if (n != 0) {
        mrb_value pair = RARRAY_PTR(convpath)[n-1];
      if (TYPE(pair) == MRB_TT_ARRAY) {
          const char *sname = mrb_enc_name(mrb_to_encoding(mrb, RARRAY_PTR(pair)[0]));
          const char *dname = mrb_enc_name(mrb_to_encoding(mrb, RARRAY_PTR(pair)[1]));
          transcoder_entry_t *entry = get_transcoder_entry(sname, dname);
          const mrb_transcoder *tr = load_transcoder_entry(mrb, entry);
          if (!tr)
            return -1;
          if (!DECORATOR_P(tr->src_encoding, tr->dst_encoding) &&
                tr->asciicompat_type == asciicompat_encoder) {
            n--;
            mrb_ary_set(mrb, convpath, len + num_decorators - 1, pair);
          }
      }
      else {
          mrb_ary_set(mrb, convpath, len + num_decorators - 1, pair);
      }
    }

    for (i = 0; i < num_decorators; i++)
        mrb_ary_set(mrb, convpath, n + i, mrb_str_new_cstr(mrb, decorators[i]));

    return 0;
}

static void
search_convpath_i(mrb_state *mrb, const char *sname, const char *dname, int depth, void *arg)
{
    mrb_value *ary_p = arg;
    mrb_value v;

    if (mrb_obj_equal(mrb, *ary_p, mrb_nil_value())) {
        *ary_p = mrb_ary_new(mrb);
    }

    if (DECORATOR_P(sname, dname)) {
        v = mrb_str_new_cstr(mrb, dname);
    }
    else {
        v = mrb_assoc_new(mrb, make_encobj(mrb, sname), make_encobj(mrb, dname));
    }
    mrb_ary_set(mrb, *ary_p, depth, v);
}

/*
 * call-seq:
 *   Encoding::Converter.search_convpath(source_encoding, destination_encoding)         -> ary
 *   Encoding::Converter.search_convpath(source_encoding, destination_encoding, opt)    -> ary
 *
 *  Returns a conversion path.
 *
 *   p Encoding::Converter.search_convpath("ISO-8859-1", "EUC-JP")
 *   #=> [[#<Encoding:ISO-8859-1>, #<Encoding:UTF-8>],
 *   #    [#<Encoding:UTF-8>, #<Encoding:EUC-JP>]]
 *
 *   p Encoding::Converter.search_convpath("ISO-8859-1", "EUC-JP", universal_newline: true)
 *   #=> [[#<Encoding:ISO-8859-1>, #<Encoding:UTF-8>],
 *   #    [#<Encoding:UTF-8>, #<Encoding:EUC-JP>],
 *   #    "universal_newline"]
 *
 *   p Encoding::Converter.search_convpath("ISO-8859-1", "UTF-32BE", universal_newline: true)
 *   #=> [[#<Encoding:ISO-8859-1>, #<Encoding:UTF-8>],
 *   #    "universal_newline",
 *   #    [#<Encoding:UTF-8>, #<Encoding:UTF-32BE>]]
 */
static mrb_value
econv_s_search_convpath(mrb_state *mrb, /*int argc, mrb_value *argv,*/ mrb_value klass)
{
  mrb_value snamev, dnamev;
  const char *sname, *dname;
  mrb_encoding *senc, *denc;
  int ecflags;
  mrb_value ecopts;
  mrb_value convpath;

  mrb_value argv[16];
  int argc;

  mrb_get_args(mrb, "*", &argv, &argc);
  econv_args(mrb, argc, argv, &snamev, &dnamev, &sname, &dname, &senc, &denc, &ecflags, &ecopts);
  convpath = mrb_nil_value();
  transcode_search_path(mrb, sname, dname, search_convpath_i, &convpath);

  if (mrb_nil_p(convpath))
      mrb_exc_raise(mrb, mrb_econv_open_exc(mrb, sname, dname, ecflags));

  if (decorate_convpath(mrb, convpath, ecflags) == -1)
      mrb_exc_raise(mrb, mrb_econv_open_exc(mrb, sname, dname, ecflags));

  return convpath;
}

/*
 * Check the existence of a conversion path.
 * Returns the number of converters in the conversion path.
 * result: >=0:success -1:failure
 */
int
mrb_econv_has_convpath_p(mrb_state *mrb, const char* from_encoding, const char* to_encoding)
{
    mrb_value convpath = mrb_nil_value();
    transcode_search_path(mrb, from_encoding, to_encoding, search_convpath_i,
                    &convpath);
    return RTEST(convpath);
}

struct mrb_econv_init_by_convpath_t {
    mrb_econv_t *ec;
    int index;
    int ret;
};

static void
mrb_econv_init_by_convpath_i(mrb_state *mrb, const char *sname, const char *dname, int depth, void *arg)
{
    struct mrb_econv_init_by_convpath_t *a = (struct mrb_econv_init_by_convpath_t *)arg;
    int ret;

    if (a->ret == -1)
        return;

    ret = mrb_econv_add_converter(mrb, a->ec, sname, dname, a->index);

    a->ret = ret;
    return;
}

static mrb_econv_t *
mrb_econv_init_by_convpath(mrb_state *mrb, mrb_value self, mrb_value convpath,
    const char **sname_p, const char **dname_p,
    mrb_encoding **senc_p, mrb_encoding**denc_p)
{
    mrb_econv_t *ec;
    long i;
    int ret, first=1;
    mrb_value elt;
    mrb_encoding *senc = 0, *denc = 0;
    const char *sname, *dname;

    ec = mrb_econv_alloc(RARRAY_LEN/*INT*/(convpath));
    DATA_PTR(self) = ec;

    for (i = 0; i < RARRAY_LEN(convpath); i++) {
        mrb_value snamev, dnamev;
        mrb_value pair;
        elt = mrb_ary_ref(mrb, convpath, i);
        if (!mrb_nil_p(pair = mrb_check_array_type(mrb, elt))) {
            if (RARRAY_LEN(pair) != 2)
                mrb_raise(mrb, E_ARGUMENT_ERROR, "not a 2-element array in convpath");
            snamev = mrb_ary_ref(mrb, pair, 0);
            enc_arg(mrb, &snamev, &sname, &senc);
            dnamev = mrb_ary_ref(mrb, pair, 1);
            enc_arg(mrb, &dnamev, &dname, &denc);
        }
        else {
            sname = "";
            //dname = StringValueCStr(elt);
            dname = mrb_string_value_cstr(mrb, &elt);
        }
        if (DECORATOR_P(sname, dname)) {
            ret = mrb_econv_add_converter(mrb, ec, sname, dname, ec->num_trans);
            if (ret == -1)
                mrb_raise(mrb, E_ARGUMENT_ERROR, "decoration failed: %s", dname);
        }
        else {
            int j = ec->num_trans;
            struct mrb_econv_init_by_convpath_t arg;
            arg.ec = ec;
            arg.index = ec->num_trans;
            arg.ret = 0;
            ret = transcode_search_path(mrb, sname, dname, mrb_econv_init_by_convpath_i, &arg);
            if (ret == -1 || arg.ret == -1)
                mrb_raise(mrb, E_ARGUMENT_ERROR, "adding conversion failed: %s to %s", sname, dname);
            if (first) {
                first = 0;
                *senc_p = senc;
                *sname_p = ec->elems[j].tc->transcoder->src_encoding;
            }
            *denc_p = denc;
            *dname_p = ec->elems[ec->num_trans-1].tc->transcoder->dst_encoding;
        }
    }

    if (first) {
      *senc_p = NULL;
      *denc_p = NULL;
      *sname_p = "";
      *dname_p = "";
    }

    ec->source_encoding_name = *sname_p;
    ec->destination_encoding_name = *dname_p;

    return ec;
}

/*
 * call-seq:
 *   Encoding::Converter.new(source_encoding, destination_encoding)
 *   Encoding::Converter.new(source_encoding, destination_encoding, opt)
 *   Encoding::Converter.new(convpath)
 *
 * possible options elements:
 *   hash form:
 *     :invalid => nil            # raise error on invalid byte sequence (default)
 *     :invalid => :replace       # replace invalid byte sequence
 *     :undef => nil              # raise error on undefined conversion (default)
 *     :undef => :replace         # replace undefined conversion
 *     :replace => string         # replacement string ("?" or "\uFFFD" if not specified)
 *     :universal_newline => true # decorator for converting CRLF and CR to LF
 *     :crlf_newline => true      # decorator for converting LF to CRLF
 *     :cr_newline => true        # decorator for converting LF to CR
 *     :xml => :text              # escape as XML CharData.
 *     :xml => :attr              # escape as XML AttValue
 *   integer form:
 *     Encoding::Converter::INVALID_REPLACE
 *     Encoding::Converter::UNDEF_REPLACE
 *     Encoding::Converter::UNDEF_HEX_CHARREF
 *     Encoding::Converter::UNIVERSAL_NEWLINE_DECORATOR
 *     Encoding::Converter::CRLF_NEWLINE_DECORATOR
 *     Encoding::Converter::CR_NEWLINE_DECORATOR
 *     Encoding::Converter::XML_TEXT_DECORATOR
 *     Encoding::Converter::XML_ATTR_CONTENT_DECORATOR
 *     Encoding::Converter::XML_ATTR_QUOTE_DECORATOR
 *
 * Encoding::Converter.new creates an instance of Encoding::Converter.
 *
 * Source_encoding and destination_encoding should be a string or
 * Encoding object.
 *
 * opt should be nil, a hash or an integer.
 *
 * convpath should be an array.
 * convpath may contain
 * - two-element arrays which contain encodings or encoding names, or
 * - strings representing decorator names.
 *
 * Encoding::Converter.new optionally takes an option.
 * The option should be a hash or an integer.
 * The option hash can contain :invalid => nil, etc.
 * The option integer should be logical-or of constants such as
 * Encoding::Converter::INVALID_REPLACE, etc.
 *
 * [:invalid => nil]
 *   Raise error on invalid byte sequence.  This is a default behavior.
 * [:invalid => :replace]
 *   Replace invalid byte sequence by replacement string.
 * [:undef => nil]
 *   Raise an error if a character in source_encoding is not defined in destination_encoding.
 *   This is a default behavior.
 * [:undef => :replace]
 *   Replace undefined character in destination_encoding with replacement string.
 * [:replace => string]
 *   Specify the replacement string.
 *   If not specified, "\uFFFD" is used for Unicode encodings and "?" for others.
 * [:universal_newline => true]
 *   Convert CRLF and CR to LF.
 * [:crlf_newline => true]
 *   Convert LF to CRLF.
 * [:cr_newline => true]
 *   Convert LF to CR.
 * [:xml => :text]
 *   Escape as XML CharData.
 *   This form can be used as a HTML 4.0 #PCDATA.
 *   - '&' -> '&amp;'
 *   - '<' -> '&lt;'
 *   - '>' -> '&gt;'
 *   - undefined characters in destination_encoding -> hexadecimal CharRef such as &#xHH;
 * [:xml => :attr]
 *   Escape as XML AttValue.
 *   The converted result is quoted as "...".
 *   This form can be used as a HTML 4.0 attribute value.
 *   - '&' -> '&amp;'
 *   - '<' -> '&lt;'
 *   - '>' -> '&gt;'
 *   - '"' -> '&quot;'
 *   - undefined characters in destination_encoding -> hexadecimal CharRef such as &#xHH;
 *
 * Examples:
 *   # UTF-16BE to UTF-8
 *   ec = Encoding::Converter.new("UTF-16BE", "UTF-8")
 *
 *   # Usually, decorators such as newline conversion are inserted last.
 *   ec = Encoding::Converter.new("UTF-16BE", "UTF-8", :universal_newline => true)
 *   p ec.convpath #=> [[#<Encoding:UTF-16BE>, #<Encoding:UTF-8>],
 *                 #    "universal_newline"]
 *
 *   # But, if the last encoding is ASCII incompatible,
 *   # decorators are inserted before the last conversion.
 *   ec = Encoding::Converter.new("UTF-8", "UTF-16BE", :crlf_newline => true)
 *   p ec.convpath #=> ["crlf_newline",
 *                 #    [#<Encoding:UTF-8>, #<Encoding:UTF-16BE>]]
 *
 *   # Conversion path can be specified directly.
 *   ec = Encoding::Converter.new(["universal_newline", ["EUC-JP", "UTF-8"], ["UTF-8", "UTF-16BE"]])
 *   p ec.convpath #=> ["universal_newline",
 *                 #    [#<Encoding:EUC-JP>, #<Encoding:UTF-8>],
 *                 #    [#<Encoding:UTF-8>, #<Encoding:UTF-16BE>]]
 */
static mrb_value
econv_init(mrb_state *mrb, /*int argc, mrb_value *argv,*/ mrb_value self)
{
  mrb_value ecopts;
  mrb_value snamev, dnamev;
  const char *sname, *dname;
  mrb_encoding *senc, *denc;
  mrb_econv_t *ec;
  int ecflags;
  mrb_value convpath;
  mrb_value argv[16];
  int argc;

  mrb_get_args(mrb, "*", &argv, &argc);
  if (mrb_check_datatype(mrb, self, &econv_data_type)) {
    mrb_raise(mrb, E_TYPE_ERROR, "already initialized");
  }

  if (argc == 1 && !mrb_nil_p(convpath = mrb_check_array_type(mrb, argv[0]))) {
    ec = mrb_econv_init_by_convpath(mrb, self, convpath, &sname, &dname, &senc, &denc);
    ecflags = 0;
    ecopts = mrb_nil_value();
  }
  else {
    econv_args(mrb, argc, argv, &snamev, &dnamev, &sname, &dname, &senc, &denc, &ecflags, &ecopts);
    ec = mrb_econv_open_opts(mrb, sname, dname, ecflags, ecopts);
  }

  if (!ec) {
    mrb_exc_raise(mrb, mrb_econv_open_exc(mrb, sname, dname, ecflags));
  }

  if (!DECORATOR_P(sname, dname)) {
    if (!senc)
        senc = make_dummy_encoding(mrb, sname);
    if (!denc)
        denc = make_dummy_encoding(mrb, dname);
  }

  ec->source_encoding = senc;
  ec->destination_encoding = denc;

  DATA_PTR(self) = ec;

  return self;
}

/*
 * call-seq:
 *   ec.inspect         -> string
 *
 * Returns a printable version of <i>ec</i>
 *
 *   ec = Encoding::Converter.new("iso-8859-1", "utf-8")
 *   puts ec.inspect    #=> #<Encoding::Converter: ISO-8859-1 to UTF-8>
 *
 */
static mrb_value
econv_inspect(mrb_state *mrb, mrb_value self)
{
    const char *cname = mrb_obj_classname(mrb, self);
    mrb_econv_t *ec;

    Data_Get_Struct(mrb, self, &econv_data_type, ec);
    if (!ec)
        return mrb_sprintf(mrb, "#<%s: uninitialized>", cname);
    else {
        const char *sname = ec->source_encoding_name;
        const char *dname = ec->destination_encoding_name;
        mrb_value str;
        str = mrb_sprintf(mrb, "#<%s: ", cname);
        econv_description(mrb, sname, dname, ec->flags, str);
        mrb_str_cat2(mrb, str, ">");
        return str;
    }
}

static mrb_econv_t *
check_econv(mrb_state *mrb, mrb_value self)
{
    mrb_econv_t *ec;

    Data_Get_Struct(mrb, self, &econv_data_type, ec);
    if (!ec) {
        mrb_raise(mrb, E_TYPE_ERROR, "uninitialized encoding converter");
    }
    return ec;
}

/*
 * call-seq:
 *   ec.source_encoding -> encoding
 *
 * Returns the source encoding as an Encoding object.
 */
static mrb_value
econv_source_encoding(mrb_state *mrb, mrb_value self)
{
    mrb_econv_t *ec = check_econv(mrb, self);
    if (!ec->source_encoding)
        return mrb_nil_value();
    return mrb_enc_from_encoding(mrb, ec->source_encoding);
}

/*
 * call-seq:
 *   ec.destination_encoding -> encoding
 *
 * Returns the destination encoding as an Encoding object.
 */
static mrb_value
econv_destination_encoding(mrb_state *mrb, mrb_value self)
{
    mrb_econv_t *ec = check_econv(mrb, self);
    if (!ec->destination_encoding)
        return mrb_nil_value();
    return mrb_enc_from_encoding(mrb, ec->destination_encoding);
}

/*
 * call-seq:
 *   ec.convpath        -> ary
 *
 * Returns the conversion path of ec.
 *
 * The result is an array of conversions.
 *
 *   ec = Encoding::Converter.new("ISO-8859-1", "EUC-JP", crlf_newline: true)
 *   p ec.convpath
 *   #=> [[#<Encoding:ISO-8859-1>, #<Encoding:UTF-8>],
 *   #    [#<Encoding:UTF-8>, #<Encoding:EUC-JP>],
 *   #    "crlf_newline"]
 *
 * Each element of the array is a pair of encodings or a string.
 * A pair means an encoding conversion.
 * A string means a decorator.
 *
 * In the above example, [#<Encoding:ISO-8859-1>, #<Encoding:UTF-8>] means
 * a converter from ISO-8859-1 to UTF-8.
 * "crlf_newline" means newline converter from LF to CRLF.
 */
static mrb_value
econv_convpath(mrb_state *mrb, mrb_value self)
{
    mrb_econv_t *ec = check_econv(mrb, self);
    mrb_value result;
    int i;

    result = mrb_ary_new(mrb);
    for (i = 0; i < ec->num_trans; i++) {
        const mrb_transcoder *tr = ec->elems[i].tc->transcoder;
        mrb_value v;
        if (DECORATOR_P(tr->src_encoding, tr->dst_encoding))
            v = mrb_str_new_cstr(mrb, tr->dst_encoding);
        else
            v = mrb_assoc_new(mrb, make_encobj(mrb, tr->src_encoding), make_encobj(mrb, tr->dst_encoding));
        mrb_ary_push(mrb, result, v);
    }
    return result;
}

static mrb_value
econv_result_to_symbol(mrb_econv_result_t res)
{
    switch (res) {
      case econv_invalid_byte_sequence: return sym_invalid_byte_sequence;
      case econv_incomplete_input: return sym_incomplete_input;
      case econv_undefined_conversion: return sym_undefined_conversion;
      case econv_destination_buffer_full: return sym_destination_buffer_full;
      case econv_source_buffer_empty: return sym_source_buffer_empty;
      case econv_finished: return sym_finished;
      case econv_after_output: return sym_after_output;
      default: return mrb_fixnum_value(res); /* should not be reached */
    }
}

mrb_value econv_primitive_cnvproc(mrb_state *mrb, int argc, mrb_value *argv, mrb_value self)
{
    mrb_value input, output, output_byteoffset_v, output_bytesize_v, opt, flags_v;
    mrb_econv_t *ec = check_econv(mrb, self);
    mrb_econv_result_t res;
    const unsigned char *ip, *is;
    unsigned char *op, *os;
    long output_byteoffset, output_bytesize;
    unsigned long output_byteend;
    int flags;

    //mrb_scan_args(argc, argv, "23", &input, &output, &output_byteoffset_v, &output_bytesize_v, &opt);
    input  = argv[0];
    output = argv[1];
    output_byteoffset_v = argv[2];
    output_bytesize_v   = argv[3];
    opt    = argv[4];

    if (argc < 3)//mrb_nil_p(output_byteoffset_v))
        output_byteoffset = 0; /* dummy */
    else
        output_byteoffset = mrb_fixnum(output_byteoffset_v);

    if (argc < 4)//mrb_nil_p(output_bytesize_v))
        output_bytesize = 0; /* dummy */
    else
        output_bytesize = mrb_fixnum(output_bytesize_v);

    if (argc < 5) {//mrb_nil_p(opt)) {
        flags = 0;
    }
    else if (!mrb_nil_p(flags_v = mrb_check_to_integer(mrb, opt, "to_int"))) {
        flags = mrb_fixnum(flags_v);
    }
    else {
        mrb_value v;
        opt = mrb_convert_type(mrb, opt, MRB_TT_HASH, "Hash", "to_hash");
        flags = 0;
        v = mrb_hash_get(mrb, opt, sym_partial_input);
        if (RTEST(v))
            flags |= ECONV_PARTIAL_INPUT;
        v = mrb_hash_get(mrb, opt, sym_after_output);
        if (RTEST(v))
            flags |= ECONV_AFTER_OUTPUT;
    }

    //StringValue(output);
    mrb_string_value(mrb, &output);
    if (!mrb_nil_p(input))
      //StringValue(input);
      mrb_string_value(mrb, &input);
    mrb_str_modify(mrb, output);

    if (mrb_nil_p(output_bytesize_v)) {
        output_bytesize = STR_BUF_MIN_SIZE;
        if (!mrb_nil_p(input) && output_bytesize < RSTRING_LEN(input))
            output_bytesize = RSTRING_LEN(input);
    }

  retry:

    if (mrb_nil_p(output_byteoffset_v))
        output_byteoffset = RSTRING_LEN(output);

    if (output_byteoffset < 0)
        mrb_raise(mrb, E_ARGUMENT_ERROR, "negative output_byteoffset");

    if (RSTRING_LEN(output) < output_byteoffset)
        mrb_raise(mrb, E_ARGUMENT_ERROR, "output_byteoffset too big");

    if (output_bytesize < 0)
        mrb_raise(mrb, E_ARGUMENT_ERROR, "negative output_bytesize");

    output_byteend = (unsigned long)output_byteoffset +
                     (unsigned long)output_bytesize;

    if (output_byteend < (unsigned long)output_byteoffset ||
        LONG_MAX < output_byteend)
        mrb_raise(mrb, E_ARGUMENT_ERROR, "output_byteoffset+output_bytesize too big");

    if (mrb_str_capacity(output) < output_byteend)
        mrb_str_resize(mrb, output, output_byteend);

    if (mrb_nil_p(input)) {
        ip = is = NULL;
    }
    else {
        ip = (const unsigned char *)RSTRING_PTR(input);
        is = ip + RSTRING_LEN(input);
    }

    op = (unsigned char *)RSTRING_PTR(output) + output_byteoffset;
    os = op + output_bytesize;

    res = mrb_econv_convert(mrb, ec, &ip, is, &op, os, flags);
    mrb_str_set_len(mrb, output, op-(unsigned char *)RSTRING_PTR(output));
    if (!mrb_nil_p(input))
        mrb_str_drop_bytes(mrb, input, ip - (unsigned char *)RSTRING_PTR(input));

    if (mrb_nil_p(output_bytesize_v) && res == econv_destination_buffer_full) {
        if (LONG_MAX / 2 < output_bytesize)
            mrb_raise(mrb, E_ARGUMENT_ERROR, "too long conversion result");
        output_bytesize *= 2;
        output_byteoffset_v = mrb_nil_value();
        goto retry;
    }

    if (ec->destination_encoding) {
        mrb_enc_associate(mrb, output, ec->destination_encoding);
    }

    return econv_result_to_symbol(res);
}

/*
 * call-seq:
 *   ec.primitive_convert(source_buffer, destination_buffer) -> symbol
 *   ec.primitive_convert(source_buffer, destination_buffer, destination_byteoffset) -> symbol
 *   ec.primitive_convert(source_buffer, destination_buffer, destination_byteoffset, destination_bytesize) -> symbol
 *   ec.primitive_convert(source_buffer, destination_buffer, destination_byteoffset, destination_bytesize, opt) -> symbol
 *
 * possible opt elements:
 *   hash form:
 *     :partial_input => true           # source buffer may be part of larger source
 *     :after_output => true            # stop conversion after output before input
 *   integer form:
 *     Encoding::Converter::PARTIAL_INPUT
 *     Encoding::Converter::AFTER_OUTPUT
 *
 * possible results:
 *    :invalid_byte_sequence
 *    :incomplete_input
 *    :undefined_conversion
 *    :after_output
 *    :destination_buffer_full
 *    :source_buffer_empty
 *    :finished
 *
 * primitive_convert converts source_buffer into destination_buffer.
 *
 * source_buffer should be a string or nil.
 * nil means a empty string.
 *
 * destination_buffer should be a string.
 *
 * destination_byteoffset should be an integer or nil.
 * nil means the end of destination_buffer.
 * If it is omitted, nil is assumed.
 *
 * destination_bytesize should be an integer or nil.
 * nil means unlimited.
 * If it is omitted, nil is assumed.
 *
 * opt should be nil, a hash or an integer.
 * nil means no flags.
 * If it is omitted, nil is assumed.
 *
 * primitive_convert converts the content of source_buffer from beginning
 * and store the result into destination_buffer.
 *
 * destination_byteoffset and destination_bytesize specify the region which
 * the converted result is stored.
 * destination_byteoffset specifies the start position in destination_buffer in bytes.
 * If destination_byteoffset is nil,
 * destination_buffer.bytesize is used for appending the result.
 * destination_bytesize specifies maximum number of bytes.
 * If destination_bytesize is nil,
 * destination size is unlimited.
 * After conversion, destination_buffer is resized to
 * destination_byteoffset + actually produced number of bytes.
 * Also destination_buffer's encoding is set to destination_encoding.
 *
 * primitive_convert drops the converted part of source_buffer.
 * the dropped part is converted in destination_buffer or
 * buffered in Encoding::Converter object.
 *
 * primitive_convert stops conversion when one of following condition met.
 * - invalid byte sequence found in source buffer (:invalid_byte_sequence)
 * - unexpected end of source buffer (:incomplete_input)
 *   this occur only when :partial_input is not specified.
 * - character not representable in output encoding (:undefined_conversion)
 * - after some output is generated, before input is done (:after_output)
 *   this occur only when :after_output is specified.
 * - destination buffer is full (:destination_buffer_full)
 *   this occur only when destination_bytesize is non-nil.
 * - source buffer is empty (:source_buffer_empty)
 *   this occur only when :partial_input is specified.
 * - conversion is finished (:finished)
 *
 * example:
 *   ec = Encoding::Converter.new("UTF-8", "UTF-16BE")
 *   ret = ec.primitive_convert(src="pi", dst="", nil, 100)
 *   p [ret, src, dst] #=> [:finished, "", "\x00p\x00i"]
 *
 *   ec = Encoding::Converter.new("UTF-8", "UTF-16BE")
 *   ret = ec.primitive_convert(src="pi", dst="", nil, 1)
 *   p [ret, src, dst] #=> [:destination_buffer_full, "i", "\x00"]
 *   ret = ec.primitive_convert(src, dst="", nil, 1)
 *   p [ret, src, dst] #=> [:destination_buffer_full, "", "p"]
 *   ret = ec.primitive_convert(src, dst="", nil, 1)
 *   p [ret, src, dst] #=> [:destination_buffer_full, "", "\x00"]
 *   ret = ec.primitive_convert(src, dst="", nil, 1)
 *   p [ret, src, dst] #=> [:finished, "", "i"]
 *
 */
static mrb_value
econv_primitive_convert(mrb_state *mrb, /*int argc, mrb_value *argv,*/ mrb_value self)
{
  mrb_value argv[16];
  int argc;

  mrb_get_args(mrb, "*", &argv, &argc);
  return econv_primitive_cnvproc(mrb, argc, argv, self);
}

/*
 * call-seq:
 *   ec.convert(source_string) -> destination_string
 *
 * Convert source_string and return destination_string.
 *
 * source_string is assumed as a part of source.
 * i.e.  :partial_input=>true is specified internally.
 * finish method should be used last.
 *
 *   ec = Encoding::Converter.new("utf-8", "euc-jp")
 *   puts ec.convert("\u3042").dump     #=> "\xA4\xA2"
 *   puts ec.finish.dump                #=> ""
 *
 *   ec = Encoding::Converter.new("euc-jp", "utf-8")
 *   puts ec.convert("\xA4").dump       #=> ""
 *   puts ec.convert("\xA2").dump       #=> "\xE3\x81\x82"
 *   puts ec.finish.dump                #=> ""
 *
 *   ec = Encoding::Converter.new("utf-8", "iso-2022-jp")
 *   puts ec.convert("\xE3").dump       #=> "".force_encoding("ISO-2022-JP")
 *   puts ec.convert("\x81").dump       #=> "".force_encoding("ISO-2022-JP")
 *   puts ec.convert("\x82").dump       #=> "\e$B$\"".force_encoding("ISO-2022-JP")
 *   puts ec.finish.dump                #=> "\e(B".force_encoding("ISO-2022-JP")
 *
 * If a conversion error occur,
 * Encoding::UndefinedConversionError or
 * Encoding::InvalidByteSequenceError is raised.
 * Encoding::Converter#convert doesn't supply methods to recover or restart
 * from these exceptions.
 * When you want to handle these conversion errors,
 * use Encoding::Converter#primitive_convert.
 *
 */
static mrb_value
econv_convert(mrb_state *mrb, mrb_value self)
{
    mrb_value source_string;
    mrb_value ret, dst;
    mrb_value av[5];
    int ac;
    mrb_econv_t *ec = check_econv(mrb, self);

    mrb_get_args(mrb, "o", &source_string);
    //StringValue(source_string);
    mrb_string_value(mrb, &source_string);

    dst = mrb_str_new(mrb, NULL, 0);

    av[0] = mrb_str_dup(mrb, source_string);
    av[1] = dst;
    av[2] = mrb_nil_value();
    av[3] = mrb_nil_value();
    av[4] = mrb_fixnum_value(ECONV_PARTIAL_INPUT);
    ac = 5;

    ret = econv_primitive_cnvproc(mrb, ac, av, self);

    if (mrb_obj_equal(mrb, ret, sym_invalid_byte_sequence) ||
        mrb_obj_equal(mrb, ret, sym_undefined_conversion) ||
        mrb_obj_equal(mrb, ret, sym_incomplete_input)) {
        mrb_value exc = make_econv_exception(mrb, ec);
        mrb_exc_raise(mrb, exc);
    }

    if (mrb_obj_equal(mrb, ret, sym_finished)) {
        mrb_raise(mrb, E_ARGUMENT_ERROR, "converter already finished");
    }

    if (!mrb_obj_equal(mrb, ret, sym_source_buffer_empty)) {
        mrb_bug("unexpected result of econv_primitive_convert");
    }

    return dst;
}

/*
 * call-seq:
 *   ec.finish -> string
 *
 * Finishes the converter.
 * It returns the last part of the converted string.
 *
 *   ec = Encoding::Converter.new("utf-8", "iso-2022-jp")
 *   p ec.convert("\u3042")     #=> "\e$B$\""
 *   p ec.finish                #=> "\e(B"
 */
static mrb_value
econv_finish(mrb_state *mrb, mrb_value self)
{
    mrb_value ret, dst;
    mrb_value av[5];
    int ac;
    mrb_econv_t *ec = check_econv(mrb, self);

    dst = mrb_str_new(mrb, NULL, 0);

    av[0] = mrb_nil_value();
    av[1] = dst;
    av[2] = mrb_nil_value();
    av[3] = mrb_nil_value();
    av[4] = mrb_fixnum_value(0);
    ac = 5;

    ret = econv_primitive_cnvproc(mrb, ac, av, self);

    if (mrb_obj_equal(mrb, ret, sym_invalid_byte_sequence) ||
        mrb_obj_equal(mrb, ret, sym_undefined_conversion)  ||
        mrb_obj_equal(mrb, ret, sym_incomplete_input))     {
        mrb_value exc = make_econv_exception(mrb, ec);
        mrb_exc_raise(mrb, exc);
    }

    if (!mrb_obj_equal(mrb, ret, sym_finished)) {
        mrb_bug("unexpected result of econv_primitive_convert");
    }

    return dst;
}

/*
 * call-seq:
 *   ec.primitive_errinfo -> array
 *
 * primitive_errinfo returns important information regarding the last error
 * as a 5-element array:
 *
 *   [result, enc1, enc2, error_bytes, readagain_bytes]
 *
 * result is the last result of primitive_convert.
 *
 * Other elements are only meaningful when result is
 * :invalid_byte_sequence, :incomplete_input or :undefined_conversion.
 *
 * enc1 and enc2 indicate a conversion step as a pair of strings.
 * For example, a converter from EUC-JP to ISO-8859-1 converts
 * a string as follows: EUC-JP -> UTF-8 -> ISO-8859-1.
 * So [enc1, enc2] is either ["EUC-JP", "UTF-8"] or ["UTF-8", "ISO-8859-1"].
 *
 * error_bytes and readagain_bytes indicate the byte sequences which caused the error.
 * error_bytes is discarded portion.
 * readagain_bytes is buffered portion which is read again on next conversion.
 *
 * Example:
 *
 *   # \xff is invalid as EUC-JP.
 *   ec = Encoding::Converter.new("EUC-JP", "Shift_JIS")
 *   ec.primitive_convert(src="\xff", dst="", nil, 10)
 *   p ec.primitive_errinfo
 *   #=> [:invalid_byte_sequence, "EUC-JP", "UTF-8", "\xFF", ""]
 *
 *   # HIRAGANA LETTER A (\xa4\xa2 in EUC-JP) is not representable in ISO-8859-1.
 *   # Since this error is occur in UTF-8 to ISO-8859-1 conversion,
 *   # error_bytes is HIRAGANA LETTER A in UTF-8 (\xE3\x81\x82).
 *   ec = Encoding::Converter.new("EUC-JP", "ISO-8859-1")
 *   ec.primitive_convert(src="\xa4\xa2", dst="", nil, 10)
 *   p ec.primitive_errinfo
 *   #=> [:undefined_conversion, "UTF-8", "ISO-8859-1", "\xE3\x81\x82", ""]
 *
 *   # partial character is invalid
 *   ec = Encoding::Converter.new("EUC-JP", "ISO-8859-1")
 *   ec.primitive_convert(src="\xa4", dst="", nil, 10)
 *   p ec.primitive_errinfo
 *   #=> [:incomplete_input, "EUC-JP", "UTF-8", "\xA4", ""]
 *
 *   # Encoding::Converter::PARTIAL_INPUT prevents invalid errors by
 *   # partial characters.
 *   ec = Encoding::Converter.new("EUC-JP", "ISO-8859-1")
 *   ec.primitive_convert(src="\xa4", dst="", nil, 10, Encoding::Converter::PARTIAL_INPUT)
 *   p ec.primitive_errinfo
 *   #=> [:source_buffer_empty, nil, nil, nil, nil]
 *
 *   # \xd8\x00\x00@ is invalid as UTF-16BE because
 *   # no low surrogate after high surrogate (\xd8\x00).
 *   # It is detected by 3rd byte (\00) which is part of next character.
 *   # So the high surrogate (\xd8\x00) is discarded and
 *   # the 3rd byte is read again later.
 *   # Since the byte is buffered in ec, it is dropped from src.
 *   ec = Encoding::Converter.new("UTF-16BE", "UTF-8")
 *   ec.primitive_convert(src="\xd8\x00\x00@", dst="", nil, 10)
 *   p ec.primitive_errinfo
 *   #=> [:invalid_byte_sequence, "UTF-16BE", "UTF-8", "\xD8\x00", "\x00"]
 *   p src
 *   #=> "@"
 *
 *   # Similar to UTF-16BE, \x00\xd8@\x00 is invalid as UTF-16LE.
 *   # The problem is detected by 4th byte.
 *   ec = Encoding::Converter.new("UTF-16LE", "UTF-8")
 *   ec.primitive_convert(src="\x00\xd8@\x00", dst="", nil, 10)
 *   p ec.primitive_errinfo
 *   #=> [:invalid_byte_sequence, "UTF-16LE", "UTF-8", "\x00\xD8", "@\x00"]
 *   p src
 *   #=> ""
 *
 */
static mrb_value
econv_primitive_errinfo(mrb_state *mrb, mrb_value self)
{
    mrb_econv_t *ec = check_econv(mrb, self);

    mrb_value ary;

    ary = mrb_ary_new_capa(mrb, 5);//mrb_ary_new2(5);

    mrb_ary_set(mrb, ary, 0, econv_result_to_symbol(ec->last_error.result));//rb_ary_store(ary, 0, econv_result_to_symbol(ec->last_error.result));
    mrb_ary_set(mrb, ary, 4, mrb_nil_value());//rb_ary_store(ary, 4, mrb_nil_value());

    if (ec->last_error.source_encoding)
        mrb_ary_set(mrb, ary, 1, mrb_str_new2(mrb, ec->last_error.source_encoding));//rb_ary_store(ary, 1, mrb_str_new2(mrb, ec->last_error.source_encoding));

    if (ec->last_error.destination_encoding)
        mrb_ary_set(mrb, ary, 2, mrb_str_new2(mrb, ec->last_error.destination_encoding));//rb_ary_store(ary, 2, mrb_str_new2(mrb, ec->last_error.destination_encoding));

    if (ec->last_error.error_bytes_start) {
        //rb_ary_store(ary, 3, mrb_str_new(mrb, (const char *)ec->last_error.error_bytes_start, ec->last_error.error_bytes_len));
        mrb_ary_set(mrb, ary, 3, mrb_str_new(mrb, (const char *)ec->last_error.error_bytes_start, ec->last_error.error_bytes_len));
        //rb_ary_store(ary, 4, mrb_str_new(mrb, (const char *)ec->last_error.error_bytes_start + ec->last_error.error_bytes_len, ec->last_error.readagain_len));
        mrb_ary_set(mrb, ary, 4, mrb_str_new(mrb, (const char *)ec->last_error.error_bytes_start + ec->last_error.error_bytes_len, ec->last_error.readagain_len));
    }

    return ary;
}

/*
 * call-seq:
 *   ec.insert_output(string) -> nil
 *
 * Inserts string into the encoding converter.
 * The string will be converted to the destination encoding and
 * output on later conversions.
 *
 * If the destination encoding is stateful,
 * string is converted according to the state and the state is updated.
 *
 * This method should be used only when a conversion error occurs.
 *
 *  ec = Encoding::Converter.new("utf-8", "iso-8859-1")
 *  src = "HIRAGANA LETTER A is \u{3042}."
 *  dst = ""
 *  p ec.primitive_convert(src, dst)    #=> :undefined_conversion
 *  puts "[#{dst.dump}, #{src.dump}]"   #=> ["HIRAGANA LETTER A is ", "."]
 *  ec.insert_output("<err>")
 *  p ec.primitive_convert(src, dst)    #=> :finished
 *  puts "[#{dst.dump}, #{src.dump}]"   #=> ["HIRAGANA LETTER A is <err>.", ""]
 *
 *  ec = Encoding::Converter.new("utf-8", "iso-2022-jp")
 *  src = "\u{306F 3041 3068 2661 3002}" # U+2661 is not representable in iso-2022-jp
 *  dst = ""
 *  p ec.primitive_convert(src, dst)    #=> :undefined_conversion
 *  puts "[#{dst.dump}, #{src.dump}]"   #=> ["\e$B$O$!$H".force_encoding("ISO-2022-JP"), "\xE3\x80\x82"]
 *  ec.insert_output "?"                # state change required to output "?".
 *  p ec.primitive_convert(src, dst)    #=> :finished
 *  puts "[#{dst.dump}, #{src.dump}]"   #=> ["\e$B$O$!$H\e(B?\e$B!#\e(B".force_encoding("ISO-2022-JP"), ""]
 *
 */
static mrb_value
econv_insert_output(mrb_state *mrb, mrb_value self)
{
	mrb_value string;
    const char *insert_enc;

    int ret;

    mrb_get_args(mrb, "o", &string);
    mrb_econv_t *ec = check_econv(mrb, self);

    //StringValue(string);
    mrb_string_value(mrb, &string);
    insert_enc = mrb_econv_encoding_to_insert_output(ec);
    string = mrb_str_encode(mrb, string, mrb_enc_from_encoding(mrb, mrb_enc_find(mrb, insert_enc)), 0, mrb_nil_value());

    ret = mrb_econv_insert_output(mrb, ec, (const unsigned char *)RSTRING_PTR(string), RSTRING_LEN(string), insert_enc);
    if (ret == -1) {
      mrb_raise(mrb, E_ARGUMENT_ERROR, "too big string");
    }

    return mrb_nil_value();
}

/*
 * call-seq
 *   ec.putback                    -> string
 *   ec.putback(max_numbytes)      -> string
 *
 * Put back the bytes which will be converted.
 *
 * The bytes are caused by invalid_byte_sequence error.
 * When invalid_byte_sequence error, some bytes are discarded and
 * some bytes are buffered to be converted later.
 * The latter bytes can be put back.
 * It can be observed by
 * Encoding::InvalidByteSequenceError#readagain_bytes and
 * Encoding::Converter#primitive_errinfo.
 *
 *   ec = Encoding::Converter.new("utf-16le", "iso-8859-1")
 *   src = "\x00\xd8\x61\x00"
 *   dst = ""
 *   p ec.primitive_convert(src, dst)   #=> :invalid_byte_sequence
 *   p ec.primitive_errinfo     #=> [:invalid_byte_sequence, "UTF-16LE", "UTF-8", "\x00\xD8", "a\x00"]
 *   p ec.putback               #=> "a\x00"
 *   p ec.putback               #=> ""          # no more bytes to put back
 *
 */
static mrb_value
econv_putback(mrb_state *mrb, /*int argc, mrb_value *argv,*/ mrb_value self)
{
  mrb_econv_t *ec = check_econv(mrb, self);
  int n;
  int putbackable;
  mrb_value str, max;

  mrb_value argv[16];
  int argc;

  //mrb_scan_args(argc, argv, "01", &max);
  mrb_get_args(mrb, "*", &argv, &argc);

  if (argc == 0)//mrb_nil_p(max))
    n = mrb_econv_putbackable(ec);
  else {
    max = argv[0];
    n = mrb_fixnum(max);
    putbackable = mrb_econv_putbackable(ec);
    if (putbackable < n)
        n = putbackable;
  }

  str = mrb_str_new(mrb, NULL, n);
  mrb_econv_putback(ec, (unsigned char *)RSTRING_PTR(str), n);

  if (ec->source_encoding) {
    mrb_enc_associate(mrb, str, ec->source_encoding);
  }

  return str;
}

/*
 * call-seq:
 *   ec.last_error -> exception or nil
 *
 * Returns an exception object for the last conversion.
 * Returns nil if the last conversion did not produce an error.
 *
 * "error" means that
 * Encoding::InvalidByteSequenceError and Encoding::UndefinedConversionError for
 * Encoding::Converter#convert and
 * :invalid_byte_sequence, :incomplete_input and :undefined_conversion for
 * Encoding::Converter#primitive_convert.
 *
 *  ec = Encoding::Converter.new("utf-8", "iso-8859-1")
 *  p ec.primitive_convert(src="\xf1abcd", dst="")       #=> :invalid_byte_sequence
 *  p ec.last_error      #=> #<Encoding::InvalidByteSequenceError: "\xF1" followed by "a" on UTF-8>
 *  p ec.primitive_convert(src, dst, nil, 1)             #=> :destination_buffer_full
 *  p ec.last_error      #=> nil
 *
 */
static mrb_value
econv_last_error(mrb_state *mrb, mrb_value self)
{
    mrb_econv_t *ec = check_econv(mrb, self);
    mrb_value exc;

    exc = make_econv_exception(mrb, ec);
    if (mrb_nil_p(exc))
        return mrb_nil_value();
    return exc;
}

/*
 * call-seq:
 *   ec.replacement -> string
 *
 * Returns the replacement string.
 *
 *  ec = Encoding::Converter.new("euc-jp", "us-ascii")
 *  p ec.replacement    #=> "?"
 *
 *  ec = Encoding::Converter.new("euc-jp", "utf-8")
 *  p ec.replacement    #=> "\uFFFD"
 */
static mrb_value
econv_get_replacement(mrb_state *mrb, mrb_value self)
{
    mrb_econv_t *ec = check_econv(mrb, self);
    int ret;
    mrb_encoding *enc;

    ret = make_replacement(mrb, ec);
    if (ret == -1) {
        mrb_raise(mrb, E_UNDEFINEDCONVERSION_ERROR, "replacement character setup failed");
    }

    enc = mrb_enc_find(mrb, ec->replacement_enc);
    return mrb_enc_str_new(mrb, (const char *)ec->replacement_str, (long)ec->replacement_len, enc);
}

/*
 * call-seq:
 *   ec.replacement = string
 *
 * Sets the replacement string.
 *
 *  ec = Encoding::Converter.new("utf-8", "us-ascii", :undef => :replace)
 *  ec.replacement = "<undef>"
 *  p ec.convert("a \u3042 b")      #=> "a <undef> b"
 */
static mrb_value
econv_set_replacement(mrb_state *mrb, mrb_value self)
{
  mrb_value arg;
  mrb_econv_t *ec = check_econv(mrb, self);
  mrb_value string = arg;
  int ret;
  mrb_encoding *enc;
  mrb_get_args(mrb, "o", &arg);

  //StringValue(string);
  mrb_string_value(mrb, &string);
  enc = mrb_enc_get(mrb, string);

  ret = mrb_econv_set_replacement(mrb, ec,
          (const unsigned char *)RSTRING_PTR(string),
          RSTRING_LEN(string),
          mrb_enc_name(enc));

  if (ret == -1) {
      /* xxx: mrb_eInvalidByteSequenceError? */
      mrb_raise(mrb, E_UNDEFINEDCONVERSION_ERROR, "replacement character setup failed");
  }

  return arg;
}

mrb_value
mrb_econv_make_exception(mrb_state *mrb, mrb_econv_t *ec)
{
    return make_econv_exception(mrb, ec);
}

void
mrb_econv_check_error(mrb_state *mrb, mrb_econv_t *ec)
{
    mrb_value exc;

    exc = make_econv_exception(mrb, ec);
    if (mrb_nil_p(exc))
        return;
    mrb_exc_raise(mrb, exc);
}

/*
 * call-seq:
 *   ecerr.source_encoding_name         -> string
 *
 * Returns the source encoding name as a string.
 */
static mrb_value
ecerr_source_encoding_name(mrb_state *mrb, mrb_value self)
{
    return mrb_attr_get(mrb, self, mrb_intern(mrb, "source_encoding_name"));
}

/*
 * call-seq:
 *   ecerr.source_encoding              -> encoding
 *
 * Returns the source encoding as an encoding object.
 *
 * Note that the result may not be equal to the source encoding of
 * the encoding converter if the conversion has multiple steps.
 *
 *  ec = Encoding::Converter.new("ISO-8859-1", "EUC-JP") # ISO-8859-1 -> UTF-8 -> EUC-JP
 *  begin
 *    ec.convert("\xa0") # NO-BREAK SPACE, which is available in UTF-8 but not in EUC-JP.
 *  rescue Encoding::UndefinedConversionError
 *    p $!.source_encoding              #=> #<Encoding:UTF-8>
 *    p $!.destination_encoding         #=> #<Encoding:EUC-JP>
 *    p $!.source_encoding_name         #=> "UTF-8"
 *    p $!.destination_encoding_name    #=> "EUC-JP"
 *  end
 *
 */
static mrb_value
ecerr_source_encoding(mrb_state *mrb, mrb_value self)
{
    return mrb_attr_get(mrb, self, mrb_intern(mrb, "source_encoding"));
}

/*
 * call-seq:
 *   ecerr.destination_encoding_name         -> string
 *
 * Returns the destination encoding name as a string.
 */
static mrb_value
ecerr_destination_encoding_name(mrb_state *mrb, mrb_value self)
{
    return mrb_attr_get(mrb, self, mrb_intern(mrb, "destination_encoding_name"));
}

/*
 * call-seq:
 *   ecerr.destination_encoding         -> string
 *
 * Returns the destination encoding as an encoding object.
 */
static mrb_value
ecerr_destination_encoding(mrb_state *mrb, mrb_value self)
{
    return mrb_attr_get(mrb, self, mrb_intern(mrb, "destination_encoding"));
}

/*
 * call-seq:
 *   ecerr.error_char         -> string
 *
 * Returns the one-character string which cause Encoding::UndefinedConversionError.
 *
 *  ec = Encoding::Converter.new("ISO-8859-1", "EUC-JP")
 *  begin
 *    ec.convert("\xa0")
 *  rescue Encoding::UndefinedConversionError
 *    puts $!.error_char.dump   #=> "\xC2\xA0"
 *    p $!.error_char.encoding  #=> #<Encoding:UTF-8>
 *  end
 *
 */
static mrb_value
ecerr_error_char(mrb_state *mrb, mrb_value self)
{
    return mrb_attr_get(mrb, self, mrb_intern(mrb, "error_char"));
}

/*
 * call-seq:
 *   ecerr.error_bytes         -> string
 *
 * Returns the discarded bytes when Encoding::InvalidByteSequenceError occurs.
 *
 *  ec = Encoding::Converter.new("EUC-JP", "ISO-8859-1")
 *  begin
 *    ec.convert("abc\xA1\xFFdef")
 *  rescue Encoding::InvalidByteSequenceError
 *    p $!      #=> #<Encoding::InvalidByteSequenceError: "\xA1" followed by "\xFF" on EUC-JP>
 *    puts $!.error_bytes.dump          #=> "\xA1"
 *    puts $!.readagain_bytes.dump      #=> "\xFF"
 *  end
 */
static mrb_value
ecerr_error_bytes(mrb_state *mrb, mrb_value self)
{
    return mrb_attr_get(mrb, self, mrb_intern(mrb, "error_bytes"));
}

/*
 * call-seq:
 *   ecerr.readagain_bytes         -> string
 *
 * Returns the bytes to be read again when Encoding::InvalidByteSequenceError occurs.
 */
static mrb_value
ecerr_readagain_bytes(mrb_state *mrb, mrb_value self)
{
    return mrb_attr_get(mrb, self, mrb_intern(mrb, "readagain_bytes"));
}

/*
 * call-seq:
 *   ecerr.incomplete_input?         -> true or false
 *
 * Returns true if the invalid byte sequence error is caused by
 * premature end of string.
 *
 *  ec = Encoding::Converter.new("EUC-JP", "ISO-8859-1")
 *
 *  begin
 *    ec.convert("abc\xA1z")
 *  rescue Encoding::InvalidByteSequenceError
 *    p $!      #=> #<Encoding::InvalidByteSequenceError: "\xA1" followed by "z" on EUC-JP>
 *    p $!.incomplete_input?    #=> false
 *  end
 *
 *  begin
 *    ec.convert("abc\xA1")
 *    ec.finish
 *  rescue Encoding::InvalidByteSequenceError
 *    p $!      #=> #<Encoding::InvalidByteSequenceError: incomplete "\xA1" on EUC-JP>
 *    p $!.incomplete_input?    #=> true
 *  end
 */
static mrb_value
ecerr_incomplete_input(mrb_state *mrb, mrb_value self)
{
    return mrb_attr_get(mrb, self, mrb_intern(mrb, "incomplete_input"));
}

extern void Init_newline(void);

/*
 *  Document-class: Encoding::UndefinedConversionError
 *
 *  Raised by Encoding and String methods when a transcoding operation
 *  fails.
 */

/*
 *  Document-class: Encoding::InvalidByteSequenceError
 *
 *  Raised by Encoding and String methods when the string being
 *  transcoded contains a byte invalid for the either the source or
 *  target encoding.
 */

/*
 *  Document-class: Encoding::ConverterNotFoundError
 *
 *  Raised by transcoding methods when a named encoding does not
 *  correspond with a known converter.
 */

void
mrb_init_transcode(mrb_state *mrb)
{
  struct RClass *e;
  struct RClass *s;
  struct RClass *c;
  struct RClass *u;
  struct RClass *i;
  struct RClass *eConverterNotFoundError_class;
  struct RClass *eInvalidByteSequenceError_class;
  struct RClass *eUndefinedConversionError_class;
  e = mrb->encode_class;
  eUndefinedConversionError_class = mrb_define_class(mrb, "UndefinedConversionError", E_ENCODING_ERROR);
  eInvalidByteSequenceError_class = mrb_define_class(mrb, "InvalidByteSequenceError", E_ENCODING_ERROR);
  eConverterNotFoundError_class   = mrb_define_class(mrb, "ConverterNotFoundError",   E_ENCODING_ERROR);

  transcoder_table = st_init_strcasetable();

    //sym_invalid = ID2SYM(mrb_intern("invalid"));
    //sym_undef = ID2SYM(mrb_intern("undef"));
    //sym_replace = ID2SYM(mrb_intern("replace"));
    //sym_fallback = ID2SYM(mrb_intern("fallback"));
    //sym_xml = ID2SYM(mrb_intern("xml"));
    //sym_text = ID2SYM(mrb_intern("text"));
    //sym_attr = ID2SYM(mrb_intern("attr"));

    //sym_invalid_byte_sequence = ID2SYM(mrb_intern("invalid_byte_sequence"));
    //sym_undefined_conversion = ID2SYM(mrb_intern("undefined_conversion"));
    //sym_destination_buffer_full = ID2SYM(mrb_intern("destination_buffer_full"));
    //sym_source_buffer_empty = ID2SYM(mrb_intern("source_buffer_empty"));
    //sym_finished = ID2SYM(mrb_intern("finished"));
    //sym_after_output = ID2SYM(mrb_intern("after_output"));
    //sym_incomplete_input = ID2SYM(mrb_intern("incomplete_input"));
    //sym_universal_newline = ID2SYM(mrb_intern("universal_newline"));
    //sym_crlf_newline = ID2SYM(mrb_intern("crlf_newline"));
    //sym_cr_newline = ID2SYM(mrb_intern("cr_newline"));
    //sym_partial_input = ID2SYM(mrb_intern("partial_input"));

  s = mrb->string_class;
  mrb_define_method(mrb, s, "encode",  str_encode,      ARGS_ANY());
  mrb_define_method(mrb, s, "encode!", str_encode_bang, ARGS_ANY());

  c = mrb->converter_class = mrb_define_class(mrb, "Converter", mrb->encode_class);
  //mrb_cEncodingConverter = rb_define_class_under(mrb_cEncoding, "Converter", rb_cData);
  //mrb_define_alloc_func(mrb_cEncodingConverter, econv_s_allocate);
  mrb_define_class_method(mrb, c, "asciicompat_encoding",      econv_s_asciicompat_encoding,    ARGS_REQ(1)); /* 1  */
  mrb_define_class_method(mrb, c, "search_convpath",           econv_s_search_convpath,         ARGS_ANY()); /*  2  */
  mrb_define_method(mrb, s,       "initialize",                econv_init,                      ARGS_ANY());
  mrb_define_method(mrb, s,       "inspect",                   econv_inspect,                   ARGS_NONE());
  mrb_define_method(mrb, s,       "convpath",                  econv_convpath,                  ARGS_NONE());
  mrb_define_method(mrb, s,       "source_encoding",           econv_source_encoding,           ARGS_NONE());
  mrb_define_method(mrb, s,       "destination_encoding",      econv_destination_encoding,      ARGS_NONE());
  mrb_define_method(mrb, s,       "primitive_convert",         econv_primitive_convert,         ARGS_ANY());
  mrb_define_method(mrb, s,       "convert",                   econv_convert,                   ARGS_REQ(1));
  mrb_define_method(mrb, s,       "finish",                    econv_finish,                    ARGS_NONE());
  mrb_define_method(mrb, s,       "primitive_errinfo",         econv_primitive_errinfo,         ARGS_NONE());
  mrb_define_method(mrb, s,       "insert_output",             econv_insert_output,             ARGS_REQ(1));
  mrb_define_method(mrb, s,       "putback",                   econv_putback,                   ARGS_ANY());
  mrb_define_method(mrb, s,       "last_error",                econv_last_error,                ARGS_NONE());
  mrb_define_method(mrb, s,       "replacement",               econv_get_replacement,           ARGS_NONE());
  mrb_define_method(mrb, s,       "replacement=",              econv_set_replacement,           ARGS_REQ(1));

  mrb_define_const(mrb, s,        "INVALID_MASK",                mrb_fixnum_value(ECONV_INVALID_MASK));
  mrb_define_const(mrb, s,        "INVALID_REPLACE",             mrb_fixnum_value(ECONV_INVALID_REPLACE));
  mrb_define_const(mrb, s,        "UNDEF_MASK",                  mrb_fixnum_value(ECONV_UNDEF_MASK));
  mrb_define_const(mrb, s,        "UNDEF_REPLACE",               mrb_fixnum_value(ECONV_UNDEF_REPLACE));
  mrb_define_const(mrb, s,        "UNDEF_HEX_CHARREF",           mrb_fixnum_value(ECONV_UNDEF_HEX_CHARREF));
  mrb_define_const(mrb, s,        "PARTIAL_INPUT",               mrb_fixnum_value(ECONV_PARTIAL_INPUT));
  mrb_define_const(mrb, s,        "AFTER_OUTPUT",                mrb_fixnum_value(ECONV_AFTER_OUTPUT));
  mrb_define_const(mrb, s,        "UNIVERSAL_NEWLINE_DECORATOR", mrb_fixnum_value(ECONV_UNIVERSAL_NEWLINE_DECORATOR));
  mrb_define_const(mrb, s,        "CRLF_NEWLINE_DECORATOR",      mrb_fixnum_value(ECONV_CRLF_NEWLINE_DECORATOR));
  mrb_define_const(mrb, s,        "CR_NEWLINE_DECORATOR",        mrb_fixnum_value(ECONV_CR_NEWLINE_DECORATOR));
  mrb_define_const(mrb, s,        "XML_TEXT_DECORATOR",          mrb_fixnum_value(ECONV_XML_TEXT_DECORATOR));
  mrb_define_const(mrb, s,        "XML_ATTR_CONTENT_DECORATOR",  mrb_fixnum_value(ECONV_XML_ATTR_CONTENT_DECORATOR));
  mrb_define_const(mrb, s,        "XML_ATTR_QUOTE_DECORATOR",    mrb_fixnum_value(ECONV_XML_ATTR_QUOTE_DECORATOR));

  u = E_UNDEFINEDCONVERSION_ERROR;
  mrb_define_method(mrb, u,       "source_encoding_name",        ecerr_source_encoding_name,      ARGS_NONE());
  mrb_define_method(mrb, u,       "destination_encoding_name",   ecerr_destination_encoding_name, ARGS_NONE());
  mrb_define_method(mrb, u,       "source_encoding",             ecerr_source_encoding,           ARGS_NONE());
  mrb_define_method(mrb, u,       "destination_encoding",        ecerr_destination_encoding,      ARGS_NONE());
  mrb_define_method(mrb, u,       "error_char",                  ecerr_error_char,                ARGS_NONE());

  i = E_INVALIDBYTESEQUENCE_ERROR;
  mrb_define_method(mrb, i,       "source_encoding_name",        ecerr_source_encoding_name,      ARGS_NONE());
  mrb_define_method(mrb, i,       "destination_encoding_name",   ecerr_destination_encoding_name, ARGS_NONE());
  mrb_define_method(mrb, i,       "source_encoding",             ecerr_source_encoding,           ARGS_NONE());
  mrb_define_method(mrb, i,       "destination_encoding",        ecerr_destination_encoding,      ARGS_NONE());
  mrb_define_method(mrb, i,       "error_bytes",                 ecerr_error_bytes,               ARGS_NONE());
  mrb_define_method(mrb, i,       "readagain_bytes",             ecerr_readagain_bytes,           ARGS_NONE());
  mrb_define_method(mrb, i,       "incomplete_input?",           ecerr_incomplete_input,          ARGS_NONE());

  //Init_newline();
}
#endif //INCLUDE_ENCODING
