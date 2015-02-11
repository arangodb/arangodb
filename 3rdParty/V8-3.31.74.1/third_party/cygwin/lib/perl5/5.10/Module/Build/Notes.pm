package Module::Build::Notes;

# A class for persistent hashes

use strict;
use vars qw($VERSION);
$VERSION = '0.2808_01';
$VERSION = eval $VERSION;
use Data::Dumper;
use IO::File;
use Module::Build::Dumper;

sub new {
  my ($class, %args) = @_;
  my $file = delete $args{file} or die "Missing required parameter 'file' to new()";
  my $self = bless {
		    disk => {},
		    new  => {},
		    file => $file,
		    %args,
		   }, $class;
}

sub restore {
  my $self = shift;

  my $fh = IO::File->new("< $self->{file}") or die "Can't read $self->{file}: $!";
  $self->{disk} = eval do {local $/; <$fh>};
  die $@ if $@;
  $self->{new} = {};
}

sub access {
  my $self = shift;
  return $self->read() unless @_;
  
  my $key = shift;
  return $self->read($key) unless @_;
  
  my $value = shift;
  $self->write({ $key => $value });
  return $self->read($key);
}

sub has_data {
  my $self = shift;
  return keys %{$self->read()} > 0;
}

sub exists {
  my ($self, $key) = @_;
  return exists($self->{new}{$key}) || exists($self->{disk}{$key});
}

sub read {
  my $self = shift;

  if (@_) {
    # Return 1 key as a scalar
    my $key = shift;
    return $self->{new}{$key} if exists $self->{new}{$key};
    return $self->{disk}{$key};
  }
   
  # Return all data
  my $out = (keys %{$self->{new}}
	     ? {%{$self->{disk}}, %{$self->{new}}}
	     : $self->{disk});
  return wantarray ? %$out : $out;
}

sub _same {
  my ($self, $x, $y) = @_;
  return 1 if !defined($x) and !defined($y);
  return 0 if !defined($x) or  !defined($y);
  return $x eq $y;
}

sub write {
  my ($self, $href) = @_;
  $href ||= {};
  
  @{$self->{new}}{ keys %$href } = values %$href;  # Merge

  # Do some optimization to avoid unnecessary writes
  foreach my $key (keys %{ $self->{new} }) {
    next if ref $self->{new}{$key};
    next if ref $self->{disk}{$key} or !exists $self->{disk}{$key};
    delete $self->{new}{$key} if $self->_same($self->{new}{$key}, $self->{disk}{$key});
  }
  
  if (my $file = $self->{file}) {
    my ($vol, $dir, $base) = File::Spec->splitpath($file);
    $dir = File::Spec->catpath($vol, $dir, '');
    return unless -e $dir && -d $dir;  # The user needs to arrange for this

    return if -e $file and !keys %{ $self->{new} };  # Nothing to do
    
    @{$self->{disk}}{ keys %{$self->{new}} } = values %{$self->{new}};  # Merge 
    $self->_dump($file, $self->{disk});
   
    $self->{new} = {};
  }
  return $self->read;
}

sub _dump {
  my ($self, $file, $data) = @_;
  
  my $fh = IO::File->new("> $file") or die "Can't create '$file': $!";
  print {$fh} Module::Build::Dumper->_data_dump($data);
}

sub write_config_data {
  my ($self, %args) = @_;

  my $fh = IO::File->new("> $args{file}") or die "Can't create '$args{file}': $!";

  printf $fh <<'EOF', $args{config_module};
package %s;
use strict;
my $arrayref = eval do {local $/; <DATA>}
  or die "Couldn't load ConfigData data: $@";
close DATA;
my ($config, $features, $auto_features) = @$arrayref;

sub config { $config->{$_[1]} }

sub set_config { $config->{$_[1]} = $_[2] }
sub set_feature { $features->{$_[1]} = 0+!!$_[2] }  # Constrain to 1 or 0

sub auto_feature_names { grep !exists $features->{$_}, keys %%$auto_features }

sub feature_names {
  my @features = (keys %%$features, auto_feature_names());
  @features;
}

sub config_names  { keys %%$config }

sub write {
  my $me = __FILE__;
  require IO::File;

  # Can't use Module::Build::Dumper here because M::B is only a
  # build-time prereq of this module
  require Data::Dumper;

  my $mode_orig = (stat $me)[2] & 07777;
  chmod($mode_orig | 0222, $me); # Make it writeable
  my $fh = IO::File->new($me, 'r+') or die "Can't rewrite $me: $!";
  seek($fh, 0, 0);
  while (<$fh>) {
    last if /^__DATA__$/;
  }
  die "Couldn't find __DATA__ token in $me" if eof($fh);

  seek($fh, tell($fh), 0);
  my $data = [$config, $features, $auto_features];
  $fh->print( 'do{ my '
	      . Data::Dumper->new([$data],['x'])->Purity(1)->Dump()
	      . '$x; }' );
  truncate($fh, tell($fh));
  $fh->close;

  chmod($mode_orig, $me)
    or warn "Couldn't restore permissions on $me: $!";
}

sub feature {
  my ($package, $key) = @_;
  return $features->{$key} if exists $features->{$key};
  
  my $info = $auto_features->{$key} or return 0;
  
  # Under perl 5.005, each(%%$foo) isn't working correctly when $foo
  # was reanimated with Data::Dumper and eval().  Not sure why, but
  # copying to a new hash seems to solve it.
  my %%info = %%$info;
  
  require Module::Build;  # XXX should get rid of this
  while (my ($type, $prereqs) = each %%info) {
    next if $type eq 'description' || $type eq 'recommends';
    
    my %%p = %%$prereqs;  # Ditto here.
    while (my ($modname, $spec) = each %%p) {
      my $status = Module::Build->check_installed_status($modname, $spec);
      if ((!$status->{ok}) xor ($type =~ /conflicts$/)) { return 0; }
    }
  }
  return 1;
}

EOF

  my ($module_name, $notes_name) = ($args{module}, $args{config_module});
  printf $fh <<"EOF", $notes_name, $module_name;

=head1 NAME

$notes_name - Configuration for $module_name


=head1 SYNOPSIS

  use $notes_name;
  \$value = $notes_name->config('foo');
  \$value = $notes_name->feature('bar');
  
  \@names = $notes_name->config_names;
  \@names = $notes_name->feature_names;
  
  $notes_name->set_config(foo => \$new_value);
  $notes_name->set_feature(bar => \$new_value);
  $notes_name->write;  # Save changes


=head1 DESCRIPTION

This module holds the configuration data for the C<$module_name>
module.  It also provides a programmatic interface for getting or
setting that configuration data.  Note that in order to actually make
changes, you'll have to have write access to the C<$notes_name>
module, and you should attempt to understand the repercussions of your
actions.


=head1 METHODS

=over 4

=item config(\$name)

Given a string argument, returns the value of the configuration item
by that name, or C<undef> if no such item exists.

=item feature(\$name)

Given a string argument, returns the value of the feature by that
name, or C<undef> if no such feature exists.

=item set_config(\$name, \$value)

Sets the configuration item with the given name to the given value.
The value may be any Perl scalar that will serialize correctly using
C<Data::Dumper>.  This includes references, objects (usually), and
complex data structures.  It probably does not include transient
things like filehandles or sockets.

=item set_feature(\$name, \$value)

Sets the feature with the given name to the given boolean value.  The
value will be converted to 0 or 1 automatically.

=item config_names()

Returns a list of all the names of config items currently defined in
C<$notes_name>, or in scalar context the number of items.

=item feature_names()

Returns a list of all the names of features currently defined in
C<$notes_name>, or in scalar context the number of features.

=item auto_feature_names()

Returns a list of all the names of features whose availability is
dynamically determined, or in scalar context the number of such
features.  Does not include such features that have later been set to
a fixed value.

=item write()

Commits any changes from C<set_config()> and C<set_feature()> to disk.
Requires write access to the C<$notes_name> module.

=back


=head1 AUTHOR

C<$notes_name> was automatically created using C<Module::Build>.
C<Module::Build> was written by Ken Williams, but he holds no
authorship claim or copyright claim to the contents of C<$notes_name>.

=cut

__DATA__

EOF

  print {$fh} Module::Build::Dumper->_data_dump([$args{config_data}, $args{feature}, $args{auto_features}]);
}

1;
