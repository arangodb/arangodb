;# The validate routine takes a single multiline string consisting of
;# lines containing a filename plus a file test to try on it.  (The
;# file test may also be a 'cd', causing subsequent relative filenames
;# to be interpreted relative to that directory.)  After the file test
;# you may put '|| die' to make it a fatal error if the file test fails.
;# The default is '|| warn'.  The file test may optionally have a ! prepended
;# to test for the opposite condition.  If you do a cd and then list some
;# relative filenames, you may want to indent them slightly for readability.
;# If you supply your own "die" or "warn" message, you can use $file to
;# interpolate the filename.

;# Filetests may be bunched:  -rwx tests for all of -r, -w and -x.
;# Only the first failed test of the bunch will produce a warning.

;# The routine returns the number of warnings issued.

;# Usage:
;#	require "validate.pl";
;#	$warnings += do validate('
;#	/vmunix			-e || die
;#	/boot			-e || die
;#	/bin			cd
;#	    csh			-ex
;#	    csh			!-ug
;#	    sh			-ex
;#	    sh			!-ug
;#	/usr			-d || warn "What happened to $file?\n"
;#	');

sub validate {
    local($file,$test,$warnings,$oldwarnings);
    foreach $check (split(/\n/,$_[0])) {
	next if $check =~ /^#/;
	next if $check =~ /^$/;
	($file,$test) = split(' ',$check,2);
	if ($test =~ s/^(!?-)(\w{2,}\b)/$1Z/) {
	    $testlist = $2;
	    @testlist = split(//,$testlist);
	}
	else {
	    @testlist = ('Z');
	}
	$oldwarnings = $warnings;
	foreach $one (@testlist) {
	    $this = $test;
	    $this =~ s/(-\w\b)/$1 \$file/g;
	    $this =~ s/-Z/-$one/;
	    $this .= ' || warn' unless $this =~ /\|\|/;
	    $this =~ s/^(.*\S)\s*\|\|\s*(die|warn)$/$1 || do valmess('$2','$1')/;
	    $this =~ s/\bcd\b/chdir (\$cwd = \$file)/g;
	    eval $this;
	    last if $warnings > $oldwarnings;
	}
    }
    $warnings;
}

sub valmess {
    local($disposition,$this) = @_;
    $file = $cwd . '/' . $file unless $file =~ m|^/|;
    if ($this =~ /^(!?)-(\w)\s+\$file\s*$/) {
	$neg = $1;
	$tmp = $2;
	$tmp eq 'r' && ($mess = "$file is not readable by uid $>.");
	$tmp eq 'w' && ($mess = "$file is not writable by uid $>.");
	$tmp eq 'x' && ($mess = "$file is not executable by uid $>.");
	$tmp eq 'o' && ($mess = "$file is not owned by uid $>.");
	$tmp eq 'R' && ($mess = "$file is not readable by you.");
	$tmp eq 'W' && ($mess = "$file is not writable by you.");
	$tmp eq 'X' && ($mess = "$file is not executable by you.");
	$tmp eq 'O' && ($mess = "$file is not owned by you.");
	$tmp eq 'e' && ($mess = "$file does not exist.");
	$tmp eq 'z' && ($mess = "$file does not have zero size.");
	$tmp eq 's' && ($mess = "$file does not have non-zero size.");
	$tmp eq 'f' && ($mess = "$file is not a plain file.");
	$tmp eq 'd' && ($mess = "$file is not a directory.");
	$tmp eq 'l' && ($mess = "$file is not a symbolic link.");
	$tmp eq 'p' && ($mess = "$file is not a named pipe (FIFO).");
	$tmp eq 'S' && ($mess = "$file is not a socket.");
	$tmp eq 'b' && ($mess = "$file is not a block special file.");
	$tmp eq 'c' && ($mess = "$file is not a character special file.");
	$tmp eq 'u' && ($mess = "$file does not have the setuid bit set.");
	$tmp eq 'g' && ($mess = "$file does not have the setgid bit set.");
	$tmp eq 'k' && ($mess = "$file does not have the sticky bit set.");
	$tmp eq 'T' && ($mess = "$file is not a text file.");
	$tmp eq 'B' && ($mess = "$file is not a binary file.");
	if ($neg eq '!') {
	    $mess =~ s/ is not / should not be / ||
	    $mess =~ s/ does not / should not / ||
	    $mess =~ s/ not / /;
	}
	print STDERR $mess,"\n";
    }
    else {
	$this =~ s/\$file/'$file'/g;
	print STDERR "Can't do $this.\n";
    }
    if ($disposition eq 'die') { exit 1; }
    ++$warnings;
}

1;
