#
# This library is no longer being maintained, and is included for backward
# compatibility with Perl 4 programs which may require it.
#
# In particular, this should not be used as an example of modern Perl
# programming techniques.
#
# Suggested alternative: IO::Handle
#
;# Usage: &flush(FILEHANDLE)
;# flushes the named filehandle

;# Usage: &printflush(FILEHANDLE, "prompt: ")
;# prints arguments and flushes filehandle

sub flush {
    local($old) = select(shift);
    $| = 1;
    print "";
    $| = 0;
    select($old);
}

sub printflush {
    local($old) = select(shift);
    $| = 1;
    print @_;
    $| = 0;
    select($old);
}

1;
