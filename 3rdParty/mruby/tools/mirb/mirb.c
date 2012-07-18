/*
** mirb - Embeddable Interactive Ruby Shell
**
** This program takes code from the user in
** an interactive way and executes it
** immediately. It's a REPL...
*/
 
#include <string.h>

#include <mruby.h>
#include <mruby/proc.h>
#include <mruby/data.h>
#include <mruby/compile.h>

/* Guess if the user might want to enter more
 * or if he wants an evaluation of his code now */
int
is_code_block_open(struct mrb_parser_state *parser)
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
      code_block_open = TRUE;
    }
    else if (strcmp(message, "syntax error, unexpected tREGEXP_BEG") == 0) {
      code_block_open = TRUE;
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

/* Print a short remark for the user */
void print_hint(void)
{
  printf("mirb - Embeddable Interactive Ruby Shell\n");
  printf("\nThis is a very early version, please test and report errors.\n");
  printf("Thanks :)\n\n");
}

/* Print the command line prompt of the REPL */
void
print_cmdline(int code_block_open)
{
  if (code_block_open) {
    printf("* ");
  }
  else {
    printf("> ");
  }
}

int
main(void)
{
  char last_char, ruby_code[1024], last_code_line[1024];
  int char_index;
  struct mrb_parser_state *parser;
  mrb_state *mrb_interpreter;
  mrb_value mrb_return_value;
  int byte_code;
  int code_block_open = FALSE;

  print_hint();

  /* new interpreter instance */ 
  mrb_interpreter = mrb_open();
  if (mrb_interpreter == NULL) {
    fprintf(stderr, "Invalid mrb_interpreter, exiting mirb");
    return EXIT_FAILURE;
  }

  /* new parser instance */
  parser = mrb_parser_new(mrb_interpreter);
  memset(ruby_code, 0, sizeof(*ruby_code));
  memset(last_code_line, 0, sizeof(*last_code_line));

  while (TRUE) {
    print_cmdline(code_block_open);

    char_index = 0;
    while ((last_char = getchar()) != '\n') {
      if (last_char == EOF) break;
      last_code_line[char_index++] = last_char;
    }
    if (last_char == EOF) {
      printf("\n");
      break;
    }

    last_code_line[char_index] = '\0';

    if ((strcmp(last_code_line, "quit") == 0) ||
        (strcmp(last_code_line, "exit") == 0)) {
      if (code_block_open) {
        /* cancel the current block and reset */
        code_block_open = FALSE;
        memset(ruby_code, 0, sizeof(*ruby_code));
        memset(last_code_line, 0, sizeof(*last_code_line));
        continue;
      }
      else {
        /* quit the program */
        break;
      }
    }
    else {
      if (code_block_open) {
	strcat(ruby_code, "\n");
        strcat(ruby_code, last_code_line);
      }
      else {
        memset(ruby_code, 0, sizeof(*ruby_code));
        strcat(ruby_code, last_code_line);
      }

      /* parse code */
      parser->s = ruby_code;
      parser->send = ruby_code + strlen(ruby_code);
      parser->capture_errors = 1;
      parser->lineno = 1;
      mrb_parser_parse(parser);
      code_block_open = is_code_block_open(parser); 

      if (code_block_open) {
        /* no evaluation of code */
      }
      else {
        if (0 < parser->nerr) {
          /* syntax error */
          printf("line %d: %s\n", parser->error_buffer[0].lineno, parser->error_buffer[0].message);
        }
	else {
          /* generate bytecode */
          byte_code = mrb_generate_code(mrb_interpreter, parser->tree);

          /* evaluate the bytecode */
          mrb_return_value = mrb_run(mrb_interpreter,
            /* pass a proc for evaulation */
            mrb_proc_new(mrb_interpreter, mrb_interpreter->irep[byte_code]),
            mrb_top_self(mrb_interpreter));
          /* did an exception occur? */
          if (mrb_interpreter->exc) {
            mrb_p(mrb_interpreter, mrb_obj_value(mrb_interpreter->exc));
            mrb_interpreter->exc = 0;
          }
	  else {
            /* no */
            printf(" => ");
            mrb_p(mrb_interpreter, mrb_return_value);
          }
        }

        memset(ruby_code, 0, sizeof(*ruby_code));
        memset(ruby_code, 0, sizeof(*last_code_line));
      }
    }
  }
  mrb_close(mrb_interpreter);

  return 0;
}
