package Module::Build::Version;
use strict;

use vars qw($VERSION);
$VERSION = 0.7203;

eval "use version $VERSION";
if ($@) { # can't locate version files, use our own

    # Avoid redefined warnings if an old version.pm was available
    delete $version::{$_} foreach keys %version::;

    # first we get the stub version module
    my $version;
    while (<DATA>) {
	s/(\$VERSION)\s=\s\d+/\$VERSION = 0/;
	$version .= $_ if $_;
	last if /^1;$/;
    }

    # and now get the current version::vpp code
    my $vpp;
    while (<DATA>) {
	s/(\$VERSION)\s=\s\d+/\$VERSION = 0/;
	$vpp .= $_ if $_;
	last if /^1;$/;
    }

    # but we eval them in reverse order since version depends on
    # version::vpp to already exist
    eval $vpp; die $@ if $@;
    $INC{'version/vpp.pm'} = 'inside Module::Build::Version';
    eval $version; die $@ if $@;
    $INC{'version.pm'} = 'inside Module::Build::Version';
}

# now we can safely subclass version, installed or not
use vars qw(@ISA);
@ISA = qw(version);

1;
__DATA__
# stub version module to make everything else happy
package version;

use 5.005_04;
use strict;

use vars qw(@ISA $VERSION $CLASS *qv);

$VERSION = 0.000;

$CLASS = 'version';

push @ISA, "version::vpp";
*version::qv = \&version::vpp::qv;

# Preloaded methods go here.
sub import {
    my ($class) = @_;
    my $callpkg = caller();
    no strict 'refs';
    
    *{$callpkg."::qv"} = 
	    sub {return bless version::qv(shift), $class }
	unless defined(&{"$callpkg\::qv"});

}

1;
# replace everything from here to the end with the current version/vpp.pm

package version::vpp;
use strict;

use locale;
use vars qw ($VERSION @ISA @REGEXS);
$VERSION = 0.7203;

push @REGEXS, qr/
	^v?	# optional leading 'v'
	(\d*)	# major revision not required
	\.	# requires at least one decimal
	(?:(\d+)\.?){1,}
	/x;

use overload (
    '""'       => \&stringify,
    '0+'       => \&numify,
    'cmp'      => \&vcmp,
    '<=>'      => \&vcmp,
    'bool'     => \&vbool,
    'nomethod' => \&vnoop,
);

sub new
{
	my ($class, $value) = @_;
	my $self = bless ({}, ref ($class) || $class);
	
	if ( ref($value) && eval("$value->isa('version')") ) {
	    # Can copy the elements directly
	    $self->{version} = [ @{$value->{version} } ];
	    $self->{qv} = 1 if $value->{qv};
	    $self->{alpha} = 1 if $value->{alpha};
	    $self->{original} = ''.$value->{original};
	    return $self;
	}

	require POSIX;
	my $currlocale = POSIX::setlocale(&POSIX::LC_ALL);
	my $radix_comma = ( POSIX::localeconv()->{decimal_point} eq ',' );

	if ( not defined $value or $value =~ /^undef$/ ) {
	    # RT #19517 - special case for undef comparison
	    # or someone forgot to pass a value
	    push @{$self->{version}}, 0;
	    $self->{original} = "0";
	    return ($self);
	}

	if ( $#_ == 2 ) { # must be CVS-style
	    $value = 'v'.$_[2];
	}

	$value = _un_vstring($value);

	# exponential notation
	if ( $value =~ /\d+.?\d*e-?\d+/ ) {
	    $value = sprintf("%.9f",$value);
	    $value =~ s/(0+)$//;
	}
	
	# if the original locale used commas for decimal points, we
	# just replace commas with decimal places, rather than changing
	# locales
	if ( $radix_comma ) {
	    $value =~ tr/,/./;
	}

	# This is not very efficient, but it is morally equivalent
	# to the XS code (as that is the reference implementation).
	# See vutil/vutil.c for details
	my $qv = 0;
	my $alpha = 0;
	my $width = 3;
	my $saw_period = 0;
	my ($start, $last, $pos, $s);
	$s = 0;

	while ( substr($value,$s,1) =~ /\s/ ) { # leading whitespace is OK
	    $s++;
	}

	if (substr($value,$s,1) eq 'v') {
	    $s++;    # get past 'v'
	    $qv = 1; # force quoted version processing
	}

	$start = $last = $pos = $s;
		
	# pre-scan the input string to check for decimals/underbars
	while ( substr($value,$pos,1) =~ /[._\d]/ ) {
	    if ( substr($value,$pos,1) eq '.' ) {
		if ($alpha) {
		    require Carp;
		    Carp::croak("Invalid version format ".
		    	"(underscores before decimal)");
		}
		$saw_period++;
		$last = $pos;
	    }
	    elsif ( substr($value,$pos,1) eq '_' ) {
		if ($alpha) {
		    require Carp;
		    Carp::croak("Invalid version format ".
		    	"(multiple underscores)");
		}
		$alpha = 1;
		$width = $pos - $last - 1; # natural width of sub-version
	    }
	    $pos++;
	}

	if ( $alpha && !$saw_period ) {
	    require Carp;
	    Carp::croak("Invalid version format (alpha without decimal)");
	}

	if ( $alpha && $saw_period && $width == 0 ) {
	    require Carp;
	    Carp::croak("Invalid version format (misplaced _ in number)");
	}

	if ( $saw_period > 1 ) {
	    $qv = 1; # force quoted version processing
	}

	$pos = $s;

	if ( $qv ) {
	    $self->{qv} = 1;
	}

	if ( $alpha ) {
	    $self->{alpha} = 1;
	}

	if ( !$qv && $width < 3 ) {
	    $self->{width} = $width;
	}

	while ( substr($value,$pos,1) =~ /\d/ ) {
	    $pos++;
	}

	if ( substr($value,$pos,1) !~ /[a-z]/ ) { ### FIX THIS ###
	    my $rev;

	    while (1) {
		$rev = 0;
		{

		    # this is atoi() that delimits on underscores
		    my $end = $pos;
		    my $mult = 1;
		    my $orev;

		    # the following if() will only be true after the decimal
		    # point of a version originally created with a bare
		    # floating point number, i.e. not quoted in any way
		    if ( !$qv && $s > $start && $saw_period == 1 ) {
			$mult *= 100;
			while ( $s < $end ) {
			    $orev = $rev;
			    $rev += substr($value,$s,1) * $mult;
			    $mult /= 10;
			    if ( abs($orev) > abs($rev) ) {
				require Carp;
				Carp::croak("Integer overflow in version");
			    }
			    $s++;
			    if ( substr($value,$s,1) eq '_' ) {
				$s++;
			    }
			}
		    }
		    else {
			while (--$end >= $s) {
			    $orev = $rev;
			    $rev += substr($value,$end,1) * $mult;
			    $mult *= 10;
			    if ( abs($orev) > abs($rev) ) {
				require Carp;
				Carp::croak("Integer overflow in version");
			    }
			}
		    }
		}

		# Append revision
		push @{$self->{version}}, $rev;
		if ( substr($value,$pos,1) eq '.' 
		    && substr($value,$pos+1,1) =~ /\d/ ) {
		    $s = ++$pos;
		}
		elsif ( substr($value,$pos,1) eq '_' 
		    && substr($value,$pos+1,1) =~ /\d/ ) {
		    $s = ++$pos;
		}
		elsif ( substr($value,$pos,1) =~ /\d/ ) {
		    $s = $pos;
		}
		else {
		    $s = $pos;
		    last;
		}
		if ( $qv ) {
		    while ( substr($value,$pos,1) =~ /\d/ ) {
			$pos++;
		    }
		}
		else {
		    my $digits = 0;
		    while (substr($value,$pos,1) =~ /[\d_]/ && $digits < 3) {
			if ( substr($value,$pos,1) ne '_' ) {
			    $digits++;
			}
			$pos++;
		    }
		}
	    }
	}
	if ( $qv ) { # quoted versions always get at least three terms
	    my $len = scalar @{$self->{version}};
	    $len = 3 - $len;
	    while ($len-- > 0) {
		push @{$self->{version}}, 0;
	    }
	}

	if ( substr($value,$pos) ) { # any remaining text
	    warn "Version string '$value' contains invalid data; ".
	         "ignoring: '".substr($value,$pos)."'";
	}

	# cache the original value for use when stringification
	$self->{original} = substr($value,0,$pos);

	return ($self);
}

sub numify 
{
    my ($self) = @_;
    unless (_verify($self)) {
	require Carp;
	Carp::croak("Invalid version object");
    }
    my $width = $self->{width} || 3;
    my $alpha = $self->{alpha} || "";
    my $len = $#{$self->{version}};
    my $digit = $self->{version}[0];
    my $string = sprintf("%d.", $digit );

    for ( my $i = 1 ; $i < $len ; $i++ ) {
	$digit = $self->{version}[$i];
	if ( $width < 3 ) {
	    my $denom = 10**(3-$width);
	    my $quot = int($digit/$denom);
	    my $rem = $digit - ($quot * $denom);
	    $string .= sprintf("%0".$width."d_%d", $quot, $rem);
	}
	else {
	    $string .= sprintf("%03d", $digit);
	}
    }

    if ( $len > 0 ) {
	$digit = $self->{version}[$len];
	if ( $alpha && $width == 3 ) {
	    $string .= "_";
	}
	$string .= sprintf("%0".$width."d", $digit);
    }
    else # $len = 0
    {
	$string .= sprintf("000");
    }

    return $string;
}

sub normal 
{
    my ($self) = @_;
    unless (_verify($self)) {
	require Carp;
	Carp::croak("Invalid version object");
    }
    my $alpha = $self->{alpha} || "";
    my $len = $#{$self->{version}};
    my $digit = $self->{version}[0];
    my $string = sprintf("v%d", $digit );

    for ( my $i = 1 ; $i < $len ; $i++ ) {
	$digit = $self->{version}[$i];
	$string .= sprintf(".%d", $digit);
    }

    if ( $len > 0 ) {
	$digit = $self->{version}[$len];
	if ( $alpha ) {
	    $string .= sprintf("_%0d", $digit);
	}
	else {
	    $string .= sprintf(".%0d", $digit);
	}
    }

    if ( $len <= 2 ) {
	for ( $len = 2 - $len; $len != 0; $len-- ) {
	    $string .= sprintf(".%0d", 0);
	}
    }

    return $string;
}

sub stringify
{
    my ($self) = @_;
    unless (_verify($self)) {
	require Carp;
	Carp::croak("Invalid version object");
    }
    return $self->{original};
}

sub vcmp
{
    require UNIVERSAL;
    my ($left,$right,$swap) = @_;
    my $class = ref($left);
    unless ( UNIVERSAL::isa($right, $class) ) {
	$right = $class->new($right);
    }

    if ( $swap ) {
	($left, $right) = ($right, $left);
    }
    unless (_verify($left)) {
	require Carp;
	Carp::croak("Invalid version object");
    }
    unless (_verify($right)) {
	require Carp;
	Carp::croak("Invalid version object");
    }
    my $l = $#{$left->{version}};
    my $r = $#{$right->{version}};
    my $m = $l < $r ? $l : $r;
    my $lalpha = $left->is_alpha;
    my $ralpha = $right->is_alpha;
    my $retval = 0;
    my $i = 0;
    while ( $i <= $m && $retval == 0 ) {
	$retval = $left->{version}[$i] <=> $right->{version}[$i];
	$i++;
    }

    # tiebreaker for alpha with identical terms
    if ( $retval == 0 
	&& $l == $r 
	&& $left->{version}[$m] == $right->{version}[$m]
	&& ( $lalpha || $ralpha ) ) {

	if ( $lalpha && !$ralpha ) {
	    $retval = -1;
	}
	elsif ( $ralpha && !$lalpha) {
	    $retval = +1;
	}
    }

    # possible match except for trailing 0's
    if ( $retval == 0 && $l != $r ) {
	if ( $l < $r ) {
	    while ( $i <= $r && $retval == 0 ) {
		if ( $right->{version}[$i] != 0 ) {
		    $retval = -1; # not a match after all
		}
		$i++;
	    }
	}
	else {
	    while ( $i <= $l && $retval == 0 ) {
		if ( $left->{version}[$i] != 0 ) {
		    $retval = +1; # not a match after all
		}
		$i++;
	    }
	}
    }

    return $retval;  
}

sub vbool {
    my ($self) = @_;
    return vcmp($self,$self->new("0"),1);
}

sub vnoop { 
    require Carp; 
    Carp::croak("operation not supported with version object");
}

sub is_alpha {
    my ($self) = @_;
    return (exists $self->{alpha});
}

sub qv {
    my ($value) = @_;

    $value = _un_vstring($value);
    $value = 'v'.$value unless $value =~ /(^v|\d+\.\d+\.\d)/;
    my $version = version->new($value); # always use base class
    return $version;
}

sub is_qv {
    my ($self) = @_;
    return (exists $self->{qv});
}


sub _verify {
    my ($self) = @_;
    if ( ref($self)
	&& eval { exists $self->{version} }
	&& ref($self->{version}) eq 'ARRAY'
	) {
	return 1;
    }
    else {
	return 0;
    }
}

sub _un_vstring {
    my $value = shift;
    # may be a v-string
    if ( $] >= 5.006_000 && length($value) >= 3 && $value !~ /[._]/ ) {
	my $tvalue = sprintf("v%vd",$value);
	if ( $tvalue =~ /^v\d+\.\d+\.\d+$/ ) {
	    # must be a v-string
	    $value = $tvalue;
	}
    }
    return $value;
}

# Thanks to Yitzchak Scott-Thoennes for this mode of operation
{
    local $^W;
    *UNIVERSAL::VERSION = sub {
	my ($obj, $req) = @_;
	my $class = ref($obj) || $obj;

	no strict 'refs';
	eval "require $class" unless %{"$class\::"}; # already existing
	return undef if $@ =~ /Can't locate/ and not defined $req;
	
	if ( not %{"$class\::"} and $] >= 5.008) { # file but no package
	    require Carp;
	    Carp::croak( "$class defines neither package nor VERSION"
		."--version check failed");
	}
	
	my $version = eval "\$$class\::VERSION";
	if ( defined $version ) {
	    local $^W if $] <= 5.008;
	    $version = version::vpp->new($version);
	}

	if ( defined $req ) {
	    unless ( defined $version ) {
		require Carp;
		my $msg =  $] < 5.006 
		? "$class version $req required--this is only version "
		: "$class does not define \$$class\::VERSION"
		  ."--version check failed";

		if ( $ENV{VERSION_DEBUG} ) {
		    Carp::confess($msg);
		}
		else {
		    Carp::croak($msg);
		}
	    }

	    $req = version::vpp->new($req);

	    if ( $req > $version ) {
		require Carp;
		if ( $req->is_qv ) {
		    Carp::croak( 
			sprintf ("%s version %s required--".
			    "this is only version %s", $class,
			    $req->normal, $version->normal)
		    );
		}
		else {
		    Carp::croak( 
			sprintf ("%s version %s required--".
			    "this is only version %s", $class,
			    $req->stringify, $version->stringify)
		    );
		}
	    }
	}

	return defined $version ? $version->stringify : undef;
    };
}

1; #this line is important and will help the module return a true value
