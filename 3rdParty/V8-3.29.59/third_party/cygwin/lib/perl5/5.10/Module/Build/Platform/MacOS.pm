package Module::Build::Platform::MacOS;

use strict;
use vars qw($VERSION);
$VERSION = '0.2808_01';
$VERSION = eval $VERSION;
use Module::Build::Base;
use vars qw(@ISA);
@ISA = qw(Module::Build::Base);

use ExtUtils::Install;

sub have_forkpipe { 0 }

sub new {
  my $class = shift;
  my $self = $class->SUPER::new(@_);
  
  # $Config{sitelib} and $Config{sitearch} are, unfortunately, missing.
  foreach ('sitelib', 'sitearch') {
    $self->config($_ => $self->config("install$_"))
      unless $self->config($_);
  }
  
  # For some reason $Config{startperl} is filled with a bunch of crap.
  (my $sp = $self->config('startperl')) =~ s/.*Exit \{Status\}\s//;
  $self->config(startperl => $sp);
  
  return $self;
}

sub make_executable {
  my $self = shift;
  require MacPerl;
  foreach (@_) {
    MacPerl::SetFileInfo('McPL', 'TEXT', $_);
  }
}

sub dispatch {
  my $self = shift;

  if( !@_ and !@ARGV ) {
    require MacPerl;
      
    # What comes first in the action list.
    my @action_list = qw(build test install);
    my %actions = map {+($_, 1)} $self->known_actions;
    delete @actions{@action_list};
    push @action_list, sort { $a cmp $b } keys %actions;

    my %toolserver = map {+$_ => 1} qw(test disttest diff testdb);
    foreach (@action_list) {
      $_ .= ' *' if $toolserver{$_};
    }
    
    my $cmd = MacPerl::Pick("What build command? ('*' requires ToolServer)", @action_list);
    return unless defined $cmd;
    $cmd =~ s/ \*$//;
    $ARGV[0] = ($cmd);
    
    my $args = MacPerl::Ask('Any extra arguments?  (ie. verbose=1)', '');
    return unless defined $args;
    push @ARGV, $self->split_like_shell($args);
  }
  
  $self->SUPER::dispatch(@_);
}

sub ACTION_realclean {
  my $self = shift;
  chmod 0666, $self->{properties}{build_script};
  $self->SUPER::ACTION_realclean;
}

# ExtUtils::Install has a hard-coded '.' directory in versions less
# than 1.30.  We use a sneaky trick to turn that into ':'.
#
# Note that we do it here in a cross-platform way, so this code could
# actually go in Module::Build::Base.  But we put it here to be less
# intrusive for other platforms.

sub ACTION_install {
  my $self = shift;
  
  return $self->SUPER::ACTION_install(@_)
    if eval {ExtUtils::Install->VERSION('1.30'); 1};
    
  local $^W = 0; # Avoid a 'redefine' warning
  local *ExtUtils::Install::find = sub {
    my ($code, @dirs) = @_;

    @dirs = map { $_ eq '.' ? File::Spec->curdir : $_ } @dirs;

    return File::Find::find($code, @dirs);
  };
  
  return $self->SUPER::ACTION_install(@_);
}

1;
__END__

=head1 NAME

Module::Build::Platform::MacOS - Builder class for MacOS platforms

=head1 DESCRIPTION

The sole purpose of this module is to inherit from
C<Module::Build::Base> and override a few methods.  Please see
L<Module::Build> for the docs.

=head2 Overriden Methods

=over 4

=item new()

MacPerl doesn't define $Config{sitelib} or $Config{sitearch} for some
reason, but $Config{installsitelib} and $Config{installsitearch} are
there.  So we copy the install variables to the other location

=item make_executable()

On MacOS we set the file type and creator to MacPerl so it will run
with a double-click.

=item dispatch()

Because there's no easy way to say "./Build test" on MacOS, if
dispatch is called with no arguments and no @ARGV a dialog box will
pop up asking what action to take and any extra arguments.

Default action is "test".

=item ACTION_realclean()

Need to unlock the Build program before deleting.

=back

=head1 AUTHOR

Michael G Schwern <schwern@pobox.com>


=head1 SEE ALSO

perl(1), Module::Build(3), ExtUtils::MakeMaker(3)

=cut
