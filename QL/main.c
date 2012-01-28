
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <BasicsC/common.h>

#include "ast-node.h"
#include "formatter.h"
#include "parser-context.h"
#include "optimize.h"

#define MAX_BUFFER_SIZE 16384


void fail (const char *msg) {
  printf("Fatal error: %s\n",msg);
  exit(1);
}


int main (int argc,char **argv) {
  char *buffer;
  char *query;
  QL_parser_context_t *context;
  QL_formatter_t *formatter;
  QL_ast_query_collection_t *collection;
  FILE *file;
  size_t i;

  formatter = (QL_formatter_t*) TRI_Allocate(sizeof(QL_formatter_t));
  if (formatter == NULL) {
    fail("Could not allocate memory for formatter");
  }
  
  context=(QL_parser_context_t*) TRI_Allocate(sizeof(QL_parser_context_t));
  if (context == NULL) {
    fail("Could not allocate memory for context");
  }

  buffer=(char *) TRI_Allocate(sizeof(char)*MAX_BUFFER_SIZE);
  if (buffer == NULL) {
    fail("Could not allocate memory for buffer");
  }


  file = fopen("queries","rb");
  if (file == NULL) {
    fail("Query file not found");
  }

  while (fgets(buffer, MAX_BUFFER_SIZE, file)!=NULL) {
    query = buffer;
    while (*query == ' ' || *query == '\r' || *query == '\n' || *query == '\t') {
      // remove leading whitespace
      query++;
    }
    if (*query == '\0') {
      // ignore empty strings
      continue;
    }

    printf("Testing query:\n-----------------------------------------------------------------------------------------------------------\n%s\n",query);

    if (!QLParseInit(context, query)) {
      fail("Could not initialize parsing context");
    }

    if (!QLparse(context)) {
      printf("PARSE RESULT: OK\n\n");

      printf("\nSELECT PART:\n");
      QLOptimize(context->_query->_select._base);
      QLOptimizeQueryGetSelectType(context->_query);
      QLFormatterDump(context->_query->_select._base,formatter,0);


      printf("\nFROM CLAUSE:\n");
      printf("COLLECTIONS USED:\n");

      for (i=0; i < context->_query->_from._collections._nrAlloc; i++) {
        collection = context->_query->_from._collections._table[i];
        if (collection != 0) {
          printf("Name: %s, Alias: %s, Primary: %s\n", collection->_name, collection->_alias, collection->_isPrimary ? "yes" : "no");
        }
      }

      QLFormatterDump(context->_query->_from._base,formatter,0);
      

      printf("\nWHERE CONDITION:\n");
      QLOptimize(context->_query->_where._base);
      context->_query->_where._type = QLOptimizeGetWhereType(context->_query);

      if (context->_query->_where._type == QLQueryWhereTypeAlwaysTrue) {
        printf("WHERE is always true and can be ignored\n");
      } 
      else if (context->_query->_where._type == QLQueryWhereTypeAlwaysFalse) {
        printf("WHERE is always false and can be ignored\n");
      } 
      else {
        printf("WHERE must be evaluated for each record\n");
        QLFormatterDump(context->_query->_where._base,formatter,0);
      }

      printf("\nORDER:\n");
      if (context->_query->_order._base == 0) {
        printf("NO ORDER BY USED\n");
      } 
      else {
        printf("USING ORDER BY:\n");
        QLFormatterDump(context->_query->_order._base,formatter,0);
      }

      printf("\nLIMIT:\n");
      if (context->_query->_limit._isUsed) {
        printf("USING LIMIT %lu, %lu\n", context->_query->_limit._offset, context->_query->_limit._count);
      } 
      else {
        printf("NO LIMIT USED\n");
      }
    } 
    else { 
      QL_error_state_t *errorState = &context->_lexState._errorState;
      printf("PARSE ERROR AT %u:%u: %s\n",errorState->_line, errorState->_column, errorState->_message);
    }

    QLParseFree(context);
    printf("\n\n");
  }

  fclose(file);
  TRI_Free(buffer);
  TRI_Free(context);
  TRI_Free(formatter);

  return 0;
}
