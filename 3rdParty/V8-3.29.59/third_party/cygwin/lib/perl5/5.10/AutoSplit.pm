package AutoSplit;

use Exporter ();
use Config qw(%Config);
use File::Basename ();
use File::Path qw(mkpath);
use File::Spec::Functions qw(curdir catfile catdir);
use strict;
our($VERSION, @ISA, @EXPORT, @EXPORT_OK, $Verbose, $Keep, $Maxlen,
    $CheckForAutoloader, $CheckModTime);

$VERSION = "1.05_01";
@ISA = qw(Exporter);
@EXPORT = qw(&autosplit &autosplit_lib_modules);
@EXPORT_OK = qw($Verbose $Keep $Maxlen $CheckForAutoloader $CheckModTime);

=head1 NAME

AutoSplit - split a package for autoloading

=head1 SYNOPSIS

 autosplit($file, $dir, $keep, $check, $modtime);

 autosplit_lib_modules(@modules);

=head1 DESCRIPTION

This function will split up your program into files that the AutoLoader
module can handle. It is used by both the standard perl libraries and by
the MakeMaker utility, to automatically configure libraries for autoloading.

The C<autosplit> interface splits the specified file into a hierarchy 
rooted at the directory C<$dir>. It creates directories as needed to reflect
class hierarchy, and creates the file F<autosplit.ix>. This file acts as
both forward declaration of all package routines, and as timestamp for the
last update of the hierarchy.

The remaining three arguments to C<autosplit> govern other options to
the autosplitter.

=over 2

=item $keep

If the third argument, I<$keep>, is false, then any
pre-existing C<*.al> files in the autoload directory are removed if
they are no longer part of the module (obsoleted functions).
$keep defaults to 0.

=item $check

The
fourth argument, I<$check>, instructs C<autosplit> to check the module
currently being split to ensure that it includes a C<use>
specification for the AutoLoader module, and skips the module if
AutoLoader is not detected.
$check defaults to 1.

=item $modtime

Lastly, the I<$modtime> argument specifies
that C<autosplit> is to check the modification time of the module
against that of the C<autosplit.ix> file, and only split the module if
it is newer.
$modtime defaults to 1.

=back

Typical use of AutoSplit in the perl MakeMaker utility is via the command-line
with:

 perl -e 'use AutoSplit; autosplit($ARGV[0], $ARGV[1], 0, 1, 1)'

Defined as a Make macro, it is invoked with file and directory arguments;
C<autosplit> will split the specified file into the specified directory and
delete obsolete C<.al> files, after checking first that the module does use
the AutoLoader, and ensuring that the module is not already currently split
in its current form (the modtime test).

The C<autosplit_lib_modules> form is used in the building of perl. It takes
as input a list of files (modules) that are assumed to reside in a directory
B<lib> relative to the current directory. Each file is sent to the 
autosplitter one at a time, to be split into the directory B<lib/auto>.

In both usages of the autosplitter, only subroutines defined following the
perl I<__END__> token are split out into separate files. Some
routines may be placed prior to this marker to force their immediate loading
and parsing.

=head2 Multiple packages

As of version 1.01 of the AutoSplit module it is possible to have
multiple packages within a single file. Both of the following cases
are supported:

   package NAME;
   __END__
   sub AAA { ... }
   package NAME::option1;
   sub BBB { ... }
   package NAME::option2;
   sub BBB { ... }

   package NAME;
   __END__
   sub AAA { ... }
   sub NAME::option1::BBB { ... }
   sub NAME::option2::BBB { ... }

=head1 DIAGNOSTICS

C<AutoSplit> will inform the user if it is necessary to create the
top-level directory specified in the invocation. It is preferred that
the script or installation process that invokes C<AutoSplit> have
created the full directory path ahead of time. This warning may
indicate that the module is being split into an incorrect path.

C<AutoSplit> will warn the user of all subroutines whose name causes
potential file naming conflicts on machines with drastically limited
(8 characters or less) file name length. Since the subroutine name is
used as the file name, these warnings can aid in portability to such
systems.

Warnings are issued and the file skipped if C<AutoSplit> cannot locate
either the I<__END__> marker or a "package Name;"-style specification.

C<AutoSplit> will also emit general diagnostics for inability to
create directories or files.

=head1 AUTHOR

C<AutoSplit> is maintained by the perl5-porters. Please direct
any questions to the canonical mailing list. Anything that
is applicable to the CPAN release can be sent to its maintainer,
though.

Author and Maintainer: The Perl5-Porters <perl5-porters@perl.org>

Maintainer of the CPAN release: Steffen Mueller <smueller@cpan.org>

=head1 COPYRIGHT AND LICENSE

This package has been part of the perl core since the first release
of perl5. It has been released separately to CPAN so older installations
can benefit from bug fixes.

This package has the same copyright and license as the perl core:

             Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999,
        2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008
        by Larry Wall and others
    
			    All rights reserved.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of either:
    
	a) the GNU General Public License as published by the Free
	Software Foundation; either version 1, or (at your option) any
	later version, or
    
	b) the "Artistic License" which comes with this Kit.
    
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See either
    the GNU General Public License or the Artistic License for more details.
    
    You should have received a copy of the Artistic License with this
    Kit, in the file named "Artistic".  If not, I'll be glad to provide one.
    
    You should also have received a copy of the GNU General Public License
    along with this program in the file named "Copying". If not, write to the 
    Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 
    02111-1307, USA or visit their web page on the internet at
    http://www.gnu.org/copyleft/gpl.html.
    
    For those of you that choose to use the GNU General Public License,
    my interpretation of the GNU General Public License is that no Perl
    script falls under the terms of the GPL unless you explicitly put
    said script under the terms of the GPL yourself.  Furthermore, any
    object code linked with perl does not automatically fall under the
    terms of the GPL, provided such object code only adds definitions
    of subroutines and variables, and does not otherwise impair the
    resulting interpreter from executing any standard Perl script.  I
    consider linking in C subroutines in this manner to be the moral
    equivalent of defining subroutines in the Perl language itself.  You
    may sell such an object file as proprietary provided that you provide
    or offer to provide the Perl source, as specified by the GNU General
    Public License.  (This is merely an alternate way of specifying input
    to the program.)  You may also sell a binary produced by the dumping of
    a running Perl script that belongs to you, provided that you provide or
    offer to provide the Perl source as specified by the GPL.  (The
    fact that a Perl interpreter and your code are in the same binary file
    is, in this case, a form of mere aggregation.)  This is my interpretation
    of the GPL.  If you still have concerns or difficulties understanding
    my intent, feel free to contact me.  Of course, the Artistic License
    spells all this out for your protection, so you may prefer to use that.

=cut

# for portability warn about names longer than $maxlen
$Maxlen  = 8;	# 8 for dos, 11 (14-".al") for SYSVR3
$Verbose = 1;	# 0=none, 1=minimal, 2=list .al files
$Keep    = 0;
$CheckForAutoloader = 1;
$CheckModTime = 1;

my $IndexFile = "autosplit.ix";	# file also serves as timestamp
my $maxflen = 255;
$maxflen = 14 if $Config{'d_flexfnam'} ne 'define';
if (defined (&Dos::UseLFN)) {
     $maxflen = Dos::UseLFN() ? 255 : 11;
}
my $Is_VMS = ($^O eq 'VMS');

# allow checking for valid ': attrlist' attachments.
# extra jugglery required to support both 5.8 and 5.9/5.10 features
# (support for 5.8 required for cross-compiling environments)

my $attr_list = 
  $] >= 5.009005 ?
  eval <<'__QR__'
  qr{
    \s* : \s*
    (?:
	# one attribute
	(?> # no backtrack
	    (?! \d) \w+
	    (?<nested> \( (?: [^()]++ | (?&nested)++ )*+ \) ) ?
	)
	(?: \s* : \s* | \s+ (?! :) )
    )*
  }x
__QR__
  :
  do {
    # In pre-5.9.5 world we have to do dirty tricks.
    # (we use 'our' rather than 'my' here, due to the rather complex and buggy
    # behaviour of lexicals with qr// and (??{$lex}) )
    our $trick1; # yes, cannot our and assign at the same time.
    $trick1 = qr{ \( (?: (?> [^()]+ ) | (??{ $trick1 }) )* \) }x;
    our $trick2 = qr{ (?> (?! \d) \w+ (?:$trick1)? ) (?:\s*\:\s*|\s+(?!\:)) }x;
    qr{ \s* : \s* (?: $trick2 )* }x;
  };

sub autosplit{
    my($file, $autodir,  $keep, $ckal, $ckmt) = @_;
    # $file    - the perl source file to be split (after __END__)
    # $autodir - the ".../auto" dir below which to write split subs
    # Handle optional flags:
    $keep = $Keep unless defined $keep;
    $ckal = $CheckForAutoloader unless defined $ckal;
    $ckmt = $CheckModTime unless defined $ckmt;
    autosplit_file($file, $autodir, $keep, $ckal, $ckmt);
}

sub carp{
    require Carp;
    goto &Carp::carp;
}

# This function is used during perl building/installation
# ./miniperl -e 'use AutoSplit; autosplit_lib_modules(@ARGV)' ...

sub autosplit_lib_modules {
    my(@modules) = @_; # list of Module names
    local $_; # Avoid clobber.
    while (defined($_ = shift @modules)) {
	while (m#([^:]+)::([^:].*)#) { # in case specified as ABC::XYZ
	    $_ = catfile($1, $2);
	}
	s|\\|/|g;		# bug in ksh OS/2
	s#^lib/##s; # incase specified as lib/*.pm
	my($lib) = catfile(curdir(), "lib");
	if ($Is_VMS) { # may need to convert VMS-style filespecs
	    $lib =~ s#^\[\]#.\/#;
	}
	s#^$lib\W+##s; # incase specified as ./lib/*.pm
	if ($Is_VMS && /[:>\]]/) { # may need to convert VMS-style filespecs
	    my ($dir,$name) = (/(.*])(.*)/s);
	    $dir =~ s/.*lib[\.\]]//s;
	    $dir =~ s#[\.\]]#/#g;
	    $_ = $dir . $name;
	}
	autosplit_file(catfile($lib, $_), catfile($lib, "auto"),
		       $Keep, $CheckForAutoloader, $CheckModTime);
    }
    0;
}


# private functions

my $self_mod_time = (stat __FILE__)[9];

sub autosplit_file {
    my($filename, $autodir, $keep, $check_for_autoloader, $check_mod_time)
	= @_;
    my(@outfiles);
    local($_);
    local($/) = "\n";

    # where to write output files
    $autodir ||= catfile(curdir(), "lib", "auto");
    if ($Is_VMS) {
	($autodir = VMS::Filespec::unixpath($autodir)) =~ s|/\z||;
	$filename = VMS::Filespec::unixify($filename); # may have dirs
    }
    unless (-d $autodir){
	mkpath($autodir,0,0755);
	# We should never need to create the auto dir
	# here. installperl (or similar) should have done
	# it. Expecting it to exist is a valuable sanity check against
	# autosplitting into some random directory by mistake.
	print "Warning: AutoSplit had to create top-level " .
	    "$autodir unexpectedly.\n";
    }

    # allow just a package name to be used
    $filename .= ".pm" unless ($filename =~ m/\.pm\z/);

    open(my $in, "<$filename") or die "AutoSplit: Can't open $filename: $!\n";
    my($pm_mod_time) = (stat($filename))[9];
    my($autoloader_seen) = 0;
    my($in_pod) = 0;
    my($def_package,$last_package,$this_package,$fnr);
    while (<$in>) {
	# Skip pod text.
	$fnr++;
	$in_pod = 1 if /^=\w/;
	$in_pod = 0 if /^=cut/;
	next if ($in_pod || /^=cut/);
        next if /^\s*#/;

	# record last package name seen
	$def_package = $1 if (m/^\s*package\s+([\w:]+)\s*;/);
	++$autoloader_seen if m/^\s*(use|require)\s+AutoLoader\b/;
	++$autoloader_seen if m/\bISA\s*=.*\bAutoLoader\b/;
	last if /^__END__/;
    }
    if ($check_for_autoloader && !$autoloader_seen){
	print "AutoSplit skipped $filename: no AutoLoader used\n"
	    if ($Verbose>=2);
	return 0;
    }
    $_ or die "Can't find __END__ in $filename\n";

    $def_package or die "Can't find 'package Name;' in $filename\n";

    my($modpname) = _modpname($def_package); 

    # this _has_ to match so we have a reasonable timestamp file
    die "Package $def_package ($modpname.pm) does not ".
	"match filename $filename"
	    unless ($filename =~ m/\Q$modpname.pm\E$/ or
		    ($^O eq 'dos') or ($^O eq 'MSWin32') or ($^O eq 'NetWare') or
	            $Is_VMS && $filename =~ m/$modpname.pm/i);

    my($al_idx_file) = catfile($autodir, $modpname, $IndexFile);

    if ($check_mod_time){
	my($al_ts_time) = (stat("$al_idx_file"))[9] || 1;
	if ($al_ts_time >= $pm_mod_time and
	    $al_ts_time >= $self_mod_time){
	    print "AutoSplit skipped ($al_idx_file newer than $filename)\n"
		if ($Verbose >= 2);
	    return undef;	# one undef, not a list
	}
    }

    my($modnamedir) = catdir($autodir, $modpname);
    print "AutoSplitting $filename ($modnamedir)\n"
	if $Verbose;

    unless (-d $modnamedir){
	mkpath($modnamedir,0,0777);
    }

    # We must try to deal with some SVR3 systems with a limit of 14
    # characters for file names. Sadly we *cannot* simply truncate all
    # file names to 14 characters on these systems because we *must*
    # create filenames which exactly match the names used by AutoLoader.pm.
    # This is a problem because some systems silently truncate the file
    # names while others treat long file names as an error.

    my $Is83 = $maxflen==11;  # plain, case INSENSITIVE dos filenames

    my(@subnames, $subname, %proto, %package);
    my @cache = ();
    my $caching = 1;
    $last_package = '';
    my $out;
    while (<$in>) {
	$fnr++;
	$in_pod = 1 if /^=\w/;
	$in_pod = 0 if /^=cut/;
	next if ($in_pod || /^=cut/);
	# the following (tempting) old coding gives big troubles if a
	# cut is forgotten at EOF:
	# next if /^=\w/ .. /^=cut/;
	if (/^package\s+([\w:]+)\s*;/) {
	    $this_package = $def_package = $1;
	}

	if (/^sub\s+([\w:]+)(\s*(?:\(.*?\))?(?:$attr_list)?)/) {
	    print $out "# end of $last_package\::$subname\n1;\n"
		if $last_package;
	    $subname = $1;
	    my $proto = $2 || '';
	    if ($subname =~ s/(.*):://){
		$this_package = $1;
	    } else {
		$this_package = $def_package;
	    }
	    my $fq_subname = "$this_package\::$subname";
	    $package{$fq_subname} = $this_package;
	    $proto{$fq_subname} = $proto;
	    push(@subnames, $fq_subname);
	    my($lname, $sname) = ($subname, substr($subname,0,$maxflen-3));
	    $modpname = _modpname($this_package);
            my($modnamedir) = catdir($autodir, $modpname);
	    mkpath($modnamedir,0,0777);
	    my($lpath) = catfile($modnamedir, "$lname.al");
	    my($spath) = catfile($modnamedir, "$sname.al");
	    my $path;

	    if (!$Is83 and open($out, ">$lpath")){
	        $path=$lpath;
		print "  writing $lpath\n" if ($Verbose>=2);
	    } else {
		open($out, ">$spath") or die "Can't create $spath: $!\n";
		$path=$spath;
		print "  writing $spath (with truncated name)\n"
			if ($Verbose>=1);
	    }
	    push(@outfiles, $path);
	    my $lineno = $fnr - @cache;
	    print $out <<EOT;
# NOTE: Derived from $filename.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package $this_package;

#line $lineno "$filename (autosplit into $path)"
EOT
	    print $out @cache;
	    @cache = ();
	    $caching = 0;
	}
	if($caching) {
	    push(@cache, $_) if @cache || /\S/;
	} else {
	    print $out $_;
	}
	if(/^\}/) {
	    if($caching) {
		print $out @cache;
		@cache = ();
	    }
	    print $out "\n";
	    $caching = 1;
	}
	$last_package = $this_package if defined $this_package;
    }
    if ($subname) {
	print $out @cache,"1;\n# end of $last_package\::$subname\n";
	close($out);
    }
    close($in);
    
    if (!$keep){  # don't keep any obsolete *.al files in the directory
	my(%outfiles);
	# @outfiles{@outfiles} = @outfiles;
	# perl downcases all filenames on VMS (which upcases all filenames) so
	# we'd better downcase the sub name list too, or subs with upper case
	# letters in them will get their .al files deleted right after they're
	# created. (The mixed case sub name won't match the all-lowercase
	# filename, and so be cleaned up as a scrap file)
	if ($Is_VMS or $Is83) {
	    %outfiles = map {lc($_) => lc($_) } @outfiles;
	} else {
	    @outfiles{@outfiles} = @outfiles;
	}  
	my(%outdirs,@outdirs);
	for (@outfiles) {
	    $outdirs{File::Basename::dirname($_)}||=1;
	}
	for my $dir (keys %outdirs) {
	    opendir(my $outdir,$dir);
	    foreach (sort readdir($outdir)){
		next unless /\.al\z/;
		my($file) = catfile($dir, $_);
		$file = lc $file if $Is83 or $Is_VMS;
		next if $outfiles{$file};
		print "  deleting $file\n" if ($Verbose>=2);
		my($deleted,$thistime);  # catch all versions on VMS
		do { $deleted += ($thistime = unlink $file) } while ($thistime);
		carp ("Unable to delete $file: $!") unless $deleted;
	    }
	    closedir($outdir);
	}
    }

    open(my $ts,">$al_idx_file") or
	carp ("AutoSplit: unable to create timestamp file ($al_idx_file): $!");
    print $ts "# Index created by AutoSplit for $filename\n";
    print $ts "#    (file acts as timestamp)\n";
    $last_package = '';
    for my $fqs (@subnames) {
	my($subname) = $fqs;
	$subname =~ s/.*:://;
	print $ts "package $package{$fqs};\n"
	    unless $last_package eq $package{$fqs};
	print $ts "sub $subname $proto{$fqs};\n";
	$last_package = $package{$fqs};
    }
    print $ts "1;\n";
    close($ts);

    _check_unique($filename, $Maxlen, 1, @outfiles);

    @outfiles;
}

sub _modpname ($) {
    my($package) = @_;
    my $modpname = $package;
    if ($^O eq 'MSWin32') {
	$modpname =~ s#::#\\#g; 
    } else {
	my @modpnames = ();
	while ($modpname =~ m#(.*?[^:])::([^:].*)#) {
	       push @modpnames, $1;
	       $modpname = $2;
         }
	$modpname = catfile(@modpnames, $modpname);
    }
    if ($Is_VMS) {
        $modpname = VMS::Filespec::unixify($modpname); # may have dirs
    }
    $modpname;
}

sub _check_unique {
    my($filename, $maxlen, $warn, @outfiles) = @_;
    my(%notuniq) = ();
    my(%shorts)  = ();
    my(@toolong) = grep(
			length(File::Basename::basename($_))
			> $maxlen,
			@outfiles
		       );

    foreach (@toolong){
	my($dir) = File::Basename::dirname($_);
	my($file) = File::Basename::basename($_);
	my($trunc) = substr($file,0,$maxlen);
	$notuniq{$dir}{$trunc} = 1 if $shorts{$dir}{$trunc};
	$shorts{$dir}{$trunc} = $shorts{$dir}{$trunc} ?
	    "$shorts{$dir}{$trunc}, $file" : $file;
    }
    if (%notuniq && $warn){
	print "$filename: some names are not unique when " .
	    "truncated to $maxlen characters:\n";
	foreach my $dir (sort keys %notuniq){
	    print " directory $dir:\n";
	    foreach my $trunc (sort keys %{$notuniq{$dir}}) {
		print "  $shorts{$dir}{$trunc} truncate to $trunc\n";
	    }
	}
    }
}

1;
__END__

# test functions so AutoSplit.pm can be applied to itself:
sub test1 ($)   { "test 1\n"; }
sub test2 ($$)  { "test 2\n"; }
sub test3 ($$$) { "test 3\n"; }
sub testtesttesttest4_1  { "test 4\n"; }
sub testtesttesttest4_2  { "duplicate test 4\n"; }
sub Just::Another::test5 { "another test 5\n"; }
sub test6       { return join ":", __FILE__,__LINE__; }
package Yet::Another::AutoSplit;
sub testtesttesttest4_1 ($)  { "another test 4\n"; }
sub testtesttesttest4_2 ($$) { "another duplicate test 4\n"; }
package Yet::More::Attributes;
sub test_a1 ($) : locked :locked { 1; }
sub test_a2 : locked { 1; }
