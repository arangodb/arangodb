/* Test for special independant_... allocation functions in ptmalloc3.
   Contributed by Wolfram Gloger <wg@malloc.de>, 2006.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#include <stdio.h>

#include "malloc-2.8.3.h"

#define CSIZE 104

static int errors = 0;

static void
merror (const char *msg)
{
  ++errors;
  printf ("Error: %s\n", msg);
}

int
main (void)
{
  char *p1, **p2, **p3;
  int i, j;
  size_t sizes[10] = { 8, 104, 12, 333, 7, 4098, 119, 331, 1, 707 };

  printf("---- Start:\n");
  malloc_stats();
  p1 = malloc (18);
  if (p1 == NULL)
    merror ("malloc (10) failed.");
  p2 = (char **)independent_calloc (25, CSIZE, 0);
  if (p2 == NULL)
    merror ("independent_calloc (25, ...) failed.");
  p3 = (char **)independent_comalloc(10, sizes, 0);
  if (p3 == NULL)
    merror ("independent_comalloc (10, ...) failed.");

  for (i=0; i<25; ++i) {
    for (j=0; j<CSIZE; ++j)
      if (p2[i][j] != '\0')
	merror("independent_calloc memory not zeroed.");
    p2[i][CSIZE-1] = 'x';
    if (i < 10) {
      p3[i][sizes[i]-1] = 'y';
      if (i % 4 == 0) {
	p3[i] = realloc (p3[i], i*11 + sizes[9-i]);
	if (p3[i] == NULL)
	  merror ("realloc (i*11 + sizes[9-i]) failed.");
      } else if (i % 4 == 1) {
	free(p3[i]);
	p3[i] = 0;
      }
    }
    if (i % 4 == 0) {
      p2[i] = realloc (p2[i], i*7 + 3);
      if (p2[i] == NULL)
	merror ("realloc (i*7 + 3) failed.");
    } else if (i % 4 == 1) {
      free(p2[i]);
      p2[i] = 0;
    }
  }

  printf("---- Before free:\n");
  malloc_stats();
  free(p1);
  for (i=0; i<10; ++i)
    free(p3[i]);
  free(p3);
  for (i=0; i<25; ++i)
    free(p2[i]);
  free(p2);
  printf("---- After free:\n");
  malloc_stats();

  return errors != 0;
}

/*
 * Local variables:
 * c-basic-offset: 2
 * End:
 */
