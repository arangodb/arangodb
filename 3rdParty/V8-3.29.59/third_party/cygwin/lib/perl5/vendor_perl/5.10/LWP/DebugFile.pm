package LWP::DebugFile;

use strict;
use LWP::Debug ();

use vars qw($outname $outpath @ISA $last_message_time);
@ISA = ('LWP::Debug');

_init() unless $^C or !caller;
$LWP::Debug::current_level{'conns'} = 1;



sub _init {
  $outpath = $ENV{'LWPDEBUGPATH'} || ''
   unless defined $outpath;
  $outname = $ENV{'LWPDEBUGFILE'} ||
    sprintf "%slwp_%x_%x.log", $outpath, $^T,
     defined( &Win32::GetTickCount )
      ? (Win32::GetTickCount() & 0xFFFF)
      : $$
        # Using $$ under Win32 isn't nice, because the OS usually
        # reuses the $$ value almost immediately!!  So the lower
        # 16 bits of the uptime tick count is a great substitute.
   unless defined $outname;

  open LWPERR, ">>$outname" or die "Can't write-open $outname: $!";
  # binmode(LWPERR);
  {
    no strict;
    my $x = select(LWPERR);
    ++$|;
    select($x);
  }

  $last_message_time = time();
  die "Can't print to LWPERR"
   unless print LWPERR "\n# ", __PACKAGE__, " logging to $outname\n";
   # check at least the first print, just for sanity's sake!

  print LWPERR "# Time now: \{$last_message_time\} = ",
          scalar(localtime($last_message_time)), "\n";

  LWP::Debug::level($ENV{'LWPDEBUGLEVEL'} || '+');
  return;
}


BEGIN { # So we don't get redefinition warnings...
  undef &LWP::Debug::conns;
  undef &LWP::Debug::_log;
}


sub LWP::Debug::conns {
  if($LWP::Debug::current_level{'conns'}) {
    my $msg = $_[0];
    my $line;
    my $prefix = '0';
    while($msg =~ m/([^\n\r]*[\n\r]*)/g) {
      next unless length($line = $1);
      # Hex escape it:
      $line =~ s/([^\x20\x21\x23-\x7a\x7c\x7e])/
        (ord($1)<256) ? sprintf('\x%02X',ord($1))
         : sprintf('\x{%x}',ord($1))
      /eg;
      LWP::Debug::_log("S>$prefix \"$line\"");
      $prefix = '+';
    }
  }
}


sub LWP::Debug::_log
{
    my $msg = shift;
    $msg .= "\n" unless $msg =~ /\n$/;  # ensure trailing "\n"

    my($package,$filename,$line,$sub) = caller(2);
    unless((my $this_time = time()) == $last_message_time) {
      print LWPERR "# Time now: \{$this_time\} = ",
        scalar(localtime($this_time)), "\n";
      $last_message_time = $this_time;
    }
    print LWPERR "$sub: $msg";
}


1;

__END__

=head1 NAME

LWP::DebugFile - routines for tracing/debugging LWP

=head1 SYNOPSIS

If you want to see just what LWP is doing when your program calls it,
add this to the beginning of your program's source:

  use LWP::DebugFile;

For even more verbose debug output, do this instead:

  use LWP::DebugFile ('+');

=head1 DESCRIPTION

This module is like LWP::Debug in that it allows you to see what your
calls to LWP are doing behind the scenes.  But it is unlike
L<LWP::Debug|LWP::Debug> in that it sends the output to a file, instead
of to STDERR (as LWP::Debug does).

=head1 OPTIONS

The options you can use in C<use LWP::DebugFile (I<options>)> are the
same as the B<non-exporting> options available from C<use LWP::Debug
(I<options>)>.  That is, you can do things like this:

  use LWP::DebugFile qw(+);
  use LWP::Debug qw(+ -conns);
  use LWP::Debug qw(trace);

The meanings of these are explained in the
L<documentation for LWP::Debug|LWP::Debug>.
The only differences are that by default, LWP::DebugFile has C<cons>
debugging on, ad that (as mentioned earlier), only C<non-exporting>
options are available.  That is, you B<can't> do this:

  use LWP::DebugFile qw(trace); # wrong

You might expect that to export LWP::Debug's C<trace()> function,
but it doesn't work -- it's a compile-time error.

=head1 OUTPUT FILE NAMING

If you don't do anything, the output file (where all the LWP debug/trace
output goes) will be in the current directory, and will be named like
F<lwp_3db7aede_b93.log>, where I<3db7aede> is C<$^T> expressed in hex,
and C<b93> is C<$$> expressed in hex.  Presumably this is a
unique-for-all-time filename!

If you don't want the files to go in the current directory, you
can set C<$LWP::DebugFile::outpath> before you load the LWP::DebugFile
module:

  BEGIN { $LWP::DebugFile::outpath = '/tmp/crunk/' }
  use LWP::DebugFile;

Note that you must end the value with a path separator ("/" in this
case -- under MacPerl it would be ":").  With that set, you will
have output files named like F</tmp/crunk/lwp_3db7aede_b93.log>.

If you want the LWP::DebugFile output to go a specific filespec (instead
of just a uniquely named file, in whatever directory), instead set the
variable C<$LWP::DebugFile::outname>, like so:

  BEGIN { $LWP::DebugFile::outname = '/home/mojojojo/lwp.log' }
  use LWP::DebugFile;

In that case, C<$LWP::DebugFile::outpath> isn't consulted at all, and
output is always written to the file F</home/mojojojo/lwp.log>.

Note that the value of C<$LWP::DebugFile::outname> doesn't need to
be an absolute filespec.  You can do this:

  BEGIN { $LWP::DebugFile::outname = 'lwp.log' }
  use LWP::DebugFile;

In that case, output goes to a file named F<lwp.log> in the current
directory -- specifically, whatever directory is current when
LWP::DebugFile is first loaded. C<$LWP::DebugFile::outpath> is still not
consulted -- its value is used only if C<$LWP::DebugFile::outname>
isn't set.


=head1 ENVIRONMENT

If you set the environment variables C<LWPDEBUGPATH> or 
C<LWPDEBUGFILE>, their values will be used in initializing the
values of C<$LWP::DebugFile::outpath>
and C<$LWP::DebugFile::outname>.

That is, if you have C<LWPDEBUGFILE> set to F</home/mojojojo/lwp.log>,
then you can just start out your program with:

  use LWP::DebugFile;

and it will act as if you had started it like this:

  BEGIN { $LWP::DebugFile::outname = '/home/mojojojo/lwp.log' }
  use LWP::DebugFile;

=head1 IMPLEMENTATION NOTES

This module works by subclassing C<LWP::Debug>, (notably inheriting its
C<import>). It also redefines C<&LWP::Debug::conns> and
C<&LWP::Debug::_log> to make for output that is a little more verbose,
and friendlier for when you're looking at it later in a log file.

=head1 SEE ALSO

L<LWP::Debug>

=head1 COPYRIGHT AND DISCLAIMERS

Copyright (c) 2002 Sean M. Burke.

This library is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

This program is distributed in the hope that it will be useful, but
without any warranty; without even the implied warranty of
merchantability or fitness for a particular purpose.

=head1 AUTHOR

Sean M. Burke C<sburke@cpan.org>

