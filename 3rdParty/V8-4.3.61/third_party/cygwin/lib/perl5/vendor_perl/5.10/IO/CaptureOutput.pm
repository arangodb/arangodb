# $Id: CaptureOutput.pm,v 1.3 2005/03/25 12:44:14 simonflack Exp $
package IO::CaptureOutput;
use strict;
use vars qw/$VERSION @ISA @EXPORT_OK %EXPORT_TAGS/;
use Exporter;
@ISA = 'Exporter';
@EXPORT_OK = qw/capture capture_exec qxx capture_exec_combined qxy/;
%EXPORT_TAGS = (all => \@EXPORT_OK);
$VERSION = '1.0801';

sub capture (&@) { ## no critic
    my ($code, $output, $error, $output_file, $error_file) = @_;

    for ($output, $error) {
        $_ = \do { my $s; $s = ''} unless ref $_;
        $$_ = '' if $_ != \undef && !defined($$_);
    }

    # don't merge if both undef -- someone might still want to capture
    # them separately in temp files
    my $should_merge = defined $error && defined $output && $output == $error;

    my ($capture_out, $capture_err);
    if ( $output != \undef ) { 
        $capture_out = IO::CaptureOutput::_proxy->new(
            'STDOUT', $output, undef, $output_file
        );
    }
    if ( $error != \undef ) { 
        my $capture_err = IO::CaptureOutput::_proxy->new(
            'STDERR', $error, ($should_merge ? 'STDOUT' : undef), $error_file
        );
    }
    &$code();
}

sub capture_exec {
    my @args = @_;
    my ($output, $error);
    capture sub { system _shell_quote(@args) }, \$output, \$error;
    return wantarray ? ($output, $error) : $output;
}

*qxx = \&capture_exec;

sub capture_exec_combined {
    my @args = @_;
    my $output;
    capture sub { system _shell_quote(@args) }, \$output, \$output;
    return $output;
}

*qxy = \&capture_exec_combined;

# extra quoting required on Win32 systems
*_shell_quote = ($^O =~ /MSWin32/) ? \&_shell_quote_win32 : sub {@_};
sub _shell_quote_win32 {
    my @args;
    for (@_) {
        if (/[ \"]/) { # TODO: check if ^ requires escaping
            (my $escaped = $_) =~ s/([\"])/\\$1/g;
            push @args, '"' . $escaped . '"';
            next;
        }
        push @args, $_
    }
    return @args;
}

# Captures everything printed to a filehandle for the lifetime of the object
# and then transfers it to a scalar reference
package IO::CaptureOutput::_proxy;
use File::Temp 'tempfile';
use File::Basename qw/basename/;
use Symbol qw/gensym qualify qualify_to_ref/;
use Carp;

sub _is_wperl { $^O eq 'MSWin32' && basename($^X) eq 'wperl.exe' }

sub new {
    my $class = shift;
    my ($fh, $capture, $merge_fh, $capture_file) = @_;
    $fh       = qualify($fh);         # e.g. main::STDOUT
    my $fhref = qualify_to_ref($fh);  # e.g. \*STDOUT

    # Duplicate the filehandle
    my $saved;
    {
        no strict 'refs'; ## no critic - needed for 5.005
        if ( defined fileno($fh) && ! _is_wperl() ) {
            $saved = gensym;
            open $saved, ">&$fh" or croak "Can't redirect <$fh> - $!";
        }
    }

    # Create replacement filehandle if not merging
    my ($newio, $newio_file);
    if ( ! $merge_fh ) {
        $newio = gensym;
        if ($capture_file) {
            $newio_file = $capture_file;
        } else {
            (undef, $newio_file) = tempfile;
        }
        open $newio, "+>$newio_file" or croak "Can't write temp file for $fh - $!";
    }
    else {
        $newio = qualify($merge_fh);
    }

    # Redirect (or merge)
    {
        no strict 'refs'; ## no critic -- needed for 5.005
        open $fhref, ">&".fileno($newio) or croak "Can't redirect $fh - $!";
    }

    bless [$$, $fh, $saved, $capture, $newio, $newio_file, $capture_file], $class;
}

sub DESTROY {
    my $self = shift;

    my ($pid, $fh, $saved) = @{$self}[0..2];
    return unless $pid eq $$; # only cleanup in the process that is capturing

    # restore the original filehandle
    my $fh_ref = Symbol::qualify_to_ref($fh);
    select((select ($fh_ref), $|=1)[0]);
    if (defined $saved) {
        open $fh_ref, ">&". fileno($saved) or croak "Can't restore $fh - $!";
    }
    else {
        close $fh_ref;
    }

    # transfer captured data to the scalar reference if we didn't merge
    my ($capture, $newio, $newio_file) = @{$self}[3..5];
    if ($newio_file) {
        # some versions of perl complain about reading from fd 1 or 2
        # which could happen if STDOUT and STDERR were closed when $newio
        # was opened, so we just squelch warnings here and continue
        local $^W; 
        seek $newio, 0, 0;
        $$capture = do {local $/; <$newio>};
        close $newio;
    }

    # Cleanup
    return unless defined $newio_file && -e $newio_file;
    return if $self->[6]; # the "temp" file was explicitly named
    unlink $newio_file or carp "Couldn't remove temp file '$newio_file' - $!";
}

1;

__END__

=pod

=begin wikidoc

= NAME

IO::CaptureOutput - capture STDOUT and STDERR from Perl code, subprocesses or XS

= VERSION

This documentation describes version %%VERSION%%.

= SYNOPSIS

    use IO::CaptureOutput qw(capture capture_exec);

    my ($stdout, $stderr);

    sub noisy {
        warn "this sub prints to stdout and stderr!";
        print "arguments: @_";
    }

    capture { noisy(@args) } \$stdout, \$stderr;

    ($stdout, $stderr) = capture_exec( 'perl', '-e', 
        'print "Hello"; print STDERR "World!"');

= DESCRIPTION

This module provides routines for capturing STDOUT and STDERR from perl 
subroutines, forked system calls (e.g. {system()}, {fork()}) and from 
XS or C modules.

= FUNCTIONS

The following functions will be exported on demand.

== capture()

    capture \&subroutine, \$stdout, \$stderr;

Captures everything printed to {STDOUT} and {STDERR} for the duration of
{&subroutine}. {$stdout} and {$stderr} are optional scalars that will contain
{STDOUT} and {STDERR} respectively. 

{capture()} uses a code prototype so the first argument can be specified directly within 
brackets if desired.

    # shorthand with prototype
    capture { print __PACKAGE__ } \$stdout, \$stderr;

Returns the return value(s) of {&subroutine}. The sub is called in the same
context as {capture()} was called e.g.:

    @rv = capture { wantarray } ; # returns true
    $rv = capture { wantarray } ; # returns defined, but not true
    capture { wantarray };       # void, returns undef

{capture()} is able to capture output from subprocesses and C code, which
traditional {tie()} methods of output capture are unable to do.

*Note:* {capture()} will only capture output that has been written or flushed
to the filehandle.

If the two scalar references refer to the same scalar, then {STDERR} will be
merged to {STDOUT} before capturing and the scalar will hold the combined
output of both.

    capture \&subroutine, \$combined, \$combined;

Normally, {capture()} uses anonymous, temporary files for capturing output.
If desired, specific file names may be provided instead as additional options.

    capture \&subroutine, \$stdout, \$stderr, $out_file, $err_file;

Files provided will be clobbered, overwriting any previous data, but
will persist after the call to {capture()} for inspection or other manipulation.

By default, when no references are provided to hold STDOUT or STDERR, output
is captured and silently discarded.

    # Capture STDOUT, discard STDERR
    capture \&subroutine, \$stdout;

    # Discard STDOUT, capture STDERR
    capture \&subroutine, undef, \$stderr;

If either STDOUT or STDERR should be passed through to the terminal instead of
captured, provide a reference to undef -- {\undef} -- instead of a capture
variable.

    # Capture STDOUT, display STDERR
    capture \&subroutine, \$stdout, \undef;

    # Display STDOUT, capture STDERR
    capture \&subroutine, \undef, \$stderr;

== capture_exec()

    ($stdout, $stderr) = capture_exec(@args);

Captures and returns the output from {system(@args)}. In scalar context,
{capture_exec()} will return what was printed to {STDOUT}. In list context,
it returns what was printed to {STDOUT} and {STDERR}

    $stdout = capture_exec('perl', '-e', 'print "hello world"');

    ($stdout, $stderr) = capture_exec('perl', '-e', 'warn "Test"');

{capture_exec} passes its arguments to {system()} and on MSWin32 will protect
arguments with shell quotes if necessary.  This makes it a handy and slightly
more portable alternative to backticks, piped {open()} and {IPC::Open3}.

You can check the exit status of the {system()} call with the {$?}
variable. See [perlvar] for more information.

== capture_exec_combined()

    $combined = capture_exec_combined(
        'perl', '-e', 'print "hello\n"', 'warn "Test\n"
    );

This is just like {capture_exec()}, except that it merges {STDERR} with {STDOUT}
before capturing output and returns a single scalar.

*Note:* there is no guarantee that text printed to {STDOUT} and {STDERR} in the
subprocess will be appear in order. The actual order will depend on how IO
buffering is handled in the subprocess.

== qxx()

This is an alias for {capture_exec()}.

== qxy()

This is an alias for {capture_exec_combined()}.

= SEE ALSO

* [IPC::Open3]
* [IO::Capture]
* [IO::Utils]

= AUTHORS

* Simon Flack <simonflk _AT_ cpan.org> (original author)
* David Golden <dagolden _AT_ cpan.org> (co-maintainer since version 1.04)

= COPYRIGHT AND LICENSE

Portions copyright 2004, 2005 Simon Flack.  Portions copyright 2007 David
Golden.  All rights reserved.

You may distribute under the terms of either the GNU General Public License or
the Artistic License, as specified in the Perl README file.

=end wikidoc 

=cut
