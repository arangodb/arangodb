package LWP::Protocol::https10;

use strict;

# Figure out which SSL implementation to use
use vars qw($SSL_CLASS);
if ($Net::SSL::VERSION) {
    $SSL_CLASS = "Net::SSL";
}
elsif ($IO::Socket::SSL::VERSION) {
    $SSL_CLASS = "IO::Socket::SSL"; # it was already loaded
}
else {
    eval { require Net::SSL; };     # from Crypt-SSLeay
    if ($@) {
	require IO::Socket::SSL;
	$SSL_CLASS = "IO::Socket::SSL";
    }
    else {
	$SSL_CLASS = "Net::SSL";
    }
}


use vars qw(@ISA);

require LWP::Protocol::http10;
@ISA=qw(LWP::Protocol::http10);

sub _new_socket
{
    my($self, $host, $port, $timeout) = @_;
    local($^W) = 0;  # IO::Socket::INET can be noisy
    my $sock = $SSL_CLASS->new(PeerAddr => $host,
			       PeerPort => $port,
			       Proto    => 'tcp',
			       Timeout  => $timeout,
			      );
    unless ($sock) {
	# IO::Socket::INET leaves additional error messages in $@
	$@ =~ s/^.*?: //;
	die "Can't connect to $host:$port ($@)";
    }
    $sock;
}

sub _check_sock
{
    my($self, $req, $sock) = @_;
    my $check = $req->header("If-SSL-Cert-Subject");
    if (defined $check) {
	my $cert = $sock->get_peer_certificate ||
	    die "Missing SSL certificate";
	my $subject = $cert->subject_name;
	die "Bad SSL certificate subject: '$subject' !~ /$check/"
	    unless $subject =~ /$check/;
	$req->remove_header("If-SSL-Cert-Subject");  # don't pass it on
    }
}

sub _get_sock_info
{
    my $self = shift;
    $self->SUPER::_get_sock_info(@_);
    my($res, $sock) = @_;
    $res->header("Client-SSL-Cipher" => $sock->get_cipher);
    my $cert = $sock->get_peer_certificate;
    if ($cert) {
	$res->header("Client-SSL-Cert-Subject" => $cert->subject_name);
	$res->header("Client-SSL-Cert-Issuer" => $cert->issuer_name);
    }
    $res->header("Client-SSL-Warning" => "Peer certificate not verified");
}

1;
