package HTTP::Cookies::Netscape;

use strict;
use vars qw(@ISA $VERSION);

$VERSION = "5.810";

require HTTP::Cookies;
@ISA=qw(HTTP::Cookies);

sub load
{
    my($self, $file) = @_;
    $file ||= $self->{'file'} || return;
    local(*FILE, $_);
    local $/ = "\n";  # make sure we got standard record separator
    my @cookies;
    open(FILE, $file) || return;
    my $magic = <FILE>;
    unless ($magic =~ /^\#(?: Netscape)? HTTP Cookie File/) {
	warn "$file does not look like a netscape cookies file" if $^W;
	LWP::Debug::debug("$file doesn't look like a netscape cookies file. Skipping.");
	close(FILE);
	return;
    }
    LWP::Debug::debug("Okay, $file is a netscape cookies file.  Parsing.");
    my $now = time() - $HTTP::Cookies::EPOCH_OFFSET;
    while (<FILE>) {
	next if /^\s*\#/;
	next if /^\s*$/;
	tr/\n\r//d;
	my($domain,$bool1,$path,$secure, $expires,$key,$val) = split(/\t/, $_);
	LWP::Debug::debug(join '', "-Reading NS cookie: ",
	  map(" <$_>", split(/\t/, $_)));
	$secure = ($secure eq "TRUE");
	$self->set_cookie(undef,$key,$val,$path,$domain,undef,
			  0,$secure,$expires-$now, 0);
    }
    close(FILE);
    1;
}

sub save
{
    my($self, $file) = @_;
    $file ||= $self->{'file'} || return;
    local(*FILE, $_);
    open(FILE, ">$file") || return;

    print FILE <<EOT;
# Netscape HTTP Cookie File
# http://www.netscape.com/newsref/std/cookie_spec.html
# This is a generated file!  Do not edit.

EOT

    my $now = time - $HTTP::Cookies::EPOCH_OFFSET;
    $self->scan(sub {
	my($version,$key,$val,$path,$domain,$port,
	   $path_spec,$secure,$expires,$discard,$rest) = @_;
	return if $discard && !$self->{ignore_discard};
	$expires = $expires ? $expires - $HTTP::Cookies::EPOCH_OFFSET : 0;
	return if $now > $expires;
	$secure = $secure ? "TRUE" : "FALSE";
	my $bool = $domain =~ /^\./ ? "TRUE" : "FALSE";
	print FILE join("\t", $domain, $bool, $path, $secure, $expires, $key, $val), "\n";
    });
    close(FILE);
    1;
}

1;
__END__

=head1 NAME

HTTP::Cookies::Netscape - access to Netscape cookies files

=head1 SYNOPSIS

 use LWP;
 use HTTP::Cookies::Netscape;
 $cookie_jar = HTTP::Cookies::Netscape->new(
   file => "c:/program files/netscape/users/ZombieCharity/cookies.txt",
 );
 my $browser = LWP::UserAgent->new;
 $browser->cookie_jar( $cookie_jar );

=head1 DESCRIPTION

This is a subclass of C<HTTP::Cookies> that reads (and optionally
writes) Netscape/Mozilla cookie files.

See the documentation for L<HTTP::Cookies>.

=head1 CAVEATS

Please note that the Netscape/Mozilla cookie file format can't store
all the information available in the Set-Cookie2 headers, so you will
probably lose some information if you save in this format.

At time of writing, this module seems to work fine with Mozilla      
Phoenix/Firebird.

=head1 SEE ALSO

L<HTTP::Cookies::Microsoft>

=head1 COPYRIGHT

Copyright 2002-2003 Gisle Aas

This library is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=cut
