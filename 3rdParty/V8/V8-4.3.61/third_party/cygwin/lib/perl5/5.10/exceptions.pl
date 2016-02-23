# exceptions.pl
# tchrist@convex.com
#
# This library is no longer being maintained, and is included for backward
# compatibility with Perl 4 programs which may require it.
#
# In particular, this should not be used as an example of modern Perl
# programming techniques.
#
# 
# Here's a little code I use for exception handling.  It's really just
# glorfied eval/die.  The way to use use it is when you might otherwise
# exit, use &throw to raise an exception.  The first enclosing &catch
# handler looks at the exception and decides whether it can catch this kind
# (catch takes a list of regexps to catch), and if so, it returns the one it
# caught.  If it *can't* catch it, then it will reraise the exception
# for someone else to possibly see, or to die otherwise.
# 
# I use oddly named variables in order to make darn sure I don't conflict 
# with my caller.  I also hide in my own package, and eval the code in his.
# 
# The EXCEPTION: prefix is so you can tell whether it's a user-raised
# exception or a perl-raised one (eval error).
# 
# --tom
#
# examples:
#	if (&catch('/$user_input/', 'regexp', 'syntax error') {
#		warn "oops try again";
#		redo;
#	}
#
#	if ($error = &catch('&subroutine()')) { # catches anything
#
#	&throw('bad input') if /^$/;

sub catch {
    package exception;
    local($__code__, @__exceptions__) = @_;
    local($__package__) = caller;
    local($__exception__);

    eval "package $__package__; $__code__";
    if ($__exception__ = &'thrown) {
	for (@__exceptions__) {
	    return $__exception__ if /$__exception__/;
	} 
	&'throw($__exception__);
    } 
} 

sub throw {
    local($exception) = @_;
    die "EXCEPTION: $exception\n";
} 

sub thrown {
    $@ =~ /^(EXCEPTION: )+(.+)/ && $2;
} 

1;
