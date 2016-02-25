package ExtUtils::CBuilder::Base;

use strict;
use File::Spec;
use File::Basename;
use Cwd ();
use Config;
use Text::ParseWords;

use vars qw($VERSION);
$VERSION = '0.21';

sub new {
  my $class = shift;
  my $self = bless {@_}, $class;

  $self->{properties}{perl} = $class->find_perl_interpreter
    or warn "Warning: Can't locate your perl binary";

  while (my ($k,$v) = each %Config) {
    $self->{config}{$k} = $v unless exists $self->{config}{$k};
  }
  return $self;
}

sub find_perl_interpreter {
  my $perl;
  File::Spec->file_name_is_absolute($perl = $^X)
    or -f ($perl = $Config::Config{perlpath})
    or ($perl = $^X);
  return $perl;
}

sub add_to_cleanup {
  my $self = shift;
  foreach (@_) {
    $self->{files_to_clean}{$_} = 1;
  }
}

sub cleanup {
  my $self = shift;
  foreach my $file (keys %{$self->{files_to_clean}}) {
    unlink $file;
  }
}

sub object_file {
  my ($self, $filename) = @_;

  # File name, minus the suffix
  (my $file_base = $filename) =~ s/\.[^.]+$//;
  return "$file_base$self->{config}{obj_ext}";
}

sub arg_include_dirs {
  my $self = shift;
  return map {"-I$_"} @_;
}

sub arg_nolink { '-c' }

sub arg_object_file {
  my ($self, $file) = @_;
  return ('-o', $file);
}

sub arg_share_object_file {
  my ($self, $file) = @_;
  return ($self->split_like_shell($self->{config}{lddlflags}), '-o', $file);
}

sub arg_exec_file {
  my ($self, $file) = @_;
  return ('-o', $file);
}

sub arg_defines {
  my ($self, %args) = @_;
  return map "-D$_=$args{$_}", keys %args;
}

sub compile {
  my ($self, %args) = @_;
  die "Missing 'source' argument to compile()" unless defined $args{source};
  
  my $cf = $self->{config}; # For convenience

  $args{object_file} ||= $self->object_file($args{source});
  
  my @include_dirs = $self->arg_include_dirs
    (@{$args{include_dirs} || []},
     $self->perl_inc());
  
  my @defines = $self->arg_defines( %{$args{defines} || {}} );
  
  my @extra_compiler_flags = $self->split_like_shell($args{extra_compiler_flags});
  my @cccdlflags = $self->split_like_shell($cf->{cccdlflags});
  my @ccflags = $self->split_like_shell($cf->{ccflags});
  my @optimize = $self->split_like_shell($cf->{optimize});
  my @flags = (@include_dirs, @defines, @cccdlflags, @extra_compiler_flags,
	       $self->arg_nolink,
	       @ccflags, @optimize,
	       $self->arg_object_file($args{object_file}),
	      );
  
  my @cc = $self->split_like_shell($cf->{cc});
  
  $self->do_system(@cc, @flags, $args{source})
    or die "error building $args{object_file} from '$args{source}'";

  return $args{object_file};
}

sub have_compiler {
  my ($self) = @_;
  return $self->{have_compiler} if defined $self->{have_compiler};
  
  my $tmpfile = File::Spec->catfile(File::Spec->tmpdir, 'compilet.c');
  {
    local *FH;
    open FH, "> $tmpfile" or die "Can't create $tmpfile: $!";
    print FH "int boot_compilet() { return 1; }\n";
    close FH;
  }

  my ($obj_file, @lib_files);
  eval {
    $obj_file = $self->compile(source => $tmpfile);
    @lib_files = $self->link(objects => $obj_file, module_name => 'compilet');
  };
  warn $@ if $@;
  my $result = $self->{have_compiler} = $@ ? 0 : 1;
  
  foreach (grep defined, $tmpfile, $obj_file, @lib_files) {
    1 while unlink;
  }
  return $result;
}

sub lib_file {
  my ($self, $dl_file) = @_;
  $dl_file =~ s/\.[^.]+$//;
  $dl_file =~ tr/"//d;
  return "$dl_file.$self->{config}{dlext}";
}


sub exe_file {
  my ($self, $dl_file) = @_;
  $dl_file =~ s/\.[^.]+$//;
  $dl_file =~ tr/"//d;
  return "$dl_file$self->{config}{_exe}";
}

sub need_prelink { 0 }

sub extra_link_args_after_prelink { return }

sub prelink {
  my ($self, %args) = @_;
  
  ($args{dl_file} = $args{dl_name}) =~ s/.*::// unless $args{dl_file};
  
  require ExtUtils::Mksymlists;
  ExtUtils::Mksymlists::Mksymlists( # dl. abbrev for dynamic library
    DL_VARS  => $args{dl_vars}      || [],
    DL_FUNCS => $args{dl_funcs}     || {},
    FUNCLIST => $args{dl_func_list} || [],
    IMPORTS  => $args{dl_imports}   || {},
    NAME     => $args{dl_name},		# Name of the Perl module
    DLBASE   => $args{dl_base},		# Basename of DLL file
    FILE     => $args{dl_file},		# Dir + Basename of symlist file
    VERSION  => (defined $args{dl_version} ? $args{dl_version} : '0.0'),
  );
  
  # Mksymlists will create one of these files
  return grep -e, map "$args{dl_file}.$_", qw(ext def opt);
}

sub link {
  my ($self, %args) = @_;
  return $self->_do_link('lib_file', lddl => 1, %args);
}

sub link_executable {
  my ($self, %args) = @_;
  return $self->_do_link('exe_file', lddl => 0, %args);
}

sub _do_link {
  my ($self, $type, %args) = @_;

  my $cf = $self->{config}; # For convenience
  
  my $objects = delete $args{objects};
  $objects = [$objects] unless ref $objects;
  my $out = $args{$type} || $self->$type($objects->[0]);
  
  my @temp_files;
  @temp_files =
    $self->prelink(%args,
		   dl_name => $args{module_name}) if $args{lddl} && $self->need_prelink;
  
  my @linker_flags = ($self->split_like_shell($args{extra_linker_flags}),
		      $self->extra_link_args_after_prelink(%args, dl_name => $args{module_name},
							   prelink_res => \@temp_files));

  my @output = $args{lddl} ? $self->arg_share_object_file($out) : $self->arg_exec_file($out);
  my @shrp = $self->split_like_shell($cf->{shrpenv});
  my @ld = $self->split_like_shell($cf->{ld});
  
  $self->do_system(@shrp, @ld, @output, @$objects, @linker_flags)
    or die "error building $out from @$objects";
  
  return wantarray ? ($out, @temp_files) : $out;
}


sub do_system {
  my ($self, @cmd) = @_;
  print "@cmd\n" if !$self->{quiet};
  return !system(@cmd);
}

sub split_like_shell {
  my ($self, $string) = @_;
  
  return () unless defined($string);
  return @$string if UNIVERSAL::isa($string, 'ARRAY');
  $string =~ s/^\s+|\s+$//g;
  return () unless length($string);
  
  return Text::ParseWords::shellwords($string);
}

# if building perl, perl's main source directory
sub perl_src {
  # N.B. makemaker actually searches regardless of PERL_CORE, but
  # only squawks at not finding it if PERL_CORE is set

  return unless $ENV{PERL_CORE};

  my $Updir = File::Spec->updir;
  my $dir   = File::Spec->curdir;

  # Try up to 5 levels upwards
  for (0..10) {
    if (
	-f File::Spec->catfile($dir,"config_h.SH")
	&&
	-f File::Spec->catfile($dir,"perl.h")
	&&
	-f File::Spec->catfile($dir,"lib","Exporter.pm")
       ) {
      return Cwd::realpath( $dir );
    }

    $dir = File::Spec->catdir($dir, $Updir);
  }

  warn "PERL_CORE is set but I can't find your perl source!\n";
  return ''; # return empty string if $ENV{PERL_CORE} but can't find dir ???
}

# directory of perl's include files
sub perl_inc {
  my $self = shift;

  $self->perl_src() || File::Spec->catdir($self->{config}{archlibexp},"CORE");
}

sub DESTROY {
  my $self = shift;
  local($., $@, $!, $^E, $?);
  $self->cleanup();
}

1;
