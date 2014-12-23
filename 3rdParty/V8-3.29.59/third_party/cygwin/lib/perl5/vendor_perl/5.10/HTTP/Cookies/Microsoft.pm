package HTTP::Cookies::Microsoft;

use strict;

use vars qw(@ISA $VERSION);

$VERSION = "5.810";

require HTTP::Cookies;
@ISA=qw(HTTP::Cookies);

sub load_cookies_from_file
{
	my ($file) = @_;
	my @cookies;
	my ($key, $value, $domain_path, $flags, $lo_expire, $hi_expire);
	my ($lo_create, $hi_create, $sep);

	open(COOKIES, $file) || return;

	while ($key = <COOKIES>)
	{
		chomp($key);
		chomp($value     = <COOKIES>);
		chomp($domain_path= <COOKIES>);
		chomp($flags     = <COOKIES>);		# 0x0001 bit is for secure
		chomp($lo_expire = <COOKIES>);
		chomp($hi_expire = <COOKIES>);
		chomp($lo_create = <COOKIES>);
		chomp($hi_create = <COOKIES>);
		chomp($sep       = <COOKIES>);

		if (!defined($key) || !defined($value) || !defined($domain_path) ||
			!defined($flags) || !defined($hi_expire) || !defined($lo_expire) ||
			!defined($hi_create) || !defined($lo_create) || !defined($sep) ||
			($sep ne '*'))
		{
			last;
		}

		if ($domain_path =~ /^([^\/]+)(\/.*)$/)
		{
			my $domain = $1;
			my $path = $2;

			push(@cookies, {KEY => $key, VALUE => $value, DOMAIN => $domain,
					PATH => $path, FLAGS =>$flags, HIXP =>$hi_expire,
					LOXP => $lo_expire, HICREATE => $hi_create,
					LOCREATE => $lo_create});
		}
	}

	return \@cookies;
}

sub get_user_name
{
	use Win32;
	use locale;
	my $user = lc(Win32::LoginName());

	return $user;
}

# MSIE stores create and expire times as Win32 FILETIME,
# which is 64 bits of 100 nanosecond intervals since Jan 01 1601
#
# But Cookies code expects time in 32-bit value expressed
# in seconds since Jan 01 1970
#
sub epoch_time_offset_from_win32_filetime
{
	my ($high, $low) = @_;

	#--------------------------------------------------------
	# USEFUL CONSTANT
	#--------------------------------------------------------
	# 0x019db1de 0xd53e8000 is 1970 Jan 01 00:00:00 in Win32 FILETIME
	#
	# 100 nanosecond intervals == 0.1 microsecond intervals
	
	my $filetime_low32_1970 = 0xd53e8000;
	my $filetime_high32_1970 = 0x019db1de;

	#------------------------------------
	# ALGORITHM
	#------------------------------------
	# To go from 100 nanosecond intervals to seconds since 00:00 Jan 01 1970:
	#
	# 1. Adjust 100 nanosecond intervals to Jan 01 1970 base
	# 2. Divide by 10 to get to microseconds (1/millionth second)
	# 3. Divide by 1000000 (10 ^ 6) to get to seconds
	#
	# We can combine Step 2 & 3 into one divide.
	#
	# After much trial and error, I came up with the following code which
	# avoids using Math::BigInt or floating pt, but still gives correct answers

	# If the filetime is before the epoch, return 0
	if (($high < $filetime_high32_1970) ||
	    (($high == $filetime_high32_1970) && ($low < $filetime_low32_1970)))
    	{
		return 0;
	}

	# Can't multiply by 0x100000000, (1 << 32),
	# without Perl issuing an integer overflow warning
	#
	# So use two multiplies by 0x10000 instead of one multiply by 0x100000000
	#
	# The result is the same.
	#
	my $date1970 = (($filetime_high32_1970 * 0x10000) * 0x10000) + $filetime_low32_1970;
	my $time = (($high * 0x10000) * 0x10000) + $low;

	$time -= $date1970;
	$time /= 10000000;

	return $time;
}

sub load_cookie
{
	my($self, $file) = @_;
        my $now = time() - $HTTP::Cookies::EPOCH_OFFSET;
	my $cookie_data;

        if (-f $file)
        {
		# open the cookie file and get the data
		$cookie_data = load_cookies_from_file($file);

		foreach my $cookie (@{$cookie_data})
		{
			my $secure = ($cookie->{FLAGS} & 1) != 0;
			my $expires = epoch_time_offset_from_win32_filetime($cookie->{HIXP}, $cookie->{LOXP});

			$self->set_cookie(undef, $cookie->{KEY}, $cookie->{VALUE}, 
					  $cookie->{PATH}, $cookie->{DOMAIN}, undef,
					  0, $secure, $expires-$now, 0);
		}
	}
}

sub load
{
	my($self, $cookie_index) = @_;
	my $now = time() - $HTTP::Cookies::EPOCH_OFFSET;
	my $cookie_dir = '';
	my $delay_load = (defined($self->{'delayload'}) && $self->{'delayload'});
	my $user_name = get_user_name();
	my $data;

	$cookie_index ||= $self->{'file'} || return;
	if ($cookie_index =~ /[\\\/][^\\\/]+$/)
	{
		$cookie_dir = $` . "\\";
	}

	local(*INDEX, $_);

	open(INDEX, $cookie_index) || return;
	binmode(INDEX);
	if (256 != read(INDEX, $data, 256))
	{
		warn "$cookie_index file is not large enough";
		close(INDEX);
		return;
	}

	# Cookies' index.dat file starts with 32 bytes of signature
	# followed by an offset to the first record, stored as a little-endian DWORD
	my ($sig, $size) = unpack('a32 V', $data);
	
	if (($sig !~ /^Client UrlCache MMF Ver 5\.2/) || # check that sig is valid (only tested in IE6.0)
		(0x4000 != $size))
	{
		warn "$cookie_index ['$sig' $size] does not seem to contain cookies";
		close(INDEX);
		return;
	}

	if (0 == seek(INDEX, $size, 0)) # move the file ptr to start of the first record
	{
		close(INDEX);
		return;
	}

	# Cookies are usually stored in 'URL ' records in two contiguous 0x80 byte sectors (256 bytes)
	# so read in two 0x80 byte sectors and adjust if not a Cookie.
	while (256 == read(INDEX, $data, 256))
	{
		# each record starts with a 4-byte signature
		# and a count (little-endian DWORD) of 0x80 byte sectors for the record
		($sig, $size) = unpack('a4 V', $data);

		# Cookies are found in 'URL ' records
		if ('URL ' ne $sig)
		{
			# skip over uninteresting record: I've seen 'HASH' and 'LEAK' records
			if (($sig eq 'HASH') || ($sig eq 'LEAK'))
			{
				# '-2' takes into account the two 0x80 byte sectors we've just read in
				if (($size > 0) && ($size != 2))
				{
				    if (0 == seek(INDEX, ($size-2)*0x80, 1))
				    {
					    # Seek failed. Something's wrong. Gonna stop.
					    last;
				    }
				}
			}
			next;
		}

		#$REMOVE Need to check if URL records in Cookies' index.dat will
		#        ever use more than two 0x80 byte sectors
		if ($size > 2)
		{
			my $more_data = ($size-2)*0x80;

			if ($more_data != read(INDEX, $data, $more_data, 256))
			{
				last;
			}
		}

		if ($data =~ /Cookie\:$user_name\@([\x21-\xFF]+).*?($user_name\@[\x21-\xFF]+\.txt)/)
		{
			my $cookie_file = $cookie_dir . $2; # form full pathname

			if (!$delay_load)
			{
				$self->load_cookie($cookie_file);
			}
			else
			{
				my $domain = $1;

				# grab only the domain name, drop everything from the first dir sep on
				if ($domain =~ m{[\\/]})
				{
					$domain = $`;
				}

				# set the delayload cookie for this domain with 
				# the cookie_file as cookie for later-loading info
				$self->set_cookie(undef, 'cookie', $cookie_file,
						      '//+delayload', $domain, undef,
						      0, 0, $now+86400, 0);
			}
		}
	}

	close(INDEX);

	1;
}

1;

__END__

=head1 NAME

HTTP::Cookies::Microsoft - access to Microsoft cookies files

=head1 SYNOPSIS

 use LWP;
 use HTTP::Cookies::Microsoft;
 use Win32::TieRegistry(Delimiter => "/");
 my $cookies_dir = $Registry->
      {"CUser/Software/Microsoft/Windows/CurrentVersion/Explorer/Shell Folders/Cookies"};

 $cookie_jar = HTTP::Cookies::Microsoft->new(
                   file     => "$cookies_dir\\index.dat",
                   'delayload' => 1,
               );
 my $browser = LWP::UserAgent->new;
 $browser->cookie_jar( $cookie_jar );

=head1 DESCRIPTION

This is a subclass of C<HTTP::Cookies> which
loads Microsoft Internet Explorer 5.x and 6.x for Windows (MSIE)
cookie files.

See the documentation for L<HTTP::Cookies>.

=head1 METHODS

The following methods are provided:

=over 4

=item $cookie_jar = HTTP::Cookies::Microsoft->new;

The constructor takes hash style parameters. In addition
to the regular HTTP::Cookies parameters, HTTP::Cookies::Microsoft
recognizes the following:

  delayload:       delay loading of cookie data until a request
                   is actually made. This results in faster
                   runtime unless you use most of the cookies
                   since only the domain's cookie data
                   is loaded on demand.

=back

=head1 CAVEATS

Please note that the code DOESN'T support saving to the MSIE
cookie file format.

=head1 AUTHOR

Johnny Lee <typo_pl@hotmail.com>

=head1 COPYRIGHT

Copyright 2002 Johnny Lee

This library is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=cut

