package LWP::Authen::Digest;
use strict;

require Digest::MD5;

sub authenticate
{
    my($class, $ua, $proxy, $auth_param, $response,
       $request, $arg, $size) = @_;

    my($user, $pass) = $ua->get_basic_credentials($auth_param->{realm},
                                                  $request->url, $proxy);
    return $response unless defined $user and defined $pass;

    my $nc = sprintf "%08X", ++$ua->{authen_md5_nonce_count}{$auth_param->{nonce}};
    my $cnonce = sprintf "%8x", time;

    my $uri = $request->url->path_query;
    $uri = "/" unless length $uri;

    my $md5 = Digest::MD5->new;

    my(@digest);
    $md5->add(join(":", $user, $auth_param->{realm}, $pass));
    push(@digest, $md5->hexdigest);
    $md5->reset;

    push(@digest, $auth_param->{nonce});

    if ($auth_param->{qop}) {
	push(@digest, $nc, $cnonce, ($auth_param->{qop} =~ m|^auth[,;]auth-int$|) ? 'auth' : $auth_param->{qop});
    }

    $md5->add(join(":", $request->method, $uri));
    push(@digest, $md5->hexdigest);
    $md5->reset;

    $md5->add(join(":", @digest));
    my($digest) = $md5->hexdigest;
    $md5->reset;

    my %resp = map { $_ => $auth_param->{$_} } qw(realm nonce opaque);
    @resp{qw(username uri response algorithm)} = ($user, $uri, $digest, "MD5");

    if (($auth_param->{qop} || "") =~ m|^auth([,;]auth-int)?$|) {
	@resp{qw(qop cnonce nc)} = ("auth", $cnonce, $nc);
    }

    my(@order) = qw(username realm qop algorithm uri nonce nc cnonce response);
    if($request->method =~ /^(?:POST|PUT)$/) {
	$md5->add($request->content);
	my $content = $md5->hexdigest;
	$md5->reset;
	$md5->add(join(":", @digest[0..1], $content));
	$md5->reset;
	$resp{"message-digest"} = $md5->hexdigest;
	push(@order, "message-digest");
    }
    push(@order, "opaque");
    my @pairs;
    for (@order) {
	next unless defined $resp{$_};
	push(@pairs, "$_=" . qq("$resp{$_}"));
    }

    my $auth_header = $proxy ? "Proxy-Authorization" : "Authorization";
    my $auth_value  = "Digest " . join(", ", @pairs);

    # Need to check this isn't a repeated fail!
    my $r = $response;
    while ($r) {
	my $u = $r->request->{digest_user_pass};
	if ($u && $u->[0] eq $user && $u->[1] eq $pass) {
	    # here we know this failed before
	    $response->header("Client-Warning" =>
			      "Credentials for '$user' failed before");
	    return $response;
	}
	$r = $r->previous;
    }

    my $referral = $request->clone;
    $referral->header($auth_header => $auth_value);
    # we shouldn't really do this, but...
    $referral->{digest_user_pass} = [$user, $pass];

    return $ua->request($referral, $arg, $size, $response);
}

1;
