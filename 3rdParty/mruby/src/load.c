/*
** load.c - mruby binary loader
**
** See Copyright Notice in mruby.h
*/

#ifndef SIZE_MAX
 /* Some versions of VC++
  * has SIZE_MAX in stdint.h
  */
# include <limits.h>
#endif
#include <stdlib.h>
#include <string.h>
#include "mruby/dump.h"
#include "mruby/irep.h"
#include "mruby/proc.h"
#include "mruby/string.h"

#if !defined(_WIN32) && SIZE_MAX < UINT32_MAX
# define SIZE_ERROR_MUL(x, y) ((x) > SIZE_MAX / (y))
# define SIZE_ERROR(x) ((x) > SIZE_MAX)
#else
# define SIZE_ERROR_MUL(x, y) (0)
# define SIZE_ERROR(x) (0)
#endif

#if CHAR_BIT != 8
# error This code assumes CHAR_BIT == 8
#endif

static void
irep_free(size_t sirep, mrb_state *mrb)
{
  size_t i;
  void *p;

  for (i = sirep; i < mrb->irep_len; i++) {
    if (mrb->irep[i]) {
      p = mrb->irep[i]->iseq;
      if (p)
        mrb_free(mrb, p);

      p = mrb->irep[i]->pool;
      if (p)
        mrb_free(mrb, p);

      p = mrb->irep[i]->syms;
      if (p)
        mrb_free(mrb, p);

      mrb_free(mrb, mrb->irep[i]);
    }
  }
}

static size_t
offset_crc_body(void)
{
  struct rite_binary_header header;
  return ((uint8_t *)header.binary_crc - (uint8_t *)&header) + sizeof(header.binary_crc);
}

static int
read_rite_irep_record(mrb_state *mrb, const uint8_t *bin, uint32_t *len)
{
  int ret;
  size_t i;
  const uint8_t *src = bin;
  uint16_t tt, pool_data_len, snl;
  size_t plen;
  int ai = mrb_gc_arena_save(mrb);
  mrb_irep *irep = mrb_add_irep(mrb);

  // skip record size
  src += sizeof(uint32_t);

  // number of local variable
  irep->nlocals = bin_to_uint16(src);
  src += sizeof(uint16_t);

  // number of register variable
  irep->nregs = bin_to_uint16(src);
  src += sizeof(uint16_t);

  // Binary Data Section
  // ISEQ BLOCK
  irep->ilen = bin_to_uint32(src);
  src += sizeof(uint32_t);
  if (irep->ilen > 0) {
    if (SIZE_ERROR_MUL(sizeof(mrb_code), irep->ilen)) {
      ret = MRB_DUMP_GENERAL_FAILURE;
      goto error_exit;
    }
    irep->iseq = (mrb_code *)mrb_malloc(mrb, sizeof(mrb_code) * irep->ilen);
    if (irep->iseq == NULL) {
      ret = MRB_DUMP_GENERAL_FAILURE;
      goto error_exit;
    }
    for (i = 0; i < irep->ilen; i++) {
      irep->iseq[i] = bin_to_uint32(src);     //iseq
      src += sizeof(uint32_t);
    }
  }

  //POOL BLOCK
  plen = bin_to_uint32(src); /* number of pool */
  src += sizeof(uint32_t);
  if (plen > 0) {
    if (SIZE_ERROR_MUL(sizeof(mrb_value), plen)) {
      ret = MRB_DUMP_GENERAL_FAILURE;
      goto error_exit;
    }
    irep->pool = (mrb_value *)mrb_malloc(mrb, sizeof(mrb_value) * plen);
    if (irep->pool == NULL) {
      ret = MRB_DUMP_GENERAL_FAILURE;
      goto error_exit;
    }

    for (i = 0; i < plen; i++) {
      mrb_value s;
      tt = *src++; //pool TT
      pool_data_len = bin_to_uint16(src); //pool data length
      src += sizeof(uint16_t);
      s = mrb_str_new(mrb, (char *)src, pool_data_len);
      src += pool_data_len;
      switch (tt) { //pool data
      case MRB_TT_FIXNUM:
        irep->pool[i] = mrb_str_to_inum(mrb, s, 10, FALSE);
        break;

      case MRB_TT_FLOAT:
        irep->pool[i] = mrb_float_value(mrb, mrb_str_to_dbl(mrb, s, FALSE));
        break;

      case MRB_TT_STRING:
        irep->pool[i] = s;
        break;

      default:
        irep->pool[i] = mrb_nil_value();
        break;
      }
      irep->plen++;
      mrb_gc_arena_restore(mrb, ai);
    }
  }

  //SYMS BLOCK
  irep->slen = bin_to_uint32(src);  //syms length
  src += sizeof(uint32_t);
  if (irep->slen > 0) {
    if (SIZE_ERROR_MUL(sizeof(mrb_sym), irep->slen)) {
      ret = MRB_DUMP_GENERAL_FAILURE;
      goto error_exit;
    }
    irep->syms = (mrb_sym *)mrb_malloc(mrb, sizeof(mrb_sym) * irep->slen);
    if (irep->syms == NULL) {
      ret = MRB_DUMP_GENERAL_FAILURE;
      goto error_exit;
    }

    for (i = 0; i < irep->slen; i++) {
      snl = bin_to_uint16(src);               //symbol name length
      src += sizeof(uint16_t);

      if (snl == MRB_DUMP_NULL_SYM_LEN) {
        irep->syms[i] = 0;
        continue;
      }

      irep->syms[i] = mrb_intern2(mrb, (char *)src, snl);
      src += snl + 1;

      mrb_gc_arena_restore(mrb, ai);
    }
  }
  *len = src - bin;

  ret = MRB_DUMP_OK;
error_exit:
  return ret;
}

static int
read_rite_section_irep(mrb_state *mrb, const uint8_t *bin)
{
  int result;
  size_t sirep;
  uint32_t len;
  uint16_t nirep;
  uint16_t n;
  const struct rite_section_irep_header *header;

  header = (const struct rite_section_irep_header*)bin;
  bin += sizeof(struct rite_section_irep_header);

  sirep = mrb->irep_len;
  nirep = bin_to_uint16(header->nirep);

  //Read Binary Data Section
  for (n = 0; n < nirep; n++) {
    result = read_rite_irep_record(mrb, bin, &len);
    if (result != MRB_DUMP_OK)
      goto error_exit;
    bin += len;
  }

  result = nirep;
error_exit:
  if (result < MRB_DUMP_OK) {
    irep_free(sirep, mrb);
  }
  return result;
}

static int
read_rite_lineno_record(mrb_state *mrb, const uint8_t *bin, size_t irepno, uint32_t *len)
{
  int ret;
  size_t i, fname_len, niseq;
  char *fname;
  uint16_t *lines;

  ret = MRB_DUMP_OK;
  *len = 0;
  bin += sizeof(uint32_t); // record size
  *len += sizeof(uint32_t);
  fname_len = bin_to_uint16(bin);
  bin += sizeof(uint16_t);
  *len += sizeof(uint16_t);
  if (SIZE_ERROR(fname_len + 1)) {
    ret = MRB_DUMP_GENERAL_FAILURE;
    goto error_exit;
  }
  fname = (char *)mrb_malloc(mrb, fname_len + 1);
  if (fname == NULL) {
    ret = MRB_DUMP_GENERAL_FAILURE;
    goto error_exit;
  }
  memcpy(fname, bin, fname_len);
  fname[fname_len] = '\0';
  bin += fname_len;
  *len += fname_len;

  niseq = bin_to_uint32(bin);
  bin += sizeof(uint32_t); // niseq
  *len += sizeof(uint32_t);

  if (SIZE_ERROR_MUL(niseq, sizeof(uint16_t))) {
    ret = MRB_DUMP_GENERAL_FAILURE;
    goto error_exit;
  }
  lines = (uint16_t *)mrb_malloc(mrb, niseq * sizeof(uint16_t));
  if (lines == NULL) {
    ret = MRB_DUMP_GENERAL_FAILURE;
    goto error_exit;
  }
  for (i = 0; i < niseq; i++) {
    lines[i] = bin_to_uint16(bin);
    bin += sizeof(uint16_t); // niseq
    *len += sizeof(uint16_t);
 }

  mrb->irep[irepno]->filename = fname;
  mrb->irep[irepno]->lines = lines;

error_exit:

  return ret;
}

static int
read_rite_section_lineno(mrb_state *mrb, const uint8_t *bin, size_t sirep)
{
  int result;
  size_t i;
  uint32_t len;
  uint16_t nirep;
  uint16_t n;
  const struct rite_section_lineno_header *header;

  len = 0;
  header = (const struct rite_section_lineno_header*)bin;
  bin += sizeof(struct rite_section_lineno_header);

  nirep = bin_to_uint16(header->nirep);

  //Read Binary Data Section
  for (n = 0, i = sirep; n < nirep; n++, i++) {
    result = read_rite_lineno_record(mrb, bin, i, &len);
    if (result != MRB_DUMP_OK)
      goto error_exit;
    bin += len;
  }

  result = sirep + bin_to_uint16(header->sirep);
error_exit:
  return result;
}


static int
read_rite_binary_header(const uint8_t *bin, size_t *bin_size, uint16_t *crc)
{
  const struct rite_binary_header *header = (const struct rite_binary_header *)bin;

  if (memcmp(header->binary_identify, RITE_BINARY_IDENTIFIER, sizeof(header->binary_identify)) != 0) {
    return MRB_DUMP_INVALID_FILE_HEADER;
  }

  if (memcmp(header->binary_version, RITE_BINARY_FORMAT_VER, sizeof(header->binary_version)) != 0) {
    return MRB_DUMP_INVALID_FILE_HEADER;
  }

  *crc = bin_to_uint16(header->binary_crc);
  if (bin_size) {
    *bin_size = bin_to_uint32(header->binary_size);
  }

  return MRB_DUMP_OK;
}

int32_t
mrb_read_irep(mrb_state *mrb, const uint8_t *bin)
{
  int result;
  int32_t total_nirep = 0;
  const struct rite_section_header *section_header;
  uint16_t crc;
  size_t bin_size = 0;
  size_t n;
  size_t sirep;

  if ((mrb == NULL) || (bin == NULL)) {
    return MRB_DUMP_INVALID_ARGUMENT;
  }

  result = read_rite_binary_header(bin, &bin_size, &crc);
  if (result != MRB_DUMP_OK) {
    return result;
  }

  n = offset_crc_body();
  if (crc != calc_crc_16_ccitt(bin + n, bin_size - n, 0)) {
    return MRB_DUMP_INVALID_FILE_HEADER;
  }

  bin += sizeof(struct rite_binary_header);
  sirep = mrb->irep_len;

  do {
    section_header = (const struct rite_section_header *)bin;
    if (memcmp(section_header->section_identify, RITE_SECTION_IREP_IDENTIFIER, sizeof(section_header->section_identify)) == 0) {
      result = read_rite_section_irep(mrb, bin);
      if (result < MRB_DUMP_OK) {
        return result;
      }
      total_nirep += result;
    }
    else if (memcmp(section_header->section_identify, RITE_SECTION_LINENO_IDENTIFIER, sizeof(section_header->section_identify)) == 0) {
      result = read_rite_section_lineno(mrb, bin, sirep);
      if (result < MRB_DUMP_OK) {
        return result;
      }
    }
    bin += bin_to_uint32(section_header->section_size);
  } while (memcmp(section_header->section_identify, RITE_BINARY_EOF, sizeof(section_header->section_identify)) != 0);

  return sirep;
}

static void
irep_error(mrb_state *mrb, int n)
{
  static const char msg[] = "irep load error";
  mrb->exc = mrb_obj_ptr(mrb_exc_new(mrb, E_SCRIPT_ERROR, msg, sizeof(msg) - 1));
}

mrb_value
mrb_load_irep(mrb_state *mrb, const uint8_t *bin)
{
  int32_t n;

  n = mrb_read_irep(mrb, bin);
  if (n < 0) {
    irep_error(mrb, n);
    return mrb_nil_value();
  }
  return mrb_run(mrb, mrb_proc_new(mrb, mrb->irep[n]), mrb_top_self(mrb));
}

#ifdef ENABLE_STDIO

static int32_t
read_rite_section_lineno_file(mrb_state *mrb, FILE *fp, size_t sirep)
{
  int32_t result;
  size_t i;
  uint16_t nirep;
  uint16_t n;
  uint32_t len, buf_size;
  uint8_t *buf = NULL;
  const size_t record_header_size = 4;

  struct rite_section_lineno_header header;
  if (fread(&header, sizeof(struct rite_section_lineno_header), 1, fp) == 0) {
    return MRB_DUMP_READ_FAULT;
  }

  nirep = bin_to_uint16(header.nirep);

  buf_size = record_header_size;
  /* We don't need to check buf_size. As it is enough small. */
  buf = (uint8_t *)mrb_malloc(mrb, buf_size);
  if (!buf) {
    result = MRB_DUMP_GENERAL_FAILURE;
    goto error_exit;
  }

  //Read Binary Data Section
  for (n = 0, i = sirep; n < nirep; n++, i++) {
    void *ptr;

    if (fread(buf, record_header_size, 1, fp) == 0) {
      result = MRB_DUMP_READ_FAULT;
      goto error_exit;
    }
    buf_size = bin_to_uint32(&buf[0]);
    if (SIZE_ERROR(buf_size)) {
      result = MRB_DUMP_GENERAL_FAILURE;
      goto error_exit;
    }
    ptr = mrb_realloc(mrb, buf, buf_size);
    if (!ptr) {
      result = MRB_DUMP_GENERAL_FAILURE;
      goto error_exit;
    }
    buf = (uint8_t *)ptr;

    if (fread(&buf[record_header_size], buf_size - record_header_size, 1, fp) == 0) {
      result = MRB_DUMP_READ_FAULT;
      goto error_exit;
    }
    result = read_rite_lineno_record(mrb, buf, i, &len);
    if (result != MRB_DUMP_OK)
      goto error_exit;
  }

  result = sirep + bin_to_uint16(header.sirep);
error_exit:
  if (buf) {
    mrb_free(mrb, buf);
  }
  if (result < MRB_DUMP_OK) {
    irep_free(sirep, mrb);
  }
  return result;
}

static int32_t
read_rite_section_irep_file(mrb_state *mrb, FILE *fp)
{
  int32_t result;
  size_t sirep;
  uint16_t nirep;
  uint16_t n;
  uint32_t len, buf_size;
  uint8_t *buf = NULL;
  const size_t record_header_size = 1 + 4;
  struct rite_section_irep_header header;

  if (fread(&header, sizeof(struct rite_section_irep_header), 1, fp) == 0) {
    return MRB_DUMP_READ_FAULT;
  }

  sirep = mrb->irep_len;
  nirep = bin_to_uint16(header.nirep);

  buf_size = record_header_size;
  /* You don't need use SIZE_ERROR as buf_size is enough small. */
  buf = (uint8_t *)mrb_malloc(mrb, buf_size);
  if (!buf) {
    result = MRB_DUMP_GENERAL_FAILURE;
    goto error_exit;
  }

  //Read Binary Data Section
  for (n = 0; n < nirep; n++) {
    void *ptr;

    if (fread(buf, record_header_size, 1, fp) == 0) {
      result = MRB_DUMP_READ_FAULT;
      goto error_exit;
    }
    buf_size = bin_to_uint32(&buf[0]);
    if (SIZE_ERROR(buf_size)) {
      result = MRB_DUMP_GENERAL_FAILURE;
      goto error_exit;
    }
    ptr = mrb_realloc(mrb, buf, buf_size);
    if (!ptr) {
      result = MRB_DUMP_GENERAL_FAILURE;
      goto error_exit;
    }
    buf = (uint8_t *)ptr;

    if (fread(&buf[record_header_size], buf_size - record_header_size, 1, fp) == 0) {
      result = MRB_DUMP_READ_FAULT;
      goto error_exit;
    }
    result = read_rite_irep_record(mrb, buf, &len);
    if (result != MRB_DUMP_OK)
      goto error_exit;
  }

  result = nirep;
error_exit:
  if (buf) {
    mrb_free(mrb, buf);
  }
  if (result < MRB_DUMP_OK) {
    irep_free(sirep, mrb);
  }
  return result;
}

int32_t
mrb_read_irep_file(mrb_state *mrb, FILE* fp)
{
  int result;
  int32_t total_nirep = 0;
  uint8_t *buf;
  uint16_t crc, crcwk = 0;
  uint32_t section_size = 0;
  size_t nbytes;
  size_t sirep;
  struct rite_section_header section_header;
  long fpos;
  size_t block_size = 1 << 14;
  const uint8_t block_fallback_count = 4;
  int i;
  const size_t buf_size = sizeof(struct rite_binary_header);

  if ((mrb == NULL) || (fp == NULL)) {
    return MRB_DUMP_INVALID_ARGUMENT;
  }

  /* You don't need use SIZE_ERROR as buf_size is enough small. */
  buf = mrb_malloc(mrb, buf_size);
  if (!buf) {
    return MRB_DUMP_GENERAL_FAILURE;
  }
  if (fread(buf, buf_size, 1, fp) == 0) {
    mrb_free(mrb, buf);
    return MRB_DUMP_READ_FAULT;
  }
  result = read_rite_binary_header(buf, NULL, &crc);
  mrb_free(mrb, buf);
  if (result != MRB_DUMP_OK) {
    return result;
  }

  /* verify CRC */
  fpos = ftell(fp);
  /* You don't need use SIZE_ERROR as block_size is enough small. */
  for (i = 0; i < block_fallback_count; i++,block_size >>= 1){
    buf = mrb_malloc_simple(mrb, block_size);
    if (buf) break;  
  }
  if (!buf) {
    return MRB_DUMP_GENERAL_FAILURE;
  }
  fseek(fp, offset_crc_body(), SEEK_SET);
  while ((nbytes = fread(buf, 1, block_size, fp)) > 0) {
    crcwk = calc_crc_16_ccitt(buf, nbytes, crcwk);
  }
  mrb_free(mrb, buf);
  if (nbytes == 0 && ferror(fp)) {
    return MRB_DUMP_READ_FAULT;
  }
  if (crcwk != crc) {
    return MRB_DUMP_INVALID_FILE_HEADER;
  }
  fseek(fp, fpos + section_size, SEEK_SET);
  sirep = mrb->irep_len;

  // read sections
  do {
    fpos = ftell(fp);
    if (fread(&section_header, sizeof(struct rite_section_header), 1, fp) == 0) {
      return MRB_DUMP_READ_FAULT;
    }
    section_size = bin_to_uint32(section_header.section_size);

    if (memcmp(section_header.section_identify, RITE_SECTION_IREP_IDENTIFIER, sizeof(section_header.section_identify)) == 0) {
      fseek(fp, fpos, SEEK_SET);
      result = read_rite_section_irep_file(mrb, fp);
      if (result < MRB_DUMP_OK) {
        return result;
      }
      total_nirep += result;
    }
    else if (memcmp(section_header.section_identify, RITE_SECTION_LINENO_IDENTIFIER, sizeof(section_header.section_identify)) == 0) {
      fseek(fp, fpos, SEEK_SET);
      result = read_rite_section_lineno_file(mrb, fp, sirep);
      if (result < MRB_DUMP_OK) {
        return result;
      }
    }

    fseek(fp, fpos + section_size, SEEK_SET);
  } while (memcmp(section_header.section_identify, RITE_BINARY_EOF, sizeof(section_header.section_identify)) != 0);

  return sirep;
}

mrb_value
mrb_load_irep_file(mrb_state *mrb, FILE* fp)
{
  int n = mrb_read_irep_file(mrb, fp);

  if (n < 0) {
    irep_error(mrb, n);
    return mrb_nil_value();
  }
  return mrb_run(mrb, mrb_proc_new(mrb, mrb->irep[n]), mrb_top_self(mrb));
}
#endif /* ENABLE_STDIO */
