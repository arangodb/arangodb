package if;

$VERSION = '0.05';

sub work {
  my $method = shift() ? 'import' : 'unimport';
  die "Too few arguments to `use if' (some code returning an empty list in list context?)"
    unless @_ >= 2;
  return unless shift;		# CONDITION

  my $p = $_[0];		# PACKAGE
  (my $file = "$p.pm") =~ s!::!/!g;
  require $file;		# Works even if $_[0] is a keyword (like open)
  my $m = $p->can($method);
  goto &$m if $m;
}

sub import   { shift; unshift @_, 1; goto &work }
sub unimport { shift; unshift @_, 0; goto &work }

1;
__END__

=head1 NAME

if - C<use> a Perl module if a condition holds

=head1 SYNOPSIS

  use if CONDITION, MODULE => ARGUMENTS;

=head1 DESCRIPTION

The construct

  use if CONDITION, MODULE => ARGUMENTS;

has no effect unless C<CONDITION> is true.  In this case the effect is
the same as of

  use MODULE ARGUMENTS;

Above C<< => >> provides necessary quoting of C<MODULE>.  If not used (e.g.,
no ARGUMENTS to give), you'd better quote C<MODULE> yourselves.

=head1 BUGS

The current implementation does not allow specification of the
required version of the module.

=head1 AUTHOR

Ilya Zakharevich L<mailto:perl-module-if@ilyaz.org>.

=cut

