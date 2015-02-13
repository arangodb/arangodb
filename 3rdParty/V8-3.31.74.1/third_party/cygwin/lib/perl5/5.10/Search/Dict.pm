package Search::Dict;
require 5.000;
require Exporter;

use strict;

our $VERSION = '1.02';
our @ISA = qw(Exporter);
our @EXPORT = qw(look);

=head1 NAME

Search::Dict, look - search for key in dictionary file

=head1 SYNOPSIS

    use Search::Dict;
    look *FILEHANDLE, $key, $dict, $fold;

    use Search::Dict;
    look *FILEHANDLE, $params;

=head1 DESCRIPTION

Sets file position in FILEHANDLE to be first line greater than or equal
(stringwise) to I<$key>.  Returns the new file position, or -1 if an error
occurs.

The flags specify dictionary order and case folding:

If I<$dict> is true, search by dictionary order (ignore anything but word
characters and whitespace).  The default is honour all characters.

If I<$fold> is true, ignore case.  The default is to honour case.

If there are only three arguments and the third argument is a hash
reference, the keys of that hash can have values C<dict>, C<fold>, and
C<comp> or C<xfrm> (see below), and their correponding values will be
used as the parameters.

If a comparison subroutine (comp) is defined, it must return less than zero,
zero, or greater than zero, if the first comparand is less than,
equal, or greater than the second comparand.

If a transformation subroutine (xfrm) is defined, its value is used to
transform the lines read from the filehandle before their comparison.

=cut

sub look {
    my($fh,$key,$dict,$fold) = @_;
    my ($comp, $xfrm);
    if (@_ == 3 && ref $dict eq 'HASH') {
	my $params = $dict;
	$dict = 0;
	$dict = $params->{dict} if exists $params->{dict};
	$fold = $params->{fold} if exists $params->{fold};
	$comp = $params->{comp} if exists $params->{comp};
	$xfrm = $params->{xfrm} if exists $params->{xfrm};
    }
    $comp = sub { $_[0] cmp $_[1] } unless defined $comp;
    local($_);
    my(@stat) = stat($fh)
	or return -1;
    my($size, $blksize) = @stat[7,11];
    $blksize ||= 8192;
    $key =~ s/[^\w\s]//g if $dict;
    $key = lc $key       if $fold;
    # find the right block
    my($min, $max) = (0, int($size / $blksize));
    my $mid;
    while ($max - $min > 1) {
	$mid = int(($max + $min) / 2);
	seek($fh, $mid * $blksize, 0)
	    or return -1;
	<$fh> if $mid;			# probably a partial line
	$_ = <$fh>;
	$_ = $xfrm->($_) if defined $xfrm;
	chomp;
	s/[^\w\s]//g if $dict;
	$_ = lc $_   if $fold;
	if (defined($_) && $comp->($_, $key) < 0) {
	    $min = $mid;
	}
	else {
	    $max = $mid;
	}
    }
    # find the right line
    $min *= $blksize;
    seek($fh,$min,0)
	or return -1;
    <$fh> if $min;
    for (;;) {
	$min = tell($fh);
	defined($_ = <$fh>)
	    or last;
	$_ = $xfrm->($_) if defined $xfrm;
	chomp;
	s/[^\w\s]//g if $dict;
	$_ = lc $_   if $fold;
	last if $comp->($_, $key) >= 0;
    }
    seek($fh,$min,0);
    $min;
}

1;
