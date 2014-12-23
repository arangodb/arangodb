package Module::Build::YAML;

use strict;

use vars qw($VERSION @EXPORT @EXPORT_OK);
$VERSION = "0.50";
@EXPORT = ();
@EXPORT_OK = qw(Dump Load DumpFile LoadFile);

sub new {
    my $this = shift;
    my $class = ref($this) || $this;
    my $self = {};
    bless $self, $class;
    return($self);
}

sub Dump {
    shift if ($_[0] eq __PACKAGE__ || ref($_[0]) eq __PACKAGE__);
    my $yaml = "";
    foreach my $item (@_) {
        $yaml .= "---\n";
        $yaml .= &_yaml_chunk("", $item);
    }
    return $yaml;
}

sub Load {
    shift if ($_[0] eq __PACKAGE__ || ref($_[0]) eq __PACKAGE__);
    die "not yet implemented";
}

# This is basically copied out of YAML.pm and simplified a little.
sub DumpFile {
    shift if ($_[0] eq __PACKAGE__ || ref($_[0]) eq __PACKAGE__);
    my $filename = shift;
    local $/ = "\n"; # reset special to "sane"
    my $mode = '>';
    if ($filename =~ /^\s*(>{1,2})\s*(.*)$/) {
        ($mode, $filename) = ($1, $2);
    }
    open my $OUT, "$mode $filename"
      or die "Can't open $filename for writing: $!";
    print $OUT Dump(@_);
    close $OUT;
}

# This is basically copied out of YAML.pm and simplified a little.
sub LoadFile {
    shift if ($_[0] eq __PACKAGE__ || ref($_[0]) eq __PACKAGE__);
    my $filename = shift;
    open my $IN, $filename
      or die "Can't open $filename for reading: $!";
    return Load(do { local $/; <$IN> });
    close $IN;
}   

sub _yaml_chunk {
  my ($indent, $values) = @_;
  my $yaml_chunk = "";
  my $ref = ref($values);
  my ($value, @allkeys, %keyseen);
  if (!$ref) {  # a scalar
    $yaml_chunk .= &_yaml_value($values) . "\n";
  }
  elsif ($ref eq "ARRAY") {
    foreach $value (@$values) {
      $yaml_chunk .= "$indent-";
      $ref = ref($value);
      if (!$ref) {
        $yaml_chunk .= " " . &_yaml_value($value) . "\n";
      }
      else {
        $yaml_chunk .= "\n";
        $yaml_chunk .= &_yaml_chunk("$indent  ", $value);
      }
    }
  }
  else { # assume "HASH"
    if ($values->{_order} && ref($values->{_order}) eq "ARRAY") {
        @allkeys = @{$values->{_order}};
        $values = { %$values };
        delete $values->{_order};
    }
    push(@allkeys, sort keys %$values);
    foreach my $key (@allkeys) {
      next if (!defined $key || $key eq "" || $keyseen{$key});
      $keyseen{$key} = 1;
      $yaml_chunk .= "$indent$key:";
      $value = $values->{$key};
      $ref = ref($value);
      if (!$ref) {
        $yaml_chunk .= " " . &_yaml_value($value) . "\n";
      }
      else {
        $yaml_chunk .= "\n";
        $yaml_chunk .= &_yaml_chunk("$indent  ", $value);
      }
    }
  }
  return($yaml_chunk);
}

sub _yaml_value {
  my ($value) = @_;
  # undefs become ~
  return '~' if not defined $value;

  # empty strings will become empty strings
  return '""' if $value eq '';

  # allow simple scalars (without embedded quote chars) to be unquoted
  # (includes $%_+=-\;:,./)
  return $value if $value !~ /["'`~\n!\@\#^\&\*\(\)\{\}\[\]\|<>\?]/;

  # quote and escape strings with special values
  return "'$value'"
    if $value !~ /['`~\n!\#^\&\*\(\)\{\}\[\]\|\?]/;  # nothing but " or @ or < or > (email addresses)

  $value =~ s/\n/\\n/g;    # handle embedded newlines
  $value =~ s/"/\\"/g;     # handle embedded quotes
  return qq{"$value"};
}

1;

__END__

=head1 NAME

Module::Build::YAML - Provides just enough YAML support so that Module::Build works even if YAML.pm is not installed

=head1 SYNOPSIS

    use Module::Build::YAML;

    ...

=head1 DESCRIPTION

Provides just enough YAML support so that Module::Build works even if YAML.pm is not installed.

Currently, this amounts to the ability to write META.yml files when "perl Build distmeta"
is executed via the Dump() and DumpFile() functions/methods.

=head1 AUTHOR

Stephen Adkins <spadkins@gmail.com>

=head1 COPYRIGHT

Copyright (c) 2006. Stephen Adkins. All rights reserved.

This program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

See L<http://www.perl.com/perl/misc/Artistic.html>

=cut

