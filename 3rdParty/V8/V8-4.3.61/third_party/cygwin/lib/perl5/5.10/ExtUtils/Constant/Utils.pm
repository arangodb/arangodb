package ExtUtils::Constant::Utils;

use strict;
use vars qw($VERSION @EXPORT_OK @ISA $is_perl56);
use Carp;

@ISA = 'Exporter';
@EXPORT_OK = qw(C_stringify perl_stringify);
$VERSION = '0.01';

$is_perl56 = ($] < 5.007 && $] > 5.005_50);

=head1 NAME

ExtUtils::Constant::Utils - helper functions for ExtUtils::Constant

=head1 SYNOPSIS

    use ExtUtils::Constant::Utils qw (C_stringify);
    $C_code = C_stringify $stuff;

=head1 DESCRIPTION

ExtUtils::Constant::Utils packages up utility subroutines used by
ExtUtils::Constant, ExtUtils::Constant::Base and derived classes. All its
functions are explicitly exportable.

=head1 USAGE

=over 4

=item C_stringify NAME

A function which returns a 7 bit ASCII correctly \ escaped version of the
string passed suitable for C's "" or ''. It will die if passed Unicode
characters.

=cut

# Hopefully make a happy C identifier.
sub C_stringify {
  local $_ = shift;
  return unless defined $_;
  # grr 5.6.1
  confess "Wide character in '$_' intended as a C identifier"
    if tr/\0-\377// != length;
  # grr 5.6.1 moreso because its regexps will break on data that happens to
  # be utf8, which includes my 8 bit test cases.
  $_ = pack 'C*', unpack 'U*', $_ . pack 'U*' if $is_perl56;
  s/\\/\\\\/g;
  s/([\"\'])/\\$1/g;	# Grr. fix perl mode.
  s/\n/\\n/g;		# Ensure newlines don't end up in octal
  s/\r/\\r/g;
  s/\t/\\t/g;
  s/\f/\\f/g;
  s/\a/\\a/g;
  if (ord('A') == 193) { # EBCDIC has no ^\0-\177 workalike.
      s/([[:^print:]])/sprintf "\\%03o", ord $1/ge;
  } else {
      s/([^\0-\177])/sprintf "\\%03o", ord $1/ge;
  }
  unless ($] < 5.006) {
    # This will elicit a warning on 5.005_03 about [: :] being reserved unless
    # I cheat
    my $cheat = '([[:^print:]])';
    s/$cheat/sprintf "\\%03o", ord $1/ge;
  } else {
    require POSIX;
    s/([^A-Za-z0-9_])/POSIX::isprint($1) ? $1 : sprintf "\\%03o", ord $1/ge;
  }
  $_;
}

=item perl_stringify NAME

A function which returns a 7 bit ASCII correctly \ escaped version of the
string passed suitable for a perl "" string.

=cut

# Hopefully make a happy perl identifier.
sub perl_stringify {
  local $_ = shift;
  return unless defined $_;
  s/\\/\\\\/g;
  s/([\"\'])/\\$1/g;	# Grr. fix perl mode.
  s/\n/\\n/g;		# Ensure newlines don't end up in octal
  s/\r/\\r/g;
  s/\t/\\t/g;
  s/\f/\\f/g;
  s/\a/\\a/g;
  unless ($] < 5.006) {
    if ($] > 5.007) {
	if (ord('A') == 193) { # EBCDIC has no ^\0-\177 workalike.
	    s/([[:^print:]])/sprintf "\\x{%X}", ord $1/ge;
	} else {
	    s/([^\0-\177])/sprintf "\\x{%X}", ord $1/ge;
	}
    } else {
      # Grr 5.6.1. And I don't think I can use utf8; to force the regexp
      # because 5.005_03 will fail.
      # This is grim, but I also can't split on //
      my $copy;
      foreach my $index (0 .. length ($_) - 1) {
        my $char = substr ($_, $index, 1);
        $copy .= ($char le "\177") ? $char : sprintf "\\x{%X}", ord $char;
      }
      $_ = $copy;
    }
    # This will elicit a warning on 5.005_03 about [: :] being reserved unless
    # I cheat
    my $cheat = '([[:^print:]])';
    s/$cheat/sprintf "\\%03o", ord $1/ge;
  } else {
    # Turns out "\x{}" notation only arrived with 5.6
    s/([^\0-\177])/sprintf "\\x%02X", ord $1/ge;
    require POSIX;
    s/([^A-Za-z0-9_])/POSIX::isprint($1) ? $1 : sprintf "\\%03o", ord $1/ge;
  }
  $_;
}

1;
__END__

=back

=head1 AUTHOR

Nicholas Clark <nick@ccl4.org> based on the code in C<h2xs> by Larry Wall and
others
