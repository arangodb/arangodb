package ExtUtils::Liblist::Kid;

# XXX Splitting this out into its own .pm is a temporary solution.

# This kid package is to be used by MakeMaker.  It will not work if
# $self is not a Makemaker.

use 5.006;
# Broken out of MakeMaker from version 4.11

use strict;
our $VERSION = 6.44;

use Config;
use Cwd 'cwd';
use File::Basename;
use File::Spec;

sub ext {
  if   ($^O eq 'VMS')     { return &_vms_ext;      }
  elsif($^O eq 'MSWin32') { return &_win32_ext;    }
  else                    { return &_unix_os2_ext; }
}

sub _unix_os2_ext {
    my($self,$potential_libs, $verbose, $give_libs) = @_;
    $verbose ||= 0;

    if ($^O =~ 'os2' and $Config{perllibs}) { 
	# Dynamic libraries are not transitive, so we may need including
	# the libraries linked against perl.dll again.

	$potential_libs .= " " if $potential_libs;
	$potential_libs .= $Config{perllibs};
    }
    return ("", "", "", "", ($give_libs ? [] : ())) unless $potential_libs;
    warn "Potential libraries are '$potential_libs':\n" if $verbose;

    my($so)   = $Config{so};
    my($libs) = defined $Config{perllibs} ? $Config{perllibs} : $Config{libs};
    my $Config_libext = $Config{lib_ext} || ".a";


    # compute $extralibs, $bsloadlibs and $ldloadlibs from
    # $potential_libs
    # this is a rewrite of Andy Dougherty's extliblist in perl

    my(@searchpath); # from "-L/path" entries in $potential_libs
    my(@libpath) = split " ", $Config{'libpth'};
    my(@ldloadlibs, @bsloadlibs, @extralibs, @ld_run_path, %ld_run_path_seen);
    my(@libs, %libs_seen);
    my($fullname, @fullname);
    my($pwd) = cwd(); # from Cwd.pm
    my($found) = 0;

    foreach my $thislib (split ' ', $potential_libs) {

	# Handle possible linker path arguments.
	if ($thislib =~ s/^(-[LR]|-Wl,-R)//){	# save path flag type
	    my($ptype) = $1;
	    unless (-d $thislib){
		warn "$ptype$thislib ignored, directory does not exist\n"
			if $verbose;
		next;
	    }
	    my($rtype) = $ptype;
	    if (($ptype eq '-R') or ($ptype eq '-Wl,-R')) {
		if ($Config{'lddlflags'} =~ /-Wl,-R/) {
		    $rtype = '-Wl,-R';
		} elsif ($Config{'lddlflags'} =~ /-R/) {
		    $rtype = '-R';
		}
	    }
	    unless (File::Spec->file_name_is_absolute($thislib)) {
	      warn "Warning: $ptype$thislib changed to $ptype$pwd/$thislib\n";
	      $thislib = $self->catdir($pwd,$thislib);
	    }
	    push(@searchpath, $thislib);
	    push(@extralibs,  "$ptype$thislib");
	    push(@ldloadlibs, "$rtype$thislib");
	    next;
	}

	# Handle possible library arguments.
	unless ($thislib =~ s/^-l//){
	  warn "Unrecognized argument in LIBS ignored: '$thislib'\n";
	  next;
	}

	my($found_lib)=0;
	foreach my $thispth (@searchpath, @libpath) {

		# Try to find the full name of the library.  We need this to
		# determine whether it's a dynamically-loadable library or not.
		# This tends to be subject to various os-specific quirks.
		# For gcc-2.6.2 on linux (March 1995), DLD can not load
		# .sa libraries, with the exception of libm.sa, so we
		# deliberately skip them.
	    if (@fullname =
		    $self->lsdir($thispth,"^\Qlib$thislib.$so.\E[0-9]+")){
		# Take care that libfoo.so.10 wins against libfoo.so.9.
		# Compare two libraries to find the most recent version
		# number.  E.g.  if you have libfoo.so.9.0.7 and
		# libfoo.so.10.1, first convert all digits into two
		# decimal places.  Then we'll add ".00" to the shorter
		# strings so that we're comparing strings of equal length
		# Thus we'll compare libfoo.so.09.07.00 with
		# libfoo.so.10.01.00.  Some libraries might have letters
		# in the version.  We don't know what they mean, but will
		# try to skip them gracefully -- we'll set any letter to
		# '0'.  Finally, sort in reverse so we can take the
		# first element.

		#TODO: iterate through the directory instead of sorting

		$fullname = "$thispth/" .
		(sort { my($ma) = $a;
			my($mb) = $b;
			$ma =~ tr/A-Za-z/0/s;
			$ma =~ s/\b(\d)\b/0$1/g;
			$mb =~ tr/A-Za-z/0/s;
			$mb =~ s/\b(\d)\b/0$1/g;
			while (length($ma) < length($mb)) { $ma .= ".00"; }
			while (length($mb) < length($ma)) { $mb .= ".00"; }
			# Comparison deliberately backwards
			$mb cmp $ma;} @fullname)[0];
	    } elsif (-f ($fullname="$thispth/lib$thislib.$so")
		 && (($Config{'dlsrc'} ne "dl_dld.xs") || ($thislib eq "m"))){
	    } elsif (-f ($fullname="$thispth/lib${thislib}_s$Config_libext")
                 && (! $Config{'archname'} =~ /RM\d\d\d-svr4/)
		 && ($thislib .= "_s") ){ # we must explicitly use _s version
	    } elsif (-f ($fullname="$thispth/lib$thislib$Config_libext")){
	    } elsif (-f ($fullname="$thispth/$thislib$Config_libext")){
            } elsif (-f ($fullname="$thispth/lib$thislib.dll$Config_libext")){
            } elsif (-f ($fullname="$thispth/$thislib.dll")){
	    } elsif (-f ($fullname="$thispth/Slib$thislib$Config_libext")){
	    } elsif ($^O eq 'dgux'
		 && -l ($fullname="$thispth/lib$thislib$Config_libext")
		 && readlink($fullname) =~ /^elink:/s) {
		 # Some of DG's libraries look like misconnected symbolic
		 # links, but development tools can follow them.  (They
		 # look like this:
		 #
		 #    libm.a -> elink:${SDE_PATH:-/usr}/sde/\
		 #    ${TARGET_BINARY_INTERFACE:-m88kdgux}/usr/lib/libm.a
		 #
		 # , the compilation tools expand the environment variables.)
	    } else {
		warn "$thislib not found in $thispth\n" if $verbose;
		next;
	    }
	    warn "'-l$thislib' found at $fullname\n" if $verbose;
	    push @libs, $fullname unless $libs_seen{$fullname}++;
	    $found++;
	    $found_lib++;

	    # Now update library lists

	    # what do we know about this library...
	    my $is_dyna = ($fullname !~ /\Q$Config_libext\E\z/);
	    my $in_perl = ($libs =~ /\B-l\Q${thislib}\E\b/s);

            # include the path to the lib once in the dynamic linker path
            # but only if it is a dynamic lib and not in Perl itself
            my($fullnamedir) = dirname($fullname);
            push @ld_run_path, $fullnamedir
                 if $is_dyna && !$in_perl &&
                    !$ld_run_path_seen{$fullnamedir}++;

	    # Do not add it into the list if it is already linked in
	    # with the main perl executable.
	    # We have to special-case the NeXT, because math and ndbm 
	    # are both in libsys_s
	    unless ($in_perl || 
		($Config{'osname'} eq 'next' &&
		    ($thislib eq 'm' || $thislib eq 'ndbm')) ){
		push(@extralibs, "-l$thislib");
	    }

	    # We might be able to load this archive file dynamically
	    if ( ($Config{'dlsrc'} =~ /dl_next/ && $Config{'osvers'} lt '4_0')
	    ||   ($Config{'dlsrc'} =~ /dl_dld/) )
	    {
		# We push -l$thislib instead of $fullname because
		# it avoids hardwiring a fixed path into the .bs file.
		# Mkbootstrap will automatically add dl_findfile() to
		# the .bs file if it sees a name in the -l format.
		# USE THIS, when dl_findfile() is fixed: 
		# push(@bsloadlibs, "-l$thislib");
		# OLD USE WAS while checking results against old_extliblist
		push(@bsloadlibs, "$fullname");
	    } else {
		if ($is_dyna){
                    # For SunOS4, do not add in this shared library if
                    # it is already linked in the main perl executable
		    push(@ldloadlibs, "-l$thislib")
			unless ($in_perl and $^O eq 'sunos');
		} else {
		    push(@ldloadlibs, "-l$thislib");
		}
	    }
	    last;	# found one here so don't bother looking further
	}
	warn "Note (probably harmless): "
		     ."No library found for -l$thislib\n"
	    unless $found_lib>0;
    }

    unless( $found ) {
        return ('','','','', ($give_libs ? \@libs : ()));
    }
    else {
        return ("@extralibs", "@bsloadlibs", "@ldloadlibs",
                join(":",@ld_run_path), ($give_libs ? \@libs : ()));
    }
}

sub _win32_ext {

    require Text::ParseWords;

    my($self, $potential_libs, $verbose, $give_libs) = @_;
    $verbose ||= 0;

    # If user did not supply a list, we punt.
    # (caller should probably use the list in $Config{libs})
    return ("", "", "", "", ($give_libs ? [] : ())) unless $potential_libs;

    my $cc		= $Config{cc};
    my $VC		= $cc =~ /^cl/i;
    my $BC		= $cc =~ /^bcc/i;
    my $GC		= $cc =~ /^gcc/i;
    my $so		= $Config{'so'};
    my $libs		= $Config{'perllibs'};
    my $libpth		= $Config{'libpth'};
    my $libext		= $Config{'lib_ext'} || ".lib";
    my(@libs, %libs_seen);

    if ($libs and $potential_libs !~ /:nodefault/i) { 
	# If Config.pm defines a set of default libs, we always
	# tack them on to the user-supplied list, unless the user
	# specified :nodefault

	$potential_libs .= " " if $potential_libs;
	$potential_libs .= $libs;
    }
    warn "Potential libraries are '$potential_libs':\n" if $verbose;

    # normalize to forward slashes
    $libpth =~ s,\\,/,g;
    $potential_libs =~ s,\\,/,g;

    # compute $extralibs from $potential_libs

    my @searchpath;		    # from "-L/path" in $potential_libs
    my @libpath		= Text::ParseWords::quotewords('\s+', 0, $libpth);
    my @extralibs;
    my $pwd		= cwd();    # from Cwd.pm
    my $lib		= '';
    my $found		= 0;
    my $search		= 1;
    my($fullname);

    # add "$Config{installarchlib}/CORE" to default search path
    push @libpath, "$Config{installarchlib}/CORE";

    if ($VC and exists $ENV{LIB} and $ENV{LIB}) {
        push @libpath, split /;/, $ENV{LIB};
    }

    foreach (Text::ParseWords::quotewords('\s+', 0, $potential_libs)){

	my $thislib = $_;

        # see if entry is a flag
	if (/^:\w+$/) {
	    $search	= 0 if lc eq ':nosearch';
	    $search	= 1 if lc eq ':search';
	    warn "Ignoring unknown flag '$thislib'\n"
		if $verbose and !/^:(no)?(search|default)$/i;
	    next;
	}

	# if searching is disabled, do compiler-specific translations
	unless ($search) {
	    s/^-l(.+)$/$1.lib/ unless $GC;
	    s/^-L/-libpath:/ if $VC;
	    push(@extralibs, $_);
	    $found++;
	    next;
	}

	# handle possible linker path arguments
	if (s/^-L// and not -d) {
	    warn "$thislib ignored, directory does not exist\n"
		if $verbose;
	    next;
	}
	elsif (-d) {
	    unless (File::Spec->file_name_is_absolute($_)) {
	      warn "Warning: '$thislib' changed to '-L$pwd/$_'\n";
	      $_ = $self->catdir($pwd,$_);
	    }
	    push(@searchpath, $_);
	    next;
	}

	# handle possible library arguments
	if (s/^-l// and $GC and !/^lib/i) {
	    $_ = "lib$_";
	}
	$_ .= $libext if !/\Q$libext\E$/i;

	my $secondpass = 0;
    LOOKAGAIN:

        # look for the file itself
	if (-f) {
	    warn "'$thislib' found as '$_'\n" if $verbose;
	    $found++;
	    push(@extralibs, $_);
	    next;
	}

	my $found_lib = 0;
	foreach my $thispth (@searchpath, @libpath){
	    unless (-f ($fullname="$thispth\\$_")) {
		warn "'$thislib' not found as '$fullname'\n" if $verbose;
		next;
	    }
	    warn "'$thislib' found as '$fullname'\n" if $verbose;
	    $found++;
	    $found_lib++;
	    push(@extralibs, $fullname);
	    push @libs, $fullname unless $libs_seen{$fullname}++;
	    last;
	}

	# do another pass with (or without) leading 'lib' if they used -l
	if (!$found_lib and $thislib =~ /^-l/ and !$secondpass++) {
	    if ($GC) {
		goto LOOKAGAIN if s/^lib//i;
	    }
	    elsif (!/^lib/i) {
		$_ = "lib$_";
		goto LOOKAGAIN;
	    }
	}

	# give up
	warn "Note (probably harmless): "
		     ."No library found for $thislib\n"
	    unless $found_lib>0;

    }

    return ('','','','', ($give_libs ? \@libs : ())) unless $found;

    # make sure paths with spaces are properly quoted
    @extralibs = map { (/\s/ && !/^".*"$/) ? qq["$_"] : $_ } @extralibs;
    @libs = map { (/\s/ && !/^".*"$/) ? qq["$_"] : $_ } @libs;
    $lib = join(' ',@extralibs);

    # normalize back to backward slashes (to help braindead tools)
    # XXX this may break equally braindead GNU tools that don't understand
    # backslashes, either.  Seems like one can't win here.  Cursed be CP/M.
    $lib =~ s,/,\\,g;

    warn "Result: $lib\n" if $verbose;
    wantarray ? ($lib, '', $lib, '', ($give_libs ? \@libs : ())) : $lib;
}


sub _vms_ext {
  my($self, $potential_libs, $verbose, $give_libs) = @_;
  $verbose ||= 0;

  my(@crtls,$crtlstr);
  @crtls = ( ($Config{'ldflags'} =~ m-/Debug-i ? $Config{'dbgprefix'} : '')
              . 'PerlShr/Share' );
  push(@crtls, grep { not /\(/ } split /\s+/, $Config{'perllibs'});
  push(@crtls, grep { not /\(/ } split /\s+/, $Config{'libc'});
  # In general, we pass through the basic libraries from %Config unchanged.
  # The one exception is that if we're building in the Perl source tree, and
  # a library spec could be resolved via a logical name, we go to some trouble
  # to insure that the copy in the local tree is used, rather than one to
  # which a system-wide logical may point.
  if ($self->{PERL_SRC}) {
    my($locspec,$type);
    foreach my $lib (@crtls) { 
      if (($locspec,$type) = $lib =~ m{^([\w\$-]+)(/\w+)?} and $locspec =~ /perl/i) {
        if    (lc $type eq '/share')   { $locspec .= $Config{'exe_ext'}; }
        elsif (lc $type eq '/library') { $locspec .= $Config{'lib_ext'}; }
        else                           { $locspec .= $Config{'obj_ext'}; }
        $locspec = $self->catfile($self->{PERL_SRC},$locspec);
        $lib = "$locspec$type" if -e $locspec;
      }
    }
  }
  $crtlstr = @crtls ? join(' ',@crtls) : '';

  unless ($potential_libs) {
    warn "Result:\n\tEXTRALIBS: \n\tLDLOADLIBS: $crtlstr\n" if $verbose;
    return ('', '', $crtlstr, '', ($give_libs ? [] : ()));
  }

  my(%found,@fndlibs,$ldlib);
  my $cwd = cwd();
  my($so,$lib_ext,$obj_ext) = @Config{'so','lib_ext','obj_ext'};
  # List of common Unix library names and their VMS equivalents
  # (VMS equivalent of '' indicates that the library is automatically
  # searched by the linker, and should be skipped here.)
  my(@flibs, %libs_seen);
  my %libmap = ( 'm' => '', 'f77' => '', 'F77' => '', 'V77' => '', 'c' => '',
                 'malloc' => '', 'crypt' => '', 'resolv' => '', 'c_s' => '',
                 'socket' => '', 'X11' => 'DECW$XLIBSHR',
                 'Xt' => 'DECW$XTSHR', 'Xm' => 'DECW$XMLIBSHR',
                 'Xmu' => 'DECW$XMULIBSHR');
  if ($Config{'vms_cc_type'} ne 'decc') { $libmap{'curses'} = 'VAXCCURSE'; }

  warn "Potential libraries are '$potential_libs'\n" if $verbose;

  # First, sort out directories and library names in the input
  my(@dirs, @libs);
  foreach my $lib (split ' ',$potential_libs) {
    push(@dirs,$1),   next if $lib =~ /^-L(.*)/;
    push(@dirs,$lib), next if $lib =~ /[:>\]]$/;
    push(@dirs,$lib), next if -d $lib;
    push(@libs,$1),   next if $lib =~ /^-l(.*)/;
    push(@libs,$lib);
  }
  push(@dirs,split(' ',$Config{'libpth'}));

  # Now make sure we've got VMS-syntax absolute directory specs
  # (We don't, however, check whether someone's hidden a relative
  # path in a logical name.)
  foreach my $dir (@dirs) {
    unless (-d $dir) {
      warn "Skipping nonexistent Directory $dir\n" if $verbose > 1;
      $dir = '';
      next;
    }
    warn "Resolving directory $dir\n" if $verbose;
    if (File::Spec->file_name_is_absolute($dir)) { 
        $dir = $self->fixpath($dir,1); 
    }
    else { 
        $dir = $self->catdir($cwd,$dir); 
    }
  }
  @dirs = grep { length($_) } @dirs;
  unshift(@dirs,''); # Check each $lib without additions first

  LIB: foreach my $lib (@libs) {
    if (exists $libmap{$lib}) {
      next unless length $libmap{$lib};
      $lib = $libmap{$lib};
    }

    my(@variants,$cand);
    my($ctype) = '';

    # If we don't have a file type, consider it a possibly abbreviated name and
    # check for common variants.  We try these first to grab libraries before
    # a like-named executable image (e.g. -lperl resolves to perlshr.exe
    # before perl.exe).
    if ($lib !~ /\.[^:>\]]*$/) {
      push(@variants,"${lib}shr","${lib}rtl","${lib}lib");
      push(@variants,"lib$lib") if $lib !~ /[:>\]]/;
    }
    push(@variants,$lib);
    warn "Looking for $lib\n" if $verbose;
    foreach my $variant (@variants) {
      my($fullname, $name);

      foreach my $dir (@dirs) {
        my($type);

        $name = "$dir$variant";
        warn "\tChecking $name\n" if $verbose > 2;
        $fullname = VMS::Filespec::rmsexpand($name);
        if (defined $fullname and -f $fullname) {
          # It's got its own suffix, so we'll have to figure out the type
          if    ($fullname =~ /(?:$so|exe)$/i)      { $type = 'SHR'; }
          elsif ($fullname =~ /(?:$lib_ext|olb)$/i) { $type = 'OLB'; }
          elsif ($fullname =~ /(?:$obj_ext|obj)$/i) {
            warn "Note (probably harmless): "
                ."Plain object file $fullname found in library list\n";
            $type = 'OBJ';
          }
          else {
            warn "Note (probably harmless): "
                ."Unknown library type for $fullname; assuming shared\n";
            $type = 'SHR';
          }
        }
        elsif (-f ($fullname = VMS::Filespec::rmsexpand($name,$so))      or
               -f ($fullname = VMS::Filespec::rmsexpand($name,'.exe')))     {
          $type = 'SHR';
          $name = $fullname unless $fullname =~ /exe;?\d*$/i;
        }
        elsif (not length($ctype) and  # If we've got a lib already, 
                                       # don't bother
               ( -f ($fullname = VMS::Filespec::rmsexpand($name,$lib_ext)) or
                 -f ($fullname = VMS::Filespec::rmsexpand($name,'.olb'))))  {
          $type = 'OLB';
          $name = $fullname unless $fullname =~ /olb;?\d*$/i;
        }
        elsif (not length($ctype) and  # If we've got a lib already, 
                                       # don't bother
               ( -f ($fullname = VMS::Filespec::rmsexpand($name,$obj_ext)) or
                 -f ($fullname = VMS::Filespec::rmsexpand($name,'.obj'))))  {
          warn "Note (probably harmless): "
		       ."Plain object file $fullname found in library list\n";
          $type = 'OBJ';
          $name = $fullname unless $fullname =~ /obj;?\d*$/i;
        }
        if (defined $type) {
          $ctype = $type; $cand = $name;
          last if $ctype eq 'SHR';
        }
      }
      if ($ctype) { 
        # This has to precede any other CRTLs, so just make it first
        if ($cand eq 'VAXCCURSE') { unshift @{$found{$ctype}}, $cand; }  
        else                      { push    @{$found{$ctype}}, $cand; }
        warn "\tFound as $cand (really $fullname), type $ctype\n" 
          if $verbose > 1;
	push @flibs, $name unless $libs_seen{$fullname}++;
        next LIB;
      }
    }
    warn "Note (probably harmless): "
		 ."No library found for $lib\n";
  }

  push @fndlibs, @{$found{OBJ}}                      if exists $found{OBJ};
  push @fndlibs, map { "$_/Library" } @{$found{OLB}} if exists $found{OLB};
  push @fndlibs, map { "$_/Share"   } @{$found{SHR}} if exists $found{SHR};
  my $lib = join(' ',@fndlibs);

  $ldlib = $crtlstr ? "$lib $crtlstr" : $lib;
  warn "Result:\n\tEXTRALIBS: $lib\n\tLDLOADLIBS: $ldlib\n" if $verbose;
  wantarray ? ($lib, '', $ldlib, '', ($give_libs ? \@flibs : ())) : $lib;
}

1;
