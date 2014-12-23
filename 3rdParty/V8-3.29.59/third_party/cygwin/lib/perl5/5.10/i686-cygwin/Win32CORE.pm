package Win32CORE;

$VERSION = '0.02';

# There is no reason to load this module explicitly.  It will be
# initialized using xs_init() when the interpreter is constructed.

1;

__END__

=head1 NAME

Win32CORE - Win32 CORE function stubs

=head1 DESCRIPTION

This library provides stubs for the functions marked as [CORE] in L<Win32>.
See that document for usage information.  When any of these functions are
called, the full Win32 module is loaded automatically.  It is preferred
that callers of these functions explicitly C<use Win32;>.

=head1 HISTORY

Win32CORE was created to provide on cygwin those Win32:: functions that
for regular win32 builds were provided by default in perl.  In cygwin
perl releases prior to 5.8.6, this module was standalone and had to
be explicitly used.  In 5.8.6 and later, it was statically linked into
cygwin perl so this would no longer be necessary.

As of perl 5.9.5/Win32 0.27, these functions have been moved into
the Win32 module.  Win32CORE provides stubs for each of the former
CORE Win32:: functions that internally just load the Win32 module and
call it's version, and Win32CORE is statically linked to perl for both
cygwin and regular win32 builds.  This will permit these functions to
be updated in the CPAN Win32 module independently of updating perl.

=cut
