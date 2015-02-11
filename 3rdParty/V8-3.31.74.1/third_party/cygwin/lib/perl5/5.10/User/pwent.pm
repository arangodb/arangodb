package User::pwent;

use 5.006;
our $VERSION = '1.00';

use strict;
use warnings;

use Config;
use Carp;

our(@EXPORT, @EXPORT_OK, %EXPORT_TAGS);
BEGIN {
    use Exporter   ();
    @EXPORT      = qw(getpwent getpwuid getpwnam getpw);
    @EXPORT_OK   = qw(
                        pw_has

                        $pw_name    $pw_passwd  $pw_uid  $pw_gid
                        $pw_gecos   $pw_dir     $pw_shell
                        $pw_expire  $pw_change  $pw_class
                        $pw_age
                        $pw_quota   $pw_comment
                        $pw_expire

                   );
    %EXPORT_TAGS = (
        FIELDS => [ grep(/^\$pw_/, @EXPORT_OK), @EXPORT ],
        ALL    => [ @EXPORT, @EXPORT_OK ],
    );
}
use vars grep /^\$pw_/, @EXPORT_OK;

#
# XXX: these mean somebody hacked this module's source
#      without understanding the underlying assumptions.
#
my $IE = "[INTERNAL ERROR]";

# Class::Struct forbids use of @ISA
sub import { goto &Exporter::import }

use Class::Struct qw(struct);
struct 'User::pwent' => [
    name    => '$',         # pwent[0]
    passwd  => '$',         # pwent[1]
    uid     => '$',         # pwent[2]
    gid     => '$',         # pwent[3]

    # you'll only have one/none of these three
    change  => '$',         # pwent[4]
    age     => '$',         # pwent[4]
    quota   => '$',         # pwent[4]

    # you'll only have one/none of these two
    comment => '$',         # pwent[5]
    class   => '$',         # pwent[5]

    # you might not have this one
    gecos   => '$',         # pwent[6]

    dir     => '$',         # pwent[7]
    shell   => '$',         # pwent[8]

    # you might not have this one
    expire  => '$',         # pwent[9]

];


# init our groks hash to be true if the built platform knew how
# to do each struct pwd field that perl can ever under any circumstances
# know about.  we do not use /^pw_?/, but just the tails.
sub _feature_init {
    our %Groks;         # whether build system knew how to do this feature
    for my $feep ( qw{
                         pwage      pwchange   pwclass    pwcomment
                         pwexpire   pwgecos    pwpasswd   pwquota
                     }
                 )
    {
        my $short = $feep =~ /^pw(.*)/
                  ? $1
                  : do {
                        # not cluck, as we know we called ourselves,
                        # and a confession is probably imminent anyway
                        warn("$IE $feep is a funny struct pwd field");
                        $feep;
                    };

        exists $Config{ "d_" . $feep }
            || confess("$IE Configure doesn't d_$feep");
        $Groks{$short} = defined $Config{ "d_" . $feep };
    }
    # assume that any that are left are always there
    for my $feep (grep /^\$pw_/s, @EXPORT_OK) {
        $feep =~ /^\$pw_(.*)/;
        $Groks{$1} = 1 unless defined $Groks{$1};
    }
}

# With arguments, reports whether one or more fields are all implemented
# in the build machine's struct pwd pw_*.  May be whitespace separated.
# We do not use /^pw_?/, just the tails.
#
# Without arguments, returns the list of fields implemented on build
# machine, space separated in scalar context.
#
# Takes exception to being asked whether this machine's struct pwd has
# a field that Perl never knows how to provide under any circumstances.
# If the module does this idiocy to itself, the explosion is noisier.
#
sub pw_has {
    our %Groks;         # whether build system knew how to do this feature
    my $cando = 1;
    my $sploder = caller() ne __PACKAGE__
                    ? \&croak
                    : sub { confess("$IE @_") };
    if (@_ == 0) {
        my @valid = sort grep { $Groks{$_} } keys %Groks;
        return wantarray ? @valid : "@valid";
    }
    for my $feep (map { split } @_) {
        defined $Groks{$feep}
            || $sploder->("$feep is never a valid struct pwd field");
        $cando &&= $Groks{$feep};
    }
    return $cando;
}

sub _populate (@) {
    return unless @_;
    my $pwob = new();

    # Any that haven't been pw_had are assumed on "all" platforms of
    # course, this may not be so, but you can't get here otherwise,
    # since the underlying core call already took exception to your
    # impudence.

    $pw_name    = $pwob->name   ( $_[0] );
    $pw_passwd  = $pwob->passwd ( $_[1] )   if pw_has("passwd");
    $pw_uid     = $pwob->uid    ( $_[2] );
    $pw_gid     = $pwob->gid    ( $_[3] );

    if (pw_has("change")) {
        $pw_change      = $pwob->change ( $_[4] );
    }
    elsif (pw_has("age")) {
        $pw_age         = $pwob->age    ( $_[4] );
    }
    elsif (pw_has("quota")) {
        $pw_quota       = $pwob->quota  ( $_[4] );
    }

    if (pw_has("class")) {
        $pw_class       = $pwob->class  ( $_[5] );
    }
    elsif (pw_has("comment")) {
        $pw_comment     = $pwob->comment( $_[5] );
    }

    $pw_gecos   = $pwob->gecos  ( $_[6] ) if pw_has("gecos");

    $pw_dir     = $pwob->dir    ( $_[7] );
    $pw_shell   = $pwob->shell  ( $_[8] );

    $pw_expire  = $pwob->expire ( $_[9] ) if pw_has("expire");

    return $pwob;
}

sub getpwent ( ) { _populate(CORE::getpwent()) }
sub getpwnam ($) { _populate(CORE::getpwnam(shift)) }
sub getpwuid ($) { _populate(CORE::getpwuid(shift)) }
sub getpw    ($) { ($_[0] =~ /^\d+\z/s) ? &getpwuid : &getpwnam }

_feature_init();

1;
__END__

=head1 NAME

User::pwent - by-name interface to Perl's built-in getpw*() functions

=head1 SYNOPSIS

 use User::pwent;
 $pw = getpwnam('daemon')       || die "No daemon user";
 if ( $pw->uid == 1 && $pw->dir =~ m#^/(bin|tmp)?\z#s ) {
     print "gid 1 on root dir";
 }

 $real_shell = $pw->shell || '/bin/sh';

 for (($fullname, $office, $workphone, $homephone) =
        split /\s*,\s*/, $pw->gecos)
 {
    s/&/ucfirst(lc($pw->name))/ge;
 }

 use User::pwent qw(:FIELDS);
 getpwnam('daemon')             || die "No daemon user";
 if ( $pw_uid == 1 && $pw_dir =~ m#^/(bin|tmp)?\z#s ) {
     print "gid 1 on root dir";
 }

 $pw = getpw($whoever);

 use User::pwent qw/:DEFAULT pw_has/;
 if (pw_has(qw[gecos expire quota])) { .... }
 if (pw_has("name uid gid passwd"))  { .... }
 print "Your struct pwd has: ", scalar pw_has(), "\n";

=head1 DESCRIPTION

This module's default exports override the core getpwent(), getpwuid(),
and getpwnam() functions, replacing them with versions that return
C<User::pwent> objects.  This object has methods that return the
similarly named structure field name from the C's passwd structure
from F<pwd.h>, stripped of their leading "pw_" parts, namely C<name>,
C<passwd>, C<uid>, C<gid>, C<change>, C<age>, C<quota>, C<comment>,
C<class>, C<gecos>, C<dir>, C<shell>, and C<expire>.  The C<passwd>,
C<gecos>, and C<shell> fields are tainted when running in taint mode.

You may also import all the structure fields directly into your
namespace as regular variables using the :FIELDS import tag.  (Note
that this still overrides your core functions.)  Access these fields
as variables named with a preceding C<pw_> in front their method
names.  Thus, C<< $passwd_obj->shell >> corresponds to $pw_shell
if you import the fields.

The getpw() function is a simple front-end that forwards
a numeric argument to getpwuid() and the rest to getpwnam().

To access this functionality without the core overrides, pass the
C<use> an empty import list, and then access function functions
with their full qualified names.  The built-ins are always still
available via the C<CORE::> pseudo-package.

=head2 System Specifics

Perl believes that no machine ever has more than one of C<change>,
C<age>, or C<quota> implemented, nor more than one of either
C<comment> or C<class>.  Some machines do not support C<expire>,
C<gecos>, or allegedly, C<passwd>.  You may call these methods
no matter what machine you're on, but they return C<undef> if
unimplemented.

You may ask whether one of these was implemented on the system Perl
was built on by asking the importable C<pw_has> function about them.
This function returns true if all parameters are supported fields
on the build platform, false if one or more were not, and raises
an exception if you asked about a field that Perl never knows how
to provide.  Parameters may be in a space-separated string, or as
separate arguments.  If you pass no parameters, the function returns
the list of C<struct pwd> fields supported by your build platform's
C library, as a list in list context, or a space-separated string
in scalar context.  Note that just because your C library had
a field doesn't necessarily mean that it's fully implemented on
that system.

Interpretation of the C<gecos> field varies between systems, but
traditionally holds 4 comma-separated fields containing the user's
full name, office location, work phone number, and home phone number.
An C<&> in the gecos field should be replaced by the user's properly
capitalized login C<name>.  The C<shell> field, if blank, must be
assumed to be F</bin/sh>.  Perl does not do this for you.  The
C<passwd> is one-way hashed garble, not clear text, and may not be
unhashed save by brute-force guessing.  Secure systems use more a
more secure hashing than DES.  On systems supporting shadow password
systems, Perl automatically returns the shadow password entry when
called by a suitably empowered user, even if your underlying
vendor-provided C library was too short-sighted to realize it should
do this.

See passwd(5) and getpwent(3) for details.

=head1 NOTE

While this class is currently implemented using the Class::Struct
module to build a struct-like class, you shouldn't rely upon this.

=head1 AUTHOR

Tom Christiansen

=head1 HISTORY

=over 4

=item March 18th, 2000

Reworked internals to support better interface to dodgey fields
than normal Perl function provides.  Added pw_has() field.  Improved
documentation.

=back
