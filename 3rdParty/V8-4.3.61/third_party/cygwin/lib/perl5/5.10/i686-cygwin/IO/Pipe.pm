# IO::Pipe.pm
#
# Copyright (c) 1996-8 Graham Barr <gbarr@pobox.com>. All rights reserved.
# This program is free software; you can redistribute it and/or
# modify it under the same terms as Perl itself.

package IO::Pipe;

use 5.006_001;

use IO::Handle;
use strict;
our($VERSION);
use Carp;
use Symbol;

$VERSION = "1.13";

sub new {
    my $type = shift;
    my $class = ref($type) || $type || "IO::Pipe";
    @_ == 0 || @_ == 2 or croak "usage: new $class [READFH, WRITEFH]";

    my $me = bless gensym(), $class;

    my($readfh,$writefh) = @_ ? @_ : $me->handles;

    pipe($readfh, $writefh)
	or return undef;

    @{*$me} = ($readfh, $writefh);

    $me;
}

sub handles {
    @_ == 1 or croak 'usage: $pipe->handles()';
    (IO::Pipe::End->new(), IO::Pipe::End->new());
}

my $do_spawn = $^O eq 'os2' || $^O eq 'MSWin32';

sub _doit {
    my $me = shift;
    my $rw = shift;

    my $pid = $do_spawn ? 0 : fork();

    if($pid) { # Parent
        return $pid;
    }
    elsif(defined $pid) { # Child or spawn
        my $fh;
        my $io = $rw ? \*STDIN : \*STDOUT;
        my ($mode, $save) = $rw ? "r" : "w";
        if ($do_spawn) {
          require Fcntl;
          $save = IO::Handle->new_from_fd($io, $mode);
	  my $handle = shift;
          # Close in child:
	  unless ($^O eq 'MSWin32') {
            fcntl($handle, Fcntl::F_SETFD(), 1) or croak "fcntl: $!";
	  }
          $fh = $rw ? ${*$me}[0] : ${*$me}[1];
        } else {
          shift;
          $fh = $rw ? $me->reader() : $me->writer(); # close the other end
        }
        bless $io, "IO::Handle";
        $io->fdopen($fh, $mode);
	$fh->close;

        if ($do_spawn) {
          $pid = eval { system 1, @_ }; # 1 == P_NOWAIT
          my $err = $!;
    
          $io->fdopen($save, $mode);
          $save->close or croak "Cannot close $!";
          croak "IO::Pipe: Cannot spawn-NOWAIT: $err" if not $pid or $pid < 0;
          return $pid;
        } else {
          exec @_ or
            croak "IO::Pipe: Cannot exec: $!";
        }
    }
    else {
        croak "IO::Pipe: Cannot fork: $!";
    }

    # NOT Reached
}

sub reader {
    @_ >= 1 or croak 'usage: $pipe->reader( [SUB_COMMAND_ARGS] )';
    my $me = shift;

    return undef
	unless(ref($me) || ref($me = $me->new));

    my $fh  = ${*$me}[0];
    my $pid;
    $pid = $me->_doit(0, $fh, @_)
        if(@_);

    close ${*$me}[1];
    bless $me, ref($fh);
    *$me = *$fh;          # Alias self to handle
    $me->fdopen($fh->fileno,"r")
	unless defined($me->fileno);
    bless $fh;                  # Really wan't un-bless here
    ${*$me}{'io_pipe_pid'} = $pid
        if defined $pid;

    $me;
}

sub writer {
    @_ >= 1 or croak 'usage: $pipe->writer( [SUB_COMMAND_ARGS] )';
    my $me = shift;

    return undef
	unless(ref($me) || ref($me = $me->new));

    my $fh  = ${*$me}[1];
    my $pid;
    $pid = $me->_doit(1, $fh, @_)
        if(@_);

    close ${*$me}[0];
    bless $me, ref($fh);
    *$me = *$fh;          # Alias self to handle
    $me->fdopen($fh->fileno,"w")
	unless defined($me->fileno);
    bless $fh;                  # Really wan't un-bless here
    ${*$me}{'io_pipe_pid'} = $pid
        if defined $pid;

    $me;
}

package IO::Pipe::End;

our(@ISA);

@ISA = qw(IO::Handle);

sub close {
    my $fh = shift;
    my $r = $fh->SUPER::close(@_);

    waitpid(${*$fh}{'io_pipe_pid'},0)
	if(defined ${*$fh}{'io_pipe_pid'});

    $r;
}

1;

__END__

=head1 NAME

IO::Pipe - supply object methods for pipes

=head1 SYNOPSIS

	use IO::Pipe;

	$pipe = new IO::Pipe;

	if($pid = fork()) { # Parent
	    $pipe->reader();

	    while(<$pipe>) {
		...
	    }

	}
	elsif(defined $pid) { # Child
	    $pipe->writer();

	    print $pipe ...
	}

	or

	$pipe = new IO::Pipe;

	$pipe->reader(qw(ls -l));

	while(<$pipe>) {
	    ...
	}

=head1 DESCRIPTION

C<IO::Pipe> provides an interface to creating pipes between
processes.

=head1 CONSTRUCTOR

=over 4

=item new ( [READER, WRITER] )

Creates an C<IO::Pipe>, which is a reference to a newly created symbol
(see the C<Symbol> package). C<IO::Pipe::new> optionally takes two
arguments, which should be objects blessed into C<IO::Handle>, or a
subclass thereof. These two objects will be used for the system call
to C<pipe>. If no arguments are given then method C<handles> is called
on the new C<IO::Pipe> object.

These two handles are held in the array part of the GLOB until either
C<reader> or C<writer> is called.

=back

=head1 METHODS

=over 4

=item reader ([ARGS])

The object is re-blessed into a sub-class of C<IO::Handle>, and becomes a
handle at the reading end of the pipe. If C<ARGS> are given then C<fork>
is called and C<ARGS> are passed to exec.

=item writer ([ARGS])

The object is re-blessed into a sub-class of C<IO::Handle>, and becomes a
handle at the writing end of the pipe. If C<ARGS> are given then C<fork>
is called and C<ARGS> are passed to exec.

=item handles ()

This method is called during construction by C<IO::Pipe::new>
on the newly created C<IO::Pipe> object. It returns an array of two objects
blessed into C<IO::Pipe::End>, or a subclass thereof.

=back

=head1 SEE ALSO

L<IO::Handle>

=head1 AUTHOR

Graham Barr. Currently maintained by the Perl Porters.  Please report all
bugs to <perl5-porters@perl.org>.

=head1 COPYRIGHT

Copyright (c) 1996-8 Graham Barr <gbarr@pobox.com>. All rights reserved.
This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=cut
