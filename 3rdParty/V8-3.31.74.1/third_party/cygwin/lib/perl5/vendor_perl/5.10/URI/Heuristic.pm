package URI::Heuristic;

=head1 NAME

URI::Heuristic - Expand URI using heuristics

=head1 SYNOPSIS

 use URI::Heuristic qw(uf_uristr);
 $u = uf_uristr("perl");             # http://www.perl.com
 $u = uf_uristr("www.sol.no/sol");   # http://www.sol.no/sol
 $u = uf_uristr("aas");              # http://www.aas.no
 $u = uf_uristr("ftp.funet.fi");     # ftp://ftp.funet.fi
 $u = uf_uristr("/etc/passwd");      # file:/etc/passwd

=head1 DESCRIPTION

This module provides functions that expand strings into real absolute
URIs using some built-in heuristics.  Strings that already represent
absolute URIs (i.e. that start with a C<scheme:> part) are never modified
and are returned unchanged.  The main use of these functions is to
allow abbreviated URIs similar to what many web browsers allow for URIs
typed in by the user.

The following functions are provided:

=over 4

=item uf_uristr($str)

Tries to make the argument string
into a proper absolute URI string.  The "uf_" prefix stands for "User 
Friendly".  Under MacOS, it assumes that any string with a common URL 
scheme (http, ftp, etc.) is a URL rather than a local path.  So don't name 
your volumes after common URL schemes and expect uf_uristr() to construct 
valid file: URL's on those volumes for you, because it won't.

=item uf_uri($str)

Works the same way as uf_uristr() but
returns a C<URI> object.

=back

=head1 ENVIRONMENT

If the hostname portion of a URI does not contain any dots, then
certain qualified guesses are made.  These guesses are governed by
the following two environment variables:

=over 10

=item COUNTRY

The two-letter country code (ISO 3166) for your location.  If
the domain name of your host ends with two letters, then it is taken
to be the default country. See also L<Locale::Country>.

=item URL_GUESS_PATTERN

Contains a space-separated list of URL patterns to try.  The string
"ACME" is for some reason used as a placeholder for the host name in
the URL provided.  Example:

 URL_GUESS_PATTERN="www.ACME.no www.ACME.se www.ACME.com"
 export URL_GUESS_PATTERN

Specifying URL_GUESS_PATTERN disables any guessing rules based on
country.  An empty URL_GUESS_PATTERN disables any guessing that
involves host name lookups.

=back

=head1 COPYRIGHT

Copyright 1997-1998, Gisle Aas

This library is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=cut

use strict;

use vars qw(@EXPORT_OK $VERSION $MY_COUNTRY %LOCAL_GUESSING $DEBUG);

require Exporter;
*import = \&Exporter::import;
@EXPORT_OK = qw(uf_uri uf_uristr uf_url uf_urlstr);
$VERSION = "4.18";

sub MY_COUNTRY() {
    for ($MY_COUNTRY) {
	return $_ if defined;

	# First try the environment.
	$_ = $ENV{COUNTRY};
	return $_ if defined;

	# Could use LANG, LC_ALL, etc at this point, but probably too
	# much of a wild guess.  (Catalan != Canada, etc.)
	#

	# Last bit of domain name.  This may access the network.
	require Net::Domain;
	my $fqdn = Net::Domain::hostfqdn();
	$_ = lc($1) if $fqdn =~ /\.([a-zA-Z]{2})$/;
	return $_ if defined;

	# Give up.  Defined but false.
	return ($_ = 0);
    }
}

%LOCAL_GUESSING =
(
 'us' => [qw(www.ACME.gov www.ACME.mil)],
 'uk' => [qw(www.ACME.co.uk www.ACME.org.uk www.ACME.ac.uk)],
 'au' => [qw(www.ACME.com.au www.ACME.org.au www.ACME.edu.au)],
 'il' => [qw(www.ACME.co.il www.ACME.org.il www.ACME.net.il)],
 # send corrections and new entries to <gisle@aas.no>
);


sub uf_uristr ($)
{
    local($_) = @_;
    print STDERR "uf_uristr: resolving $_\n" if $DEBUG;
    return unless defined;

    s/^\s+//;
    s/\s+$//;

    if (/^(www|web|home)\./) {
	$_ = "http://$_";

    } elsif (/^(ftp|gopher|news|wais|http|https)\./) {
	$_ = "$1://$_";

    } elsif ($^O ne "MacOS" && 
	    (m,^/,      ||          # absolute file name
	     m,^\.\.?/, ||          # relative file name
	     m,^[a-zA-Z]:[/\\],)    # dosish file name
	    )
    {
	$_ = "file:$_";

    } elsif ($^O eq "MacOS" && m/:/) {
        # potential MacOS file name
	unless (m/^(ftp|gopher|news|wais|http|https|mailto):/) {
	    require URI::file;
	    my $a = URI::file->new($_)->as_string;
	    $_ = ($a =~ m/^file:/) ? $a : "file:$a";
	}
    } elsif (/^\w+([\.\-]\w+)*\@(\w+\.)+\w{2,3}$/) {
	$_ = "mailto:$_";

    } elsif (!/^[a-zA-Z][a-zA-Z0-9.+\-]*:/) {      # no scheme specified
	if (s/^([-\w]+(?:\.[-\w]+)*)([\/:\?\#]|$)/$2/) {
	    my $host = $1;

	    if ($host !~ /\./ && $host ne "localhost") {
		my @guess;
		if (exists $ENV{URL_GUESS_PATTERN}) {
		    @guess = map { s/\bACME\b/$host/; $_ }
		             split(' ', $ENV{URL_GUESS_PATTERN});
		} else {
		    if (MY_COUNTRY()) {
			my $special = $LOCAL_GUESSING{MY_COUNTRY()};
			if ($special) {
			    my @special = @$special;
			    push(@guess, map { s/\bACME\b/$host/; $_ }
                                               @special);
			} else {
			    push(@guess, 'www.$host.' . MY_COUNTRY());
			}
		    }
		    push(@guess, map "www.$host.$_",
			             "com", "org", "net", "edu", "int");
		}


		my $guess;
		for $guess (@guess) {
		    print STDERR "uf_uristr: gethostbyname('$guess.')..."
		      if $DEBUG;
		    if (gethostbyname("$guess.")) {
			print STDERR "yes\n" if $DEBUG;
			$host = $guess;
			last;
		    }
		    print STDERR "no\n" if $DEBUG;
		}
	    }
	    $_ = "http://$host$_";

	} else {
	    # pure junk, just return it unchanged...

	}
    }
    print STDERR "uf_uristr: ==> $_\n" if $DEBUG;

    $_;
}

sub uf_uri ($)
{
    require URI;
    URI->new(uf_uristr($_[0]));
}

# legacy
*uf_urlstr = \*uf_uristr;

sub uf_url ($)
{
    require URI::URL;
    URI::URL->new(uf_uristr($_[0]));
}

1;
