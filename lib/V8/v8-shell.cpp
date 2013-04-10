////////////////////////////////////////////////////////////////////////////////
/// @brief V8 shell functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "v8-shell.h"

#include "BasicsC/conversions.h"
#include "BasicsC/csv.h"
#include "BasicsC/logging.h"
#include "BasicsC/shell-colors.h"
#include "BasicsC/string-buffer.h"
#include "BasicsC/tri-strings.h"
#include "ShapedJson/shaped-json.h"

#include <fstream>

#include "V8/v8-globals.h"
#include "V8/v8-conv.h"
#include "V8/v8-json.h"
#include "V8/v8-utils.h"

using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                                   IMPORT / EXPORT
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief begins a new CSV line
////////////////////////////////////////////////////////////////////////////////

static void ProcessCsvBegin (TRI_csv_parser_t* parser, size_t row) {
  v8::Handle<v8::Array>* array = reinterpret_cast<v8::Handle<v8::Array>*>(parser->_dataBegin);

  (*array) = v8::Array::New();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new CSV field
////////////////////////////////////////////////////////////////////////////////

static void ProcessCsvAdd (TRI_csv_parser_t* parser, const char* field, size_t row, size_t column, bool escaped) {
  v8::Handle<v8::Array>* array = reinterpret_cast<v8::Handle<v8::Array>*>(parser->_dataBegin);

  (*array)->Set(column, v8::String::New(field));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ends a CSV line
////////////////////////////////////////////////////////////////////////////////

static void ProcessCsvEnd (TRI_csv_parser_t* parser, const char* field, size_t row, size_t column, bool escaped) {
  v8::Handle<v8::Array>* array = reinterpret_cast<v8::Handle<v8::Array>*>(parser->_dataBegin);

  (*array)->Set(column, v8::String::New(field));

  v8::Handle<v8::Function>* cb = reinterpret_cast<v8::Handle<v8::Function>*>(parser->_dataEnd);
  v8::Handle<v8::Number> r = v8::Number::New(row);

  v8::Handle<v8::Value> args[] = { *array, r };
  (*cb)->Call(*cb, 2, args);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                      JS functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief processes a CSV file
///
/// @FUN{processCsvFile(@FA{filename}, @FA{callback})}
///
/// Processes a CSV file. The @FA{callback} function is called for line in the
/// file. The seperator is @LIT{\,} and the quote is @LIT{"}.
///
/// Create the input file @LIT{csv.txt}
///
/// @verbinclude fluent48
///
/// If you use @FN{processCsvFile} on this file, you get
///
/// @verbinclude fluent47
///
/// @FUN{processCsvFile(@FA{filename}, @FA{callback}, @FA{options})}
///
/// Processes a CSV file. The @FA{callback} function is called for line in the
/// file. The @FA{options} argument must be an object. The value of
/// @LIT{seperator} sets the seperator character and @LIT{quote} the quote
/// character.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ProcessCsvFile (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 2) {
    TRI_V8_EXCEPTION_USAGE(scope, "processCsvFile(<filename>, <callback>[, <options>])");
  }

  // extract the filename
  TRI_Utf8ValueNFC filename(TRI_UNKNOWN_MEM_ZONE, argv[0]);

  if (*filename == 0) {
    TRI_V8_TYPE_ERROR(scope, "<filename> must be an UTF8 filename");
  }

  // extract the callback
  v8::Handle<v8::Function> cb = v8::Handle<v8::Function>::Cast(argv[1]);

  // extract the options
  v8::Handle<v8::String> separatorKey = v8::String::New("separator");
  v8::Handle<v8::String> quoteKey = v8::String::New("quote");

  string separator = ",";
  string quote = "\"";

  if (3 <= argv.Length()) {
    v8::Handle<v8::Object> options = argv[2]->ToObject();

    // separator
    if (options->Has(separatorKey)) {
      separator = TRI_ObjectToString(options->Get(separatorKey));

      if (separator.size() != 1) {
        TRI_V8_TYPE_ERROR(scope, "<options>.separator must be exactly one character");
      }
    }

    // quote
    if (options->Has(quoteKey)) {
      quote = TRI_ObjectToString(options->Get(quoteKey));

      if (quote.length() > 1) {
        TRI_V8_TYPE_ERROR(scope, "<options>.quote must be at most one character");
      }
    }
  }

  // read and convert
  int fd = TRI_OPEN(*filename, O_RDONLY);

  if (fd < 0) {
    TRI_V8_EXCEPTION_SYS(scope, "cannot open file");
  }

  TRI_csv_parser_t parser;

  TRI_InitCsvParser(&parser,
                    TRI_UNKNOWN_MEM_ZONE,
                    ProcessCsvBegin,
                    ProcessCsvAdd,
                    ProcessCsvEnd);

  TRI_SetSeparatorCsvParser(&parser, separator[0]);
  if (quote.length() > 0) {
    TRI_SetQuoteCsvParser(&parser, quote[0], true);
  }
  else {
    TRI_SetQuoteCsvParser(&parser, '\0', false);
  }

  parser._dataEnd = &cb;

  v8::Handle<v8::Array> array;
  parser._dataBegin = &array;

  char buffer[10240];

  while (true) {
    v8::HandleScope scope;

    ssize_t n = TRI_READ(fd, buffer, sizeof(buffer));

    if (n < 0) {
      TRI_DestroyCsvParser(&parser);
      TRI_V8_EXCEPTION_SYS(scope, "cannot read file");
    }
    else if (n == 0) {
      TRI_DestroyCsvParser(&parser);
      break;
    }

    TRI_ParseCsvString2(&parser, buffer, n);
  }

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief processes a JSON file
///
/// @FUN{processJsonFile(@FA{filename}, @FA{callback})}
///
/// Processes a JSON file. The file must contain the JSON objects each on its
/// own line. The @FA{callback} function is called for each object.
///
/// Create the input file @LIT{json.txt}
///
/// @verbinclude fluent49
///
/// If you use @FN{processJsonFile} on this file, you get
///
/// @verbinclude fluent50
///
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ProcessJsonFile (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 2) {
    TRI_V8_EXCEPTION_USAGE(scope, "processJsonFile(<filename>, <callback>)");
  }

  // extract the filename
  TRI_Utf8ValueNFC filename(TRI_UNKNOWN_MEM_ZONE, argv[0]);

  if (*filename == 0) {
    TRI_V8_TYPE_ERROR(scope, "<filename> must be an UTF8 filename");
  }

  // extract the callback
  v8::Handle<v8::Function> cb = v8::Handle<v8::Function>::Cast(argv[1]);

  // read and convert
  string line;
  ifstream file(*filename);

  if (file.is_open()) {
    size_t row = 0;

    while (file.good()) {
      v8::HandleScope scope;

      getline(file, line);

      const char* ptr = line.c_str();
      const char* end = ptr + line.length();

      while (ptr < end && (*ptr == ' ' || *ptr == '\t' || *ptr == '\r')) {
        ++ptr;
      }

      if (ptr == end) {
        continue;
      }

      char* error;
      v8::Handle<v8::Value> object = TRI_FromJsonString(line.c_str(), &error);

      if (object->IsUndefined()) {
        string msg = error;
        TRI_FreeString(TRI_CORE_MEM_ZONE, error);

        TRI_V8_SYNTAX_ERROR(scope, msg.c_str());
      }

      v8::Handle<v8::Number> r = v8::Number::New(row);
      v8::Handle<v8::Value> args[] = { object, r };
      cb->Call(cb, 2, args);

      row++;
    }

    file.close();
  }
  else {
    TRI_V8_EXCEPTION_SYS(scope, "cannot open file");
  }

  return scope.Close(v8::Undefined());
}

// -----------------------------------------------------------------------------
// --SECTION--                                                           GENERAL
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief stores the V8 shell functions inside the global variable
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8Shell (v8::Handle<v8::Context> context) {
  v8::HandleScope scope;

  v8::Isolate* isolate = v8::Isolate::GetCurrent();

  // .............................................................................
  // create the global functions
  // .............................................................................

  TRI_AddGlobalFunctionVocbase(context, "SYS_PROCESS_CSV_FILE", JS_ProcessCsvFile);
  TRI_AddGlobalFunctionVocbase(context, "SYS_PROCESS_JSON_FILE", JS_ProcessJsonFile);

  // .............................................................................
  // create the global variables
  // .............................................................................

  v8::Handle<v8::Object> colors = v8::Object::New();

  colors->Set(TRI_V8_STRING("COLOR_RED"),
              TRI_V8_STRING(TRI_SHELL_COLOR_RED),
              v8::ReadOnly);

  colors->Set(TRI_V8_STRING("COLOR_BOLD_RED"),
              TRI_V8_STRING(TRI_SHELL_COLOR_BOLD_RED),
              v8::ReadOnly);

  colors->Set(TRI_V8_STRING("COLOR_GREEN"),
              TRI_V8_STRING(TRI_SHELL_COLOR_GREEN),
              v8::ReadOnly);

  colors->Set(TRI_V8_STRING("COLOR_BOLD_GREEN"),
              TRI_V8_STRING(TRI_SHELL_COLOR_BOLD_GREEN),
              v8::ReadOnly);

  colors->Set(TRI_V8_STRING("COLOR_BLUE"),
              TRI_V8_STRING(TRI_SHELL_COLOR_BLUE),
              v8::ReadOnly);

  colors->Set(TRI_V8_STRING("COLOR_BOLD_BLUE"),
              TRI_V8_STRING(TRI_SHELL_COLOR_BOLD_BLUE),
              v8::ReadOnly);

  colors->Set(TRI_V8_STRING("COLOR_YELLOW"),
              TRI_V8_STRING(TRI_SHELL_COLOR_YELLOW),
              v8::ReadOnly);

  colors->Set(TRI_V8_STRING("COLOR_BOLD_YELLOW"),
              TRI_V8_STRING(TRI_SHELL_COLOR_BOLD_YELLOW),
              v8::ReadOnly);

  colors->Set(TRI_V8_STRING("COLOR_WHITE"),
              TRI_V8_STRING(TRI_SHELL_COLOR_WHITE),
              v8::ReadOnly);

  colors->Set(TRI_V8_STRING("COLOR_BOLD_WHITE"),
              TRI_V8_STRING(TRI_SHELL_COLOR_BOLD_WHITE),
              v8::ReadOnly);

  colors->Set(TRI_V8_STRING("COLOR_CYAN"),
              TRI_V8_STRING(TRI_SHELL_COLOR_CYAN),
              v8::ReadOnly);

  colors->Set(TRI_V8_STRING("COLOR_BOLD_CYAN"),
              TRI_V8_STRING(TRI_SHELL_COLOR_BOLD_CYAN),
              v8::ReadOnly);

  colors->Set(TRI_V8_STRING("COLOR_MAGENTA"),
              TRI_V8_STRING(TRI_SHELL_COLOR_MAGENTA),
              v8::ReadOnly);

  colors->Set(TRI_V8_STRING("COLOR_BOLD_MAGENTA"),
              TRI_V8_STRING(TRI_SHELL_COLOR_BOLD_MAGENTA),
              v8::ReadOnly);

  colors->Set(TRI_V8_STRING("COLOR_BLACK"),
              TRI_V8_STRING(TRI_SHELL_COLOR_BLACK),
              v8::ReadOnly);

  colors->Set(TRI_V8_STRING("COLOR_BOLD_BLACK"),
              TRI_V8_STRING(TRI_SHELL_COLOR_BOLD_BLACK),
              v8::ReadOnly);

  colors->Set(TRI_V8_STRING("COLOR_BLINK"),
              TRI_V8_STRING(TRI_SHELL_COLOR_BLINK),
              v8::ReadOnly);

  colors->Set(TRI_V8_STRING("COLOR_BRIGHT"),
              TRI_V8_STRING(TRI_SHELL_COLOR_BRIGHT),
              v8::ReadOnly);

  colors->Set(TRI_V8_STRING("COLOR_RESET"),
              TRI_V8_STRING(TRI_SHELL_COLOR_RESET),
              v8::ReadOnly);

  TRI_AddGlobalVariableVocbase(context, "COLORS", colors);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
