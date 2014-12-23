package Shell;
use 5.006_001;
use strict;
use warnings;
use File::Spec::Functions;

our($capture_stderr, $raw, $VERSION, $AUTOLOAD);

$VERSION = '0.72_01';
$VERSION = eval $VERSION;

sub new { bless \my $foo, shift }
sub DESTROY { }

sub import {
    my $self = shift;
    my ($callpack, $callfile, $callline) = caller;
    my @EXPORT;
    if (@_) {
        @EXPORT = @_;
    } else {
        @EXPORT = 'AUTOLOAD';
    }
    foreach my $sym (@EXPORT) {
        no strict 'refs';
        *{"${callpack}::$sym"} = \&{"Shell::$sym"};
    }
}

# NOTE: this is used to enable constant folding in 
# expressions like (OS eq 'MSWin32') and 
# (OS eq 'os2') just like it happened in  0.6  version 
# which used eval "string" to install subs on the fly.
use constant OS => $^O;

=begin private

=item B<_make_cmd>

  $sub = _make_cmd($cmd);
  $sub = $shell->_make_cmd($cmd);

Creates a closure which invokes the system command C<$cmd>.

=end private

=cut

sub _make_cmd {
    shift if ref $_[0] && $_[0]->isa( 'Shell' );
    my $cmd = shift;
    my $null = File::Spec::Functions::devnull();
    $Shell::capture_stderr ||= 0;
    # closing over $^O, $cmd, and $null
    return sub {
            shift if ref $_[0] && $_[0]->isa( 'Shell' );
            if (@_ < 1) {
                $Shell::capture_stderr ==  1 ? `$cmd 2>&1` : 
                $Shell::capture_stderr == -1 ? `$cmd 2>$null` : 
                `$cmd`;
            } elsif (OS eq 'os2') {
                local(*SAVEOUT, *READ, *WRITE);

                open SAVEOUT, '>&STDOUT' or die;
                pipe READ, WRITE or die;
                open STDOUT, '>&WRITE' or die;
                close WRITE;

                my $pid = system(1, $cmd, @_);
                die "Can't execute $cmd: $!\n" if $pid < 0;

                open STDOUT, '>&SAVEOUT' or die;
                close SAVEOUT;

                if (wantarray) {
                    my @ret = <READ>;
                    close READ;
                    waitpid $pid, 0;
                    @ret;
                } else {
                    local($/) = undef;
                    my $ret = <READ>;
                    close READ;
                    waitpid $pid, 0;
                    $ret;
                }
            } else {
                my $a;
                my @arr = @_;
                unless( $Shell::raw ){
                  if (OS eq 'MSWin32') {
                    # XXX this special-casing should not be needed
                    # if we do quoting right on Windows. :-(
                    #
                    # First, escape all quotes.  Cover the case where we
                    # want to pass along a quote preceded by a backslash
                    # (i.e., C<"param \""" end">).
                    # Ugly, yup?  You know, windoze.
                    # Enclose in quotes only the parameters that need it:
                    #   try this: c:> dir "/w"
                    #   and this: c:> dir /w
                    for (@arr) {
                        s/"/\\"/g;
                        s/\\\\"/\\\\"""/g;
                        $_ = qq["$_"] if /\s/;
                    }
                  } else {
                    for (@arr) {
                        s/(['\\])/\\$1/g;
                        $_ = $_;
                     }
                  }
                }
                push @arr, '2>&1'        if $Shell::capture_stderr ==  1;
                push @arr, '2>$null' if $Shell::capture_stderr == -1;
                open(SUBPROC, join(' ', $cmd, @arr, '|'))
                    or die "Can't exec $cmd: $!\n";
                if (wantarray) {
                    my @ret = <SUBPROC>;
                    close SUBPROC;        # XXX Oughta use a destructor.
                    @ret;
                } else {
                    local($/) = undef;
                    my $ret = <SUBPROC>;
                    close SUBPROC;
                    $ret;
                }
            }
        };
        }

sub AUTOLOAD {
    shift if ref $_[0] && $_[0]->isa( 'Shell' );
    my $cmd = $AUTOLOAD;
    $cmd =~ s/^.*:://;
    no strict 'refs';
    *$AUTOLOAD = _make_cmd($cmd);
    goto &$AUTOLOAD;
}

1;

__END__

=head1 NAME

Shell - run shell commands transparently within perl

=head1 SYNOPSIS

   use Shell qw(cat ps cp);
   $passwd = cat('</etc/passwd');
   @pslines = ps('-ww'),
   cp("/etc/passwd", "/tmp/passwd");

   # object oriented 
   my $sh = Shell->new;
   print $sh->ls('-l');

=head1 DESCRIPTION

=head2 Caveats

This package is included as a show case, illustrating a few Perl features.
It shouldn't be used for production programs. Although it does provide a 
simple interface for obtaining the standard output of arbitrary commands,
there may be better ways of achieving what you need.

Running shell commands while obtaining standard output can be done with the
C<qx/STRING/> operator, or by calling C<open> with a filename expression that
ends with C<|>, giving you the option to process one line at a time.
If you don't need to process standard output at all, you might use C<system>
(in preference of doing a print with the collected standard output).

Since Shell.pm and all of the aforementioned techniques use your system's
shell to call some local command, none of them is portable across different 
systems. Note, however, that there are several built in functions and 
library packages providing portable implementations of functions operating
on files, such as: C<glob>, C<link> and C<unlink>, C<mkdir> and C<rmdir>, 
C<rename>, C<File::Compare>, C<File::Copy>, C<File::Find> etc.

Using Shell.pm while importing C<foo> creates a subroutine C<foo> in the
namespace of the importing package. Calling C<foo> with arguments C<arg1>,
C<arg2>,... results in a shell command C<foo arg1 arg2...>, where the 
function name and the arguments are joined with a blank. (See the subsection 
on Escaping magic characters.) Since the result is essentially a command
line to be passed to the shell, your notion of arguments to the Perl
function is not necessarily identical to what the shell treats as a
command line token, to be passed as an individual argument to the program.
Furthermore, note that this implies that C<foo> is callable by file name
only, which frequently depends on the setting of the program's environment.

Creating a Shell object gives you the opportunity to call any command
in the usual OO notation without requiring you to announce it in the
C<use Shell> statement. Don't assume any additional semantics being
associated with a Shell object: in no way is it similar to a shell
process with its environment or current working directory or any
other setting.

=head2 Escaping Magic Characters

It is, in general, impossible to take care of quoting the shell's
magic characters. For some obscure reason, however, Shell.pm quotes
apostrophes (C<'>) and backslashes (C<\>) on UNIX, and spaces and
quotes (C<">) on Windows.

=head2 Configuration

If you set $Shell::capture_stderr to 1, the module will attempt to
capture the standard error output of the process as well. This is
done by adding C<2E<gt>&1> to the command line, so don't try this on
a system not supporting this redirection.

Setting $Shell::capture_stderr to -1 will send standard error to the
bit bucket (i.e., the equivalent of adding C<2E<gt>/dev/null> to the
command line).  The same caveat regarding redirection applies.

If you set $Shell::raw to true no quoting whatsoever is done.

=head1 BUGS

Quoting should be off by default.

It isn't possible to call shell built in commands, but it can be
done by using a workaround, e.g. shell( '-c', 'set' ).

Capturing standard error does not work on some systems (e.g. VMS).

=head1 AUTHOR

  Date: Thu, 22 Sep 94 16:18:16 -0700
  Message-Id: <9409222318.AA17072@scalpel.netlabs.com>
  To: perl5-porters@isu.edu
  From: Larry Wall <lwall@scalpel.netlabs.com>
  Subject: a new module I just wrote

Here's one that'll whack your mind a little out.

    #!/usr/bin/perl

    use Shell;

    $foo = echo("howdy", "<funny>", "world");
    print $foo;

    $passwd = cat("</etc/passwd");
    print $passwd;

    sub ps;
    print ps -ww;

    cp("/etc/passwd", "/etc/passwd.orig");

That's maybe too gonzo.  It actually exports an AUTOLOAD to the current
package (and uncovered a bug in Beta 3, by the way).  Maybe the usual
usage should be

    use Shell qw(echo cat ps cp);

Larry Wall

Changes by Jenda@Krynicky.cz and Dave Cottle <d.cottle@csc.canterbury.ac.nz>.

Changes for OO syntax and bug fixes by Casey West <casey@geeknest.com>.

C<$Shell::raw> and pod rewrite by Wolfgang Laun.

Rewritten to use closures rather than C<eval "string"> by Adriano Ferreira.

=cut
