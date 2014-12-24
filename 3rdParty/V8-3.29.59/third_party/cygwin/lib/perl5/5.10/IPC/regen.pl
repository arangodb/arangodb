use strict;

unless (@ARGV) {
  @ARGV = qw( constants );
}

my %gen = map { ($_ => 1) } @ARGV;

if (delete $gen{constants}) {
  make_constants();
}

for my $key (keys %gen) {
  print STDERR "Invalid request to regenerate $key!\n";
}

sub make_constants
{
  unless (eval { require ExtUtils::Constant; 1 }) {
    my @files = qw( const-c.inc const-xs.inc );

    die "Cannot regenerate constants:\n$@\n" if grep { !-f } @files;

    my @deps = qw( regen.pl lib/IPC/SysV.pm );

    my $oldage = (sort { $a <=> $b } map { -M } @files)[-1];  # age of oldest file
    my $depage = (sort { $a <=> $b } map { -M } @deps)[0];    # age of newest dependency
    my @outdated = grep { (-M) > $depage } @files;
    my @newdeps = grep { (-M) < $oldage } @deps;

    print STDERR <<EOM;

***********************************************************************

  The following files seem to be out of date:

    @outdated

  The reason is probably that you modified these files:

    @newdeps

  If you're absolutely sure you didn't touch the files, please ignore
  this message.

  Otherwise, please install the ExtUtils::Constant module.

***********************************************************************

EOM

    exit 0;   # will build anyway, since the files exist
  }

  my $source = 'lib/IPC/SysV.pm';
  local $_;
  local *SYSV;

  open SYSV, $source or die "$source: $!\n";

  my $parse = 0;
  my @const;

  while (<SYSV>) {
    if ($parse) {
      if (/^\)/) { $parse++; last }
      push @const, split;
    }
    /^\@EXPORT_OK\s*=/ and $parse++;
  }

  close SYSV;

  die "couldn't parse $source" if $parse != 2;

  eval {
    ExtUtils::Constant::WriteConstants(
      NAME       => 'IPC::SysV',
      NAMES      => \@const,
      XS_FILE    => 'const-xs.inc',
      C_FILE     => 'const-c.inc',
      XS_SUBNAME => '_constant',
    );
  };

  if ($@) {
    my $err = "Cannot regenerate constants:\n$@\n";
    if ($[ < 5.006) {
      print STDERR $err;
      exit 0;
    }
    die $err;
  }

  print "Writing const-xs.inc\n";
  print "Writing const-c.inc\n";
}
