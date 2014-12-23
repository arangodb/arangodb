# IO::Socket::INET.pm
#
# Copyright (c) 1997-8 Graham Barr <gbarr@pobox.com>. All rights reserved.
# This program is free software; you can redistribute it and/or
# modify it under the same terms as Perl itself.

package IO::Socket::INET;

use strict;
our(@ISA, $VERSION);
use IO::Socket;
use Socket;
use Carp;
use Exporter;
use Errno;

@ISA = qw(IO::Socket);
$VERSION = "1.31";

my $EINVAL = exists(&Errno::EINVAL) ? Errno::EINVAL() : 1;

IO::Socket::INET->register_domain( AF_INET );

my %socket_type = ( tcp  => SOCK_STREAM,
		    udp  => SOCK_DGRAM,
		    icmp => SOCK_RAW
		  );
my %proto_number;
$proto_number{tcp}  = Socket::IPPROTO_TCP()  if defined &Socket::IPPROTO_TCP;
$proto_number{udp}  = Socket::IPPROTO_UDP()  if defined &Socket::IPPROTO_UDP;
$proto_number{icmp} = Socket::IPPROTO_ICMP() if defined &Socket::IPPROTO_ICMP;
my %proto_name = reverse %proto_number;

sub new {
    my $class = shift;
    unshift(@_, "PeerAddr") if @_ == 1;
    return $class->SUPER::new(@_);
}

sub _cache_proto {
    my @proto = @_;
    for (map lc($_), $proto[0], split(' ', $proto[1])) {
	$proto_number{$_} = $proto[2];
    }
    $proto_name{$proto[2]} = $proto[0];
}

sub _get_proto_number {
    my $name = lc(shift);
    return undef unless defined $name;
    return $proto_number{$name} if exists $proto_number{$name};

    my @proto = getprotobyname($name);
    return undef unless @proto;
    _cache_proto(@proto);

    return $proto[2];
}

sub _get_proto_name {
    my $num = shift;
    return undef unless defined $num;
    return $proto_name{$num} if exists $proto_name{$num};

    my @proto = getprotobynumber($num);
    return undef unless @proto;
    _cache_proto(@proto);

    return $proto[0];
}

sub _sock_info {
  my($addr,$port,$proto) = @_;
  my $origport = $port;
  my @serv = ();

  $port = $1
	if(defined $addr && $addr =~ s,:([\w\(\)/]+)$,,);

  if(defined $proto  && $proto =~ /\D/) {
    my $num = _get_proto_number($proto);
    unless (defined $num) {
      $@ = "Bad protocol '$proto'";
      return;
    }
    $proto = $num;
  }

  if(defined $port) {
    my $defport = ($port =~ s,\((\d+)\)$,,) ? $1 : undef;
    my $pnum = ($port =~ m,^(\d+)$,)[0];

    @serv = getservbyname($port, _get_proto_name($proto) || "")
	if ($port =~ m,\D,);

    $port = $serv[2] || $defport || $pnum;
    unless (defined $port) {
	$@ = "Bad service '$origport'";
	return;
    }

    $proto = _get_proto_number($serv[3]) if @serv && !$proto;
  }

 return ($addr || undef,
	 $port || undef,
	 $proto || undef
	);
}

sub _error {
    my $sock = shift;
    my $err = shift;
    {
      local($!);
      my $title = ref($sock).": ";
      $@ = join("", $_[0] =~ /^$title/ ? "" : $title, @_);
      $sock->close()
	if(defined fileno($sock));
    }
    $! = $err;
    return undef;
}

sub _get_addr {
    my($sock,$addr_str, $multi) = @_;
    my @addr;
    if ($multi && $addr_str !~ /^\d+(?:\.\d+){3}$/) {
	(undef, undef, undef, undef, @addr) = gethostbyname($addr_str);
    } else {
	my $h = inet_aton($addr_str);
	push(@addr, $h) if defined $h;
    }
    @addr;
}

sub configure {
    my($sock,$arg) = @_;
    my($lport,$rport,$laddr,$raddr,$proto,$type);


    $arg->{LocalAddr} = $arg->{LocalHost}
	if exists $arg->{LocalHost} && !exists $arg->{LocalAddr};

    ($laddr,$lport,$proto) = _sock_info($arg->{LocalAddr},
					$arg->{LocalPort},
					$arg->{Proto})
			or return _error($sock, $!, $@);

    $laddr = defined $laddr ? inet_aton($laddr)
			    : INADDR_ANY;

    return _error($sock, $EINVAL, "Bad hostname '",$arg->{LocalAddr},"'")
	unless(defined $laddr);

    $arg->{PeerAddr} = $arg->{PeerHost}
	if exists $arg->{PeerHost} && !exists $arg->{PeerAddr};

    unless(exists $arg->{Listen}) {
	($raddr,$rport,$proto) = _sock_info($arg->{PeerAddr},
					    $arg->{PeerPort},
					    $proto)
			or return _error($sock, $!, $@);
    }

    $proto ||= _get_proto_number('tcp');

    $type = $arg->{Type} || $socket_type{lc _get_proto_name($proto)};

    my @raddr = ();

    if(defined $raddr) {
	@raddr = $sock->_get_addr($raddr, $arg->{MultiHomed});
	return _error($sock, $EINVAL, "Bad hostname '",$arg->{PeerAddr},"'")
	    unless @raddr;
    }

    while(1) {

	$sock->socket(AF_INET, $type, $proto) or
	    return _error($sock, $!, "$!");

        if (defined $arg->{Blocking}) {
	    defined $sock->blocking($arg->{Blocking})
		or return _error($sock, $!, "$!");
	}

	if ($arg->{Reuse} || $arg->{ReuseAddr}) {
	    $sock->sockopt(SO_REUSEADDR,1) or
		    return _error($sock, $!, "$!");
	}

	if ($arg->{ReusePort}) {
	    $sock->sockopt(SO_REUSEPORT,1) or
		    return _error($sock, $!, "$!");
	}

	if ($arg->{Broadcast}) {
		$sock->sockopt(SO_BROADCAST,1) or
		    return _error($sock, $!, "$!");
	}

	if($lport || ($laddr ne INADDR_ANY) || exists $arg->{Listen}) {
	    $sock->bind($lport || 0, $laddr) or
		    return _error($sock, $!, "$!");
	}

	if(exists $arg->{Listen}) {
	    $sock->listen($arg->{Listen} || 5) or
		return _error($sock, $!, "$!");
	    last;
	}

 	# don't try to connect unless we're given a PeerAddr
 	last unless exists($arg->{PeerAddr});
 
        $raddr = shift @raddr;

	return _error($sock, $EINVAL, 'Cannot determine remote port')
		unless($rport || $type == SOCK_DGRAM || $type == SOCK_RAW);

	last
	    unless($type == SOCK_STREAM || defined $raddr);

	return _error($sock, $EINVAL, "Bad hostname '",$arg->{PeerAddr},"'")
	    unless defined $raddr;

#        my $timeout = ${*$sock}{'io_socket_timeout'};
#        my $before = time() if $timeout;

	undef $@;
        if ($sock->connect(pack_sockaddr_in($rport, $raddr))) {
#            ${*$sock}{'io_socket_timeout'} = $timeout;
            return $sock;
        }

	return _error($sock, $!, $@ || "Timeout")
	    unless @raddr;

#	if ($timeout) {
#	    my $new_timeout = $timeout - (time() - $before);
#	    return _error($sock,
#                         (exists(&Errno::ETIMEDOUT) ? Errno::ETIMEDOUT() : $EINVAL),
#                         "Timeout") if $new_timeout <= 0;
#	    ${*$sock}{'io_socket_timeout'} = $new_timeout;
#        }

    }

    $sock;
}

sub connect {
    @_ == 2 || @_ == 3 or
       croak 'usage: $sock->connect(NAME) or $sock->connect(PORT, ADDR)';
    my $sock = shift;
    return $sock->SUPER::connect(@_ == 1 ? shift : pack_sockaddr_in(@_));
}

sub bind {
    @_ == 2 || @_ == 3 or
       croak 'usage: $sock->bind(NAME) or $sock->bind(PORT, ADDR)';
    my $sock = shift;
    return $sock->SUPER::bind(@_ == 1 ? shift : pack_sockaddr_in(@_))
}

sub sockaddr {
    @_ == 1 or croak 'usage: $sock->sockaddr()';
    my($sock) = @_;
    my $name = $sock->sockname;
    $name ? (sockaddr_in($name))[1] : undef;
}

sub sockport {
    @_ == 1 or croak 'usage: $sock->sockport()';
    my($sock) = @_;
    my $name = $sock->sockname;
    $name ? (sockaddr_in($name))[0] : undef;
}

sub sockhost {
    @_ == 1 or croak 'usage: $sock->sockhost()';
    my($sock) = @_;
    my $addr = $sock->sockaddr;
    $addr ? inet_ntoa($addr) : undef;
}

sub peeraddr {
    @_ == 1 or croak 'usage: $sock->peeraddr()';
    my($sock) = @_;
    my $name = $sock->peername;
    $name ? (sockaddr_in($name))[1] : undef;
}

sub peerport {
    @_ == 1 or croak 'usage: $sock->peerport()';
    my($sock) = @_;
    my $name = $sock->peername;
    $name ? (sockaddr_in($name))[0] : undef;
}

sub peerhost {
    @_ == 1 or croak 'usage: $sock->peerhost()';
    my($sock) = @_;
    my $addr = $sock->peeraddr;
    $addr ? inet_ntoa($addr) : undef;
}

1;

__END__

=head1 NAME

IO::Socket::INET - Object interface for AF_INET domain sockets

=head1 SYNOPSIS

    use IO::Socket::INET;

=head1 DESCRIPTION

C<IO::Socket::INET> provides an object interface to creating and using sockets
in the AF_INET domain. It is built upon the L<IO::Socket> interface and
inherits all the methods defined by L<IO::Socket>.

=head1 CONSTRUCTOR

=over 4

=item new ( [ARGS] )

Creates an C<IO::Socket::INET> object, which is a reference to a
newly created symbol (see the C<Symbol> package). C<new>
optionally takes arguments, these arguments are in key-value pairs.

In addition to the key-value pairs accepted by L<IO::Socket>,
C<IO::Socket::INET> provides.


    PeerAddr	Remote host address          <hostname>[:<port>]
    PeerHost	Synonym for PeerAddr
    PeerPort	Remote port or service       <service>[(<no>)] | <no>
    LocalAddr	Local host bind	address      hostname[:port]
    LocalHost	Synonym for LocalAddr
    LocalPort	Local host bind	port         <service>[(<no>)] | <no>
    Proto	Protocol name (or number)    "tcp" | "udp" | ...
    Type	Socket type                  SOCK_STREAM | SOCK_DGRAM | ...
    Listen	Queue size for listen
    ReuseAddr	Set SO_REUSEADDR before binding
    Reuse	Set SO_REUSEADDR before binding (deprecated, prefer ReuseAddr)
    ReusePort	Set SO_REUSEPORT before binding
    Broadcast	Set SO_BROADCAST before binding
    Timeout	Timeout	value for various operations
    MultiHomed  Try all addresses for multi-homed hosts
    Blocking    Determine if connection will be blocking mode

If C<Listen> is defined then a listen socket is created, else if the
socket type, which is derived from the protocol, is SOCK_STREAM then
connect() is called.

Although it is not illegal, the use of C<MultiHomed> on a socket
which is in non-blocking mode is of little use. This is because the
first connect will never fail with a timeout as the connect call
will not block.

The C<PeerAddr> can be a hostname or the IP-address on the
"xx.xx.xx.xx" form.  The C<PeerPort> can be a number or a symbolic
service name.  The service name might be followed by a number in
parenthesis which is used if the service is not known by the system.
The C<PeerPort> specification can also be embedded in the C<PeerAddr>
by preceding it with a ":".

If C<Proto> is not given and you specify a symbolic C<PeerPort> port,
then the constructor will try to derive C<Proto> from the service
name.  As a last resort C<Proto> "tcp" is assumed.  The C<Type>
parameter will be deduced from C<Proto> if not specified.

If the constructor is only passed a single argument, it is assumed to
be a C<PeerAddr> specification.

If C<Blocking> is set to 0, the connection will be in nonblocking mode.
If not specified it defaults to 1 (blocking mode).

Examples:

   $sock = IO::Socket::INET->new(PeerAddr => 'www.perl.org',
                                 PeerPort => 'http(80)',
                                 Proto    => 'tcp');

   $sock = IO::Socket::INET->new(PeerAddr => 'localhost:smtp(25)');

   $sock = IO::Socket::INET->new(Listen    => 5,
                                 LocalAddr => 'localhost',
                                 LocalPort => 9000,
                                 Proto     => 'tcp');

   $sock = IO::Socket::INET->new('127.0.0.1:25');

   $sock = IO::Socket::INET->new(PeerPort  => 9999,
                                 PeerAddr  => inet_ntoa(INADDR_BROADCAST),
                                 Proto     => udp,    
                                 LocalAddr => 'localhost',
                                 Broadcast => 1 ) 
                             or die "Can't bind : $@\n";

 NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE

As of VERSION 1.18 all IO::Socket objects have autoflush turned on
by default. This was not the case with earlier releases.

 NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE

=back

=head2 METHODS

=over 4

=item sockaddr ()

Return the address part of the sockaddr structure for the socket

=item sockport ()

Return the port number that the socket is using on the local host

=item sockhost ()

Return the address part of the sockaddr structure for the socket in a
text form xx.xx.xx.xx

=item peeraddr ()

Return the address part of the sockaddr structure for the socket on
the peer host

=item peerport ()

Return the port number for the socket on the peer host.

=item peerhost ()

Return the address part of the sockaddr structure for the socket on the
peer host in a text form xx.xx.xx.xx

=back

=head1 SEE ALSO

L<Socket>, L<IO::Socket>

=head1 AUTHOR

Graham Barr. Currently maintained by the Perl Porters.  Please report all
bugs to <perl5-porters@perl.org>.

=head1 COPYRIGHT

Copyright (c) 1996-8 Graham Barr <gbarr@pobox.com>. All rights reserved.
This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=cut
