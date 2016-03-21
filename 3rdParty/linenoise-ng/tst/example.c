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

int main (int argc, char** argv) {
  linenoiseInstallWindowChangeHandler();

  while(argc > 1) {
    argc--;
    argv++;
    if (!strcmp(*argv, "--keycodes")) {
      linenoisePrintKeyCodes();
      exit(0);
    }
  }

  const char* file = "./history";

  linenoiseHistoryLoad(file);
  linenoiseSetCompletionCallback(completionHook);

  printf("starting...\n");

  char const* prompt = "\x1b[1;32mlinenoise\x1b[0m> ";

  while (1) {
    char* result = linenoise(prompt);

    if (result == NULL) {
      break;
    } else if (!strncmp(result, "/history", 8)) {
      /* Display the current history. */
      for (int index = 0; ; ++index) {
        char* hist = linenoiseHistoryLine(index);
        if (hist == NULL) break;
        printf("%4d: %s\n", index, hist);
        free(hist);
       }
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
