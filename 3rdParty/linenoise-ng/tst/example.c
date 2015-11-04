#include "linenoise.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char* examples[] = {
  "db", "hello", "hallo", "hans", "hansekogge", "seamann", "quetzalcoatl", "quit", "power", NULL
};

void completionHook (char const* prefix, linenoiseCompletions* lc) {
  size_t i;

  for (i = 0;  examples[i] != NULL; ++i) {
    if (strncmp(prefix, examples[i], strlen(prefix)) == 0) {
      linenoiseAddCompletion(lc, examples[i]);
    }
  }
}

int main () {
  const char* file = "./history";

  linenoiseHistoryLoad(file);
  linenoiseSetCompletionCallback(completionHook);

  printf("starting...\n");

  char const* prompt = "linenoise> ";

  while (1) {
    char* result = linenoise(prompt);

    if (result == NULL) {
      break;
    }
    if (*result == '\0') {
      free(result);
      break;
    }

    printf("thanks for the input.\n");
    linenoiseHistoryAdd(result);
    free(result);
  }
    
  linenoiseHistorySave(file);
  linenoiseHistoryFree();
}
