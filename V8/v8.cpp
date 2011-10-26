// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8-vocbase.h"

#include <readline/readline.h> // NOLINT
#include <readline/history.h> // NOLINT

#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <sstream>

extern "C" {
#define TRI_WITHIN_C
#include <Basics/logging.h>
#include <VocBase/vocbase.h>
#include <VocBase/document-collection.h>
#undef TRI_WITHIN_C
}

#ifdef COMPRESS_STARTUP_DATA_BZ2
#error Using compressed startup data is not supported for this sample
#endif

#if RL_READLINE_VERSION >= 0x0500
#define completion_matches rl_completion_matches
#endif

using namespace v8;

/**
 * This sample program shows how to implement a simple javascript shell
 * based on V8.  This includes initializing V8 with command line options,
 * creating global functions, compiling and executing strings.
 *
 * For a more sophisticated shell, consider using the debug shell D8.
 */

////////////////////////////////////////////////////////////////////////////////
/// @brief "print" function name
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> PrintFuncName;

////////////////////////////////////////////////////////////////////////////////
/// @brief "last" variable name
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> LastVariableName;




v8::Handle<v8::ObjectTemplate> CreateShellContext();
void RunShell(v8::Handle<v8::Context> context);
int RunMain(v8::Handle<v8::Context> context, int argc, char* argv[]);
bool ExecuteString(v8::Handle<v8::Context> context,
                   v8::Handle<v8::String> source,
                   v8::Handle<v8::Value> name,
                   bool print_result,
                   bool report_exceptions);
v8::Handle<v8::Value> Read(const v8::Arguments& args);
v8::Handle<v8::Value> Quit(const v8::Arguments& args);
v8::Handle<v8::Value> Version(const v8::Arguments& args);
v8::Handle<v8::String> ReadFile(const char* name);
void ReportException(v8::TryCatch* handler);


static bool run_shell;


static TRI_vocbase_t* VocBase;









class LineEditor {
 public:
  enum Type { DUMB = 0, READLINE = 1 };
  LineEditor(Type type, const char* name);
  virtual ~LineEditor() { }

  virtual char* Prompt(const char* prompt) = 0;
  virtual bool Open() { return true; }
  virtual bool Close() { return true; }
  virtual void AddHistory(const char* str) { }

  const char* name() { return name_; }
  static LineEditor* Get();

 private:
  Type type_;
  const char* name_;
  LineEditor* next_;
  static LineEditor* first_;
};

LineEditor* LineEditor::first_ = 0;


const char* HistoryFileName = ".d8_history";
const int MaxHistoryEntries = 1000;


LineEditor::LineEditor(Type type, const char* name)
    : type_(type),
      name_(name),
      next_(first_) {
  first_ = this;
}


LineEditor* LineEditor::Get() {
  LineEditor* current = first_;
  LineEditor* best = current;
  while (current != NULL) {
    if (current->type_ > best->type_)
      best = current;
    current = current->next_;
  }
  return best;
}






class ReadLineEditor: public LineEditor {
 public:
  ReadLineEditor() : LineEditor(LineEditor::READLINE, "readline") { }
  virtual char* Prompt(const char* prompt);
  virtual bool Open();
  virtual bool Close();
  virtual void AddHistory(const char* str);
 private:
  static char** AttemptedCompletion(const char* text, int start, int end);
  static char* CompletionGenerator(const char* text, int state);
  static char kWordBreakCharacters[];
};


static ReadLineEditor read_line_editor;

char ReadLineEditor::kWordBreakCharacters[] = {' ', '\t', '\n', '"',
    '\\', '\'', '`', '@', '.', '>', '<', '=', ';', '|', '&', '{', '(',
    '\0'};


bool ReadLineEditor::Open() {
  rl_initialize();
  rl_attempted_completion_function = AttemptedCompletion;
  rl_completer_word_break_characters = kWordBreakCharacters;
  rl_bind_key('\t', rl_complete);
  using_history();
  stifle_history(MaxHistoryEntries);
  return read_history(HistoryFileName) == 0;
}


bool ReadLineEditor::Close() {
  return write_history(HistoryFileName) == 0;
}


char* ReadLineEditor::Prompt(const char* prompt) {
  char* result = readline(prompt);
  return result;
}


void ReadLineEditor::AddHistory(const char* str) {
  // Do not record empty input.
  if (strlen(str) == 0) return;
  // Remove duplicate history entry.
  history_set_pos(history_length-1);
  if (current_history()) {
    do {
      if (strcmp(current_history()->line, str) == 0) {
        remove_history(where_history());
        break;
      }
    } while (previous_history());
  }
  add_history(str);
}


char** ReadLineEditor::AttemptedCompletion(const char* text,
                                           int start,
                                           int end) {
  char** result = completion_matches(text, CompletionGenerator);
  rl_attempted_completion_over = true;
  return result;
}


char* ReadLineEditor::CompletionGenerator(const char* text, int state) {
  static unsigned current_index;
  static Persistent<Array> current_completions;
  if (state == 0) {
    char* full_text = strndup(rl_line_buffer, rl_point);

    HandleScope scope;
    Handle<Array> completions = Array::New();
/*
    Handle<Array> completions =
      Shell::GetCompletions(String::New(text), String::New(*full_text));
*/
    current_completions = Persistent<Array>::New(completions);
    current_index = 0;
  }
  if (current_index < current_completions->Length()) {
    HandleScope scope;
    Handle<Integer> index = Integer::New(current_index);
    Handle<Value> str_obj = current_completions->Get(index);
    current_index++;
    String::Utf8Value str(str_obj);
    return strdup(*str);
  } else {
    current_completions.Dispose();
    current_completions.Clear();
    return NULL;
  }
}


































































int main(int argc, char* argv[]) {
  v8::V8::SetFlagsFromCommandLine(&argc, argv, true);

  run_shell = (argc == 1);

  TRI_InitialiseLogging(false);
  TRI_InitialiseVocBase();

  TRI_SetLogLevelLogging("info");

  char const* path = "/tmp/vocbase";
  VocBase = TRI_OpenVocBase(path);

  if (VocBase == 0) {
    printf("cannot open database '%s'\n", path);
    exit(EXIT_FAILURE);
  }



  v8::HandleScope handle_scope;

  v8::Persistent<v8::Context> context = InitV8VocBridge(VocBase);

  PrintFuncName = v8::Persistent<v8::String>::New(v8::String::New("print"));
  LastVariableName = v8::Persistent<v8::String>::New(v8::String::New("last"));

  int result = RunMain(context, argc, argv);
  if (run_shell) RunShell(context);
  context->Exit();
  context.Dispose();
  v8::V8::Dispose();

  return result;
}


// Extracts a C string from a V8 Utf8Value.
const char* ToCString(const v8::String::Utf8Value& value) {
  return *value ? *value : "<string conversion failed>";
}









// Creates a new execution environment containing the built-in
// functions.
v8::Handle<v8::ObjectTemplate> CreateShellContext() {

  // Create a template for the global object.
  v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New();


  // Bind the global 'read' function to the C++ Read callback.
  global->Set(v8::String::New("read"), v8::FunctionTemplate::New(Read));

  // Bind the 'quit' function
  global->Set(v8::String::New("quit"), v8::FunctionTemplate::New(Quit));

  // Bind the 'version' function
  global->Set(v8::String::New("version"), v8::FunctionTemplate::New(Version));

  return global;
}


















// The callback that is invoked by v8 whenever the JavaScript 'read'
// function is called.  This function loads the content of the file named in
// the argument into a JavaScript string.
v8::Handle<v8::Value> Read(const v8::Arguments& args) {
  if (args.Length() != 1) {
    return v8::ThrowException(v8::String::New("Bad parameters"));
  }
  v8::String::Utf8Value file(args[0]);
  if (*file == NULL) {
    return v8::ThrowException(v8::String::New("Error loading file"));
  }
  v8::Handle<v8::String> source = ReadFile(*file);
  if (source.IsEmpty()) {
    return v8::ThrowException(v8::String::New("Error loading file"));
  }
  return source;
}




// The callback that is invoked by v8 whenever the JavaScript 'quit'
// function is called.  Quits.
v8::Handle<v8::Value> Quit(const v8::Arguments& args) {
  // If not arguments are given args[0] will yield undefined which
  // converts to the integer value 0.
  int exit_code = args[0]->Int32Value();
  fflush(stdout);
  fflush(stderr);
  exit(exit_code);
  return v8::Undefined();
}


v8::Handle<v8::Value> Version(const v8::Arguments& args) {
  return v8::String::New(v8::V8::GetVersion());
}


// Reads a file into a v8 string.
v8::Handle<v8::String> ReadFile(const char* name) {
  FILE* file = fopen(name, "rb");
  if (file == NULL) return v8::Handle<v8::String>();

  fseek(file, 0, SEEK_END);
  int size = ftell(file);
  rewind(file);

  char* chars = new char[size + 1];
  chars[size] = '\0';
  for (int i = 0; i < size;) {
    int read = fread(&chars[i], 1, size - i, file);
    i += read;
  }
  fclose(file);
  v8::Handle<v8::String> result = v8::String::New(chars, size);
  delete[] chars;
  return result;
}


// Process remaining command line arguments and execute files
int RunMain(v8::Handle<v8::Context> context, int argc, char* argv[]) {
  for (int i = 1; i < argc; i++) {
    const char* str = argv[i];
    if (strcmp(str, "--shell") == 0) {
      run_shell = true;
    } else if (strcmp(str, "-f") == 0) {
      // Ignore any -f flags for compatibility with the other stand-
      // alone JavaScript engines.
      continue;
    } else if (strncmp(str, "--", 2) == 0) {
      printf("Warning: unknown flag %s.\nTry --help for options\n", str);
    } else if (strcmp(str, "-e") == 0 && i + 1 < argc) {
      // Execute argument given to -e option directly.
      v8::Handle<v8::String> file_name = v8::String::New("unnamed");
      v8::Handle<v8::String> source = v8::String::New(argv[++i]);
      if (!ExecuteString(context, source, file_name, false, true)) return 1;
    } else {
      // Use all other arguments as names of files to load and run.
      v8::Handle<v8::String> file_name = v8::String::New(str);
      v8::Handle<v8::String> source = ReadFile(str);
      if (source.IsEmpty()) {
        printf("Error reading '%s'\n", str);
        continue;
      }
      if (!ExecuteString(context, source, file_name, false, true)) return 1;
    }
  }
  return 0;
}


// The read-eval-execute loop of the shell.
void RunShell(v8::Handle<v8::Context> context) {

  printf("V8 version %s [sample shell]\n", v8::V8::GetVersion());
  static const int kBufferSize = 256;

  // Enter the execution environment before evaluating any code.
  v8::Context::Scope context_scope(context);
  v8::Local<v8::String> name(v8::String::New("(shell)"));

/*
  while (true) {
    char buffer[kBufferSize];
    printf("> ");
    char* str = fgets(buffer, kBufferSize, stdin);
    if (str == NULL) break;
    v8::HandleScope handle_scope;
    ExecuteString(v8::String::New(str), name, true, true);
  }
*/

  LineEditor* console = LineEditor::Get();
  printf("V8 version %s [console: %s]\n", V8::GetVersion(), console->name());
  console->Open();
  while (true) {
    while(!V8::IdleNotification()) {}
    char* input = console->Prompt("avocado> ");
    if (input == 0) break;
    if (*input == '\0') continue;
    console->AddHistory(input);
    HandleScope inner_scope;
    ExecuteString(context, String::New(input), name, true, true);
  }




  printf("\n");
}



////////////////////////////////////////////////////////////////////////////////
/// @brief executes a string within the current v8 context
////////////////////////////////////////////////////////////////////////////////

bool ExecuteString (v8::Handle<v8::Context> context,
                    v8::Handle<v8::String> source,
                    v8::Handle<v8::Value> name,
                    bool printResult,
                    bool reportExceptions) {
  v8::HandleScope handleScope;
  v8::TryCatch tryCatch;

  v8::Handle<v8::Script> script = v8::Script::Compile(source, name);

  // compilation failed, print errors that happened during compilation
  if (script.IsEmpty()) {
    if (reportExceptions) {
      ReportException(&tryCatch);
    }

    return false;
  }

  // compilation succeeded, run the script
  else {
    v8::Handle<v8::Value> result = script->Run();

    if (result.IsEmpty()) {
      assert(tryCatch.HasCaught());

      // print errors that happened during execution
      if (reportExceptions) {
        ReportException(&tryCatch);
      }

      return false;
    }
    else {
      assert(! tryCatch.HasCaught());

      // if all went well and the result wasn't undefined then print the returned value
      if (printResult && ! result->IsUndefined()) {
        v8::Handle<v8::Function> print = v8::Handle<v8::Function>::Cast(context->Global()->Get(PrintFuncName));

        v8::Handle<v8::Value> args[] = { result };
        print->Call(print, 1, args);
      }

      context->Global()->Set(LastVariableName, result);

      return true;
    }
  }
}


void ReportException(v8::TryCatch* try_catch) {
  v8::HandleScope handle_scope;
  v8::String::Utf8Value exception(try_catch->Exception());
  const char* exception_string = ToCString(exception);
  v8::Handle<v8::Message> message = try_catch->Message();
  if (message.IsEmpty()) {
    // V8 didn't provide any extra information about this error; just
    // print the exception.
    printf("%s\n", exception_string);
  } else {
    // Print (filename):(line number): (message).
    v8::String::Utf8Value filename(message->GetScriptResourceName());
    const char* filename_string = ToCString(filename);
    int linenum = message->GetLineNumber();
    printf("%s:%i: %s\n", filename_string, linenum, exception_string);
    // Print line of source code.
    v8::String::Utf8Value sourceline(message->GetSourceLine());
    const char* sourceline_string = ToCString(sourceline);
    printf("%s\n", sourceline_string);
    // Print wavy underline (GetUnderline is deprecated).
    int start = message->GetStartColumn();
    for (int i = 0; i < start; i++) {
      printf(" ");
    }
    int end = message->GetEndColumn();
    for (int i = start; i < end; i++) {
      printf("^");
    }
    printf("\n");
    v8::String::Utf8Value stack_trace(try_catch->StackTrace());
    if (stack_trace.length() > 0) {
      const char* stack_trace_string = ToCString(stack_trace);
      printf("%s\n", stack_trace_string);
    }
  }
}
