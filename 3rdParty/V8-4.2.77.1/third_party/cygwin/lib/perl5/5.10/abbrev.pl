;# Usage:
;#	%foo = ();
;#	&abbrev(*foo,LIST);
;#	...
;#	$long = $foo{$short};

#
# This library is no longer being maintained, and is included for backward
# compatibility with Perl 4 programs which may require it.
#
# In particular, this should not be used as an example of modern Perl
# programming techniques.
#
# Suggested alternative: Text::Abbrev
#

package abbrev;

sub main'abbrev {
    local(*domain) = @_;
    shift(@_);
    @cmp = @_;
    local($[) = 0;
    foreach $name (@_) {
	@extra = split(//,$name);
	$abbrev = shift(@extra);
	$len = 1;
	foreach $cmp (@cmp) {
	    next if $cmp eq $name;
	    while (@extra && substr($cmp,0,$len) eq $abbrev) {
		$abbrev .= shift(@extra);
		++$len;
	    }
	}
	$domain{$abbrev} = $name;
	while ($#extra >= 0) {
	    $abbrev .= shift(@extra);
	    $domain{$abbrev} = $name;
	}
    }
}

1;
