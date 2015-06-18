package Module::Build::PPMMaker;

use strict;
use vars qw($VERSION);
$VERSION = '0.2808_01';
$VERSION = eval $VERSION;

# This code is mostly borrowed from ExtUtils::MM_Unix 6.10_03, with a
# few tweaks based on the PPD spec at
# http://www.xav.com/perl/site/lib/XML/PPD.html

# The PPD spec is based on <http://www.w3.org/TR/NOTE-OSD>

sub new {
  my $package = shift;
  return bless {@_}, $package;
}

sub make_ppd {
  my ($self, %args) = @_;
  my $build = delete $args{build};

  my @codebase;
  if (exists $args{codebase}) {
    @codebase = ref $args{codebase} ? @{$args{codebase}} : ($args{codebase});
  } else {
    my $distfile = $build->ppm_name . '.tar.gz';
    print "Using default codebase '$distfile'\n";
    @codebase = ($distfile);
  }

  my %dist;
  foreach my $info (qw(name author abstract version)) {
    my $method = "dist_$info";
    $dist{$info} = $build->$method() or die "Can't determine distribution's $info\n";
  }
  $dist{version} = $self->_ppd_version($dist{version});

  $self->_simple_xml_escape($_) foreach $dist{abstract}, @{$dist{author}};

  # TODO: could add <LICENSE HREF=...> tag if we knew what the URLs were for
  # various licenses
  my $ppd = <<"PPD";
<SOFTPKG NAME=\"$dist{name}\" VERSION=\"$dist{version}\">
    <TITLE>$dist{name}</TITLE>
    <ABSTRACT>$dist{abstract}</ABSTRACT>
@{[ join "\n", map "    <AUTHOR>$_</AUTHOR>", @{$dist{author}} ]}
    <IMPLEMENTATION>
PPD

  # TODO: We could set <IMPLTYPE VALUE="PERL" /> or maybe
  # <IMPLTYPE VALUE="PERL/XS" /> ???

  # We don't include recommended dependencies because PPD has no way
  # to distinguish them from normal dependencies.  We don't include
  # build_requires dependencies because the PPM installer doesn't
  # build or test before installing.  And obviously we don't include
  # conflicts either.
  
  foreach my $type (qw(requires)) {
    my $prereq = $build->$type();
    while (my ($modname, $spec) = each %$prereq) {
      next if $modname eq 'perl';

      my $min_version = '0.0';
      foreach my $c ($build->_parse_conditions($spec)) {
        my ($op, $version) = $c =~ /^\s*  (<=?|>=?|==|!=)  \s*  ([\w.]+)  \s*$/x;

        # This is a nasty hack because it fails if there is no >= op
        if ($op eq '>=') {
          $min_version = $version;
          last;
        }
      }

      # Another hack - dependencies are on modules, but PPD expects
      # them to be on distributions (I think).
      $modname =~ s/::/-/g;

      $ppd .= sprintf(<<'EOF', $modname, $self->_ppd_version($min_version));
        <DEPENDENCY NAME="%s" VERSION="%s" />
EOF

    }
  }

  # We only include these tags if this module involves XS, on the
  # assumption that pure Perl modules will work on any OS.  PERLCORE,
  # unfortunately, seems to indicate that a module works with _only_
  # that version of Perl, and so is only appropriate when a module
  # uses XS.
  if (keys %{$build->find_xs_files}) {
    my $perl_version = $self->_ppd_version($build->perl_version);
    $ppd .= sprintf(<<'EOF', $perl_version, $^O, $self->_varchname($build->config) );
        <PERLCORE VERSION="%s" />
        <OS NAME="%s" />
        <ARCHITECTURE NAME="%s" />
EOF
  }

  foreach my $codebase (@codebase) {
    $self->_simple_xml_escape($codebase);
    $ppd .= sprintf(<<'EOF', $codebase);
        <CODEBASE HREF="%s" />
EOF
  }

  $ppd .= <<'EOF';
    </IMPLEMENTATION>
</SOFTPKG>
EOF

  my $ppd_file = "$dist{name}.ppd";
  my $fh = IO::File->new(">$ppd_file")
    or die "Cannot write to $ppd_file: $!";
  print $fh $ppd;
  close $fh;

  return $ppd_file;
}

sub _ppd_version {
  my ($self, $version) = @_;

  # generates something like "0,18,0,0"
  return join ',', (split(/\./, $version), (0)x4)[0..3];
}

sub _varchname {  # Copied from PPM.pm
  my ($self, $config) = @_;
  my $varchname = $config->{archname};
  # Append "-5.8" to architecture name for Perl 5.8 and later
  if (defined($^V) && ord(substr($^V,1)) >= 8) {
    $varchname .= sprintf("-%d.%d", ord($^V), ord(substr($^V,1)));
  }
  return $varchname;
}

{
  my %escapes = (
		 "\n" => "\\n",
		 '"' => '&quot;',
		 '&' => '&amp;',
		 '>' => '&gt;',
		 '<' => '&lt;',
		);
  my $rx = join '|', keys %escapes;
  
  sub _simple_xml_escape {
    $_[1] =~ s/($rx)/$escapes{$1}/go;
  }
}

1;
__END__


=head1 NAME

Module::Build::PPMMaker - Perl Package Manager file creation


=head1 SYNOPSIS

  On the command line, builds a .ppd file:
  ./Build ppd


=head1 DESCRIPTION

This package contains the code that builds F<.ppd> "Perl Package
Description" files, in support of ActiveState's "Perl Package
Manager".  Details are here:
L<http://aspn.activestate.com/ASPN/Downloads/ActivePerl/PPM/>


=head1 AUTHOR

Dave Rolsky <autarch@urth.org>, Ken Williams <kwilliams@cpan.org>


=head1 COPYRIGHT

Copyright (c) 2001-2006 Ken Williams.  All rights reserved.

This library is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.


=head1 SEE ALSO

perl(1), Module::Build(3)

=cut
