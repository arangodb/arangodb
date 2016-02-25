package ExtUtils::CBuilder::Platform::VMS;

use strict;
use ExtUtils::CBuilder::Base;

use vars qw($VERSION @ISA);
$VERSION = '0.22';
@ISA = qw(ExtUtils::CBuilder::Base);

use File::Spec::Functions qw(catfile catdir);

# We do prelink, but don't want the parent to redo it.

sub need_prelink { 0 }

sub arg_defines {
  my ($self, %args) = @_;

  s/"/""/g foreach values %args;

  my @config_defines;

  # VMS can only have one define qualifier; add the one from config, if any.
  if ($self->{config}{ccflags} =~ s{/  def[^=]+  =+  \(?  ([^\/\)]*)  } {}ix) {
    push @config_defines, $1;
  }

  return '' unless keys(%args) || @config_defines;

  return ('/define=(' 
          . join(',', 
		 @config_defines,
                 map "\"$_" . ( length($args{$_}) ? "=$args{$_}" : '') . "\"", 
                     keys %args) 
          . ')');
}

sub arg_include_dirs {
  my ($self, @dirs) = @_;

  # VMS can only have one include list, add the one from config.
  if ($self->{config}{ccflags} =~ s{/inc[^=]+(?:=)+(?:\()?([^\/\)]*)} {}i) {
    unshift @dirs, $1;
  }
  return unless @dirs;

  return ('/include=(' . join(',', @dirs) . ')');
}

sub _do_link {
  my ($self, $type, %args) = @_;
  
  my $objects = delete $args{objects};
  $objects = [$objects] unless ref $objects;
  
  if ($args{lddl}) {

    # prelink will call Mksymlists, which creates the extension-specific
    # linker options file and populates it with the boot symbol.

    my @temp_files = $self->prelink(%args, dl_name => $args{module_name});

    # We now add the rest of what we need to the linker options file.  We
    # should replicate the functionality of C<ExtUtils::MM_VMS::dlsyms>,
    # but there is as yet no infrastructure for handling object libraries,
    # so for now we depend on object files being listed individually on the 
    # command line, which should work for simple cases.  We do bring in our
    # own version of C<ExtUtils::Liblist::Kid::ext> so that any additional
    # libraries (including PERLSHR) can be added to the options file.

    my @optlibs = $self->_liblist_ext( $args{'libs'} );

    my $optfile = 'sys$disk:[]' . $temp_files[0];
    open my $opt_fh, '>>', $optfile 
        or die "_do_link: Unable to open $optfile: $!";
    for my $lib (@optlibs) {print $opt_fh "$lib\n" if length $lib }
    close $opt_fh;

    $objects->[-1] .= ',';
    push @$objects, $optfile . '/OPTIONS,';

    # This one not needed for DEC C, but leave for completeness.
    push @$objects, $self->perl_inc() . 'perlshr_attr.opt/OPTIONS';
  }

  return $self->SUPER::_do_link($type, %args, objects => $objects);
}

sub arg_nolink { return; }

sub arg_object_file {
  my ($self, $file) = @_;
  return "/obj=$file";
}

sub arg_exec_file {
  my ($self, $file) = @_;
  return ("/exe=$file");
}

sub arg_share_object_file {
  my ($self, $file) = @_;
  return ("$self->{config}{lddlflags}=$file");
}


sub lib_file {
  my ($self, $dl_file) = @_;
  $dl_file =~ s/\.[^.]+$//;
  $dl_file =~ tr/"//d;
  $dl_file = $dl_file .= '.' . $self->{config}{dlext};

  # Need to create with the same name as DynaLoader will load with.
  if (defined &DynaLoader::mod2fname) {
    my ($dev,$dir,$file) = File::Spec->splitpath($dl_file);
    $file = DynaLoader::mod2fname([$file]);
    $dl_file = File::Spec->catpath($dev,$dir,$file);
  }
  return $dl_file;
}

# The following is reproduced almost verbatim from ExtUtils::Liblist::Kid::_vms_ext.
# We can't just call that because it's tied up with the MakeMaker object hierarchy.

sub _liblist_ext {
  my($self, $potential_libs,$verbose,$give_libs) = @_;
  $verbose ||= 0;

  my(@crtls,$crtlstr);
  @crtls = ( ($self->{'config'}{'ldflags'} =~ m-/Debug-i ? $self->{'config'}{'dbgprefix'} : '')
              . 'PerlShr/Share' );
  push(@crtls, grep { not /\(/ } split /\s+/, $self->{'config'}{'perllibs'});
  push(@crtls, grep { not /\(/ } split /\s+/, $self->{'config'}{'libc'});
  # In general, we pass through the basic libraries from %Config unchanged.
  # The one exception is that if we're building in the Perl source tree, and
  # a library spec could be resolved via a logical name, we go to some trouble
  # to insure that the copy in the local tree is used, rather than one to
  # which a system-wide logical may point.
  if ($self->perl_src) {
    my($lib,$locspec,$type);
    foreach $lib (@crtls) { 
      if (($locspec,$type) = $lib =~ m{^([\w\$-]+)(/\w+)?} and $locspec =~ /perl/i) {
        if    (lc $type eq '/share')   { $locspec .= $self->{'config'}{'exe_ext'}; }
        elsif (lc $type eq '/library') { $locspec .= $self->{'config'}{'lib_ext'}; }
        else                           { $locspec .= $self->{'config'}{'obj_ext'}; }
        $locspec = catfile($self->perl_src, $locspec);
        $lib = "$locspec$type" if -e $locspec;
      }
    }
  }
  $crtlstr = @crtls ? join(' ',@crtls) : '';

  unless ($potential_libs) {
    warn "Result:\n\tEXTRALIBS: \n\tLDLOADLIBS: $crtlstr\n" if $verbose;
    return ('', '', $crtlstr, '', ($give_libs ? [] : ()));
  }

  my(@dirs,@libs,$dir,$lib,%found,@fndlibs,$ldlib);
  my $cwd = cwd();
  my($so,$lib_ext,$obj_ext) = @{$self->{'config'}}{'so','lib_ext','obj_ext'};
  # List of common Unix library names and their VMS equivalents
  # (VMS equivalent of '' indicates that the library is automatically
  # searched by the linker, and should be skipped here.)
  my(@flibs, %libs_seen);
  my %libmap = ( 'm' => '', 'f77' => '', 'F77' => '', 'V77' => '', 'c' => '',
                 'malloc' => '', 'crypt' => '', 'resolv' => '', 'c_s' => '',
                 'socket' => '', 'X11' => 'DECW$XLIBSHR',
                 'Xt' => 'DECW$XTSHR', 'Xm' => 'DECW$XMLIBSHR',
                 'Xmu' => 'DECW$XMULIBSHR');
  if ($self->{'config'}{'vms_cc_type'} ne 'decc') { $libmap{'curses'} = 'VAXCCURSE'; }

  warn "Potential libraries are '$potential_libs'\n" if $verbose;

  # First, sort out directories and library names in the input
  foreach $lib (split ' ',$potential_libs) {
    push(@dirs,$1),   next if $lib =~ /^-L(.*)/;
    push(@dirs,$lib), next if $lib =~ /[:>\]]$/;
    push(@dirs,$lib), next if -d $lib;
    push(@libs,$1),   next if $lib =~ /^-l(.*)/;
    push(@libs,$lib);
  }
  push(@dirs,split(' ',$self->{'config'}{'libpth'}));

  # Now make sure we've got VMS-syntax absolute directory specs
  # (We don't, however, check whether someone's hidden a relative
  # path in a logical name.)
  foreach $dir (@dirs) {
    unless (-d $dir) {
      warn "Skipping nonexistent Directory $dir\n" if $verbose > 1;
      $dir = '';
      next;
    }
    warn "Resolving directory $dir\n" if $verbose;
    if (!File::Spec->file_name_is_absolute($dir)) { 
        $dir = catdir($cwd,$dir); 
    }
  }
  @dirs = grep { length($_) } @dirs;
  unshift(@dirs,''); # Check each $lib without additions first

  LIB: foreach $lib (@libs) {
    if (exists $libmap{$lib}) {
      next unless length $libmap{$lib};
      $lib = $libmap{$lib};
    }

    my(@variants,$variant,$cand);
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
    foreach $variant (@variants) {
      my($fullname, $name);

      foreach $dir (@dirs) {
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
  $lib = join(' ',@fndlibs);

  $ldlib = $crtlstr ? "$lib $crtlstr" : $lib;
  warn "Result:\n\tEXTRALIBS: $lib\n\tLDLOADLIBS: $ldlib\n" if $verbose;
  wantarray ? ($lib, '', $ldlib, '', ($give_libs ? \@flibs : ())) : $lib;
}

1;
