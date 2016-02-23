package IPC::Run3;

=head1 NAME

IPC::Run3 - run a subprocess with input/ouput redirection

=head1 VERSION

version 0.040

=cut

$VERSION = '0.040';

=head1 SYNOPSIS

    use IPC::Run3;    # Exports run3() by default

    run3 \@cmd, \$in, \$out, \$err;

=head1 DESCRIPTION

This module allows you to run a subprocess and redirect stdin, stdout,
and/or stderr to files and perl data structures.  It aims to satisfy 99% of the
need for using C<system>, C<qx>, and C<open3> 
with a simple, extremely Perlish API.

Speed, simplicity, and portability are paramount.  (That's speed of Perl code;
which is often much slower than the kind of buffered I/O that this module uses
to spool input to and output from the child command.)

=cut

use 5.006_000;				# i.e. v5.6.0

@EXPORT = qw( run3 );
%EXPORT_TAGS = ( all => \@EXPORT );
@ISA = qw( Exporter );
use Exporter;

use strict;
use constant debugging => $ENV{IPCRUN3DEBUG} || $ENV{IPCRUNDEBUG} || 0;
use constant profiling => $ENV{IPCRUN3PROFILE} || $ENV{IPCRUNPROFILE} || 0;
use constant is_win32  => 0 <= index $^O, "Win32";

BEGIN {
   if ( is_win32 ) {
      eval "use Win32 qw( GetOSName ); 1" or die $@;
   }
}

#use constant is_win2k => is_win32 && GetOSName() =~ /Win2000/i;
#use constant is_winXP => is_win32 && GetOSName() =~ /WinXP/i;

use Carp qw( croak );
use File::Temp qw( tempfile );
use POSIX qw( dup dup2 );

# We cache the handles of our temp files in order to
# keep from having to incur the (largish) overhead of File::Temp
my %fh_cache;
my $fh_cache_pid = $$;

my $profiler;

sub _profiler { $profiler } # test suite access

BEGIN {
    if ( profiling ) {
        eval "use Time::HiRes qw( gettimeofday ); 1" or die $@;
        if ( $ENV{IPCRUN3PROFILE} =~ /\A\d+\z/ ) {
            require IPC::Run3::ProfPP;
            IPC::Run3::ProfPP->import;
            $profiler = IPC::Run3::ProfPP->new(Level => $ENV{IPCRUN3PROFILE});
        } else {
            my ( $dest, undef, $class ) =
               reverse split /(=)/, $ENV{IPCRUN3PROFILE}, 2;
            $class = "IPC::Run3::ProfLogger"
                unless defined $class && length $class;
            if ( not eval "require $class" ) {
                my $e = $@;
                $class = "IPC::Run3::$class";
                eval "require IPC::Run3::$class" or die $e;
            }
            $profiler = $class->new( Destination => $dest );
        }
        $profiler->app_call( [ $0, @ARGV ], scalar gettimeofday() );
    }
}


END {
    $profiler->app_exit( scalar gettimeofday() ) if profiling;
}

sub _spool_data_to_child {
    my ( $type, $source, $binmode_it ) = @_;

    # If undef (not \undef) passed, they want the child to inherit
    # the parent's STDIN.
    return undef unless defined $source;
    warn "binmode()ing STDIN\n" if is_win32 && debugging && $binmode_it;

    my $fh;
    if ( ! $type ) {
        open $fh, "<", $source or croak "$!: $source";
        if ( is_win32 ) {
            binmode $fh, ":raw"; # Remove all layers
            binmode $fh, ":crlf" unless $binmode_it;
        }
        warn "run3(): feeding file '$source' to child STDIN\n"
            if debugging >= 2;
    } elsif ( $type eq "FH" ) {
        $fh = $source;
        warn "run3(): feeding filehandle '$source' to child STDIN\n"
            if debugging >= 2;
    } else {
        $fh = $fh_cache{in} ||= tempfile;
        truncate $fh, 0;
        seek $fh, 0, 0;
        if ( is_win32 ) {
            binmode $fh, ":raw"; # Remove any previous layers
            binmode $fh, ":crlf" unless $binmode_it;
        }
        my $seekit;
        if ( $type eq "SCALAR" ) {

            # When the run3()'s caller asks to feed an empty file
            # to the child's stdin, we want to pass a live file
            # descriptor to an empty file (like /dev/null) so that
            # they don't get surprised by invalid fd errors and get
            # normal EOF behaviors.
            return $fh unless defined $$source;  # \undef passed

            warn "run3(): feeding SCALAR to child STDIN",
                debugging >= 3
                   ? ( ": '", $$source, "' (", length $$source, " chars)" )
                   : (),
                "\n"
                if debugging >= 2;

            $seekit = length $$source;
            print $fh $$source or die "$! writing to temp file";

        } elsif ( $type eq "ARRAY" ) {
            warn "run3(): feeding ARRAY to child STDIN",
                debugging >= 3 ? ( ": '", @$source, "'" ) : (),
                "\n"
            if debugging >= 2;

            print $fh @$source or die "$! writing to temp file";
            $seekit = grep length, @$source;
        } elsif ( $type eq "CODE" ) {
            warn "run3(): feeding output of CODE ref '$source' to child STDIN\n"
                if debugging >= 2;
            my $parms = [];  # TODO: get these from $options
            while (1) {
                my $data = $source->( @$parms );
                last unless defined $data;
                print $fh $data or die "$! writing to temp file";
                $seekit = length $data;
            }
        }

        seek $fh, 0, 0 or croak "$! seeking on temp file for child's stdin"
            if $seekit;
    }

    croak "run3() can't redirect $type to child stdin"
        unless defined $fh;

    return $fh;
}

sub _fh_for_child_output {
    my ( $what, $type, $dest, $options ) = @_;

    my $fh;
    if ( $type eq "SCALAR" && $dest == \undef ) {
        warn "run3(): redirecting child $what to oblivion\n"
            if debugging >= 2;

        $fh = $fh_cache{nul} ||= do {
            open $fh, ">", File::Spec->devnull;
	    $fh;
        };
    } elsif ( $type eq "FH" ) {
        $fh = $dest;
        warn "run3(): redirecting $what to filehandle '$dest'\n"
            if debugging >= 3;
    } elsif ( !$type ) {
        warn "run3(): feeding child $what to file '$dest'\n"
            if debugging >= 2;

        open $fh, $options->{"append_$what"} ? ">>" : ">", $dest 
	    or croak "$!: $dest";
    } else {
        warn "run3(): capturing child $what\n"
            if debugging >= 2;

        $fh = $fh_cache{$what} ||= tempfile;
        seek $fh, 0, 0;
        truncate $fh, 0;
    }

    if ( is_win32 ) {
        warn "binmode()ing $what\n" if debugging && $options->{"binmode_$what"};
        binmode $fh, ":raw";
        binmode $fh, ":crlf" unless $options->{"binmode_$what"};
    }
    return $fh;
}

sub _read_child_output_fh {
    my ( $what, $type, $dest, $fh, $options ) = @_;

    return if $type eq "SCALAR" && $dest == \undef;

    seek $fh, 0, 0 or croak "$! seeking on temp file for child $what";

    if ( $type eq "SCALAR" ) {
        warn "run3(): reading child $what to SCALAR\n"
            if debugging >= 3;

        # two read()s are used instead of 1 so that the first will be
        # logged even it reads 0 bytes; the second won't.
        my $count = read $fh, $$dest, 10_000, 
	    $options->{"append_$what"} ? length $$dest : 0;
        while (1) {
            croak "$! reading child $what from temp file"
                unless defined $count;

            last unless $count;

            warn "run3(): read $count bytes from child $what",
                debugging >= 3 ? ( ": '", substr( $$dest, -$count ), "'" ) : (),
                "\n"
                if debugging >= 2;

            $count = read $fh, $$dest, 10_000, length $$dest;
        }
    } elsif ( $type eq "ARRAY" ) {
	if ($options->{"append_$what"}) {
	    push @$dest, <$fh>;
	} else {
	    @$dest = <$fh>;
	}
        if ( debugging >= 2 ) {
            my $count = 0;
            $count += length for @$dest;
            warn
                "run3(): read ",
                scalar @$dest,
                " records, $count bytes from child $what",
                debugging >= 3 ? ( ": '", @$dest, "'" ) : (),
                "\n";
        }
    } elsif ( $type eq "CODE" ) {
        warn "run3(): capturing child $what to CODE ref\n"
            if debugging >= 3;

        local $_;
        while ( <$fh> ) {
            warn
                "run3(): read ",
                length,
                " bytes from child $what",
                debugging >= 3 ? ( ": '", $_, "'" ) : (),
                "\n"
                if debugging >= 2;

            $dest->( $_ );
        }
    } else {
        croak "run3() can't redirect child $what to a $type";
    }

}

sub _type {
    my ( $redir ) = @_;
    return "FH" if eval { $redir->isa("IO::Handle") };
    my $type = ref $redir;
    return $type eq "GLOB" ? "FH" : $type;
}

sub _max_fd {
    my $fd = dup(0);
    POSIX::close $fd;
    return $fd;
}

my $run_call_time;
my $sys_call_time;
my $sys_exit_time;

sub run3 {
    $run_call_time = gettimeofday() if profiling;

    my $options = @_ && ref $_[-1] eq "HASH" ? pop : {};

    my ( $cmd, $stdin, $stdout, $stderr ) = @_;

    print STDERR "run3(): running ", 
       join( " ", map "'$_'", ref $cmd ? @$cmd : $cmd ), 
       "\n"
       if debugging;

    if ( ref $cmd ) {
        croak "run3(): empty command"     unless @$cmd;
        croak "run3(): undefined command" unless defined $cmd->[0];
        croak "run3(): command name ('')" unless length  $cmd->[0];
    } else {
        croak "run3(): missing command" unless @_;
        croak "run3(): undefined command" unless defined $cmd;
        croak "run3(): command ('')" unless length  $cmd;
    }

    my $in_type  = _type $stdin;
    my $out_type = _type $stdout;
    my $err_type = _type $stderr;

    if ($fh_cache_pid != $$) {
	# fork detected, close all cached filehandles and clear the cache
	close $_ foreach values %fh_cache;
	%fh_cache = ();
	$fh_cache_pid = $$;
    }
    
    # This routine procedes in stages so that a failure in an early
    # stage prevents later stages from running, and thus from needing
    # cleanup.

    my $in_fh  = _spool_data_to_child $in_type, $stdin,
        $options->{binmode_stdin} if defined $stdin;

    my $out_fh = _fh_for_child_output "stdout", $out_type, $stdout,
        $options if defined $stdout;

    my $tie_err_to_out =
        defined $stderr && defined $stdout && $stderr eq $stdout;

    my $err_fh = $tie_err_to_out
        ? $out_fh
        : _fh_for_child_output "stderr", $err_type, $stderr,
            $options if defined $stderr;

    # this should make perl close these on exceptions
    local *STDIN_SAVE;
    local *STDOUT_SAVE;
    local *STDERR_SAVE;

    my $saved_fd0 = dup( 0 ) if defined $in_fh;

#    open STDIN_SAVE,  "<&STDIN"#  or croak "run3(): $! saving STDIN"
#        if defined $in_fh;
    open STDOUT_SAVE, ">&STDOUT" or croak "run3(): $! saving STDOUT"
        if defined $out_fh;
    open STDERR_SAVE, ">&STDERR" or croak "run3(): $! saving STDERR"
        if defined $err_fh;

    my $ok = eval {
        # The open() call here seems to not force fd 0 in some cases;
        # I ran in to trouble when using this in VCP, not sure why.
        # the dup2() seems to work.
        dup2( fileno $in_fh, 0 )
#        open STDIN,  "<&=" . fileno $in_fh
            or croak "run3(): $! redirecting STDIN"
            if defined $in_fh;

#        close $in_fh or croak "$! closing STDIN temp file"
#            if ref $stdin;

        open STDOUT, ">&" . fileno $out_fh
            or croak "run3(): $! redirecting STDOUT"
            if defined $out_fh;

        open STDERR, ">&" . fileno $err_fh
            or croak "run3(): $! redirecting STDERR"
            if defined $err_fh;

        $sys_call_time = gettimeofday() if profiling;

        my $r = ref $cmd
              ? system { $cmd->[0] }
                       is_win32
                           ? map {
                                 # Probably need to offer a win32 escaping
                                 # option, every command may be different.
                                 ( my $s = $_ ) =~ s/"/"""/g;
                                 $s = qq{"$s"};
                                 $s;
                             } @$cmd
                           : @$cmd
              : system $cmd;

        $sys_exit_time = gettimeofday() if profiling;

        unless ( defined $r && $r != -1 ) {
            if ( debugging ) {
                my $err_fh = defined $err_fh ? \*STDERR_SAVE : \*STDERR;
                print $err_fh "run3(): system() error $!\n"
            }
            die $!;
        }

        if ( debugging ) {
            my $err_fh = defined $err_fh ? \*STDERR_SAVE : \*STDERR;
            print $err_fh "run3(): \$? is $?\n"
        }
        1;
    };
    my $x = $@;

    my @errs;

    if ( defined $saved_fd0 ) {
        dup2( $saved_fd0, 0 );
        POSIX::close( $saved_fd0 );
    }

#    open STDIN,  "<&STDIN_SAVE"#  or push @errs, "run3(): $! restoring STDIN"
#        if defined $in_fh;
    open STDOUT, ">&STDOUT_SAVE" or push @errs, "run3(): $! restoring STDOUT"
        if defined $out_fh;
    open STDERR, ">&STDERR_SAVE" or push @errs, "run3(): $! restoring STDERR"
        if defined $err_fh;

    croak join ", ", @errs if @errs;

    die $x unless $ok;

    _read_child_output_fh "stdout", $out_type, $stdout, $out_fh, $options
        if defined $out_fh && $out_type && $out_type ne "FH";
    _read_child_output_fh "stderr", $err_type, $stderr, $err_fh, $options
        if defined $err_fh && $err_type && $err_type ne "FH" && !$tie_err_to_out;
    $profiler->run_exit(
       $cmd,
       $run_call_time,
       $sys_call_time,
       $sys_exit_time,
       scalar gettimeofday() 
    ) if profiling;

    return 1;
}

1;

__END__

=head2 C<< run3($cmd, $stdin, $stdout, $stderr, \%options) >>

All parameters after C<$cmd> are optional.

The parameters C<$stdin>, C<$stdout> and C<$stderr> indicate
how the child's corresponding filehandle 
(C<STDIN>, C<STDOUT> and C<STDERR>, resp.) will be redirected.
Because the redirects come last, this allows C<STDOUT> and C<STDERR> to default
to the parent's by just not specifying them -- a common use case.

C<run3> returns true if the command executes and throws an exception otherwise.
It leaves C<$?> intact for inspection of exit and wait status.

=head3 C<$cmd>

Usually C<$cmd> will be an ARRAY reference and the child is invoked via

  system @$cmd;

But C<$cmd> may also be a string in which case the child is invoked via

  system $cmd;

(cf. L<perlfunc/system> for the difference and the pitfalls of using
the latter form).

=head3 C<$stdin>, C<$stdout>, C<$stderr>

The parameters C<$stdin>, C<$stdout> and C<$stderr> 
can take one of the following forms:

=over 4

=item C<undef> (or not specified at all)

The child inherits the corresponding filehandle from the parent.

  run3 \@cmd, $stdin;                   # child writes to same STDOUT and STDERR as parent
  run3 \@cmd, undef, $stdout, $stderr;  # child reads from same STDIN as parent

=item C<\undef>

The child's filehandle is redirected from or to the
local equivalent of C</dev/null> (as returned by 
C<< File::Spec->devnull() >>).

  run3 \@cmd, \undef, $stdout, $stderr; # child reads from /dev/null

=item a simple scalar

The parameter is taken to be the name of a file to read from
or write to. In the latter case, the file will be opened via

  open FH, ">", ...

i.e. it is created if it doesn't exist and truncated otherwise.
Note that the file is opened by the parent which will L<croak|Carp/croak>
in case of failure.

  run3 \@cmd, \undef, "out.txt";        # child writes to file "out.txt"

=item a filehandle (either a reference to a GLOB or an C<IO::Handle>)

The filehandle is inherited by the child.

  open my $fh, ">", "out.txt";
  print $fh "prologue\n";
  ...
  run3 \@cmd, \undef, $fh;              # child writes to $fh
  ...
  print $fh "epilogue\n";
  close $fh;

=item a SCALAR reference 

The referenced scalar is treated as a string to be read from or
written to. In the latter case, the previous content of the string
is overwritten.

  my $out;
  run3 \@cmd, \undef, \$out;           # child writes into string 
  run3 \@cmd, \<<EOF;                  # child reads from string (can use "here" notation)
  Input
  to 
  child
  EOF

=item an ARRAY reference 

For C<$stdin>, the elements of C<@$stdin> are simply spooled to the child.

For C<$stdout> or C<$stderr>, the child's corresponding file descriptor 
is read line by line (as determined by the current setting of C<$/>)
into C<@$stdout> or C<@$stderr>, resp. The previous content of the array
is overwritten.

  my @lines;
  run3 \@cmd, \undef, \@lines;         # child writes into array

=item a CODE reference 

For C<$stdin>, C<&$stdin> will be called repeatedly (with no arguments) and 
the return values are spooled to the child. C<&$stdin> must signal the end of
input by returning C<undef>. 

For C<$stdout> or C<$stderr>, the child's corresponding file descriptor 
is read line by line (as determined by the current setting of C<$/>)
and C<&$stdout> or C<&$stderr>, resp., is called with the contents of the line.
Note that there's no end-of-file indication.

  my $i = 0;
  sub producer {
    return $i < 10 ? "line".$i++."\n" : undef;
  }
    
  run3 \@cmd, \&producer;              # child reads 10 lines

Note that this form of redirecting the child's I/O doesn't imply
any form of concurrency between parent and child - run3()'s method of
operation is the same no matter which form of redirection you specify.

=back

If the same value is passed for C<$stdout> and C<$stderr>, then the child
will write both C<STDOUT> and C<STDERR> to the same filehandle.
In general, this means that

    run3 \@cmd, \undef, "foo.txt", "foo.txt";
    run3 \@cmd, \undef, \$both, \$both;

will DWIM and pass a single file handle to the child for both C<STDOUT> and
C<STDERR>, collecting all into file "foo.txt" or C<$both>.

=head3 C<\%options>

The last parameter, C<\%options>, must be a hash reference if present. 

Currently the following
keys are supported: 

=over 4

=item C<binmode_stdin>, C<binmode_stdout>, C<binmode_stderr>

If their value is true then the corresponding
parameter C<$stdin>, C<$stdout> or C<$stderr>, resp., operates
in "binary" mode (cf. L<perlfunc/binmode>).
The default is to operate in "text" mode.
(This is only relevant for platforms where these modes differ.)

=item C<append_stdout>, C<append_stderr>

If their value is true then the corresponding
parameter C<$stdout> or C<$stderr>, resp., will append the child's output
to the existing "contents" of the redirector. This only makes
sense if the redirector is a simple scalar (the corresponding file
is opened in append mode), a SCALAR reference (the output is 
appended to the previous contents of the string) 
or an ARRAY reference (the output is C<push>ed onto the 
previous contents of the array).

=back 

=head1 HOW IT WORKS

=over 4

=item (1)

For each redirector C<$stdin>, C<$stdout>, and C<$stderr>, 
C<run3()> furnishes a filehandle:

=over 4

=item *

if the redirector already specifies a filehandle it just uses that

=item *

if the redirector specifies a filename, C<run3()> opens the file
in the appropriate mode

=item *

in all other cases, C<run3()> opens a temporary file 
(using L<tempfile|Temp/tempfile>)

=back

=item (2)

If C<run3()> opened a temporary file for C<$stdin> in step (1),
it writes the data using the specified method (either
from a string, an array or returnd by a function) to the temporary file and rewinds it.

=item (3)

C<run3()> saves the parent's C<STDIN>, C<STDOUT> and C<STDERR> by duplicating
them to new filehandles. It duplicates the filehandles from step (1)
to C<STDIN>, C<STDOUT> and C<STDERR>, resp.

=item (4)

C<run3()> runs the child by invoking L<system|perlfunc/system> 
with C<$cmd> as specified above.

=item (5)

C<run3()> restores the parent's C<STDIN>, C<STDOUT> and C<STDERR> saved in step (3).

=item (6)

If C<run3()> opened a temporary file for C<$stdout> or C<$stderr> in step (1),
it rewinds it and reads back its contents using the specified method 
(either to a string, an array or by calling a function).

=item (7)

C<run3()> closes all filehandles that it opened explicitly in step (1).

=back

Note that when using temporary files, C<run3()> tries to amortize the overhead
by reusing them (i.e. it keeps them open and rewinds and truncates them
before the next operation).

=head1 LIMITATIONS

Often uses intermediate files (determined by File::Temp, and thus by the
File::Spec defaults and the TMPDIR env. variable) for speed, portability and
simplicity.

Use extrem caution when using C<run3> in a threaded environment if
concurrent calls of C<run3> are possible. Most likely, I/O from different
invocations will get mixed up. The reason is that in most thread 
implementations all threads in a process share the same STDIN/STDOUT/STDERR.
Known failures are Perl ithreads on Linux and Win32. Note that C<fork>
on Win32 is emulated via Win32 threads and hence I/O mix up is possible
between forked children here (C<run3> is "fork safe" on Unix, though).

=head1 DEBUGGING

To enable debugging use the IPCRUN3DEBUG environment variable to
a non-zero integer value:

  $ IPCRUN3DEBUG=1 myapp

=head1 PROFILING

To enable profiling, set IPCRUN3PROFILE to a number to enable emitting profile
information to STDERR (1 to get timestamps, 2 to get a summary report at the
END of the program, 3 to get mini reports after each run) or to a filename to
emit raw data to a file for later analysis.

=head1 COMPARISON

Here's how it stacks up to existing APIs:

=head2 compared to C<system()>, C<qx''>, C<open "...|">, C<open "|...">

=over

=item + 

redirects more than one file descriptor

=item + 

returns TRUE on success, FALSE on failure

=item + 

throws an error if problems occur in the parent process (or the pre-exec child)

=item + 

allows a very perlish interface to Perl data structures and subroutines

=item + 

allows 1 word invocations to avoid the shell easily:

 run3 ["foo"];  # does not invoke shell

=item - 

does not return the exit code, leaves it in $?

=back

=head2 compared to C<open2()>, C<open3()>

=over

=item + 

no lengthy, error prone polling/select loop needed

=item +

hides OS dependancies

=item + 

allows SCALAR, ARRAY, and CODE references to source and sink I/O

=item + 

I/O parameter order is like C<open3()>  (not like C<open2()>).

=item - 

does not allow interaction with the subprocess

=back

=head2 compared to L<IPC::Run::run()|IPC::Run/run>

=over

=item + 

smaller, lower overhead, simpler, more portable

=item +

no select() loop portability issues

=item +

does not fall prey to Perl closure leaks

=item -

does not allow interaction with the subprocess (which
IPC::Run::run() allows by redirecting subroutines)

=item -

lacks many features of C<IPC::Run::run()> (filters, pipes,
redirects, pty support)

=back

=head1 COPYRIGHT

Copyright 2003, R. Barrie Slaymaker, Jr., All Rights Reserved

=head1 LICENSE

You may use this module under the terms of the BSD, Artistic, or GPL licenses,
any version.

=head1 AUTHOR

Barrie Slaymaker E<lt>C<barries@slaysys.com>E<gt>

Ricardo SIGNES E<lt>C<rjbs@cpan.org>E<gt> performed some routine maintenance in
2005, thanks to help from the following ticket and/or patch submitters: Jody
Belka, Roderich Schupp, David Morel, and anonymous others.

=cut
