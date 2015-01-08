# $Id: Piece.pm 72 2007-11-19 01:26:10Z matt $

package Time::Piece;

use strict;

require Exporter;
require DynaLoader;
use Time::Seconds;
use Carp;
use Time::Local;
use UNIVERSAL qw(isa);

our @ISA = qw(Exporter DynaLoader);

our @EXPORT = qw(
    localtime
    gmtime
);

our %EXPORT_TAGS = (
    ':override' => 'internal',
    );

our $VERSION = '1.12';

bootstrap Time::Piece $VERSION;

my $DATE_SEP = '-';
my $TIME_SEP = ':';
my @MON_LIST = qw(Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec);
my @FULLMON_LIST = qw(January February March April May June July
                      August September October November December);
my @DAY_LIST = qw(Sun Mon Tue Wed Thu Fri Sat);
my @FULLDAY_LIST = qw(Sunday Monday Tuesday Wednesday Thursday Friday Saturday);

use constant 'c_sec' => 0;
use constant 'c_min' => 1;
use constant 'c_hour' => 2;
use constant 'c_mday' => 3;
use constant 'c_mon' => 4;
use constant 'c_year' => 5;
use constant 'c_wday' => 6;
use constant 'c_yday' => 7;
use constant 'c_isdst' => 8;
use constant 'c_epoch' => 9;
use constant 'c_islocal' => 10;

sub localtime {
    unshift @_, __PACKAGE__ unless eval { $_[0]->isa('Time::Piece') };
    my $class = shift;
    my $time  = shift;
    $time = time if (!defined $time);
    $class->_mktime($time, 1);
}

sub gmtime {
    unshift @_, __PACKAGE__ unless eval { $_[0]->isa('Time::Piece') };
    my $class = shift;
    my $time  = shift;
    $time = time if (!defined $time);
    $class->_mktime($time, 0);
}

sub new {
    my $class = shift;
    my ($time) = @_;
    
    my $self;
    
    if (defined($time)) {
        $self = $class->localtime($time);
    }
    elsif (ref($class) && $class->isa(__PACKAGE__)) {
        $self = $class->_mktime($class->epoch, $class->[c_islocal]);
    }
    else {
        $self = $class->localtime();
    }
    
    return bless $self, $class;
}

sub parse {
    my $proto = shift;
    my $class = ref($proto) || $proto;
    my @components;
    if (@_ > 1) {
        @components = @_;
    }
    else {
        @components = shift =~ /(\d+)$DATE_SEP(\d+)$DATE_SEP(\d+)(?:(?:T|\s+)(\d+)$TIME_SEP(\d+)(?:$TIME_SEP(\d+)))/;
        @components = reverse(@components[0..5]);
    }
    return $class->new(_strftime("%s", @components));
}

sub _mktime {
    my ($class, $time, $islocal) = @_;
    $class = eval { (ref $class) && (ref $class)->isa('Time::Piece') }
           ? ref $class
           : $class;
    if (ref($time)) {
        $time->[c_epoch] = undef;
        return wantarray ? @$time : bless [@$time, $islocal], $class;
    }
    _tzset();
    my @time = $islocal ?
            CORE::localtime($time)
                :
            CORE::gmtime($time);
    wantarray ? @time : bless [@time, $time, $islocal], $class;
}

my %_special_exports = (
  localtime => sub { my $c = $_[0]; sub { $c->localtime(@_) } },
  gmtime    => sub { my $c = $_[0]; sub { $c->gmtime(@_)    } },
);

sub export {
  my ($class, $to, @methods) = @_;
  for my $method (@methods) {
    if (exists $_special_exports{$method}) {
      no strict 'refs';
      no warnings 'redefine';
      *{$to . "::$method"} = $_special_exports{$method}->($class);
    } else {
      $class->SUPER::export($to, $method);
    }
  }
}

sub import {
    # replace CORE::GLOBAL localtime and gmtime if required
    my $class = shift;
    my %params;
    map($params{$_}++,@_,@EXPORT);
    if (delete $params{':override'}) {
        $class->export('CORE::GLOBAL', keys %params);
    }
    else {
        $class->export((caller)[0], keys %params);
    }
}

## Methods ##

sub sec {
    my $time = shift;
    $time->[c_sec];
}

*second = \&sec;

sub min {
    my $time = shift;
    $time->[c_min];
}

*minute = \&min;

sub hour {
    my $time = shift;
    $time->[c_hour];
}

sub mday {
    my $time = shift;
    $time->[c_mday];
}

*day_of_month = \&mday;

sub mon {
    my $time = shift;
    $time->[c_mon] + 1;
}

sub _mon {
    my $time = shift;
    $time->[c_mon];
}

sub month {
    my $time = shift;
    if (@_) {
        return $_[$time->[c_mon]];
    }
    elsif (@MON_LIST) {
        return $MON_LIST[$time->[c_mon]];
    }
    else {
        return $time->strftime('%b');
    }
}

*monname = \&month;

sub fullmonth {
    my $time = shift;
    if (@_) {
        return $_[$time->[c_mon]];
    }
    elsif (@FULLMON_LIST) {
        return $FULLMON_LIST[$time->[c_mon]];
    }
    else {
        return $time->strftime('%B');
    }
}

sub year {
    my $time = shift;
    $time->[c_year] + 1900;
}

sub _year {
    my $time = shift;
    $time->[c_year];
}

sub yy {
    my $time = shift;
    my $res = $time->[c_year] % 100;
    return $res > 9 ? $res : "0$res";
}

sub wday {
    my $time = shift;
    $time->[c_wday] + 1;
}

sub _wday {
    my $time = shift;
    $time->[c_wday];
}

*day_of_week = \&_wday;

sub wdayname {
    my $time = shift;
    if (@_) {
        return $_[$time->[c_wday]];
    }
    elsif (@DAY_LIST) {
        return $DAY_LIST[$time->[c_wday]];
    }
    else {
        return $time->strftime('%a');
    }
}

*day = \&wdayname;

sub fullday {
    my $time = shift;
    if (@_) {
        return $_[$time->[c_wday]];
    }
    elsif (@FULLDAY_LIST) {
        return $FULLDAY_LIST[$time->[c_wday]];
    }
    else {
        return $time->strftime('%A');
    }
}

sub yday {
    my $time = shift;
    $time->[c_yday];
}

*day_of_year = \&yday;

sub isdst {
    my $time = shift;
    $time->[c_isdst];
}

*daylight_savings = \&isdst;

# Thanks to Tony Olekshy <olekshy@cs.ualberta.ca> for this algorithm
sub tzoffset {
    my $time = shift;
    
    return Time::Seconds->new(0) unless $time->[c_islocal];

    my $epoch = $time->epoch;

    my $j = sub {

        my ($s,$n,$h,$d,$m,$y) = @_; $m += 1; $y += 1900;

        $time->_jd($y, $m, $d, $h, $n, $s);

    };

    # Compute floating offset in hours.
    #
    my $delta = 24 * (&$j(CORE::localtime $epoch) - &$j(CORE::gmtime $epoch));

    # Return value in seconds rounded to nearest minute.
    return Time::Seconds->new( int($delta * 60 + ($delta >= 0 ? 0.5 : -0.5)) * 60 );
}

sub epoch {
    my $time = shift;
    if (defined($time->[c_epoch])) {
        return $time->[c_epoch];
    }
    else {
        my $epoch = $time->[c_islocal] ?
          timelocal(@{$time}[c_sec .. c_mon], $time->[c_year]+1900)
          :
          timegm(@{$time}[c_sec .. c_mon], $time->[c_year]+1900);
        $time->[c_epoch] = $epoch;
        return $epoch;
    }
}

sub hms {
    my $time = shift;
    my $sep = @_ ? shift(@_) : $TIME_SEP;
    sprintf("%02d$sep%02d$sep%02d", $time->[c_hour], $time->[c_min], $time->[c_sec]);
}

*time = \&hms;

sub ymd {
    my $time = shift;
    my $sep = @_ ? shift(@_) : $DATE_SEP;
    sprintf("%d$sep%02d$sep%02d", $time->year, $time->mon, $time->[c_mday]);
}

*date = \&ymd;

sub mdy {
    my $time = shift;
    my $sep = @_ ? shift(@_) : $DATE_SEP;
    sprintf("%02d$sep%02d$sep%d", $time->mon, $time->[c_mday], $time->year);
}

sub dmy {
    my $time = shift;
    my $sep = @_ ? shift(@_) : $DATE_SEP;
    sprintf("%02d$sep%02d$sep%d", $time->[c_mday], $time->mon, $time->year);
}

sub datetime {
    my $time = shift;
    my %seps = (date => $DATE_SEP, T => 'T', time => $TIME_SEP, @_);
    return join($seps{T}, $time->date($seps{date}), $time->time($seps{time}));
}



# Julian Day is always calculated for UT regardless
# of local time
sub julian_day {
    my $time = shift;
    # Correct for localtime
    $time = $time->gmtime( $time->epoch ) if $time->[c_islocal];

    # Calculate the Julian day itself
    my $jd = $time->_jd( $time->year, $time->mon, $time->mday,
                        $time->hour, $time->min, $time->sec);

    return $jd;
}

# MJD is defined as JD - 2400000.5 days
sub mjd {
    return shift->julian_day - 2_400_000.5;
}

# Internal calculation of Julian date. Needed here so that
# both tzoffset and mjd/jd methods can share the code
# Algorithm from Hatcher 1984 (QJRAS 25, 53-55), and
#  Hughes et al, 1989, MNRAS, 238, 15
# See: http://adsabs.harvard.edu/cgi-bin/nph-bib_query?bibcode=1989MNRAS.238.1529H&db_key=AST
# for more details

sub _jd {
    my $self = shift;
    my ($y, $m, $d, $h, $n, $s) = @_;

    # Adjust input parameters according to the month
    $y = ( $m > 2 ? $y : $y - 1);
    $m = ( $m > 2 ? $m - 3 : $m + 9);

    # Calculate the Julian Date (assuming Julian calendar)
    my $J = int( 365.25 *( $y + 4712) )
      + int( (30.6 * $m) + 0.5)
        + 59
          + $d
            - 0.5;

    # Calculate the Gregorian Correction (since we have Gregorian dates)
    my $G = 38 - int( 0.75 * int(49+($y/100)));

    # Calculate the actual Julian Date
    my $JD = $J + $G;

    # Modify to include hours/mins/secs in floating portion.
    return $JD + ($h + ($n + $s / 60) / 60) / 24;
}

sub week {
    my $self = shift;

    my $J  = $self->julian_day;
    # Julian day is independent of time zone so add on tzoffset
    # if we are using local time here since we want the week day
    # to reflect the local time rather than UTC
    $J += ($self->tzoffset/(24*3600)) if $self->[c_islocal];

    # Now that we have the Julian day including fractions
    # convert it to an integer Julian Day Number using nearest
    # int (since the day changes at midday we oconvert all Julian
    # dates to following midnight).
    $J = int($J+0.5);

    use integer;
    my $d4 = ((($J + 31741 - ($J % 7)) % 146097) % 36524) % 1461;
    my $L  = $d4 / 1460;
    my $d1 = (($d4 - $L) % 365) + $L;
    return $d1 / 7 + 1;
}

sub _is_leap_year {
    my $year = shift;
    return (($year %4 == 0) && !($year % 100 == 0)) || ($year % 400 == 0)
               ? 1 : 0;
}

sub is_leap_year {
    my $time = shift;
    my $year = $time->year;
    return _is_leap_year($year);
}

my @MON_LAST = qw(31 28 31 30 31 30 31 31 30 31 30 31);

sub month_last_day {
    my $time = shift;
    my $year = $time->year;
    my $_mon = $time->_mon;
    return $MON_LAST[$_mon] + ($_mon == 1 ? _is_leap_year($year) : 0);
}

sub strftime {
    my $time = shift;
    my $tzname = $time->[c_islocal] ? '%Z' : 'UTC';
    my $format = @_ ? shift(@_) : "%a, %d %b %Y %H:%M:%S $tzname";
    if (!defined $time->[c_wday]) {
        if ($time->[c_islocal]) {
            return _strftime($format, CORE::localtime($time->epoch));
        }
        else {
            return _strftime($format, CORE::gmtime($time->epoch));
        }
    }
    return _strftime($format, (@$time)[c_sec..c_isdst]);
}

sub strptime {
    my $time = shift;
    my $string = shift;
    my $format = @_ ? shift(@_) : "%a, %d %b %Y %H:%M:%S %Z";
    my @vals = _strptime($string, $format);
#    warn(sprintf("got vals: %d-%d-%d %d:%d:%d\n", reverse(@vals)));
    return scalar $time->_mktime(\@vals, (ref($time) ? $time->[c_islocal] : 0));
}

sub day_list {
    shift if ref($_[0]) && $_[0]->isa(__PACKAGE__); # strip first if called as a method
    my @old = @DAY_LIST;
    if (@_) {
        @DAY_LIST = @_;
    }
    return @old;
}

sub mon_list {
    shift if ref($_[0]) && $_[0]->isa(__PACKAGE__); # strip first if called as a method
    my @old = @MON_LIST;
    if (@_) {
        @MON_LIST = @_;
    }
    return @old;
}

sub time_separator {
    shift if ref($_[0]) && $_[0]->isa(__PACKAGE__);
    my $old = $TIME_SEP;
    if (@_) {
        $TIME_SEP = $_[0];
    }
    return $old;
}

sub date_separator {
    shift if ref($_[0]) && $_[0]->isa(__PACKAGE__);
    my $old = $DATE_SEP;
    if (@_) {
        $DATE_SEP = $_[0];
    }
    return $old;
}

use overload '""' => \&cdate,
             'cmp' => \&str_compare,
             'fallback' => undef;

sub cdate {
    my $time = shift;
    if ($time->[c_islocal]) {
        return scalar(CORE::localtime($time->epoch));
    }
    else {
        return scalar(CORE::gmtime($time->epoch));
    }
}

sub str_compare {
    my ($lhs, $rhs, $reverse) = @_;
    if (UNIVERSAL::isa($rhs, 'Time::Piece')) {
        $rhs = "$rhs";
    }
    return $reverse ? $rhs cmp $lhs->cdate : $lhs->cdate cmp $rhs;
}

use overload
        '-' => \&subtract,
        '+' => \&add;

sub subtract {
    my $time = shift;
    my $rhs = shift;
    if (UNIVERSAL::isa($rhs, 'Time::Seconds')) {
        $rhs = $rhs->seconds;
    }

    if (shift)
    {
	# SWAPED is set (so someone tried an expression like NOTDATE - DATE).
	# Imitate Perl's standard behavior and return the result as if the
	# string $time resolves to was subtracted from NOTDATE.  This way,
	# classes which override this one and which have a stringify function
	# that resolves to something that looks more like a number don't need
	# to override this function.
	return $rhs - "$time";
    }
    
    if (UNIVERSAL::isa($rhs, 'Time::Piece')) {
        return Time::Seconds->new($time->epoch - $rhs->epoch);
    }
    else {
        # rhs is seconds.
        return $time->_mktime(($time->epoch - $rhs), $time->[c_islocal]);
    }
}

sub add {
    my $time = shift;
    my $rhs = shift;
    if (UNIVERSAL::isa($rhs, 'Time::Seconds')) {
        $rhs = $rhs->seconds;
    }
    croak "Invalid rhs of addition: $rhs" if ref($rhs);

    return $time->_mktime(($time->epoch + $rhs), $time->[c_islocal]);
}

use overload
        '<=>' => \&compare;

sub get_epochs {
    my ($lhs, $rhs, $reverse) = @_;
    if (!UNIVERSAL::isa($rhs, 'Time::Piece')) {
        $rhs = $lhs->new($rhs);
    }
    if ($reverse) {
        return $rhs->epoch, $lhs->epoch;
    }
    return $lhs->epoch, $rhs->epoch;
}

sub compare {
    my ($lhs, $rhs) = get_epochs(@_);
    return $lhs <=> $rhs;
}

1;
__END__

=head1 NAME

Time::Piece - Object Oriented time objects

=head1 SYNOPSIS

    use Time::Piece;
    
    my $t = localtime;
    print "Time is $t\n";
    print "Year is ", $t->year, "\n";

=head1 DESCRIPTION

This module replaces the standard localtime and gmtime functions with
implementations that return objects. It does so in a backwards
compatible manner, so that using localtime/gmtime in the way documented
in perlfunc will still return what you expect.

The module actually implements most of an interface described by
Larry Wall on the perl5-porters mailing list here:
http://www.xray.mpe.mpg.de/mailing-lists/perl5-porters/2000-01/msg00241.html

=head1 USAGE

After importing this module, when you use localtime or gmtime in a scalar
context, rather than getting an ordinary scalar string representing the
date and time, you get a Time::Piece object, whose stringification happens
to produce the same effect as the localtime and gmtime functions. There is 
also a new() constructor provided, which is the same as localtime(), except
when passed a Time::Piece object, in which case it's a copy constructor. The
following methods are available on the object:

    $t->sec                 # also available as $t->second
    $t->min                 # also available as $t->minute
    $t->hour                # 24 hour
    $t->mday                # also available as $t->day_of_month
    $t->mon                 # 1 = January
    $t->_mon                # 0 = January
    $t->monname             # Feb
    $t->month               # same as $t->monname
    $t->fullmonth           # February
    $t->year                # based at 0 (year 0 AD is, of course 1 BC)
    $t->_year               # year minus 1900
    $t->yy                  # 2 digit year
    $t->wday                # 1 = Sunday
    $t->_wday               # 0 = Sunday
    $t->day_of_week         # 0 = Sunday
    $t->wdayname            # Tue
    $t->day                 # same as wdayname
    $t->fullday             # Tuesday
    $t->yday                # also available as $t->day_of_year, 0 = Jan 01
    $t->isdst               # also available as $t->daylight_savings

    $t->hms                 # 12:34:56
    $t->hms(".")            # 12.34.56
    $t->time                # same as $t->hms

    $t->ymd                 # 2000-02-29
    $t->date                # same as $t->ymd
    $t->mdy                 # 02-29-2000
    $t->mdy("/")            # 02/29/2000
    $t->dmy                 # 29-02-2000
    $t->dmy(".")            # 29.02.2000
    $t->datetime            # 2000-02-29T12:34:56 (ISO 8601)
    $t->cdate               # Tue Feb 29 12:34:56 2000
    "$t"                    # same as $t->cdate

    $t->epoch               # seconds since the epoch
    $t->tzoffset            # timezone offset in a Time::Seconds object

    $t->julian_day          # number of days since Julian period began
    $t->mjd                 # modified Julian date (JD-2400000.5 days)

    $t->week                # week number (ISO 8601)

    $t->is_leap_year        # true if it its
    $t->month_last_day      # 28-31

    $t->time_separator($s)  # set the default separator (default ":")
    $t->date_separator($s)  # set the default separator (default "-")
    $t->day_list(@days)     # set the default weekdays
    $t->mon_list(@days)     # set the default months

    $t->strftime(FORMAT)    # same as POSIX::strftime (without the overhead
                            # of the full POSIX extension)
    $t->strftime()          # "Tue, 29 Feb 2000 12:34:56 GMT"
    
    Time::Piece->strptime(STRING, FORMAT)
                            # see strptime man page. Creates a new
                            # Time::Piece object

=head2 Local Locales

Both wdayname (day) and monname (month) allow passing in a list to use
to index the name of the days against. This can be useful if you need
to implement some form of localisation without actually installing or
using locales.

  my @days = qw( Dimanche Lundi Merdi Mercredi Jeudi Vendredi Samedi );

  my $french_day = localtime->day(@days);

These settings can be overriden globally too:

  Time::Piece::day_list(@days);

Or for months:

  Time::Piece::mon_list(@months);

And locally for months:

  print localtime->month(@months);

=head2 Date Calculations

It's possible to use simple addition and subtraction of objects:

    use Time::Seconds;
    
    my $seconds = $t1 - $t2;
    $t1 += ONE_DAY; # add 1 day (constant from Time::Seconds)

The following are valid ($t1 and $t2 are Time::Piece objects):

    $t1 - $t2; # returns Time::Seconds object
    $t1 - 42; # returns Time::Piece object
    $t1 + 533; # returns Time::Piece object

However adding a Time::Piece object to another Time::Piece object
will cause a runtime error.

Note that the first of the above returns a Time::Seconds object, so
while examining the object will print the number of seconds (because
of the overloading), you can also get the number of minutes, hours,
days, weeks and years in that delta, using the Time::Seconds API.

=head2 Date Comparisons

Date comparisons are also possible, using the full suite of "<", ">",
"<=", ">=", "<=>", "==" and "!=".

=head2 Date Parsing

Time::Piece links to your C library's strptime() function, allowing
you incredibly flexible date parsing routines. For example:

  my $t = Time::Piece->strptime("Sun 3rd Nov, 1943",
                                "%A %drd %b, %Y");
  
  print $t->strftime("%a, %d %b %Y");

Outputs:

  Wed, 03 Nov 1943

(see, it's even smart enough to fix my obvious date bug)

For more information see "man strptime", which should be on all unix
systems.

=head2 YYYY-MM-DDThh:mm:ss

The ISO 8601 standard defines the date format to be YYYY-MM-DD, and
the time format to be hh:mm:ss (24 hour clock), and if combined, they
should be concatenated with date first and with a capital 'T' in front
of the time.

=head2 Week Number

The I<week number> may be an unknown concept to some readers.  The ISO
8601 standard defines that weeks begin on a Monday and week 1 of the
year is the week that includes both January 4th and the first Thursday
of the year.  In other words, if the first Monday of January is the
2nd, 3rd, or 4th, the preceding days of the January are part of the
last week of the preceding year.  Week numbers range from 1 to 53.

=head2 Global Overriding

Finally, it's possible to override localtime and gmtime everywhere, by
including the ':override' tag in the import list:

    use Time::Piece ':override';

=head1 AUTHOR

Matt Sergeant, matt@sergeant.org
Jarkko Hietaniemi, jhi@iki.fi (while creating Time::Piece for core perl)

=head1 License

This module is free software, you may distribute it under the same terms
as Perl.

=head1 SEE ALSO

The excellent Calendar FAQ at http://www.tondering.dk/claus/calendar.html

=head1 BUGS

The test harness leaves much to be desired. Patches welcome.

=cut
