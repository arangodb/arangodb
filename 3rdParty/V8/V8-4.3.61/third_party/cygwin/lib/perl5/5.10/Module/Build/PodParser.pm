package Module::Build::PodParser;

use strict;
use vars qw($VERSION);
$VERSION = '0.2808_01';
$VERSION = eval $VERSION;
use vars qw(@ISA);

sub new {
  # Perl is so fun.
  my $package = shift;

  my $self;

  # Try using Pod::Parser first
  if (eval{ require Pod::Parser; 1; }) {
    @ISA = qw(Pod::Parser);
    $self = $package->SUPER::new(@_);
    $self->{have_pod_parser} = 1;
  } else {
    @ISA = ();
    *parse_from_filehandle = \&_myparse_from_filehandle;
    $self = bless {have_pod_parser => 0, @_}, $package;
  }

  unless ($self->{fh}) {
    die "No 'file' or 'fh' parameter given" unless $self->{file};
    $self->{fh} = IO::File->new($self->{file}) or die "Couldn't open $self->{file}: $!";
  }

  return $self;
}

sub _myparse_from_filehandle {
  my ($self, $fh) = @_;
  
  local $_;
  while (<$fh>) {
    next unless /^=(?!cut)/ .. /^=cut/;  # in POD
    last if ($self->{abstract}) = /^  (?:  [a-z:]+  \s+ - \s+  )  (.*\S)  /ix;
  }
  
  my @author;
  while (<$fh>) {
    next unless /^=head1\s+AUTHORS?/ ... /^=/;
    next if /^=/;
    push @author, $_ if /\@/;
  }
  return unless @author;
  s/^\s+|\s+$//g foreach @author;
  
  $self->{author} = \@author;
  
  return;
}

sub get_abstract {
  my $self = shift;
  return $self->{abstract} if defined $self->{abstract};
  
  $self->parse_from_filehandle($self->{fh});

  return $self->{abstract};
}

sub get_author {
  my $self = shift;
  return $self->{author} if defined $self->{author};
  
  $self->parse_from_filehandle($self->{fh});

  return $self->{author} || [];
}

################## Pod::Parser overrides ###########
sub initialize {
  my $self = shift;
  $self->{_head} = '';
  $self->SUPER::initialize();
}

sub command {
  my ($self, $cmd, $text) = @_;
  if ( $cmd eq 'head1' ) {
    $text =~ s/^\s+//;
    $text =~ s/\s+$//;
    $self->{_head} = $text;
  }
}

sub textblock {
  my ($self, $text) = @_;
  $text =~ s/^\s+//;
  $text =~ s/\s+$//;
  if ($self->{_head} eq 'NAME') {
    my ($name, $abstract) = split( /\s+-\s+/, $text, 2 );
    $self->{abstract} = $abstract;
  } elsif ($self->{_head} =~ /^AUTHORS?$/) {
    push @{$self->{author}}, $text if $text =~ /\@/;
  }
}

sub verbatim {}
sub interior_sequence {}

1;
