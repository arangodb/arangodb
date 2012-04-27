
#include <BasicsC/common.h>
#include <BasicsC/strings.h>

#include "Ahuacatl/ast-node.h"
#include "Ahuacatl/parser.h"
#include "Ahuacatl/ast-dump.h"
#include "Ahuacatl/ast-codegen-js.h"
#include "Ahuacatl/grammar.h"

char* TRI_ParseGetErrorMessage(const char* const query, const size_t line, const size_t column) {
  size_t currentLine = 1;
  size_t currentColumn = 1;
  const char* p = query;
  size_t offset;
  char c;
 
  while ((c = *p++)) {
    if (c == '\n') {
      ++currentLine;
      currentColumn = 0;
    }
    else if (c == '\r') {
      if (*p == '\n') {
        ++currentLine;
        currentColumn = 0;
        p++;
      }
    }

    ++currentColumn;

    if (currentLine >= line && currentColumn >= column) {
      break;
    }
  }

  offset = p - query;
  if (strlen(query) < offset + 32) {
    return TRI_DuplicateString2(query + offset, strlen(query) - offset);
  }

  return TRI_Concatenate2String(TRI_DuplicateString2(query + offset, 32), "...");
}

int main(int argc, char* argv[]) {
  TRI_aql_parse_context_t* context;
  
  if (argc <= 1) {
    printf("please specify a query in argv[1]\n");
    return 1;
  } 
 
  context = TRI_CreateParseContextAql(argv[1]); 
  if (!context) {
    printf("error\n");
    return 1;
  }

  if (Ahuacatlparse(context)) {
    printf("error\n");
  }

  if (context->_first) {
    TRI_DumpAql((TRI_aql_node_t*) context->_first);
    TRI_GenerateCodeAql((TRI_aql_node_t*) context->_first);
  }

  TRI_FreeParseContextAql(context);

  return 0;
}
