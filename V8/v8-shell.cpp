////////////////////////////////////////////////////////////////////////////////
/// @brief V8 shell functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "v8-shell.h"

#define TRI_WITHIN_C
#include <Basics/conversions.h>
#include <Basics/csv.h>
#include <Basics/logging.h>
#include <Basics/string-buffer.h>
#include <Basics/strings.h>

#include "ShapedJson/shaped-json.h"
#undef TRI_WITHIN_C

#include <fstream>

#include "V8/v8-json.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"

using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                                   IMPORT / EXPORT
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

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

static void ProcessCsvAdd (TRI_csv_parser_t* parser, char const* field, size_t row, size_t column) {
  v8::Handle<v8::Array>* array = reinterpret_cast<v8::Handle<v8::Array>*>(parser->_dataBegin);

  (*array)->Set(column, v8::String::New(field));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ends a CSV line
////////////////////////////////////////////////////////////////////////////////

static void ProcessCsvEnd (TRI_csv_parser_t* parser, char const* field, size_t row, size_t column) {
  v8::Handle<v8::Array>* array = reinterpret_cast<v8::Handle<v8::Array>*>(parser->_dataBegin);

  (*array)->Set(column, v8::String::New(field));

  v8::Handle<v8::Function>* cb = reinterpret_cast<v8::Handle<v8::Function>*>(parser->_dataEnd);
  v8::Handle<v8::Number> r = v8::Number::New(row);

  v8::Handle<v8::Value> args[] = { *array, r };
  (*cb)->Call(*cb, 2, args);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                      JS functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief processes a CSV file
///
/// @FUN{processCsvFile(@FA{filename}, @FA{callback})}
///
/// Processes a CSV file. The @FA{callback} function is called for line in the
/// file. The seperator is @CODE{\,} and the quote is @CODE{"}.
///
/// Create the input file @CODE{csv.txt}
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
/// @CODE{seperator} sets the seperator character and @CODE{quote} the quote
/// character.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ProcessCsvFile (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 2) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: processCsvFile(<filename>, <callback>[, <options>])")));
  }

  // extract the filename
  v8::String::Utf8Value filename(argv[0]);

  if (*filename == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("<filename> must be an UTF8 filename")));
  }

  // extract the callback
  v8::Handle<v8::Function> cb = v8::Handle<v8::Function>::Cast(argv[1]);

  // extract the options
  v8::Handle<v8::String> separatorKey = v8::String::New("separator");
  v8::Handle<v8::String> quoteKey = v8::String::New("quote");

  char separator = ',';
  char quote = '"';

  if (3 <= argv.Length()) {
    v8::Handle<v8::Object> options = argv[2]->ToObject();
    bool error;

    // separator
    if (options->Has(separatorKey)) {
      separator = TRI_ObjectToCharacter(options->Get(separatorKey), error);

      if (error) {
        return scope.Close(v8::ThrowException(v8::String::New("<options>.separator must be a character")));
      }
    }

    // quote
    if (options->Has(quoteKey)) {
      quote = TRI_ObjectToCharacter(options->Get(quoteKey), error);

      if (error) {
        return scope.Close(v8::ThrowException(v8::String::New("<options>.quote must be a character")));
      }
    }
  }

  // read and convert
  int fd = open(*filename, O_RDONLY);

  if (fd < 0) {
    return scope.Close(v8::ThrowException(v8::String::New(TRI_LAST_ERROR_STR)));
  }

  TRI_csv_parser_t parser;

  TRI_InitCsvParser(&parser,
                    ProcessCsvBegin,
                    ProcessCsvAdd,
                    ProcessCsvEnd);

  parser._separator = separator;
  parser._quote = quote;

  parser._dataEnd = &cb;

  v8::Handle<v8::Array> array;
  parser._dataBegin = &array;

  char buffer[10240];

  while (true) {
    v8::HandleScope scope;

    ssize_t n = read(fd, buffer, sizeof(buffer));

    if (n < 0) {
      TRI_DestroyCsvParser(&parser);
      return scope.Close(v8::ThrowException(v8::String::New(TRI_LAST_ERROR_STR)));
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
/// Create the input file @CODE{json.txt}
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
    return scope.Close(v8::ThrowException(v8::String::New("usage: processJsonFile(<filename>, <callback>)")));
  }

  // extract the filename
  v8::String::Utf8Value filename(argv[0]);

  if (*filename == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("<filename> must be an UTF8 filename")));
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

      char const* ptr = line.c_str();
      char const* end = ptr + line.length();

      while (ptr < end && (*ptr == ' ' || *ptr == '\t' || *ptr == '\r')) {
        ++ptr;
      }

      if (ptr == end) {
        continue;
      }

      char* error;
      v8::Handle<v8::Value> object = TRI_FromJsonString(line.c_str(), &error);

      if (object->IsUndefined()) {
        v8::Handle<v8::String> msg = v8::String::New(error);
        TRI_FreeString(error);
        return scope.Close(v8::ThrowException(msg));
      }

      v8::Handle<v8::Number> r = v8::Number::New(row);
      v8::Handle<v8::Value> args[] = { object, r };
      cb->Call(cb, 2, args);

      row++;
    }

    file.close();
  }
  else {
    return scope.Close(v8::ThrowException(v8::String::New("cannot open file")));
  }

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                           GENERAL
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      JS functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief changes the log-level
///
/// @FUN{logLevel()}
///
/// Returns the current log-level as string.
///
/// @verbinclude fluent37
///
/// @FUN{logLevel(@FA{level})}
///
/// Changes the current log-level. Valid log-level are:
///
/// - fatal
/// - error
/// - warning
/// - info
/// - debug
/// - trace
///
/// @verbinclude fluent38
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_LogLevel (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (1 <= argv.Length()) {
    v8::String::Utf8Value str(argv[0]);

    TRI_SetLogLevelLogging(*str);
  }

  return scope.Close(v8::String::New(TRI_LogLevelLogging()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief outputs the arguments
///
/// @FUN{output(@FA{string1}, @FA{string2}, @FA{string3}, ...)}
///
/// Outputs the arguments to standard output.
///
/// @verbinclude fluent39
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Output (v8::Arguments const& argv) {
  for (int i = 0; i < argv.Length(); i++) {
    v8::HandleScope scope;

    // extract the next argument
    v8::Handle<v8::Value> val = argv[i];

    // convert it into a string
    v8::String::Utf8Value utf8(val);

    if (*utf8 == 0) {
      continue;
    }

    // print the argument
    char const* ptr = *utf8;
    size_t len = utf8.length();

    while (0 < len) {
      ssize_t n = write(1, ptr, len);

      if (n < 0) {
        return v8::Undefined();
      }

      len -= n;
      ptr += n;
    }
  }

  return v8::Undefined();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints the arguments
///
/// @FUN{print(@FA{arg1}, @FA{arg2}, @FA{arg3}, ...)}
///
/// Prints the arguments. If an argument is an object having a function print,
/// then this function is called. Otherwise @FN{toJson} is used.  After each
/// argument a newline is printed.
///
/// @verbinclude fluent40
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Print (v8::Arguments const& argv) {
  TRI_v8_global_t* v8g;
  v8::HandleScope globalScope;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  v8::Handle<v8::Function> toJson = v8::Handle<v8::Function>::Cast(
    argv.Holder()->CreationContext()->Global()->Get(v8g->ToJsonFuncName));

  for (int i = 0; i < argv.Length(); i++) {
    v8::HandleScope scope;
    TRI_string_buffer_t buffer;
    bool printBuffer = true;

    TRI_InitStringBuffer(&buffer);

    // extract the next argument
    v8::Handle<v8::Value> val = argv[i];

    // convert the object into json - if possible
    if (val->IsObject()) {
      v8::Handle<v8::Object> obj = val->ToObject();

      if (obj->Has(v8g->PrintFuncName)) {
        v8::Handle<v8::Function> print = v8::Handle<v8::Function>::Cast(obj->Get(v8g->PrintFuncName));
        v8::Handle<v8::Value> args[] = { val };
        print->Call(obj, 1, args);
        printBuffer = false;
      }
      else {
        v8::Handle<v8::Value> args[] = { val };
        v8::Handle<v8::Value> cnv = toJson->Call(toJson, 1, args);

        v8::String::Utf8Value utf8(cnv);

        if (*utf8 == 0) {
          TRI_AppendStringStringBuffer(&buffer, "[object]");
        }
        else {
          TRI_AppendString2StringBuffer(&buffer, *utf8, utf8.length());
        }
      }
    }
    else if (val->IsString()) {
      v8::String::Utf8Value utf8(val->ToString());

      if (*utf8 == 0) {
        TRI_AppendStringStringBuffer(&buffer, "[string]");
      }
      else {
        size_t out;
        char* e = TRI_EscapeCString(*utf8, utf8.length(), &out);

        TRI_AppendCharStringBuffer(&buffer, '"');
        TRI_AppendString2StringBuffer(&buffer, e, out);
        TRI_AppendCharStringBuffer(&buffer, '"');

        TRI_FreeString(e);
      }
    }
    else {
      v8::String::Utf8Value utf8(val);

      if (*utf8 == 0) {
        TRI_AppendStringStringBuffer(&buffer, "[value]");
      }
      else {
        TRI_AppendString2StringBuffer(&buffer, *utf8, utf8.length());
      }
    }

    // print the argument
    if (printBuffer) {
      char const* ptr = TRI_BeginStringBuffer(&buffer);
      size_t len = TRI_LengthStringBuffer(&buffer);

      while (0 < len) {
        ssize_t n = write(1, ptr, len);

        if (n < 0) {
          TRI_DestroyStringBuffer(&buffer);
          return v8::Undefined();
        }

        len -= n;
        ptr += n;
      }
    }

    // print a new line
    ssize_t n = write(1, "\n", 1);

    if (n < 0) {
      TRI_DestroyStringBuffer(&buffer);
      return v8::Undefined();
    }

    TRI_DestroyStringBuffer(&buffer);
  }

  return v8::Undefined();
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief stores the V8 shell functions inside the global variable
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8Shell (v8::Handle<v8::Context> context) {
  v8::HandleScope scope;

  // check the isolate
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  TRI_v8_global_t* v8g = (TRI_v8_global_t*) isolate->GetData();

  if (v8g == 0) {
    v8g = new TRI_v8_global_t;
    isolate->SetData(v8g);
  }

  // .............................................................................
  // global function names
  // .............................................................................

  if (v8g->PrintFuncName.IsEmpty()) {
    v8g->PrintFuncName = v8::Persistent<v8::String>::New(v8::String::New("print"));
  }

  if (v8g->ToJsonFuncName.IsEmpty()) {
    v8g->ToJsonFuncName = v8::Persistent<v8::String>::New(v8::String::New("toJson"));
  }

  // .............................................................................
  // create the global functions
  // .............................................................................

  context->Global()->Set(v8::String::New("logLevel"),
                         v8::FunctionTemplate::New(JS_LogLevel)->GetFunction(),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("output"),
                         v8::FunctionTemplate::New(JS_Output)->GetFunction(),
                         v8::ReadOnly);

  context->Global()->Set(v8g->PrintFuncName,
                         v8::FunctionTemplate::New(JS_Print)->GetFunction(),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("processCsvFile"),
                         v8::FunctionTemplate::New(JS_ProcessCsvFile)->GetFunction(),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("processJsonFile"),
                         v8::FunctionTemplate::New(JS_ProcessJsonFile)->GetFunction(),
                         v8::ReadOnly);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\)"
// End:
