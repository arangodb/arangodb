#!env python
""" removes terminal control sequences and other non ascii characters """
import unicodedata
import re
import sys

is_tty = sys.stdout.isatty()
PROGRESS_COUNT = 0

# 7-bit C1 ANSI sequences
ANSI_ESCAPE_B = re.compile(
    rb"""
    \x1B  # ESC
    \xE2  # throbber...
    \xA0  # throbber...
    \xA7  # throbber...
    \x8F  # throbber...
    \r    # cariage return
    (?:   # 7-bit C1 Fe (except CSI)
        [@-Z\\-_]
    |     # or [ for CSI, followed by a control sequence
        \[
        [0-?]*  # Parameter bytes
        [ -/]*  # Intermediate bytes
        [@-~]   # Final byte
    )
""",
    re.VERBOSE,
)


def ascii_convert(the_bytes: bytes):
    """convert string to only be ascii without control sequences"""
    return ANSI_ESCAPE_B.sub(rb"", the_bytes).decode("utf-8")


# 7-bit C1 ANSI sequences
ANSI_ESCAPE = re.compile(
    r"""
    \x1B  # ESC
    \xE2  # throbber...
    \xA0  # throbber...
    \xA7  # throbber...
    \x8F  # throbber...
    \r    # cariage return
    (?:   # 7-bit C1 Fe (except CSI)
        [@-Z\\-_]
    |     # or [ for CSI, followed by a control sequence
        \[
        [0-?]*  # Parameter bytes
        [ -/]*  # Intermediate bytes
        [@-~]   # Final byte
    )
""",
    re.VERBOSE,
)


def ascii_convert_str(the_str: str):
    """convert string to only be ascii without control sequences"""
    return ANSI_ESCAPE.sub(rb"", the_str)


def ascii_print(string):
    """convert string to only be ascii without control sequences"""
    string = ANSI_ESCAPE.sub("", string)
    print("".join(ch for ch in string if ch == "\n" or unicodedata.category(ch)[0] != "C"))


def print_progress(char):
    """print a throbber alike that immediately is sent to the console"""
    # pylint: disable=global-statement
    global PROGRESS_COUNT
    print(char, end="")
    PROGRESS_COUNT += 1
    if not is_tty and PROGRESS_COUNT % 10 == 0:
        # add a linebreak so we see something in jenkins (if):
        print("\n")
    sys.stdout.flush()
