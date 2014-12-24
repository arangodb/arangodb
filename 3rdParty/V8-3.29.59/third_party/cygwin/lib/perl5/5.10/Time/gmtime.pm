package Time::gmtime;
use strict;
use 5.006_001;

use Time::tm;

our(@ISA, @EXPORT, @EXPORT_OK, %EXPORT_TAGS, $VERSION);
BEGIN { 
    use Exporter   ();
    @ISA         = qw(Exporter Time::tm);
    @EXPORT      = qw(gmtime gmctime);
    @EXPORT_OK   = qw(  
			$tm_sec $tm_min $tm_hour $tm_mday 
			$tm_mon $tm_year $tm_wday $tm_yday 
			$tm_isdst
		    );
    %EXPORT_TAGS = ( FIELDS => [ @EXPORT_OK, @EXPORT ] );
    $VERSION     = 1.03;
}
use vars      @EXPORT_OK;

sub populate (@) {
    return unless @_;
    my $tmob = Time::tm->new();
    @$tmob = (
		$tm_sec, $tm_min, $tm_hour, $tm_mday, 
		$tm_mon, $tm_year, $tm_wday, $tm_yday, 
		$tm_isdst )
	    = @_;
    return $tmob;
} 

sub gmtime (;$)    { populate CORE::gmtime(@_ ? shift : time)}
sub gmctime (;$)   { scalar   CORE::gmtime(@_ ? shift : time)} 

1;
__END__

=head1 NAME

Time::gmtime - by-name interface to Perl's built-in gmtime() function

=head1 SYNOPSIS

 use Time::gmtime;
 $gm = gmtime();
 printf "The day in Greenwich is %s\n", 
    (qw(Sun Mon Tue Wed Thu Fri Sat Sun))[ $gm->wday() ];

 use Time::gmtime qw(:FIELDS);
 gmtime();
 printf "The day in Greenwich is %s\n", 
    (qw(Sun Mon Tue Wed Thu Fri Sat Sun))[ $tm_wday ];

 $now = gmctime();

 use Time::gmtime;
 use File::stat;
 $date_string = gmctime(stat($file)->mtime);

=head1 DESCRIPTION

This module's default exports override the core gmtime() function,
replacing it with a version that returns "Time::tm" objects.
This object has methods that return the similarly named structure field
name from the C's tm structure from F<time.h>; namely sec, min, hour,
mday, mon, year, wday, yday, and isdst.

You may also import all the structure fields directly into your namespace
as regular variables using the :FIELDS import tag.  (Note that this
still overrides your core functions.)  Access these fields as variables
named with a preceding C<tm_> in front their method names.  Thus,
C<$tm_obj-E<gt>mday()> corresponds to $tm_mday if you import the fields.

The gmctime() function provides a way of getting at the 
scalar sense of the original CORE::gmtime() function.

To access this functionality without the core overrides,
pass the C<use> an empty import list, and then access
function functions with their full qualified names.
On the other hand, the built-ins are still available
via the C<CORE::> pseudo-package.

=head1 NOTE

While this class is currently implemented using the Class::Struct
module to build a struct-like class, you shouldn't rely upon this.

=head1 AUTHOR

Tom Christiansen
