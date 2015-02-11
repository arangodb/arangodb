package HTTP::Cookies;

use strict;
use HTTP::Date qw(str2time time2str);
use HTTP::Headers::Util qw(split_header_words join_header_words);
use LWP::Debug ();

use vars qw($VERSION $EPOCH_OFFSET);
$VERSION = "5.810";

# Legacy: because "use "HTTP::Cookies" used be the ONLY way
#  to load the class HTTP::Cookies::Netscape.
require HTTP::Cookies::Netscape;

$EPOCH_OFFSET = 0;  # difference from Unix epoch
if ($^O eq "MacOS") {
    require Time::Local;
    $EPOCH_OFFSET = Time::Local::timelocal(0,0,0,1,0,70);
}

# A HTTP::Cookies object is a hash.  The main attribute is the
# COOKIES 3 level hash:  $self->{COOKIES}{$domain}{$path}{$key}.

sub new
{
    my $class = shift;
    my $self = bless {
	COOKIES => {},
    }, $class;
    my %cnf = @_;
    for (keys %cnf) {
	$self->{lc($_)} = $cnf{$_};
    }
    $self->load;
    $self;
}


sub add_cookie_header
{
    my $self = shift;
    my $request = shift || return;
    my $url = $request->url;
    my $scheme = $url->scheme;
    unless ($scheme =~ /^https?\z/) {
	LWP::Debug::debug("Will not add cookies to non-HTTP requests");
	return;
    }

    my $domain = _host($request, $url);
    $domain = "$domain.local" unless $domain =~ /\./;
    my $secure_request = ($scheme eq "https");
    my $req_path = _url_path($url);
    my $req_port = $url->port;
    my $now = time();
    _normalize_path($req_path) if $req_path =~ /%/;

    my @cval;    # cookie values for the "Cookie" header
    my $set_ver;
    my $netscape_only = 0; # An exact domain match applies to any cookie

    while ($domain =~ /\./) {

        LWP::Debug::debug("Checking $domain for cookies");
	my $cookies = $self->{COOKIES}{$domain};
	next unless $cookies;
	if ($self->{delayload} && defined($cookies->{'//+delayload'})) {
	    my $cookie_data = $cookies->{'//+delayload'}{'cookie'};
	    delete $self->{COOKIES}{$domain};
	    $self->load_cookie($cookie_data->[1]);
	    $cookies = $self->{COOKIES}{$domain};
	    next unless $cookies;  # should not really happen
	}

	# Want to add cookies corresponding to the most specific paths
	# first (i.e. longest path first)
	my $path;
	for $path (sort {length($b) <=> length($a) } keys %$cookies) {
            LWP::Debug::debug("- checking cookie path=$path");
	    if (index($req_path, $path) != 0) {
	        LWP::Debug::debug("  path $path:$req_path does not fit");
		next;
	    }

	    my($key,$array);
	    while (($key,$array) = each %{$cookies->{$path}}) {
		my($version,$val,$port,$path_spec,$secure,$expires) = @$array;
	        LWP::Debug::debug(" - checking cookie $key=$val");
		if ($secure && !$secure_request) {
		    LWP::Debug::debug("   not a secure requests");
		    next;
		}
		if ($expires && $expires < $now) {
		    LWP::Debug::debug("   expired");
		    next;
		}
		if ($port) {
		    my $found;
		    if ($port =~ s/^_//) {
			# The correponding Set-Cookie attribute was empty
			$found++ if $port eq $req_port;
			$port = "";
		    }
		    else {
			my $p;
			for $p (split(/,/, $port)) {
			    $found++, last if $p eq $req_port;
			}
		    }
		    unless ($found) {
		        LWP::Debug::debug("   port $port:$req_port does not fit");
			next;
		    }
		}
		if ($version > 0 && $netscape_only) {
		    LWP::Debug::debug("   domain $domain applies to " .
				      "Netscape-style cookies only");
		    next;
		}

	        LWP::Debug::debug("   it's a match");

		# set version number of cookie header.
	        # XXX: What should it be if multiple matching
                #      Set-Cookie headers have different versions themselves
		if (!$set_ver++) {
		    if ($version >= 1) {
			push(@cval, "\$Version=$version");
		    }
		    elsif (!$self->{hide_cookie2}) {
			$request->header(Cookie2 => '$Version="1"');
		    }
		}

		# do we need to quote the value
		if ($val =~ /\W/ && $version) {
		    $val =~ s/([\\\"])/\\$1/g;
		    $val = qq("$val");
		}

		# and finally remember this cookie
		push(@cval, "$key=$val");
		if ($version >= 1) {
		    push(@cval, qq(\$Path="$path"))     if $path_spec;
		    push(@cval, qq(\$Domain="$domain")) if $domain =~ /^\./;
		    if (defined $port) {
			my $p = '$Port';
			$p .= qq(="$port") if length $port;
			push(@cval, $p);
		    }
		}

	    }
        }

    } continue {
	# Try with a more general domain, alternately stripping
	# leading name components and leading dots.  When this
	# results in a domain with no leading dot, it is for
	# Netscape cookie compatibility only:
	#
	# a.b.c.net	Any cookie
	# .b.c.net	Any cookie
	# b.c.net	Netscape cookie only
	# .c.net	Any cookie

	if ($domain =~ s/^\.+//) {
	    $netscape_only = 1;
	}
	else {
	    $domain =~ s/[^.]*//;
	    $netscape_only = 0;
	}
    }

    $request->header(Cookie => join("; ", @cval)) if @cval;

    $request;
}


sub extract_cookies
{
    my $self = shift;
    my $response = shift || return;

    my @set = split_header_words($response->_header("Set-Cookie2"));
    my @ns_set = $response->_header("Set-Cookie");

    return $response unless @set || @ns_set;  # quick exit

    my $request = $response->request;
    my $url = $request->url;
    my $req_host = _host($request, $url);
    $req_host = "$req_host.local" unless $req_host =~ /\./;
    my $req_port = $url->port;
    my $req_path = _url_path($url);
    _normalize_path($req_path) if $req_path =~ /%/;

    if (@ns_set) {
	# The old Netscape cookie format for Set-Cookie
	# http://wp.netscape.com/newsref/std/cookie_spec.html
	# can for instance contain an unquoted "," in the expires
	# field, so we have to use this ad-hoc parser.
	my $now = time();

	# Build a hash of cookies that was present in Set-Cookie2
	# headers.  We need to skip them if we also find them in a
	# Set-Cookie header.
	my %in_set2;
	for (@set) {
	    $in_set2{$_->[0]}++;
	}

	my $set;
	for $set (@ns_set) {
	    my @cur;
	    my $param;
	    my $expires;
	    my $first_param = 1;
	    for $param (split(/;\s*/, $set)) {
		my($k,$v) = split(/\s*=\s*/, $param, 2);
		if (defined $v) {
		    $v =~ s/\s+$//;
		    #print "$k => $v\n";
		}
		else {
		    $k =~ s/\s+$//;
		    #print "$k => undef";
		}
		if (!$first_param && lc($k) eq "expires") {
		    my $etime = str2time($v);
		    if ($etime) {
			push(@cur, "Max-Age" => str2time($v) - $now);
			$expires++;
		    }
		}
		else {
		    push(@cur, $k => $v);
		}
		$first_param = 0;
	    }
            next unless @cur;
	    next if $in_set2{$cur[0]};

#	    push(@cur, "Port" => $req_port);
	    push(@cur, "Discard" => undef) unless $expires;
	    push(@cur, "Version" => 0);
	    push(@cur, "ns-cookie" => 1);
	    push(@set, \@cur);
	}
    }

  SET_COOKIE:
    for my $set (@set) {
	next unless @$set >= 2;

	my $key = shift @$set;
	my $val = shift @$set;

        LWP::Debug::debug("Set cookie $key => $val");

	my %hash;
	while (@$set) {
	    my $k = shift @$set;
	    my $v = shift @$set;
	    my $lc = lc($k);
	    # don't loose case distinction for unknown fields
	    $k = $lc if $lc =~ /^(?:discard|domain|max-age|
                                    path|port|secure|version)$/x;
	    if ($k eq "discard" || $k eq "secure") {
		$v = 1 unless defined $v;
	    }
	    next if exists $hash{$k};  # only first value is signigicant
	    $hash{$k} = $v;
	};

	my %orig_hash = %hash;
	my $version   = delete $hash{version};
	$version = 1 unless defined($version);
	my $discard   = delete $hash{discard};
	my $secure    = delete $hash{secure};
	my $maxage    = delete $hash{'max-age'};
	my $ns_cookie = delete $hash{'ns-cookie'};

	# Check domain
	my $domain  = delete $hash{domain};
	$domain = lc($domain) if defined $domain;
	if (defined($domain)
	    && $domain ne $req_host && $domain ne ".$req_host") {
	    if ($domain !~ /\./ && $domain ne "local") {
	        LWP::Debug::debug("Domain $domain contains no dot");
		next SET_COOKIE;
	    }
	    $domain = ".$domain" unless $domain =~ /^\./;
	    if ($domain =~ /\.\d+$/) {
	        LWP::Debug::debug("IP-address $domain illeagal as domain");
		next SET_COOKIE;
	    }
	    my $len = length($domain);
	    unless (substr($req_host, -$len) eq $domain) {
	        LWP::Debug::debug("Domain $domain does not match host $req_host");
		next SET_COOKIE;
	    }
	    my $hostpre = substr($req_host, 0, length($req_host) - $len);
	    if ($hostpre =~ /\./ && !$ns_cookie) {
	        LWP::Debug::debug("Host prefix contain a dot: $hostpre => $domain");
		next SET_COOKIE;
	    }
	}
	else {
	    $domain = $req_host;
	}

	my $path = delete $hash{path};
	my $path_spec;
	if (defined $path && $path ne '') {
	    $path_spec++;
	    _normalize_path($path) if $path =~ /%/;
	    if (!$ns_cookie &&
                substr($req_path, 0, length($path)) ne $path) {
	        LWP::Debug::debug("Path $path is not a prefix of $req_path");
		next SET_COOKIE;
	    }
	}
	else {
	    $path = $req_path;
	    $path =~ s,/[^/]*$,,;
	    $path = "/" unless length($path);
	}

	my $port;
	if (exists $hash{port}) {
	    $port = delete $hash{port};
	    if (defined $port) {
		$port =~ s/\s+//g;
		my $found;
		for my $p (split(/,/, $port)) {
		    unless ($p =~ /^\d+$/) {
		      LWP::Debug::debug("Bad port $port (not numeric)");
			next SET_COOKIE;
		    }
		    $found++ if $p eq $req_port;
		}
		unless ($found) {
		    LWP::Debug::debug("Request port ($req_port) not found in $port");
		    next SET_COOKIE;
		}
	    }
	    else {
		$port = "_$req_port";
	    }
	}
	$self->set_cookie($version,$key,$val,$path,$domain,$port,$path_spec,$secure,$maxage,$discard, \%hash)
	    if $self->set_cookie_ok(\%orig_hash);
    }

    $response;
}

sub set_cookie_ok
{
    1;
}


sub set_cookie
{
    my $self = shift;
    my($version,
       $key, $val, $path, $domain, $port,
       $path_spec, $secure, $maxage, $discard, $rest) = @_;

    # path and key can not be empty (key can't start with '$')
    return $self if !defined($path) || $path !~ m,^/, ||
	            !defined($key)  || $key  =~ m,^\$,;

    # ensure legal port
    if (defined $port) {
	return $self unless $port =~ /^_?\d+(?:,\d+)*$/;
    }

    my $expires;
    if (defined $maxage) {
	if ($maxage <= 0) {
	    delete $self->{COOKIES}{$domain}{$path}{$key};
	    return $self;
	}
	$expires = time() + $maxage;
    }
    $version = 0 unless defined $version;

    my @array = ($version, $val,$port,
		 $path_spec,
		 $secure, $expires, $discard);
    push(@array, {%$rest}) if defined($rest) && %$rest;
    # trim off undefined values at end
    pop(@array) while !defined $array[-1];

    $self->{COOKIES}{$domain}{$path}{$key} = \@array;
    $self;
}


sub save
{
    my $self = shift;
    my $file = shift || $self->{'file'} || return;
    local(*FILE);
    open(FILE, ">$file") or die "Can't open $file: $!";
    print FILE "#LWP-Cookies-1.0\n";
    print FILE $self->as_string(!$self->{ignore_discard});
    close(FILE);
    1;
}


sub load
{
    my $self = shift;
    my $file = shift || $self->{'file'} || return;
    local(*FILE, $_);
    local $/ = "\n";  # make sure we got standard record separator
    open(FILE, $file) or return;
    my $magic = <FILE>;
    unless ($magic =~ /^\#LWP-Cookies-(\d+\.\d+)/) {
	warn "$file does not seem to contain cookies";
	return;
    }
    while (<FILE>) {
	next unless s/^Set-Cookie3:\s*//;
	chomp;
	my $cookie;
	for $cookie (split_header_words($_)) {
	    my($key,$val) = splice(@$cookie, 0, 2);
	    my %hash;
	    while (@$cookie) {
		my $k = shift @$cookie;
		my $v = shift @$cookie;
		$hash{$k} = $v;
	    }
	    my $version   = delete $hash{version};
	    my $path      = delete $hash{path};
	    my $domain    = delete $hash{domain};
	    my $port      = delete $hash{port};
	    my $expires   = str2time(delete $hash{expires});

	    my $path_spec = exists $hash{path_spec}; delete $hash{path_spec};
	    my $secure    = exists $hash{secure};    delete $hash{secure};
	    my $discard   = exists $hash{discard};   delete $hash{discard};

	    my @array =	($version,$val,$port,
			 $path_spec,$secure,$expires,$discard);
	    push(@array, \%hash) if %hash;
	    $self->{COOKIES}{$domain}{$path}{$key} = \@array;
	}
    }
    close(FILE);
    1;
}


sub revert
{
    my $self = shift;
    $self->clear->load;
    $self;
}


sub clear
{
    my $self = shift;
    if (@_ == 0) {
	$self->{COOKIES} = {};
    }
    elsif (@_ == 1) {
	delete $self->{COOKIES}{$_[0]};
    }
    elsif (@_ == 2) {
	delete $self->{COOKIES}{$_[0]}{$_[1]};
    }
    elsif (@_ == 3) {
	delete $self->{COOKIES}{$_[0]}{$_[1]}{$_[2]};
    }
    else {
	require Carp;
        Carp::carp('Usage: $c->clear([domain [,path [,key]]])');
    }
    $self;
}


sub clear_temporary_cookies
{
    my($self) = @_;

    $self->scan(sub {
        if($_[9] or        # "Discard" flag set
           not $_[8]) {    # No expire field?
            $_[8] = -1;            # Set the expire/max_age field
            $self->set_cookie(@_); # Clear the cookie
        }
      });
}


sub DESTROY
{
    my $self = shift;
    $self->save if $self->{'autosave'};
}


sub scan
{
    my($self, $cb) = @_;
    my($domain,$path,$key);
    for $domain (sort keys %{$self->{COOKIES}}) {
	for $path (sort keys %{$self->{COOKIES}{$domain}}) {
	    for $key (sort keys %{$self->{COOKIES}{$domain}{$path}}) {
		my($version,$val,$port,$path_spec,
		   $secure,$expires,$discard,$rest) =
		       @{$self->{COOKIES}{$domain}{$path}{$key}};
		$rest = {} unless defined($rest);
		&$cb($version,$key,$val,$path,$domain,$port,
		     $path_spec,$secure,$expires,$discard,$rest);
	    }
	}
    }
}


sub as_string
{
    my($self, $skip_discard) = @_;
    my @res;
    $self->scan(sub {
	my($version,$key,$val,$path,$domain,$port,
	   $path_spec,$secure,$expires,$discard,$rest) = @_;
	return if $discard && $skip_discard;
	my @h = ($key, $val);
	push(@h, "path", $path);
	push(@h, "domain" => $domain);
	push(@h, "port" => $port) if defined $port;
	push(@h, "path_spec" => undef) if $path_spec;
	push(@h, "secure" => undef) if $secure;
	push(@h, "expires" => HTTP::Date::time2isoz($expires)) if $expires;
	push(@h, "discard" => undef) if $discard;
	my $k;
	for $k (sort keys %$rest) {
	    push(@h, $k, $rest->{$k});
	}
	push(@h, "version" => $version);
	push(@res, "Set-Cookie3: " . join_header_words(\@h));
    });
    join("\n", @res, "");
}

sub _host
{
    my($request, $url) = @_;
    if (my $h = $request->header("Host")) {
	$h =~ s/:\d+$//;  # might have a port as well
	return lc($h);
    }
    return lc($url->host);
}

sub _url_path
{
    my $url = shift;
    my $path;
    if($url->can('epath')) {
       $path = $url->epath;    # URI::URL method
    }
    else {
       $path = $url->path;           # URI::_generic method
    }
    $path = "/" unless length $path;
    $path;
}

sub _normalize_path  # so that plain string compare can be used
{
    my $x;
    $_[0] =~ s/%([0-9a-fA-F][0-9a-fA-F])/
	         $x = uc($1);
                 $x eq "2F" || $x eq "25" ? "%$x" :
                                            pack("C", hex($x));
              /eg;
    $_[0] =~ s/([\0-\x20\x7f-\xff])/sprintf("%%%02X",ord($1))/eg;
}

1;

__END__

=head1 NAME

HTTP::Cookies - HTTP cookie jars

=head1 SYNOPSIS

  use HTTP::Cookies;
  $cookie_jar = HTTP::Cookies->new(
    file => "$ENV{'HOME'}/lwp_cookies.dat',
    autosave => 1,
  );

  use LWP;
  my $browser = LWP::UserAgent->new;
  $browser->cookie_jar($cookie_jar);

Or for an empty and temporary cookie jar:

  use LWP;
  my $browser = LWP::UserAgent->new;
  $browser->cookie_jar( {} );

=head1 DESCRIPTION

This class is for objects that represent a "cookie jar" -- that is, a
database of all the HTTP cookies that a given LWP::UserAgent object
knows about.

Cookies are a general mechanism which server side connections can use
to both store and retrieve information on the client side of the
connection.  For more information about cookies refer to
<URL:http://wp.netscape.com/newsref/std/cookie_spec.html> and
<URL:http://www.cookiecentral.com/>.  This module also implements the
new style cookies described in I<RFC 2965>.
The two variants of cookies are supposed to be able to coexist happily.

Instances of the class I<HTTP::Cookies> are able to store a collection
of Set-Cookie2: and Set-Cookie: headers and are able to use this
information to initialize Cookie-headers in I<HTTP::Request> objects.
The state of a I<HTTP::Cookies> object can be saved in and restored from
files.

=head1 METHODS

The following methods are provided:

=over 4

=item $cookie_jar = HTTP::Cookies->new

The constructor takes hash style parameters.  The following
parameters are recognized:

  file:            name of the file to restore cookies from and save cookies to
  autosave:        save during destruction (bool)
  ignore_discard:  save even cookies that are requested to be discarded (bool)
  hide_cookie2:    do not add Cookie2 header to requests

Future parameters might include (not yet implemented):

  max_cookies               300
  max_cookies_per_domain    20
  max_cookie_size           4096

  no_cookies   list of domain names that we never return cookies to

=item $cookie_jar->add_cookie_header( $request )

The add_cookie_header() method will set the appropriate Cookie:-header
for the I<HTTP::Request> object given as argument.  The $request must
have a valid url attribute before this method is called.

=item $cookie_jar->extract_cookies( $response )

The extract_cookies() method will look for Set-Cookie: and
Set-Cookie2: headers in the I<HTTP::Response> object passed as
argument.  Any of these headers that are found are used to update
the state of the $cookie_jar.

=item $cookie_jar->set_cookie( $version, $key, $val, $path, $domain, $port, $path_spec, $secure, $maxage, $discard, \%rest )

The set_cookie() method updates the state of the $cookie_jar.  The
$key, $val, $domain, $port and $path arguments are strings.  The
$path_spec, $secure, $discard arguments are boolean values. The $maxage
value is a number indicating number of seconds that this cookie will
live.  A value <= 0 will delete this cookie.  %rest defines
various other attributes like "Comment" and "CommentURL".

=item $cookie_jar->save

=item $cookie_jar->save( $file )

This method file saves the state of the $cookie_jar to a file.
The state can then be restored later using the load() method.  If a
filename is not specified we will use the name specified during
construction.  If the attribute I<ignore_discard> is set, then we
will even save cookies that are marked to be discarded.

The default is to save a sequence of "Set-Cookie3" lines.
"Set-Cookie3" is a proprietary LWP format, not known to be compatible
with any browser.  The I<HTTP::Cookies::Netscape> sub-class can
be used to save in a format compatible with Netscape.

=item $cookie_jar->load

=item $cookie_jar->load( $file )

This method reads the cookies from the file and adds them to the
$cookie_jar.  The file must be in the format written by the save()
method.

=item $cookie_jar->revert

This method empties the $cookie_jar and re-loads the $cookie_jar
from the last save file.

=item $cookie_jar->clear

=item $cookie_jar->clear( $domain )

=item $cookie_jar->clear( $domain, $path )

=item $cookie_jar->clear( $domain, $path, $key )

Invoking this method without arguments will empty the whole
$cookie_jar.  If given a single argument only cookies belonging to
that domain will be removed.  If given two arguments, cookies
belonging to the specified path within that domain are removed.  If
given three arguments, then the cookie with the specified key, path
and domain is removed.

=item $cookie_jar->clear_temporary_cookies

Discard all temporary cookies. Scans for all cookies in the jar
with either no expire field or a true C<discard> flag. To be
called when the user agent shuts down according to RFC 2965.

=item $cookie_jar->scan( \&callback )

The argument is a subroutine that will be invoked for each cookie
stored in the $cookie_jar.  The subroutine will be invoked with
the following arguments:

  0  version
  1  key
  2  val
  3  path
  4  domain
  5  port
  6  path_spec
  7  secure
  8  expires
  9  discard
 10  hash

=item $cookie_jar->as_string

=item $cookie_jar->as_string( $skip_discardables )

The as_string() method will return the state of the $cookie_jar
represented as a sequence of "Set-Cookie3" header lines separated by
"\n".  If $skip_discardables is TRUE, it will not return lines for
cookies with the I<Discard> attribute.

=back

=head1 SEE ALSO

L<HTTP::Cookies::Netscape>, L<HTTP::Cookies::Microsoft>

=head1 COPYRIGHT

Copyright 1997-2002 Gisle Aas

This library is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

