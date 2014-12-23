package ExtUtils::Manifest;

require Exporter;
use Config;
use File::Basename;
use File::Copy 'copy';
use File::Find;
use File::Spec;
use Carp;
use strict;

use vars qw($VERSION @ISA @EXPORT_OK 
          $Is_MacOS $Is_VMS 
          $Debug $Verbose $Quiet $MANIFEST $DEFAULT_MSKIP);

$VERSION = '1.51_01';
@ISA=('Exporter');
@EXPORT_OK = qw(mkmanifest
                manicheck  filecheck  fullcheck  skipcheck
                manifind   maniread   manicopy   maniadd
               );

$Is_MacOS = $^O eq 'MacOS';
$Is_VMS   = $^O eq 'VMS';
require VMS::Filespec if $Is_VMS;

$Debug   = $ENV{PERL_MM_MANIFEST_DEBUG} || 0;
$Verbose = defined $ENV{PERL_MM_MANIFEST_VERBOSE} ?
                   $ENV{PERL_MM_MANIFEST_VERBOSE} : 1;
$Quiet = 0;
$MANIFEST = 'MANIFEST';

$DEFAULT_MSKIP = File::Spec->catfile( dirname(__FILE__), "$MANIFEST.SKIP" );


=head1 NAME

ExtUtils::Manifest - utilities to write and check a MANIFEST file

=head1 SYNOPSIS

    use ExtUtils::Manifest qw(...funcs to import...);

    mkmanifest();

    my @missing_files    = manicheck;
    my @skipped          = skipcheck;
    my @extra_files      = filecheck;
    my($missing, $extra) = fullcheck;

    my $found    = manifind();

    my $manifest = maniread();

    manicopy($read,$target);

    maniadd({$file => $comment, ...});


=head1 DESCRIPTION

=head2 Functions

ExtUtils::Manifest exports no functions by default.  The following are
exported on request

=over 4

=item mkmanifest

    mkmanifest();

Writes all files in and below the current directory to your F<MANIFEST>.
It works similar to

    find . > MANIFEST

All files that match any regular expression in a file F<MANIFEST.SKIP>
(if it exists) are ignored.

Any existing F<MANIFEST> file will be saved as F<MANIFEST.bak>.  Lines
from the old F<MANIFEST> file is preserved, including any comments
that are found in the existing F<MANIFEST> file in the new one.

=cut

sub _sort {
    return sort { lc $a cmp lc $b } @_;
}

sub mkmanifest {
    my $manimiss = 0;
    my $read = (-r 'MANIFEST' && maniread()) or $manimiss++;
    $read = {} if $manimiss;
    local *M;
    my $bakbase = $MANIFEST;
    $bakbase =~ s/\./_/g if $Is_VMS; # avoid double dots
    rename $MANIFEST, "$bakbase.bak" unless $manimiss;
    open M, ">$MANIFEST" or die "Could not open $MANIFEST: $!";
    my $skip = _maniskip();
    my $found = manifind();
    my($key,$val,$file,%all);
    %all = (%$found, %$read);
    $all{$MANIFEST} = ($Is_VMS ? "$MANIFEST\t\t" : '') . 'This list of files'
        if $manimiss; # add new MANIFEST to known file list
    foreach $file (_sort keys %all) {
	if ($skip->($file)) {
	    # Policy: only remove files if they're listed in MANIFEST.SKIP.
	    # Don't remove files just because they don't exist.
	    warn "Removed from $MANIFEST: $file\n" if $Verbose and exists $read->{$file};
	    next;
	}
	if ($Verbose){
	    warn "Added to $MANIFEST: $file\n" unless exists $read->{$file};
	}
	my $text = $all{$file};
	$file = _unmacify($file);
	my $tabs = (5 - (length($file)+1)/8);
	$tabs = 1 if $tabs < 1;
	$tabs = 0 unless $text;
	print M $file, "\t" x $tabs, $text, "\n";
    }
    close M;
}

# Geez, shouldn't this use File::Spec or File::Basename or something?  
# Why so careful about dependencies?
sub clean_up_filename {
  my $filename = shift;
  $filename =~ s|^\./||;
  $filename =~ s/^:([^:]+)$/$1/ if $Is_MacOS;
  return $filename;
}


=item manifind

    my $found = manifind();

returns a hash reference. The keys of the hash are the files found
below the current directory.

=cut

sub manifind {
    my $p = shift || {};
    my $found = {};

    my $wanted = sub {
	my $name = clean_up_filename($File::Find::name);
	warn "Debug: diskfile $name\n" if $Debug;
	return if -d $_;
	
        if( $Is_VMS ) {
            $name =~ s#(.*)\.$#\L$1#;
            $name = uc($name) if $name =~ /^MANIFEST(\.SKIP)?$/i;
        }
	$found->{$name} = "";
    };

    # We have to use "$File::Find::dir/$_" in preprocess, because 
    # $File::Find::name is unavailable.
    # Also, it's okay to use / here, because MANIFEST files use Unix-style 
    # paths.
    find({wanted => $wanted},
	 $Is_MacOS ? ":" : ".");

    return $found;
}


=item manicheck

    my @missing_files = manicheck();

checks if all the files within a C<MANIFEST> in the current directory
really do exist. If C<MANIFEST> and the tree below the current
directory are in sync it silently returns an empty list.
Otherwise it returns a list of files which are listed in the
C<MANIFEST> but missing from the directory, and by default also
outputs these names to STDERR.

=cut

sub manicheck {
    return _check_files();
}


=item filecheck

    my @extra_files = filecheck();

finds files below the current directory that are not mentioned in the
C<MANIFEST> file. An optional file C<MANIFEST.SKIP> will be
consulted. Any file matching a regular expression in such a file will
not be reported as missing in the C<MANIFEST> file. The list of any
extraneous files found is returned, and by default also reported to
STDERR.

=cut

sub filecheck {
    return _check_manifest();
}


=item fullcheck

    my($missing, $extra) = fullcheck();

does both a manicheck() and a filecheck(), returning then as two array
refs.

=cut

sub fullcheck {
    return [_check_files()], [_check_manifest()];
}


=item skipcheck

    my @skipped = skipcheck();

lists all the files that are skipped due to your C<MANIFEST.SKIP>
file.

=cut

sub skipcheck {
    my($p) = @_;
    my $found = manifind();
    my $matches = _maniskip();

    my @skipped = ();
    foreach my $file (_sort keys %$found){
        if (&$matches($file)){
            warn "Skipping $file\n";
            push @skipped, $file;
            next;
        }
    }

    return @skipped;
}


sub _check_files {
    my $p = shift;
    my $dosnames=(defined(&Dos::UseLFN) && Dos::UseLFN()==0);
    my $read = maniread() || {};
    my $found = manifind($p);

    my(@missfile) = ();
    foreach my $file (_sort keys %$read){
        warn "Debug: manicheck checking from $MANIFEST $file\n" if $Debug;
        if ($dosnames){
            $file = lc $file;
            $file =~ s=(\.(\w|-)+)=substr ($1,0,4)=ge;
            $file =~ s=((\w|-)+)=substr ($1,0,8)=ge;
        }
        unless ( exists $found->{$file} ) {
            warn "No such file: $file\n" unless $Quiet;
            push @missfile, $file;
        }
    }

    return @missfile;
}


sub _check_manifest {
    my($p) = @_;
    my $read = maniread() || {};
    my $found = manifind($p);
    my $skip  = _maniskip();

    my @missentry = ();
    foreach my $file (_sort keys %$found){
        next if $skip->($file);
        warn "Debug: manicheck checking from disk $file\n" if $Debug;
        unless ( exists $read->{$file} ) {
            my $canon = $Is_MacOS ? "\t" . _unmacify($file) : '';
            warn "Not in $MANIFEST: $file$canon\n" unless $Quiet;
            push @missentry, $file;
        }
    }

    return @missentry;
}


=item maniread

    my $manifest = maniread();
    my $manifest = maniread($manifest_file);

reads a named C<MANIFEST> file (defaults to C<MANIFEST> in the current
directory) and returns a HASH reference with files being the keys and
comments being the values of the HASH.  Blank lines and lines which
start with C<#> in the C<MANIFEST> file are discarded.

=cut

sub maniread {
    my ($mfile) = @_;
    $mfile ||= $MANIFEST;
    my $read = {};
    local *M;
    unless (open M, $mfile){
        warn "Problem opening $mfile: $!";
        return $read;
    }
    local $_;
    while (<M>){
        chomp;
        next if /^\s*#/;

        my($file, $comment) = /^(\S+)\s*(.*)/;
        next unless $file;

        if ($Is_MacOS) {
            $file = _macify($file);
            $file =~ s/\\([0-3][0-7][0-7])/sprintf("%c", oct($1))/ge;
        }
        elsif ($Is_VMS) {
            require File::Basename;
            my($base,$dir) = File::Basename::fileparse($file);
            # Resolve illegal file specifications in the same way as tar
            $dir =~ tr/./_/;
            my(@pieces) = split(/\./,$base);
            if (@pieces > 2) { $base = shift(@pieces) . '.' . join('_',@pieces); }
            my $okfile = "$dir$base";
            warn "Debug: Illegal name $file changed to $okfile\n" if $Debug;
            $file = $okfile;
            $file = lc($file) unless $file =~ /^MANIFEST(\.SKIP)?$/;
        }

        $read->{$file} = $comment;
    }
    close M;
    $read;
}

# returns an anonymous sub that decides if an argument matches
sub _maniskip {
    my @skip ;
    my $mfile = "$MANIFEST.SKIP";
    _check_mskip_directives($mfile) if -f $mfile;
    local(*M, $_);
    open M, $mfile or open M, $DEFAULT_MSKIP or return sub {0};
    while (<M>){
	chomp;
	s/\r//;
	next if /^#/;
	next if /^\s*$/;
	push @skip, _macify($_);
    }
    close M;
    return sub {0} unless (scalar @skip > 0);

    my $opts = $Is_VMS ? '(?i)' : '';

    # Make sure each entry is isolated in its own parentheses, in case
    # any of them contain alternations
    my $regex = join '|', map "(?:$_)", @skip;

    return sub { $_[0] =~ qr{$opts$regex} };
}

# checks for the special directives
#   #!include_default
#   #!include /path/to/some/manifest.skip
# in a custom MANIFEST.SKIP for, for including
# the content of, respectively, the default MANIFEST.SKIP
# and an external manifest.skip file
sub _check_mskip_directives {
    my $mfile = shift;
    local (*M, $_);
    my @lines = ();
    my $flag = 0;
    unless (open M, $mfile) {
        warn "Problem opening $mfile: $!";
        return;
    }
    while (<M>) {
        if (/^#!include_default\s*$/) {
	    if (my @default = _include_mskip_file()) {
	        push @lines, @default;
		warn "Debug: Including default MANIFEST.SKIP\n" if $Debug;
		$flag++;
	    }
	    next;
        }
	if (/^#!include\s+(.*)\s*$/) {
	    my $external_file = $1;
	    if (my @external = _include_mskip_file($external_file)) {
	        push @lines, @external;
		warn "Debug: Including external $external_file\n" if $Debug;
		$flag++;
	    }
            next;
        }
        push @lines, $_;
    }
    close M;
    return unless $flag;
    my $bakbase = $mfile;
    $bakbase =~ s/\./_/g if $Is_VMS;  # avoid double dots
    rename $mfile, "$bakbase.bak";
    warn "Debug: Saving original $mfile as $bakbase.bak\n" if $Debug;
    unless (open M, ">$mfile") {
        warn "Problem opening $mfile: $!";
        return;
    }
    print M $_ for (@lines);
    close M;
    return;
}

# returns an array containing the lines of an external
# manifest.skip file, if given, or $DEFAULT_MSKIP
sub _include_mskip_file {
    my $mskip = shift || $DEFAULT_MSKIP;
    unless (-f $mskip) {
        warn qq{Included file "$mskip" not found - skipping};
        return;
    }
    local (*M, $_);
    unless (open M, $mskip) {
        warn "Problem opening $mskip: $!";
        return;
    }
    my @lines = ();
    push @lines, "\n#!start included $mskip\n";
    push @lines, $_ while <M>;
    close M;
    push @lines, "#!end included $mskip\n\n";
    return @lines;
}

=item manicopy

    manicopy(\%src, $dest_dir);
    manicopy(\%src, $dest_dir, $how);

Copies the files that are the keys in %src to the $dest_dir.  %src is
typically returned by the maniread() function.

    manicopy( maniread(), $dest_dir );

This function is useful for producing a directory tree identical to the 
intended distribution tree. 

$how can be used to specify a different methods of "copying".  Valid
values are C<cp>, which actually copies the files, C<ln> which creates
hard links, and C<best> which mostly links the files but copies any
symbolic link to make a tree without any symbolic link.  C<cp> is the 
default.

=cut

sub manicopy {
    my($read,$target,$how)=@_;
    croak "manicopy() called without target argument" unless defined $target;
    $how ||= 'cp';
    require File::Path;
    require File::Basename;

    $target = VMS::Filespec::unixify($target) if $Is_VMS;
    File::Path::mkpath([ $target ],! $Quiet,$Is_VMS ? undef : 0755);
    foreach my $file (keys %$read){
    	if ($Is_MacOS) {
	    if ($file =~ m!:!) { 
	   	my $dir = _maccat($target, $file);
		$dir =~ s/[^:]+$//;
	    	File::Path::mkpath($dir,1,0755);
	    }
	    cp_if_diff($file, _maccat($target, $file), $how);
	} else {
	    $file = VMS::Filespec::unixify($file) if $Is_VMS;
	    if ($file =~ m!/!) { # Ilya, that hurts, I fear, or maybe not?
		my $dir = File::Basename::dirname($file);
		$dir = VMS::Filespec::unixify($dir) if $Is_VMS;
		File::Path::mkpath(["$target/$dir"],! $Quiet,$Is_VMS ? undef : 0755);
	    }
	    cp_if_diff($file, "$target/$file", $how);
	}
    }
}

sub cp_if_diff {
    my($from, $to, $how)=@_;
    -f $from or carp "$0: $from not found";
    my($diff) = 0;
    local(*F,*T);
    open(F,"< $from\0") or die "Can't read $from: $!\n";
    if (open(T,"< $to\0")) {
        local $_;
	while (<F>) { $diff++,last if $_ ne <T>; }
	$diff++ unless eof(T);
	close T;
    }
    else { $diff++; }
    close F;
    if ($diff) {
	if (-e $to) {
	    unlink($to) or confess "unlink $to: $!";
	}
        STRICT_SWITCH: {
	    best($from,$to), last STRICT_SWITCH if $how eq 'best';
	    cp($from,$to), last STRICT_SWITCH if $how eq 'cp';
	    ln($from,$to), last STRICT_SWITCH if $how eq 'ln';
	    croak("ExtUtils::Manifest::cp_if_diff " .
		  "called with illegal how argument [$how]. " .
		  "Legal values are 'best', 'cp', and 'ln'.");
	}
    }
}

sub cp {
    my ($srcFile, $dstFile) = @_;
    my ($access,$mod) = (stat $srcFile)[8,9];

    copy($srcFile,$dstFile);
    utime $access, $mod + ($Is_VMS ? 1 : 0), $dstFile;
    _manicopy_chmod($srcFile, $dstFile);
}


sub ln {
    my ($srcFile, $dstFile) = @_;
    return &cp if $Is_VMS or ($^O eq 'MSWin32' and Win32::IsWin95());
    link($srcFile, $dstFile);

    unless( _manicopy_chmod($srcFile, $dstFile) ) {
        unlink $dstFile;
        return;
    }
    1;
}

# 1) Strip off all group and world permissions.
# 2) Let everyone read it.
# 3) If the owner can execute it, everyone can.
sub _manicopy_chmod {
    my($srcFile, $dstFile) = @_;

    my $perm = 0444 | (stat $srcFile)[2] & 0700;
    chmod( $perm | ( $perm & 0100 ? 0111 : 0 ), $dstFile );
}

# Files that are often modified in the distdir.  Don't hard link them.
my @Exceptions = qw(MANIFEST META.yml SIGNATURE);
sub best {
    my ($srcFile, $dstFile) = @_;

    my $is_exception = grep $srcFile =~ /$_/, @Exceptions;
    if ($is_exception or !$Config{d_link} or -l $srcFile) {
	cp($srcFile, $dstFile);
    } else {
	ln($srcFile, $dstFile) or cp($srcFile, $dstFile);
    }
}

sub _macify {
    my($file) = @_;

    return $file unless $Is_MacOS;

    $file =~ s|^\./||;
    if ($file =~ m|/|) {
	$file =~ s|/+|:|g;
	$file = ":$file";
    }

    $file;
}

sub _maccat {
    my($f1, $f2) = @_;

    return "$f1/$f2" unless $Is_MacOS;

    $f1 .= ":$f2";
    $f1 =~ s/([^:]:):/$1/g;
    return $f1;
}

sub _unmacify {
    my($file) = @_;

    return $file unless $Is_MacOS;

    $file =~ s|^:||;
    $file =~ s|([/ \n])|sprintf("\\%03o", unpack("c", $1))|ge;
    $file =~ y|:|/|;

    $file;
}


=item maniadd

  maniadd({ $file => $comment, ...});

Adds an entry to an existing F<MANIFEST> unless its already there.

$file will be normalized (ie. Unixified).  B<UNIMPLEMENTED>

=cut

sub maniadd {
    my($additions) = shift;

    _normalize($additions);
    _fix_manifest($MANIFEST);

    my $manifest = maniread();
    my @needed = grep { !exists $manifest->{$_} } keys %$additions;
    return 1 unless @needed;

    open(MANIFEST, ">>$MANIFEST") or 
      die "maniadd() could not open $MANIFEST: $!";

    foreach my $file (_sort @needed) {
        my $comment = $additions->{$file} || '';
        printf MANIFEST "%-40s %s\n", $file, $comment;
    }
    close MANIFEST or die "Error closing $MANIFEST: $!";

    return 1;
}


# Sometimes MANIFESTs are missing a trailing newline.  Fix this.
sub _fix_manifest {
    my $manifest_file = shift;

    open MANIFEST, $MANIFEST or die "Could not open $MANIFEST: $!";

    # Yes, we should be using seek(), but I'd like to avoid loading POSIX
    # to get SEEK_*
    my @manifest = <MANIFEST>;
    close MANIFEST;

    unless( $manifest[-1] =~ /\n\z/ ) {
        open MANIFEST, ">>$MANIFEST" or die "Could not open $MANIFEST: $!";
        print MANIFEST "\n";
        close MANIFEST;
    }
}


# UNIMPLEMENTED
sub _normalize {
    return;
}


=back

=head2 MANIFEST

A list of files in the distribution, one file per line.  The MANIFEST
always uses Unix filepath conventions even if you're not on Unix.  This
means F<foo/bar> style not F<foo\bar>.

Anything between white space and an end of line within a C<MANIFEST>
file is considered to be a comment.  Any line beginning with # is also
a comment.

    # this a comment
    some/file
    some/other/file            comment about some/file


=head2 MANIFEST.SKIP

The file MANIFEST.SKIP may contain regular expressions of files that
should be ignored by mkmanifest() and filecheck(). The regular
expressions should appear one on each line. Blank lines and lines
which start with C<#> are skipped.  Use C<\#> if you need a regular
expression to start with a C<#>.

For example:

    # Version control files and dirs.
    \bRCS\b
    \bCVS\b
    ,v$
    \B\.svn\b

    # Makemaker generated files and dirs.
    ^MANIFEST\.
    ^Makefile$
    ^blib/
    ^MakeMaker-\d

    # Temp, old and emacs backup files.
    ~$
    \.old$
    ^#.*#$
    ^\.#

If no MANIFEST.SKIP file is found, a default set of skips will be
used, similar to the example above.  If you want nothing skipped,
simply make an empty MANIFEST.SKIP file.

In one's own MANIFEST.SKIP file, certain directives
can be used to include the contents of other MANIFEST.SKIP
files. At present two such directives are recognized.

=over 4

=item #!include_default

This inserts the contents of the default MANIFEST.SKIP file

=item #!include /Path/to/another/manifest.skip

This inserts the contents of the specified external file

=back

The included contents will be inserted into the MANIFEST.SKIP
file in between I<#!start included /path/to/manifest.skip>
and I<#!end included /path/to/manifest.skip> markers.
The original MANIFEST.SKIP is saved as MANIFEST.SKIP.bak.

=head2 EXPORT_OK

C<&mkmanifest>, C<&manicheck>, C<&filecheck>, C<&fullcheck>,
C<&maniread>, and C<&manicopy> are exportable.

=head2 GLOBAL VARIABLES

C<$ExtUtils::Manifest::MANIFEST> defaults to C<MANIFEST>. Changing it
results in both a different C<MANIFEST> and a different
C<MANIFEST.SKIP> file. This is useful if you want to maintain
different distributions for different audiences (say a user version
and a developer version including RCS).

C<$ExtUtils::Manifest::Quiet> defaults to 0. If set to a true value,
all functions act silently.

C<$ExtUtils::Manifest::Debug> defaults to 0.  If set to a true value,
or if PERL_MM_MANIFEST_DEBUG is true, debugging output will be
produced.

=head1 DIAGNOSTICS

All diagnostic output is sent to C<STDERR>.

=over 4

=item C<Not in MANIFEST:> I<file>

is reported if a file is found which is not in C<MANIFEST>.

=item C<Skipping> I<file>

is reported if a file is skipped due to an entry in C<MANIFEST.SKIP>.

=item C<No such file:> I<file>

is reported if a file mentioned in a C<MANIFEST> file does not
exist.

=item C<MANIFEST:> I<$!>

is reported if C<MANIFEST> could not be opened.

=item C<Added to MANIFEST:> I<file>

is reported by mkmanifest() if $Verbose is set and a file is added
to MANIFEST. $Verbose is set to 1 by default.

=back

=head1 ENVIRONMENT

=over 4

=item B<PERL_MM_MANIFEST_DEBUG>

Turns on debugging

=back

=head1 SEE ALSO

L<ExtUtils::MakeMaker> which has handy targets for most of the functionality.

=head1 AUTHOR

Andreas Koenig C<andreas.koenig@anima.de>

Maintained by Michael G Schwern C<schwern@pobox.com> within the
ExtUtils-MakeMaker package and, as a separate CPAN package, by
Randy Kobes C<r.kobes@uwinnipeg.ca>.

=cut

1;
