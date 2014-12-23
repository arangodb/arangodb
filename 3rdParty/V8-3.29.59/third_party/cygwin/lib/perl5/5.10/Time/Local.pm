package Time::Local;

require Exporter;
use Carp;
use Config;
use strict;
use integer;

use vars qw( $VERSION @ISA @EXPORT @EXPORT_OK );
$VERSION   = '1.18';

@ISA       = qw( Exporter );
@EXPORT    = qw( timegm timelocal );
@EXPORT_OK = qw( timegm_nocheck timelocal_nocheck );

my @MonthDays = ( 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 );

# Determine breakpoint for rolling century
my $ThisYear    = ( localtime() )[5];
my $Breakpoint  = ( $ThisYear + 50 ) % 100;
my $NextCentury = $ThisYear - $ThisYear % 100;
$NextCentury += 100 if $Breakpoint < 50;
my $Century = $NextCentury - 100;
my $SecOff  = 0;

my ( %Options, %Cheat );

use constant SECS_PER_MINUTE => 60;
use constant SECS_PER_HOUR   => 3600;
use constant SECS_PER_DAY    => 86400;

my $MaxInt = ( ( 1 << ( 8 * $Config{intsize} - 2 ) ) -1 ) * 2 + 1;
my $MaxDay = int( ( $MaxInt - ( SECS_PER_DAY / 2 ) ) / SECS_PER_DAY ) - 1;

if ( $^O eq 'MacOS' ) {
    # time_t is unsigned...
    $MaxInt = ( 1 << ( 8 * $Config{intsize} ) ) - 1;
}
else {
    $MaxInt = ( ( 1 << ( 8 * $Config{intsize} - 2 ) ) - 1 ) * 2 + 1;
}

# Determine the EPOC day for this machine
my $Epoc = 0;
if ( $^O eq 'vos' ) {
    # work around posix-977 -- VOS doesn't handle dates in the range
    # 1970-1980.
    $Epoc = _daygm( 0, 0, 0, 1, 0, 70, 4, 0 );
}
elsif ( $^O eq 'MacOS' ) {
    $MaxDay *=2 if $^O eq 'MacOS';  # time_t unsigned ... quick hack?
    # MacOS time() is seconds since 1 Jan 1904, localtime
    # so we need to calculate an offset to apply later
    $Epoc = 693901;
    $SecOff = timelocal( localtime(0)) - timelocal( gmtime(0) ) ;
    $Epoc += _daygm( gmtime(0) );
}
else {
    $Epoc = _daygm( gmtime(0) );
}

%Cheat = ();    # clear the cache as epoc has changed

sub _daygm {

    # This is written in such a byzantine way in order to avoid
    # lexical variables and sub calls, for speed
    return $_[3] + (
        $Cheat{ pack( 'ss', @_[ 4, 5 ] ) } ||= do {
            my $month = ( $_[4] + 10 ) % 12;
            my $year  = $_[5] + 1900 - $month / 10;

            ( ( 365 * $year )
              + ( $year / 4 )
              - ( $year / 100 )
              + ( $year / 400 )
              + ( ( ( $month * 306 ) + 5 ) / 10 )
            )
            - $Epoc;
        }
    );
}

sub _timegm {
    my $sec =
        $SecOff + $_[0] + ( SECS_PER_MINUTE * $_[1] ) + ( SECS_PER_HOUR * $_[2] );

    return $sec + ( SECS_PER_DAY * &_daygm );
}

sub timegm {
    my ( $sec, $min, $hour, $mday, $month, $year ) = @_;

    if ( $year >= 1000 ) {
        $year -= 1900;
    }
    elsif ( $year < 100 and $year >= 0 ) {
        $year += ( $year > $Breakpoint ) ? $Century : $NextCentury;
    }

    unless ( $Options{no_range_check} ) {
        if ( abs($year) >= 0x7fff ) {
            $year += 1900;
            croak
                "Cannot handle date ($sec, $min, $hour, $mday, $month, *$year*)";
        }

        croak "Month '$month' out of range 0..11"
            if $month > 11
            or $month < 0;

	my $md = $MonthDays[$month];
        ++$md
            if $month == 1 && _is_leap_year( $year + 1900 );

        croak "Day '$mday' out of range 1..$md"  if $mday > $md or $mday < 1;
        croak "Hour '$hour' out of range 0..23"  if $hour > 23  or $hour < 0;
        croak "Minute '$min' out of range 0..59" if $min > 59   or $min < 0;
        croak "Second '$sec' out of range 0..59" if $sec > 59   or $sec < 0;
    }

    my $days = _daygm( undef, undef, undef, $mday, $month, $year );

    unless ($Options{no_range_check} or abs($days) < $MaxDay) {
        my $msg = '';
        $msg .= "Day too big - $days > $MaxDay\n" if $days > $MaxDay;

	$year += 1900;
        $msg .=  "Cannot handle date ($sec, $min, $hour, $mday, $month, $year)";

	croak $msg;
    }

    return $sec
           + $SecOff
           + ( SECS_PER_MINUTE * $min )
           + ( SECS_PER_HOUR * $hour )
           + ( SECS_PER_DAY * $days );
}

sub _is_leap_year {
    return 0 if $_[0] % 4;
    return 1 if $_[0] % 100;
    return 0 if $_[0] % 400;

    return 1;
}

sub timegm_nocheck {
    local $Options{no_range_check} = 1;
    return &timegm;
}

sub timelocal {
    my $ref_t = &timegm;
    my $loc_for_ref_t = _timegm( localtime($ref_t) );

    my $zone_off = $loc_for_ref_t - $ref_t
        or return $loc_for_ref_t;

    # Adjust for timezone
    my $loc_t = $ref_t - $zone_off;

    # Are we close to a DST change or are we done
    my $dst_off = $ref_t - _timegm( localtime($loc_t) );

    # If this evaluates to true, it means that the value in $loc_t is
    # the _second_ hour after a DST change where the local time moves
    # backward.
    if ( ! $dst_off &&
         ( ( $ref_t - SECS_PER_HOUR ) - _timegm( localtime( $loc_t - SECS_PER_HOUR ) ) < 0 )
       ) {
        return $loc_t - SECS_PER_HOUR;
    }

    # Adjust for DST change
    $loc_t += $dst_off;

    return $loc_t if $dst_off > 0;

    # If the original date was a non-extent gap in a forward DST jump,
    # we should now have the wrong answer - undo the DST adjustment
    my ( $s, $m, $h ) = localtime($loc_t);
    $loc_t -= $dst_off if $s != $_[0] || $m != $_[1] || $h != $_[2];

    return $loc_t;
}

sub timelocal_nocheck {
    local $Options{no_range_check} = 1;
    return &timelocal;
}

1;

__END__

=head1 NAME

Time::Local - efficiently compute time from local and GMT time

=head1 SYNOPSIS

    $time = timelocal($sec,$min,$hour,$mday,$mon,$year);
    $time = timegm($sec,$min,$hour,$mday,$mon,$year);

=head1 DESCRIPTION

This module provides functions that are the inverse of built-in perl
functions C<localtime()> and C<gmtime()>. They accept a date as a
six-element array, and return the corresponding C<time(2)> value in
seconds since the system epoch (Midnight, January 1, 1970 GMT on Unix,
for example). This value can be positive or negative, though POSIX
only requires support for positive values, so dates before the
system's epoch may not work on all operating systems.

It is worth drawing particular attention to the expected ranges for
the values provided. The value for the day of the month is the actual
day (ie 1..31), while the month is the number of months since January
(0..11). This is consistent with the values returned from
C<localtime()> and C<gmtime()>.

=head1 FUNCTIONS

=head2 C<timelocal()> and C<timegm()>

This module exports two functions by default, C<timelocal()> and
C<timegm()>.

The C<timelocal()> and C<timegm()> functions perform range checking on
the input $sec, $min, $hour, $mday, and $mon values by default.

=head2 C<timelocal_nocheck()> and C<timegm_nocheck()>

If you are working with data you know to be valid, you can speed your
code up by using the "nocheck" variants, C<timelocal_nocheck()> and
C<timegm_nocheck()>. These variants must be explicitly imported.

    use Time::Local 'timelocal_nocheck';

    # The 365th day of 1999
    print scalar localtime timelocal_nocheck 0,0,0,365,0,99;

If you supply data which is not valid (month 27, second 1,000) the
results will be unpredictable (so don't do that).

=head2 Year Value Interpretation

Strictly speaking, the year should be specified in a form consistent
with C<localtime()>, i.e. the offset from 1900. In order to make the
interpretation of the year easier for humans, however, who are more
accustomed to seeing years as two-digit or four-digit values, the
following conventions are followed:

=over 4

=item *

Years greater than 999 are interpreted as being the actual year,
rather than the offset from 1900. Thus, 1964 would indicate the year
Martin Luther King won the Nobel prize, not the year 3864.

=item *

Years in the range 100..999 are interpreted as offset from 1900, so
that 112 indicates 2012. This rule also applies to years less than
zero (but see note below regarding date range).

=item *

Years in the range 0..99 are interpreted as shorthand for years in the
rolling "current century," defined as 50 years on either side of the
current year. Thus, today, in 1999, 0 would refer to 2000, and 45 to
2045, but 55 would refer to 1955. Twenty years from now, 55 would
instead refer to 2055. This is messy, but matches the way people
currently think about two digit dates. Whenever possible, use an
absolute four digit year instead.

=back

The scheme above allows interpretation of a wide range of dates,
particularly if 4-digit years are used.

=head2 Limits of time_t

The range of dates that can be actually be handled depends on the size
of C<time_t> (usually a signed integer) on the given
platform. Currently, this is 32 bits for most systems, yielding an
approximate range from Dec 1901 to Jan 2038.

Both C<timelocal()> and C<timegm()> croak if given dates outside the
supported range.

=head2 Ambiguous Local Times (DST)

Because of DST changes, there are many time zones where the same local
time occurs for two different GMT times on the same day. For example,
in the "Europe/Paris" time zone, the local time of 2001-10-28 02:30:00
can represent either 2001-10-28 00:30:00 GMT, B<or> 2001-10-28
01:30:00 GMT.

When given an ambiguous local time, the timelocal() function should
always return the epoch for the I<earlier> of the two possible GMT
times.

=head2 Non-Existent Local Times (DST)

When a DST change causes a locale clock to skip one hour forward,
there will be an hour's worth of local times that don't exist. Again,
for the "Europe/Paris" time zone, the local clock jumped from
2001-03-25 01:59:59 to 2001-03-25 03:00:00.

If the C<timelocal()> function is given a non-existent local time, it
will simply return an epoch value for the time one hour later.

=head2 Negative Epoch Values

Negative epoch (C<time_t>) values are not officially supported by the
POSIX standards, so this module's tests do not test them. On some
systems, they are known not to work. These include MacOS (pre-OSX) and
Win32.

On systems which do support negative epoch values, this module should
be able to cope with dates before the start of the epoch, down the
minimum value of time_t for the system.

=head1 IMPLEMENTATION

These routines are quite efficient and yet are always guaranteed to
agree with C<localtime()> and C<gmtime()>. We manage this by caching
the start times of any months we've seen before. If we know the start
time of the month, we can always calculate any time within the month.
The start times are calculated using a mathematical formula. Unlike
other algorithms that do multiple calls to C<gmtime()>.

The C<timelocal()> function is implemented using the same cache. We
just assume that we're translating a GMT time, and then fudge it when
we're done for the timezone and daylight savings arguments. Note that
the timezone is evaluated for each date because countries occasionally
change their official timezones. Assuming that C<localtime()> corrects
for these changes, this routine will also be correct.

=head1 BUGS

The whole scheme for interpreting two-digit years can be considered a
bug.

=head1 SUPPORT

Support for this module is provided via the datetime@perl.org email
list. See http://lists.perl.org/ for more details.

Please submit bugs to the CPAN RT system at
http://rt.cpan.org/NoAuth/ReportBug.html?Queue=Time-Local or via email
at bug-time-local@rt.cpan.org.

=head1 COPYRIGHT

Copyright (c) 1997-2003 Graham Barr, 2003-2007 David Rolsky.  All
rights reserved.  This program is free software; you can redistribute
it and/or modify it under the same terms as Perl itself.

The full text of the license can be found in the LICENSE file included
with this module.

=head1 AUTHOR

This module is based on a Perl 4 library, timelocal.pl, that was
included with Perl 4.036, and was most likely written by Tom
Christiansen.

The current version was written by Graham Barr.

It is now being maintained separately from the Perl core by Dave
Rolsky, <autarch@urth.org>.

=cut
