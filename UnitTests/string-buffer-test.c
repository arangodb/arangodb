#include "../BasicsC/string-buffer.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

/*
 * gcc -g -I . -L . -lavocado UnitTests/string-buffer-test.c && ./a.out 
 * from base dir.
 */

static int tst_tst_cnt;
static int tst_err_cnt;

#define STR "The quick brown fox jumped over the laxy dog"
#define STRSTR  STR STR

#define TWNTYA "aaaaaaaaaaaaaaaaaaaa"

#define ABC "ABCDEFGHIJKLMNOP"
#define REP "REPDEFGHIJKLMNOP"
#define AEP "AEPDEFGHIJKLMNOP"

static cmp_int (int should, int is, char * name) {
  if (should != is) {
    printf("'%s' failed! should: %d is: %d\n", name, should, is);
    tst_err_cnt++;
  }
  tst_tst_cnt++;
}

static cmp_ptr (const char * should, const char * is, char * name) {
  if (should != is) {
    printf("'%s' failed! should: %p is: %p\n", name, should, is);
    tst_err_cnt++;
  }
  tst_tst_cnt++;
}
static cmp_str (char * should, char * is, size_t len, char * name) {
  if (strncmp(should, is, len)) {
    printf("'%s' failed! should: \n>>%s<< is: \n>>%s<<\n", name, should, is);
    tst_err_cnt++;
  }
  tst_tst_cnt++;
}

static cmp_bool (int value, char * name) {
  if (!value) {
    printf("'%s' failed! should be truish is: %d\n", name, value);
    tst_err_cnt++;
  }
  tst_tst_cnt++;
}

static void tst_str_append() {
  int l1, l2;

  TRI_string_buffer_t sb, sb2;
  TRI_InitStringBuffer(&sb);
  TRI_AppendStringStringBuffer(&sb, STR);
  TRI_AppendStringStringBuffer(&sb, STR);
  l1 = strnlen( STRSTR, 1024);
  l2 = strnlen( sb._buffer, 1024);
  
  cmp_int((int)l1, (int)l2, "basic append (len)");
  cmp_str(STRSTR, sb._buffer, l1, "basic append (cmp)");
  
  TRI_AppendString2StringBuffer(&sb, ABC, 3); // ABC ... Z

  l2 = strnlen( sb._buffer, 1024);
  cmp_str(STRSTR"ABC", sb._buffer, l2, "basic append 2 (cmp)");

  TRI_ClearStringBuffer(&sb);
  TRI_AppendStringStringBuffer(&sb, STR);
  TRI_InitStringBuffer(&sb2);
  TRI_AppendStringStringBuffer(&sb2, STR);
  TRI_AppendStringBufferStringBuffer(&sb, &sb2);

  l2 = strnlen( sb._buffer, 1024);
  cmp_str(STRSTR, sb._buffer, l2, "basic append 3 (cmp)");
  cmp_str(STR, sb2._buffer, l2, "basic append 4 (cmp)");

  TRI_FreeStringBuffer(&sb);

}

static void tst_char_append() {
  int l1, l2, i;

  TRI_string_buffer_t sb;
  TRI_InitStringBuffer(&sb);
  
  for (i=0l; i!=20; ++i) {
    TRI_AppendCharStringBuffer(&sb, 'a');
  }

  l1 = strnlen( TWNTYA, 1024);
  l2 = strnlen( sb._buffer, 1024);
  
  cmp_int((int)l1, (int)l2, "char append (len)");
  cmp_str(TWNTYA, sb._buffer, l1, "char append (cmp)");

  TRI_FreeStringBuffer(&sb);
}
static void tst_swp() {
  int l1, l2, i;

  TRI_string_buffer_t sb1, sb2;
  TRI_InitStringBuffer(&sb1);
  TRI_InitStringBuffer(&sb2);
  
  for (i=0l; i!=20; ++i) {
    TRI_AppendCharStringBuffer(&sb1, 'a');
  }

  TRI_AppendStringStringBuffer(&sb2, STR);
  
  TRI_SwapStringBuffer(&sb1, &sb2);

  l1 = strnlen( TWNTYA, 1024);
  l2 = strnlen( STR, 1024);
  
  cmp_str(TWNTYA, sb2._buffer, l1, "swp test 1");
  cmp_str(STR, sb1._buffer, l2, "swp test 2");

  TRI_FreeStringBuffer(&sb1);
  TRI_FreeStringBuffer(&sb2);
}
static void tst_begin_end_empty_clear() {
  int l1, i;
  const char * ptr;

  TRI_string_buffer_t sb;
  TRI_InitStringBuffer(&sb);
  
  TRI_AppendStringStringBuffer(&sb, STR);
  
  ptr = TRI_BeginStringBuffer(&sb);
  cmp_ptr(sb._buffer, ptr, "begin test");
  

  l1 = strnlen(STR, 1024);
  ptr = TRI_EndStringBuffer(&sb);
  cmp_ptr(sb._buffer+l1, ptr, "end test");

  cmp_bool((int)!TRI_EmptyStringBuffer(&sb), "empty 1");
  TRI_ClearStringBuffer(&sb);
  cmp_bool((int)TRI_EmptyStringBuffer(&sb), "empty 2");

  TRI_FreeStringBuffer(&sb);
}

static void tst_cpy() {
  int l1, l2, i;

  TRI_string_buffer_t sb1, sb2;
  TRI_InitStringBuffer(&sb1);
  TRI_InitStringBuffer(&sb2);
  
  for (i=0l; i!=20; ++i) {
    TRI_AppendCharStringBuffer(&sb1, 'a');
  }

  TRI_AppendStringStringBuffer(&sb2, STR);
  
  TRI_CopyStringBuffer(&sb1, &sb2);

  l1 = strnlen( STR, 1024);

  cmp_int(l1, strnlen(sb1._buffer, l1) , "copy (len)"); 
  cmp_str(STR, sb2._buffer, l1, "cpy test 1");
  cmp_str(STR, sb1._buffer, l1, "cpy test 2");

  TRI_FreeStringBuffer(&sb1);
  TRI_FreeStringBuffer(&sb2);
}

#define Z_2_T "0123456789A"
#define F_2_T "56789A"
#define Z_2_F "012345"
static void tst_erase_frnt() {
  int l;

  TRI_string_buffer_t sb;
  TRI_InitStringBuffer(&sb);
  TRI_AppendStringStringBuffer(&sb, Z_2_T);
  TRI_EraseFrontStringBuffer(&sb, 5);

  l = strnlen(sb._buffer, 1024);
  cmp_str(F_2_T, sb._buffer, l, "erase front");


  TRI_EraseFrontStringBuffer(&sb, 15);
  
  cmp_bool(TRI_EmptyStringBuffer(&sb), "erase front2");

  TRI_FreeStringBuffer(&sb);
}

static void tst_replace () {
  int l;

  TRI_string_buffer_t sb;
  TRI_string_buffer_t sb2;

  TRI_InitStringBuffer(&sb);
  TRI_AppendStringStringBuffer(&sb, ABC);
  TRI_ReplaceStringStringBuffer(&sb, "REP", 3);
  
  l = strnlen(sb._buffer, 1024);
  cmp_str(REP, sb._buffer, l, "replace1");

  TRI_ReplaceStringStringBuffer(&sb, ABC, 1);
  l = strnlen(sb._buffer, 1024);
  cmp_str(AEP, sb._buffer, l, "replace2");

  TRI_ClearStringBuffer(&sb);
  TRI_AppendStringStringBuffer(&sb, ABC);

  TRI_InitStringBuffer(&sb2);
  TRI_AppendStringStringBuffer(&sb2, "REP");

  TRI_ReplaceStringBufferStringBuffer(&sb, &sb2);
  l = strnlen(sb._buffer, 1024);
  cmp_str(REP, sb._buffer, l, "replace stringbuffer 1");

//  TRI_ClearStringBuffer(&sb2);
//  TRI_AppendStringStringBuffer(&sb2, ABC);
//
//  TRI_ReplaceStringBufferStringBuffer(&sb, &sb2, 1);
//  l = strnlen(sb._buffer, 1024);
//  cmp_str(AEP, sb._buffer, l, "replace stringbuffer 2");

  
}

#define ONETWOTHREE "123"
void tst_smpl_utils () {
  // these are built on prev. tested building blocks...
  TRI_string_buffer_t sb;
  TRI_InitStringBuffer(&sb);
  TRI_AppendInteger3StringBuffer(&sb, 123);
  cmp_str(ONETWOTHREE, sb._buffer, 1024, "append int3");
  TRI_ClearStringBuffer(&sb);
  TRI_AppendInteger3StringBuffer(&sb, 1234);
  cmp_str("234", sb._buffer, 1024, "append int3");

  
  TRI_AppendDoubleStringBuffer(&sb, 12.0);
  cmp_str("23412", sb._buffer, 1024, "append int3");

  TRI_AppendDoubleStringBuffer(&sb, -12.125);
  cmp_str("23412-12.125", sb._buffer, 1024, "append int3");
}

void tst_report () {
  printf("%d test run. %d failed.\n", tst_tst_cnt, tst_err_cnt);
}

int main () {
  tst_str_append();
  tst_char_append();
  tst_swp();
  tst_begin_end_empty_clear();
  tst_cpy();
  tst_erase_frnt();
  tst_replace();
  tst_smpl_utils();
  tst_report();
  return 0;
}

