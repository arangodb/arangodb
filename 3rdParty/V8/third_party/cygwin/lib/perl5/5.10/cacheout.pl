#
# This library is no longer being maintained, and is included for backward
# compatibility with Perl 4 programs which may require it.
#
# In particular, this should not be used as an example of modern Perl
# programming techniques.
#
# Suggested alternative: FileCache

# Open in their package.

sub cacheout'open {
    open($_[0], $_[1]);
}

# Close as well

sub cacheout'close {
    close($_[0]);
}

# But only this sub name is visible to them.

sub cacheout {
    package cacheout;

    ($file) = @_;
    if (!$isopen{$file}) {
	if (++$numopen > $maxopen) {
	    local(@lru) = sort {$isopen{$a} <=> $isopen{$b};} keys(%isopen);
	    splice(@lru, $maxopen / 3);
	    $numopen -= @lru;
	    for (@lru) { &close($_); delete $isopen{$_}; }
	}
	&open($file, ($saw{$file}++ ? '>>' : '>') . $file)
	    || die "Can't create $file: $!\n";
    }
    $isopen{$file} = ++$seq;
}

package cacheout;

$seq = 0;
$numopen = 0;

if (open(PARAM,'/usr/include/sys/param.h')) {
    local($_, $.);
    while (<PARAM>) {
	$maxopen = $1 - 4 if /^\s*#\s*define\s+NOFILE\s+(\d+)/;
    }
    close PARAM;
}
$maxopen = 16 unless $maxopen;

1;
