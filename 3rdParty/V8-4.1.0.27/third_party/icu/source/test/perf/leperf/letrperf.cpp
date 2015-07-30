/***************************************************************************
*
*   Copyright (C) 2013, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
************************************************************************/
/**
 * Usage:
 * build against a configured (but not built) ICU.
 * example: cc -O2 test_LETableReference.cpp -I. -I/xsrl/II/include -I/xsrl/E/icu/source/tools/ctestfw
 */
#include "unicode/utimer.h"
#include "LETableReference.h"
#include <stdio.h>
#include <stdlib.h>

#define ITEM_COUNT 10000

long *items = 0;

struct OneObject {
  long items[ITEM_COUNT];
};

struct Long {
  long v;
};

struct CompObject {
  Long items[ITEM_COUNT];
};


void time_null(void * /*ref*/) {
  for(int i=0;i<ITEM_COUNT;i++) {
    if(items[i]==2) {
      return;
    }
  }
  puts("error");
  abort();
}

void time_obj(void * ref) {
  OneObject &obj = *((OneObject*)ref);
  for(int i=0;i<ITEM_COUNT;i++) {
    if(obj.items[i]==2) {
      return;
    }
  }
  puts("error");
  abort();
}
void time_obj2(void * ref) {
  long *items2 = ((OneObject*)ref)->items;
  for(int i=0;i<ITEM_COUNT;i++) {
    if(items2[i]==2) {
      return;
    }
  }
  puts("error");
  abort();
}

void time_letr1(void * ref) {
  OneObject &obj = *((OneObject*)ref);
  LETableReference data((const le_uint8*)ref, sizeof(OneObject));
  LEErrorCode success = LE_NO_ERROR;

  LEReferenceTo<OneObject> stuff(data, success);
  if(LE_FAILURE(success)) {
    puts("failure");
    abort();
  }
  long *items2 = ((OneObject*)ref)->items;
  for(int i=0;i<ITEM_COUNT;i++) {
    if(items[i]==2) {
      return;
    }
  }
  puts("error");
  abort();
}


void time_letr2(void * ref) {
  OneObject &obj = *((OneObject*)ref);
  LETableReference data((const le_uint8*)ref, sizeof(OneObject));
  LEErrorCode success = LE_NO_ERROR;

  long *items2 = ((OneObject*)ref)->items;
  for(int i=0;i<ITEM_COUNT;i++) {
    LEReferenceTo<OneObject> stuff(data, success);
    if(LE_FAILURE(success)) {
      puts("failure");
      abort();
    }
    if(items[i]==2) {
      return;
    }
  }
  puts("error");
  abort();
}

static void time_letr3(void * ref) {
  LETableReference data((const le_uint8*)ref, sizeof(OneObject));
  LEErrorCode success = LE_NO_ERROR;
  LEReferenceTo<CompObject> comp(data, success);  
  LEReferenceToArrayOf<Long> longs(comp, success, (size_t)0, ITEM_COUNT);
  if(LE_FAILURE(success)) {
    puts("failure");
    abort();
  }

  for(int i=0;i<ITEM_COUNT;i++) {
    const Long &item = longs.getObject(i, success);
    if(LE_FAILURE(success)) {
      puts("failure");
      abort();
    }
    if(item.v==2) {
      return;
    }
  }
  puts("error");
  abort();
}


int main() {
  double runTime = 2.0;
  printf("Test of LETableReference<> timing. %.1fs per run.\n", runTime);
  items = new long[ITEM_COUNT];
  OneObject *oo = new OneObject();
  CompObject *oo2 = new CompObject();
  for(int i=0;i<ITEM_COUNT-1;i++) {
    items[i] = oo->items[i] = oo2->items[i].v = (i%1024)+3;
  }
  items[ITEM_COUNT-1] = oo->items[ITEM_COUNT-1] = oo2->items[ITEM_COUNT-1].v = 2; // last one

  puts("will call once..");
  time_letr3((void*)oo2);
  puts("testing all..");
 
  int32_t loopCount;
  double time_taken;

#define showTime(x,y)  printf("%s:\ttesting...\r",  #x);   fflush(stdout); \
  time_taken = utimer_loopUntilDone(runTime, &loopCount, x, y); \
  printf("%s:\t%.1fs\t#%d\t%.1f/s\n", #x, time_taken, loopCount, loopCount/(double)time_taken);

  // clear out cache
  {
    double oldTime = runTime;
    runTime = 0.25;
    showTime(time_null, NULL); 
    showTime(time_null, NULL); 
    showTime(time_null, NULL); 
    showTime(time_null, NULL); 
    runTime = oldTime;
  }
  puts("-- ready to start --");


  showTime(time_null, NULL); 
  showTime(time_obj, (void*)oo);
  showTime(time_obj2, (void*)oo);
  showTime(time_letr1, (void*)oo2);
  showTime(time_letr2, (void*)oo2);
  showTime(time_letr3, (void*)oo2);
  showTime(time_null, NULL);
  
  delete [] items;
  delete oo;
  delete oo2;
}
