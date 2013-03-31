////////////////////////////////////////////////////////////////////////////////
/// @brief V8 line editor
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

#include "MRLineEditor.h"

#include <readline/readline.h>
#include <readline/history.h>

#include "BasicsC/tri-strings.h"

#if RL_READLINE_VERSION >= 0x0500
#define completion_matches rl_completion_matches
#endif

#include "mruby.h"
#include "mruby/compile.h"

using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup MRShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the code block is open
///
/// Copied from mirb.c
////////////////////////////////////////////////////////////////////////////////

static int IsCodeBlockOpen (struct mrb_parser_state *parser)
{
  int code_block_open = FALSE;

  /* check for unterminated string */
  if (parser->sterm) return TRUE;

  /* check if parser error are available */
  if (0 < parser->nerr) {
    const char *unexpected_end = "syntax error, unexpected $end";
    const char *message = parser->error_buffer[0].message;

    /* a parser error occur, we have to check if */
    /* we need to read one more line or if there is */
    /* a different issue which we have to show to */
    /* the user */

    if (strncmp(message, unexpected_end, strlen(unexpected_end)) == 0) {
      code_block_open = TRUE;
    }
    else if (strcmp(message, "syntax error, unexpected keyword_end") == 0) {
      code_block_open = FALSE;
    }
    else if (strcmp(message, "syntax error, unexpected tREGEXP_BEG") == 0) {
      code_block_open = FALSE;
    }
    return code_block_open;
  }

  switch (parser->lstate) {

  /* all states which need more code */

  case EXPR_BEG:
    /* an expression was just started, */
    /* we can't end it like this */
    code_block_open = TRUE;
    break;
  case EXPR_DOT:
    /* a message dot was the last token, */
    /* there has to come more */
    code_block_open = TRUE;
    break;
  case EXPR_CLASS:
    /* a class keyword is not enough! */
    /* we need also a name of the class */
    code_block_open = TRUE;
    break;
  case EXPR_FNAME:
    /* a method name is necessary */
    code_block_open = TRUE;
    break;
  case EXPR_VALUE:
    /* if, elsif, etc. without condition */
    code_block_open = TRUE;
    break;

  /* now all the states which are closed */

  case EXPR_ARG:
    /* an argument is the last token */
    code_block_open = FALSE;
    break;

  /* all states which are unsure */

  case EXPR_CMDARG:
    break;
  case EXPR_END:
    /* an expression was ended */
    break;
  case EXPR_ENDARG:
    /* closing parenthese */
    break;
  case EXPR_ENDFN:
    /* definition end */
    break;
  case EXPR_MID:
    /* jump keyword like break, return, ... */
    break;
  case EXPR_MAX_STATE:
    /* don't know what to do with this token */
    break;
  default:
    /* this state is unexpected! */
    break;
  }

  return code_block_open;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                class MRLineEditor
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup MRLineEditor
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new editor
////////////////////////////////////////////////////////////////////////////////

MRLineEditor::MRLineEditor (mrb_state* mrb, string const& history)
  : LineEditor(history), _current(), _mrb(mrb) {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup MRLineEditor
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief line editor open
////////////////////////////////////////////////////////////////////////////////

bool MRLineEditor::open (const bool autoComplete) {
  return LineEditor::open(autoComplete);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup LineEditor
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief check if line is complete
////////////////////////////////////////////////////////////////////////////////

bool MRLineEditor::isComplete (string const& source, size_t lineno, size_t column) {
  char* text = TRI_DuplicateString(source.c_str());

  mrb_state* mrb = mrb_open();
  mrb_parser_state* parser = mrb_parser_new(mrb);
  parser->s = text;
  parser->send = text + source.size();
  parser->capture_errors = 1;
  parser->lineno = 1;

  mrbc_context* cxt = mrbc_context_new(mrb);
  cxt->capture_errors = 1;

  mrb_parser_parse(parser, cxt);
  bool codeBlockOpen = IsCodeBlockOpen(parser);

  TRI_FreeString(TRI_CORE_MEM_ZONE, text);

  mrbc_context_free(mrb, cxt);
  mrb_parser_free(parser);
  mrb_close(mrb);

  return ! codeBlockOpen;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
