# IO::Socket.pm
#
# Copyright (c) 1997-8 Graham Barr <gbarr@pobox.com>. All rights reserved.
# This program is free software; you can redistribute it and/or
# modify it under the same terms as Perl itself.

package IO::Socket;

require 5.006;

use IO::Handle;
use Socket 1.3;
use Carp;
use strict;
our(@ISA, $VERSION, @EXPORT_OK);
use Exporter;
use Errno;

# legacy

require IO::Socket::INET;
require IO::Socket::UNIX if ($^O ne 'epoc' && $^O ne 'symbian');

@ISA = qw(IO::Handle);

$VERSION = "1.30_01";

@EXPORT_OK = qw(sockatmark);

sub import {
    my $pkg = shift;
    if (@_ && $_[0] eq 'sockatmark') { # not very extensible but for now, fast
	Exporter::export_to_level('IO::Socket', 1, $pkg, 'sockatmark');
    } else {
	my $callpkg = caller;
	Exporter::export 'Socket', $callpkg, @_;
    }
}

sub new {
    my($class,%arg) = @_;
    my $sock = $class->SUPER::new();

    $sock->autoflush(1);

    ${*$sock}{'io_socket_timeout'} = delete $arg{Timeout};

    return scalar(%arg) ? $sock->configure(\%arg)
			: $sock;
}

my @domain2pkg;

sub register_domain {
    my($p,$d) = @_;
    $domain2pkg[$d] = $p;
}

sub configure {
    my($sock,$arg) = @_;
    my $domain = delete $arg->{Domain};

    croak 'IO::Socket: Cannot configure a generic socket'
	unless defined $domain;

    croak "IO::Socket: Unsupported socket domain"
	unless defined $domain2pkg[$domain];

    croak "IO::Socket: Cannot configure socket in domain '$domain'"
	unless ref($sock) eq "IO::Socket";

    bless($sock, $domain2pkg[$domain]);
    $sock->configure($arg);
}

sub socket {
    @_ == 4 or croak 'usage: $sock->socket(DOMAIN, TYPE, PROTOCOL)';
    my($sock,$domain,$type,$protocol) = @_;

    socket($sock,$domain,$type,$protocol) or
    	return undef;

    ${*$sock}{'io_socket_domain'} = $domain;
    ${*$sock}{'io_socket_type'}   = $type;
    ${*$sock}{'io_socket_proto'}  = $protocol;

    $sock;
}

sub socketpair {
    @_ == 4 || croak 'usage: IO::Socket->socketpair(DOMAIN, TYPE, PROTOCOL)';
    my($class,$domain,$type,$protocol) = @_;
    my $sock1 = $class->new();
    my $sock2 = $class->new();

    socketpair($sock1,$sock2,$domain,$type,$protocol) or
    	return ();

    ${*$sock1}{'io_socket_type'}  = ${*$sock2}{'io_socket_type'}  = $type;
    ${*$sock1}{'io_socket_proto'} = ${*$sock2}{'io_socket_proto'} = $protocol;

    ($sock1,$sock2);
}

sub connect {
    @_ == 2 or croak 'usage: $sock->connect(NAME)';
    my $sock = shift;
    my $addr = shift;
    my $timeout = ${*$sock}{'io_socket_timeout'};
    my $err;
    my $blocking;

    $blocking = $sock->blocking(0) if $timeout;
    if (!connect($sock, $addr)) {
	if (defined $timeout && ($!{EINPROGRESS} || $!{EWOULDBLOCK})) {
	    require IO::Select;

	    my $sel = new IO::Select $sock;

	    undef $!;
	    if (!$sel->can_write($timeout)) {
		$err = $! || (exists &Errno::ETIMEDOUT ? &Errno::ETIMEDOUT : 1);
		$@ = "connect: timeout";
	    }
	    elsif (!connect($sock,$addr) &&
                not ($!{EISCONN} || ($! == 10022 && $^O eq 'MSWin32'))
            ) {
		# Some systems refuse to re-connect() to
		# an already open socket and set errno to EISCONN.
		# Windows sets errno to WSAEINVAL (10022)
		$err = $!;
		$@ = "connect: $!";
	    }
	}
        elsif ($blocking || !($!{EINPROGRESS} || $!{EWOULDBLOCK}))  {
	    $err = $!;
	    $@ = "connect: $!";
	}
    }

    $sock->blocking(1) if $blocking;

    $! = $err if $err;

    $err ? undef : $sock;
}

# Enable/disable blocking IO on sockets.
# Without args return the current status of blocking,
# with args change the mode as appropriate, returning the
# old setting, or in case of error during the mode change
# undef.

sub blocking {
    my $sock = shift;

    return $sock->SUPER::blocking(@_)
        if $^O ne 'MSWin32';

    # Windows handles blocking differently
    #
    # http://groups.google.co.uk/group/perl.perl5.porters/browse_thread/thread/b4e2b1d88280ddff/630b667a66e3509f?#630b667a66e3509f
    # http://msdn.microsoft.com/library/default.asp?url=/library/en-us/winsock/winsock/ioctlsocket_2.asp
    #
    # 0x8004667e is FIONBIO
    #
    # which is used to set blocking behaviour.

    # NOTE: 
    # This is a little confusing, the perl keyword for this is
    # 'blocking' but the OS level behaviour is 'non-blocking', probably
    # because sockets are blocking by default.
    # Therefore internally we have to reverse the semantics.

    my $orig= !${*$sock}{io_sock_nonblocking};
        
    return $orig unless @_;

    my $block = shift;
    
    if ( !$block != !$orig ) {
        ${*$sock}{io_sock_nonblocking} = $block ? 0 : 1;
        ioctl($sock, 0x8004667e, pack("L!",${*$sock}{io_sock_nonblocking}))
            or return undef;
    }
    
    return $orig;        
}


sub close {
    @_ == 1 or croak 'usage: $sock->close()';
    my $sock = shift;
    ${*$sock}{'io_socket_peername'} = undef;
    $sock->SUPER::close();
}

sub bind {
    @_ == 2 or croak 'usage: $sock->bind(NAME)';
    my $sock = shift;
    my $addr = shift;

    return bind($sock, $addr) ? $sock
			      : undef;
}

sub listen {
    @_ >= 1 && @_ <= 2 or croak 'usage: $sock->listen([QUEUE])';
    my($sock,$queue) = @_;
    $queue = 5
	unless $queue && $queue > 0;

    return listen($sock, $queue) ? $sock
				 : undef;
}

sub accept {
    @_ == 1 || @_ == 2 or croak 'usage $sock->accept([PKG])';
    my $sock = shift;
    my $pkg = shift || $sock;
    my $timeout = ${*$sock}{'io_socket_timeout'};
    my $new = $pkg->new(Timeout => $timeout);
    my $peer = undef;

    if(defined $timeout) {
	require IO::Select;

	my $sel = new IO::Select $sock;

	unless ($sel->can_read($timeout)) {
	    $@ = 'accept: timeout';
	    $! = (exists &Errno::ETIMEDOUT ? &Errno::ETIMEDOUT : 1);
	    return;
	}
    }

    $peer = accept($new,$sock)
	or return;

    return wantarray ? ($new, $peer)
    	      	     : $new;
}

sub sockname {
    @_ == 1 or croak 'usage: $sock->sockname()';
    getsockname($_[0]);
}

sub peername {
    @_ == 1 or croak 'usage: $sock->peername()';
    my($sock) = @_;
    ${*$sock}{'io_socket_peername'} ||= getpeername($sock);
}

sub connected {
    @_ == 1 or croak 'usage: $sock->connected()';
    my($sock) = @_;
    getpeername($sock);
}

sub send {
    @_ >= 2 && @_ <= 4 or croak 'usage: $sock->send(BUF, [FLAGS, [TO]])';
    my $sock  = $_[0];
    my $flags = $_[2] || 0;
    my $peer  = $_[3] || $sock->peername;

    croak 'send: Cannot determine peer address'
	 unless(defined $peer);

    my $r = defined(getpeername($sock))
	? send($sock, $_[1], $flags)
	: send($sock, $_[1], $flags, $peer);

    # remember who we send to, if it was successful
    ${*$sock}{'io_socket_peername'} = $peer
	if(@_ == 4 && defined $r);

    $r;
}

sub recv {
    @_ == 3 || @_ == 4 or croak 'usage: $sock->recv(BUF, LEN [, FLAGS])';
    my $sock  = $_[0];
    my $len   = $_[2];
    my $flags = $_[3] || 0;

    # remember who we recv'd from
    ${*$sock}{'io_socket_peername'} = recv($sock, $_[1]='', $len, $flags);
}

sub shutdown {
    @_ == 2 or croak 'usage: $sock->shutdown(HOW)';
    my($sock, $how) = @_;
    ${*$sock}{'io_socket_peername'} = undef;
    shutdown($sock, $how);
}

sub setsockopt {
    @_ == 4 or croak '$sock->setsockopt(LEVEL, OPTNAME, OPTVAL)';
    setsockopt($_[0],$_[1],$_[2],$_[3]);
}

my $intsize = length(pack("i",0));

sub getsockopt {
    @_ == 3 or croak '$sock->getsockopt(LEVEL, OPTNAME)';
    my $r = getsockopt($_[0],$_[1],$_[2]);
    # Just a guess
    $r = unpack("i", $r)
	if(defined $r && length($r) == $intsize);
    $r;
}

sub sockopt {
    my $sock = shift;
    @_ == 1 ? $sock->getsockopt(SOL_SOCKET,@_)
	    : $sock->setsockopt(SOL_SOCKET,@_);
}

sub atmark {
    @_ == 1 or croak 'usage: $sock->atmark()';
    my($sock) = @_;
    sockatmark($sock);
}

sub timeout {
    @_ == 1 || @_ == 2 or croak 'usage: $sock->timeout([VALUE])';
    my($sock,$val) = @_;
    my $r = ${*$sock}{'io_socket_timeout'};

    ${*$sock}{'io_socket_timeout'} = defined $val ? 0 + $val : $val
	if(@_ == 2);

    $r;
}

sub sockdomain {
    @_ == 1 or croak 'usage: $sock->sockdomain()';
    my $sock = shift;
    ${*$sock}{'io_socket_domain'};
}

sub socktype {
    @_ == 1 or croak 'usage: $sock->socktype()';
    my $sock = shift;
    ${*$sock}{'io_socket_type'}
}

sub protocol {
    @_ == 1 or croak 'usage: $sock->protocol()';
    my($sock) = @_;
    ${*$sock}{'io_socket_proto'};
}

1;

__END__

=head1 NAME

IO::Socket - Object interface to socket communications

=head1 SYNOPSIS

    use IO::Socket;

=head1 DESCRIPTION

C<IO::Socket> provides an object interface to creating and using sockets. It
is built upon the L<IO::Handle> interface and inherits all the methods defined
by L<IO::Handle>.

C<IO::Socket> only defines methods for those operations which are common to all
types of socket. Operations which are specified to a socket in a particular 
domain have methods defined in sub classes of C<IO::Socket>

C<IO::Socket> will export all functions (and constants) defined by L<Socket>.

=head1 CONSTRUCTOR

=over 4

=item new ( [ARGS] )

Creates an C<IO::Socket>, which is a reference to a
newly created symbol (see the C<Symbol> package). C<new>
optionally takes arguments, these arguments are in key-value pairs.
C<new> only looks for one key C<Domain> which tells new which domain
the socket will be in. All other arguments will be passed to the
configuration method of the package for that domain, See below.

 NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE

As of VERSION 1.18 all IO::Socket objects have autoflush turned on
by default. This was not the case with earlier releases.

 NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE

=back

=head1 METHODS

See L<perlfunc> for complete descriptions of each of the following
supported C<IO::Socket> methods, which are just front ends for the
corresponding built-in functions:

    socket
    socketpair
    bind
    listen
    accept
    send
    recv
    peername (getpeername)
    sockname (getsockname)
    shutdown

Some methods take slightly different arguments to those defined in L<perlfunc>
in attempt to make the interface more flexible. These are

=over 4

=item accept([PKG])

perform the system call C<accept> on the socket and return a new
object. The new object will be created in the same class as the listen
socket, unless C<PKG> is specified. This object can be used to
communicate with the client that was trying to connect.

In a scalar context the new socket is returned, or undef upon
failure. In a list context a two-element array is returned containing
the new socket and the peer address; the list will be empty upon
failure.

The timeout in the [PKG] can be specified as zero to effect a "poll",
but you shouldn't do that because a new IO::Select object will be
created behind the scenes just to do the single poll.  This is
horrendously inefficient.  Use rather true select() with a zero
timeout on the handle, or non-blocking IO.

=item socketpair(DOMAIN, TYPE, PROTOCOL)

Call C<socketpair> and return a list of two sockets created, or an
empty list on failure.

=back

Additional methods that are provided are:

=over 4

=item atmark

True if the socket is currently positioned at the urgent data mark,
false otherwise.

    use IO::Socket;

    my $sock = IO::Socket::INET->new('some_server');
    $sock->read($data, 1024) until $sock->atmark;

Note: this is a reasonably new addition to the family of socket
functions, so all systems may not support this yet.  If it is
unsupported by the system, an attempt to use this method will
abort the program.

The atmark() functionality is also exportable as sockatmark() function:

	use IO::Socket 'sockatmark';

This allows for a more traditional use of sockatmark() as a procedural
socket function.  If your system does not support sockatmark(), the
C<use> declaration will fail at compile time.

=item connected

If the socket is in a connected state the peer address is returned.
If the socket is not in a connected state then undef will be returned.

=item protocol

Returns the numerical number for the protocol being used on the socket, if
known. If the protocol is unknown, as with an AF_UNIX socket, zero
is returned.

=item sockdomain

Returns the numerical number for the socket domain type. For example, for
an AF_INET socket the value of &AF_INET will be returned.

=item sockopt(OPT [, VAL])

Unified method to both set and get options in the SOL_SOCKET level. If called
with one argument then getsockopt is called, otherwise setsockopt is called.

=item socktype

Returns the numerical number for the socket type. For example, for
a SOCK_STREAM socket the value of &SOCK_STREAM will be returned.

=item timeout([VAL])

Set or get the timeout value associated with this socket. If called without
any arguments then the current setting is returned. If called with an argument
the current setting is changed and the previous value returned.

=back

=head1 SEE ALSO

L<Socket>, L<IO::Handle>, L<IO::Socket::INET>, L<IO::Socket::UNIX>

=head1 AUTHOR

Graham Barr.  atmark() by Lincoln Stein.  Currently maintained by the
Perl Porters.  Please report all bugs to <perl5-porters@perl.org>.

=head1 COPYRIGHT

Copyright (c) 1997-8 Graham Barr <gbarr@pobox.com>. All rights reserved.
This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

The atmark() implementation: Copyright 2001, Lincoln Stein <lstein@cshl.org>.
This module is distributed under the same terms as Perl itself.
Feel free to use, modify and redistribute it as long as you retain
the correct attribution.

=cut
