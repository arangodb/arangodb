////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "v8-shell.h"

#include "ApplicationFeatures/ShellColorsFeature.h"
#include "ApplicationFeatures/V8SecurityFeature.h"
#include "Basics/Exceptions.h"
#include "Basics/Utf8Helper.h"
#include "Basics/csv.h"
#include "Basics/debugging.h"
#include "Basics/error.h"
#include "Basics/operating-system.h"
#include "Basics/tri-strings.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"

#include <velocypack/Builder.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef _WIN32
#include "Basics/win-utils.h"
#endif

using namespace arangodb;

namespace {

////////////////////////////////////////////////////////////////////////////////
/// @brief begins a new CSV line
////////////////////////////////////////////////////////////////////////////////

static void ProcessCsvBegin(TRI_csv_parser_t* parser, size_t row) {
  v8::Isolate* isolate = (v8::Isolate*)parser->_data;
  v8::Handle<v8::Array>* array =
      reinterpret_cast<v8::Handle<v8::Array>*>(parser->_dataBegin);

  (*array) = v8::Array::New(isolate);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new CSV field
////////////////////////////////////////////////////////////////////////////////

static void ProcessCsvAdd(TRI_csv_parser_t* parser, char const* field, size_t,
                          size_t row, size_t column, bool escaped) {
  // cppcheck-suppress *
  v8::Isolate* isolate = (v8::Isolate*)parser->_data;

  v8::Handle<v8::Array>* array =
      reinterpret_cast<v8::Handle<v8::Array>*>(parser->_dataBegin);

  (*array)->Set(TRI_IGETC, (uint32_t)column, TRI_V8_STRING(isolate, field)).FromMaybe(true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ends a CSV line
////////////////////////////////////////////////////////////////////////////////

static void ProcessCsvEnd(TRI_csv_parser_t* parser, char const* field, size_t,
                          size_t row, size_t column, bool escaped) {
  v8::Isolate* isolate = (v8::Isolate*)parser->_data;
  v8::Handle<v8::Array>* array =
      reinterpret_cast<v8::Handle<v8::Array>*>(parser->_dataBegin);

  (*array)->Set(TRI_IGETC, (uint32_t)column, TRI_V8_STRING(isolate, field)).FromMaybe(false);

  v8::Handle<v8::Function>* cb =
      reinterpret_cast<v8::Handle<v8::Function>*>(parser->_dataEnd);
  v8::Handle<v8::Number> r = v8::Integer::New(isolate, (int)row);

  v8::Handle<v8::Value> args[] = {*array, r};
  (*cb)->Call(TRI_IGETC, *cb, 2, args).FromMaybe(v8::Local<v8::Value>());
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
/// @brief processes a CSV file
///
/// @FUN{processCsvFile(@FA{filename}, @FA{callback})}
///
/// Processes a CSV file. The @FA{callback} function is called for line in the
/// file. The separator is @LIT{\,} and the quote is @LIT{"}.
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
/// @LIT{separator} sets the separator character and @LIT{quote} the quote
/// character.
////////////////////////////////////////////////////////////////////////////////

static void JS_ProcessCsvFile(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "processCsvFile(<filename>, <callback>[, <options>])");
  }

  // extract the filename
  TRI_Utf8ValueNFC filename(isolate, args[0]);

  if (*filename == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<filename> must be an UTF8 filename");
  }

  TRI_GET_GLOBALS();
  V8SecurityFeature& v8security = v8g->_server.getFeature<V8SecurityFeature>();

  if (!v8security.isAllowedToAccessPath(isolate, *filename, FSAccessType::READ)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                   "not allowed to read files in this path");
  }

  // extract the callback
  v8::Handle<v8::Function> cb = v8::Handle<v8::Function>::Cast(args[1]);

  // extract the options
  v8::Handle<v8::String> separatorKey =
      TRI_V8_ASCII_STRING(isolate, "separator");
  v8::Handle<v8::String> quoteKey = TRI_V8_ASCII_STRING(isolate, "quote");

  std::string separator = ",";
  std::string quote = "\"";

  if (3 <= args.Length()) {
    v8::Handle<v8::Object> options = TRI_GetObject(context, args[2]);

    // separator
    if (TRI_HasProperty(context, isolate, options, separatorKey)) {
      separator = TRI_ObjectToString(context, isolate, options->Get(context, separatorKey));

      if (separator.size() != 1) {
        TRI_V8_THROW_TYPE_ERROR(
            "<options>.separator must be exactly one character");
      }
    }

    // quote
    if (TRI_HasProperty(context, isolate, options, quoteKey)) {
      quote = TRI_ObjectToString(context, isolate, options->Get(context, quoteKey));

      if (quote.length() > 1) {
        TRI_V8_THROW_TYPE_ERROR(
            "<options>.quote must be at most one character");
      }
    }
  }

  // read and convert
  int fd = TRI_OPEN(*filename, O_RDONLY | TRI_O_CLOEXEC);

  if (fd < 0) {
    TRI_V8_THROW_EXCEPTION_SYS("cannot open file");
  }

  TRI_csv_parser_t parser;

  TRI_InitCsvParser(&parser, ProcessCsvBegin, ProcessCsvAdd, ProcessCsvEnd, isolate);

  TRI_SetSeparatorCsvParser(&parser, separator[0]);
  if (quote.length() > 0) {
    TRI_SetQuoteCsvParser(&parser, quote[0], true);
  } else {
    TRI_SetQuoteCsvParser(&parser, '\0', false);
  }

  parser._dataEnd = &cb;

  v8::Handle<v8::Array> array;
  parser._dataBegin = &array;

  char buffer[10240];

  while (true) {
    TRI_read_return_t n = TRI_READ(fd, buffer, static_cast<TRI_read_t>(sizeof(buffer)));

    if (n < 0) {
      TRI_DestroyCsvParser(&parser);
      TRI_CLOSE(fd);
      TRI_V8_THROW_EXCEPTION_SYS("cannot read file");
    } else if (n == 0) {
      TRI_DestroyCsvParser(&parser);
      break;
    }

    TRI_ParseCsvString(&parser, buffer, n);
  }

  TRI_CLOSE(fd);

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
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

static void JS_ProcessJsonFile(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("processJsonFile(<filename>, <callback>)");
  }

  // extract the filename
  TRI_Utf8ValueNFC filename(isolate, args[0]);

  if (*filename == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<filename> must be an UTF8 filename");
  }

  TRI_GET_GLOBALS();
  V8SecurityFeature& v8security = v8g->_server.getFeature<V8SecurityFeature>();

  if (!v8security.isAllowedToAccessPath(isolate, *filename, FSAccessType::READ)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                   "not allowed to read files in this path");
  }

  // extract the callback
  v8::Handle<v8::Function> cb = v8::Handle<v8::Function>::Cast(args[1]);

  // read and convert
  std::string line;
#ifndef _MSC_VER
  std::ifstream file(*filename);
#else
  std::ifstream file;
  file.open(arangodb::basics::toWString(*filename));
#endif


  auto builder = std::make_shared<VPackBuilder>();
  VPackParser parser(builder);

  if (file.is_open()) {
    size_t row = 0;

    while (file.good()) {
      std::getline(file, line);

      char const* ptr = line.data();
      char const* end = ptr + line.length();

      while (ptr < end && (*ptr == ' ' || *ptr == '\t' || *ptr == '\r')) {
        ++ptr;
      }

      if (ptr == end) {
        continue;
      }

      builder->clear();
      v8::Handle<v8::Value> object;

      try {
        parser.parse(ptr, end - ptr);
        object = TRI_VPackToV8(isolate, builder->slice(), parser.options, nullptr); 
      } catch (std::exception const& ex) {
        TRI_V8_THROW_SYNTAX_ERROR(ex.what());
      }

      v8::Handle<v8::Number> r = v8::Integer::New(isolate, (int)row);
      v8::Handle<v8::Value> args[] = {object, r};
      cb->Call(TRI_IGETC, cb, 2, args).FromMaybe(v8::Local<v8::Value>());

      row++;
    }

    file.close();
  } else {
    TRI_V8_THROW_EXCEPTION_SYS("cannot open file");
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stores the V8 shell functions inside the global variable
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8Shell(v8::Isolate* isolate) {
  v8::HandleScope scope(isolate);

  // .............................................................................
  // create the global functions
  // .............................................................................

  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate,
                                                   "SYS_PROCESS_CSV_FILE"),
                               JS_ProcessCsvFile);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate,
                                                   "SYS_PROCESS_JSON_FILE"),
                               JS_ProcessJsonFile);

  bool isTty = (isatty(STDOUT_FILENO) != 0);
  // on Linux, isatty() == 0 may also indicate an error. we can ignore this
  // safely
  // because if isatty returns an error we should not assume we're printing on a
  // terminal

  // .............................................................................
  // create the global variables
  // .............................................................................

  v8::Handle<v8::Object> colors = v8::Object::New(isolate);

  colors
      ->DefineOwnProperty(TRI_IGETC, TRI_V8_ASCII_STRING(isolate, "COLOR_RED"),
                          isTty ? TRI_V8_ASCII_STRING(isolate, ShellColorsFeature::SHELL_COLOR_RED)
                                : v8::String::Empty(isolate),
                          v8::ReadOnly)
      .FromMaybe(false);

  colors
      ->DefineOwnProperty(TRI_IGETC,
                          TRI_V8_ASCII_STRING(isolate, "COLOR_BOLD_RED"),
                          isTty ? TRI_V8_ASCII_STRING(isolate, ShellColorsFeature::SHELL_COLOR_BOLD_RED)
                                : v8::String::Empty(isolate),
                          v8::ReadOnly)
      .FromMaybe(false);

  colors
      ->DefineOwnProperty(TRI_IGETC, TRI_V8_ASCII_STRING(isolate, "COLOR_GREEN"),
                          isTty ? TRI_V8_ASCII_STRING(isolate, ShellColorsFeature::SHELL_COLOR_GREEN)
                                : v8::String::Empty(isolate),
                          v8::ReadOnly)
      .FromMaybe(false);

  colors
      ->DefineOwnProperty(TRI_IGETC,
                          TRI_V8_ASCII_STRING(isolate, "COLOR_BOLD_GREEN"),
                          isTty ? TRI_V8_ASCII_STRING(isolate, ShellColorsFeature::SHELL_COLOR_BOLD_GREEN)
                                : v8::String::Empty(isolate),
                          v8::ReadOnly)
      .FromMaybe(false);

  colors
      ->DefineOwnProperty(TRI_IGETC, TRI_V8_ASCII_STRING(isolate, "COLOR_BLUE"),
                          isTty ? TRI_V8_ASCII_STRING(isolate, ShellColorsFeature::SHELL_COLOR_BLUE)
                                : v8::String::Empty(isolate),
                          v8::ReadOnly)
      .FromMaybe(false);

  colors
      ->DefineOwnProperty(TRI_IGETC,
                          TRI_V8_ASCII_STRING(isolate, "COLOR_BOLD_BLUE"),
                          isTty ? TRI_V8_ASCII_STRING(isolate, ShellColorsFeature::SHELL_COLOR_BOLD_BLUE)
                                : v8::String::Empty(isolate),
                          v8::ReadOnly)
      .FromMaybe(false);

  colors
      ->DefineOwnProperty(TRI_IGETC,
                          TRI_V8_ASCII_STRING(isolate, "COLOR_YELLOW"),
                          isTty ? TRI_V8_ASCII_STRING(isolate, ShellColorsFeature::SHELL_COLOR_YELLOW)
                                : v8::String::Empty(isolate),
                          v8::ReadOnly)
      .FromMaybe(false);

  colors
      ->DefineOwnProperty(TRI_IGETC,
                          TRI_V8_ASCII_STRING(isolate, "COLOR_BOLD_YELLOW"),
                          isTty ? TRI_V8_ASCII_STRING(isolate, ShellColorsFeature::SHELL_COLOR_BOLD_YELLOW)
                                : v8::String::Empty(isolate),
                          v8::ReadOnly)
      .FromMaybe(false);

  colors
      ->DefineOwnProperty(TRI_IGETC, TRI_V8_ASCII_STRING(isolate, "COLOR_WHITE"),
                          isTty ? TRI_V8_ASCII_STRING(isolate, ShellColorsFeature::SHELL_COLOR_WHITE)
                                : v8::String::Empty(isolate),
                          v8::ReadOnly)
      .FromMaybe(false);

  colors
      ->DefineOwnProperty(TRI_IGETC,
                          TRI_V8_ASCII_STRING(isolate, "COLOR_BOLD_WHITE"),
                          isTty ? TRI_V8_ASCII_STRING(isolate, ShellColorsFeature::SHELL_COLOR_BOLD_WHITE)
                                : v8::String::Empty(isolate),
                          v8::ReadOnly)
      .FromMaybe(false);

  colors
      ->DefineOwnProperty(TRI_IGETC, TRI_V8_ASCII_STRING(isolate, "COLOR_CYAN"),
                          isTty ? TRI_V8_ASCII_STRING(isolate, ShellColorsFeature::SHELL_COLOR_CYAN)
                                : v8::String::Empty(isolate),
                          v8::ReadOnly)
      .FromMaybe(false);

  colors
      ->DefineOwnProperty(TRI_IGETC,
                          TRI_V8_ASCII_STRING(isolate, "COLOR_BOLD_CYAN"),
                          isTty ? TRI_V8_ASCII_STRING(isolate, ShellColorsFeature::SHELL_COLOR_BOLD_CYAN)
                                : v8::String::Empty(isolate),
                          v8::ReadOnly)
      .FromMaybe(false);

  colors
      ->DefineOwnProperty(TRI_IGETC,
                          TRI_V8_ASCII_STRING(isolate, "COLOR_MAGENTA"),
                          isTty ? TRI_V8_ASCII_STRING(isolate, ShellColorsFeature::SHELL_COLOR_MAGENTA)
                                : v8::String::Empty(isolate),
                          v8::ReadOnly)
      .FromMaybe(false);

  colors
      ->DefineOwnProperty(TRI_IGETC,
                          TRI_V8_ASCII_STRING(isolate, "COLOR_BOLD_MAGENTA"),
                          isTty ? TRI_V8_ASCII_STRING(isolate, ShellColorsFeature::SHELL_COLOR_BOLD_MAGENTA)
                                : v8::String::Empty(isolate),
                          v8::ReadOnly)
      .FromMaybe(false);

  colors
      ->DefineOwnProperty(TRI_IGETC, TRI_V8_ASCII_STRING(isolate, "COLOR_BLACK"),
                          isTty ? TRI_V8_ASCII_STRING(isolate, ShellColorsFeature::SHELL_COLOR_BLACK)
                                : v8::String::Empty(isolate),
                          v8::ReadOnly)
      .FromMaybe(false);

  colors
      ->DefineOwnProperty(TRI_IGETC,
                          TRI_V8_ASCII_STRING(isolate, "COLOR_BOLD_BLACK"),
                          isTty ? TRI_V8_ASCII_STRING(isolate, ShellColorsFeature::SHELL_COLOR_BOLD_BLACK)
                                : v8::String::Empty(isolate),
                          v8::ReadOnly)
      .FromMaybe(false);

  colors
      ->DefineOwnProperty(TRI_IGETC, TRI_V8_ASCII_STRING(isolate, "COLOR_BLINK"),
                          isTty ? TRI_V8_ASCII_STRING(isolate, ShellColorsFeature::SHELL_COLOR_BLINK)
                                : v8::String::Empty(isolate),
                          v8::ReadOnly)
      .FromMaybe(false);

  colors
      ->DefineOwnProperty(TRI_IGETC,
                          TRI_V8_ASCII_STRING(isolate, "COLOR_BRIGHT"),
                          isTty ? TRI_V8_ASCII_STRING(isolate, ShellColorsFeature::SHELL_COLOR_BRIGHT)
                                : v8::String::Empty(isolate),
                          v8::ReadOnly)
      .FromMaybe(false);

  colors
      ->DefineOwnProperty(TRI_IGETC, TRI_V8_ASCII_STRING(isolate, "COLOR_RESET"),
                          isTty ? TRI_V8_ASCII_STRING(isolate, ShellColorsFeature::SHELL_COLOR_RESET)
                                : v8::String::Empty(isolate),
                          v8::ReadOnly)
      .FromMaybe(false);

  TRI_AddGlobalVariableVocbase(isolate, TRI_V8_ASCII_STRING(isolate, "COLORS"), colors);
}
