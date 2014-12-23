;# Usage:
;#	require 'stat.pl';
;#	@ary = stat(foo);
;#	$st_dev = @ary[$ST_DEV];
;#
$ST_DEV =	0 + $[;
$ST_INO =	1 + $[;
$ST_MODE =	2 + $[;
$ST_NLINK =	3 + $[;
$ST_UID =	4 + $[;
$ST_GID =	5 + $[;
$ST_RDEV =	6 + $[;
$ST_SIZE =	7 + $[;
$ST_ATIME =	8 + $[;
$ST_MTIME =	9 + $[;
$ST_CTIME =	10 + $[;
$ST_BLKSIZE =	11 + $[;
$ST_BLOCKS =	12 + $[;

;# Usage:
;#	require 'stat.pl';
;#	do Stat('foo');		# sets st_* as a side effect
;#
sub Stat {
    ($st_dev,$st_ino,$st_mode,$st_nlink,$st_uid,$st_gid,$st_rdev,$st_size,
	$st_atime,$st_mtime,$st_ctime,$st_blksize,$st_blocks) = stat(shift(@_));
}

1;
