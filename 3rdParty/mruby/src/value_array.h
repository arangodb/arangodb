#ifndef MRB_VALUE_ARRAY_H__
#define MRB_VALUE_ARRAY_H__

#include "mruby.h"

static inline void
value_move(mrb_value *s1, const mrb_value *s2, size_t n)
{
  if (s1 > s2 && s1 < s2 + n)
  {
    s1 += n;
    s2 += n;
    while (n-- > 0) {
      *--s1 = *--s2;
    }
  }
  else if (s1 != s2) {
    while (n-- > 0) {
      *s1++ = *s2++;
    }
  }
  else {
    /* nothing to do. */
  }
}

#endif /* MRB_VALUE_ARRAY_H__ */
