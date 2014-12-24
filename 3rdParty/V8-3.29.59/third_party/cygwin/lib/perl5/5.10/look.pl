;# Usage: &look(*FILEHANDLE,$key,$dict,$fold)
#
# This library is no longer being maintained, and is included for backward
# compatibility with Perl 4 programs which may require it.
#
# In particular, this should not be used as an example of modern Perl
# programming techniques.
#
;# Sets file position in FILEHANDLE to be first line greater than or equal
;# (stringwise) to $key.  Pass flags for dictionary order and case folding.

sub look {
    local(*FH,$key,$dict,$fold) = @_;
    local($max,$min,$mid,$_);
    local($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
       $blksize,$blocks) = stat(FH);
    $blksize = 8192 unless $blksize;
    $key =~ s/[^\w\s]//g if $dict;
    $key = lc $key if $fold;
    $max = int($size / $blksize);
    while ($max - $min > 1) {
	$mid = int(($max + $min) / 2);
	seek(FH,$mid * $blksize,0);
	$_ = <FH> if $mid;		# probably a partial line
	$_ = <FH>;
	chop;
	s/[^\w\s]//g if $dict;
	$_ = lc $_ if $fold;
	if ($_ lt $key) {
	    $min = $mid;
	}
	else {
	    $max = $mid;
	}
    }
    $min *= $blksize;
    seek(FH,$min,0);
    <FH> if $min;
    while (<FH>) {
	chop;
	s/[^\w\s]//g if $dict;
	$_ = lc $_ if $fold;
	last if $_ ge $key;
	$min = tell(FH);
    }
    seek(FH,$min,0);
    $min;
}

1;
