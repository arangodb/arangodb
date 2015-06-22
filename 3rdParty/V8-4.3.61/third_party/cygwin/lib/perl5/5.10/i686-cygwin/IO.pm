#

package IO;

use XSLoader ();
use Carp;
use strict;
use warnings;

our $VERSION = "1.23_01";
XSLoader::load 'IO', $VERSION;

sub import {
    shift;

    warnings::warnif('deprecated', qq{Parameterless "use IO" deprecated})
        if @_ == 0 ;
    
    my @l = @_ ? @_ : qw(Handle Seekable File Pipe Socket Dir);

    eval join("", map { "require IO::" . (/(\w+)/)[0] . ";\n" } @l)
	or croak $@;
}

1;

__END__

=head1 NAME

IO - load various IO modules

=head1 SYNOPSIS

    use IO qw(Handle File);  # loads IO modules, here IO::Handle, IO::File
    use IO;                  # DEPRECATED

=head1 DESCRIPTION

C<IO> provides a simple mechanism to load several of the IO modules
in one go.  The IO modules belonging to the core are:

      IO::Handle
      IO::Seekable
      IO::File
      IO::Pipe
      IO::Socket
      IO::Dir
      IO::Select
      IO::Poll

Some other IO modules don't belong to the perl core but can be loaded
as well if they have been installed from CPAN.  You can discover which
ones exist by searching for "^IO::" on http://search.cpan.org.

For more information on any of these modules, please see its respective
documentation.

=head1 DEPRECATED

    use IO;                # loads all the modules listed below

The loaded modules are IO::Handle, IO::Seekable, IO::File, IO::Pipe,
IO::Socket, IO::Dir.  You should instead explicitly import the IO
modules you want.

=cut

