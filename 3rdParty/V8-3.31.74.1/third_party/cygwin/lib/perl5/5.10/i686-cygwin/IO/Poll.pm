
# IO::Poll.pm
#
# Copyright (c) 1997-8 Graham Barr <gbarr@pobox.com>. All rights reserved.
# This program is free software; you can redistribute it and/or
# modify it under the same terms as Perl itself.

package IO::Poll;

use strict;
use IO::Handle;
use Exporter ();
our(@ISA, @EXPORT_OK, @EXPORT, $VERSION);

@ISA = qw(Exporter);
$VERSION = "0.07";

@EXPORT = qw( POLLIN
	      POLLOUT
	      POLLERR
	      POLLHUP
	      POLLNVAL
	    );

@EXPORT_OK = qw(
 POLLPRI   
 POLLRDNORM
 POLLWRNORM
 POLLRDBAND
 POLLWRBAND
 POLLNORM  
	       );

# [0] maps fd's to requested masks
# [1] maps fd's to returned  masks
# [2] maps fd's to handles
sub new {
    my $class = shift;

    my $self = bless [{},{},{}], $class;

    $self;
}

sub mask {
    my $self = shift;
    my $io = shift;
    my $fd = fileno($io);
    return unless defined $fd;
    if (@_) {
	my $mask = shift;
	if($mask) {
	  $self->[0]{$fd}{$io} = $mask; # the error events are always returned
	  $self->[1]{$fd}      = 0;     # output mask
	  $self->[2]{$io}      = $io;   # remember handle
	} else {
          delete $self->[0]{$fd}{$io};
          unless(%{$self->[0]{$fd}}) {
            # We no longer have any handles for this FD
            delete $self->[1]{$fd};
            delete $self->[0]{$fd};
          }
          delete $self->[2]{$io};
	}
    }
    
    return unless exists $self->[0]{$fd} and exists $self->[0]{$fd}{$io};
	return $self->[0]{$fd}{$io};
}


sub poll {
    my($self,$timeout) = @_;

    $self->[1] = {};

    my($fd,$mask,$iom);
    my @poll = ();

    while(($fd,$iom) = each %{$self->[0]}) {
	$mask   = 0;
	$mask  |= $_ for values(%$iom);
	push(@poll,$fd => $mask);
    }

    my $ret = @poll ? _poll(defined($timeout) ? $timeout * 1000 : -1,@poll) : 0;

    return $ret
	unless $ret > 0;

    while(@poll) {
	my($fd,$got) = splice(@poll,0,2);
	$self->[1]{$fd} = $got if $got;
    }

    return $ret;  
}

sub events {
    my $self = shift;
    my $io = shift;
    my $fd = fileno($io);
    exists $self->[1]{$fd} and exists $self->[0]{$fd}{$io} 
                ? $self->[1]{$fd} & ($self->[0]{$fd}{$io}|POLLHUP|POLLERR|POLLNVAL)
	: 0;
}

sub remove {
    my $self = shift;
    my $io = shift;
    $self->mask($io,0);
}

sub handles {
    my $self = shift;
    return values %{$self->[2]} unless @_;

    my $events = shift || 0;
    my($fd,$ev,$io,$mask);
    my @handles = ();

    while(($fd,$ev) = each %{$self->[1]}) {
	while (($io,$mask) = each %{$self->[0]{$fd}}) {
	    $mask |= POLLHUP|POLLERR|POLLNVAL;  # must allow these
	    push @handles,$self->[2]{$io} if ($ev & $mask) & $events;
	}
    }
    return @handles;
}

1;

__END__

=head1 NAME

IO::Poll - Object interface to system poll call

=head1 SYNOPSIS

    use IO::Poll qw(POLLRDNORM POLLWRNORM POLLIN POLLHUP);

    $poll = new IO::Poll;

    $poll->mask($input_handle => POLLIN);
    $poll->mask($output_handle => POLLOUT);

    $poll->poll($timeout);

    $ev = $poll->events($input);

=head1 DESCRIPTION

C<IO::Poll> is a simple interface to the system level poll routine.

=head1 METHODS

=over 4

=item mask ( IO [, EVENT_MASK ] )

If EVENT_MASK is given, then, if EVENT_MASK is non-zero, IO is added to the
list of file descriptors and the next call to poll will check for
any event specified in EVENT_MASK. If EVENT_MASK is zero then IO will be
removed from the list of file descriptors.

If EVENT_MASK is not given then the return value will be the current
event mask value for IO.

=item poll ( [ TIMEOUT ] )

Call the system level poll routine. If TIMEOUT is not specified then the
call will block. Returns the number of handles which had events
happen, or -1 on error.

=item events ( IO )

Returns the event mask which represents the events that happened on IO
during the last call to C<poll>.

=item remove ( IO )

Remove IO from the list of file descriptors for the next poll.

=item handles( [ EVENT_MASK ] )

Returns a list of handles. If EVENT_MASK is not given then a list of all
handles known will be returned. If EVENT_MASK is given then a list
of handles will be returned which had one of the events specified by
EVENT_MASK happen during the last call ti C<poll>

=back

=head1 SEE ALSO

L<poll(2)>, L<IO::Handle>, L<IO::Select>

=head1 AUTHOR

Graham Barr. Currently maintained by the Perl Porters.  Please report all
bugs to <perl5-porters@perl.org>.

=head1 COPYRIGHT

Copyright (c) 1997-8 Graham Barr <gbarr@pobox.com>. All rights reserved.
This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=cut
