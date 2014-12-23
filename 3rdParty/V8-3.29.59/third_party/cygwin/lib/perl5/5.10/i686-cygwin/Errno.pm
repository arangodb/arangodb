#
# This file is auto-generated. ***ANY*** changes here will be lost
#

package Errno;
our (@EXPORT_OK,%EXPORT_TAGS,@ISA,$VERSION,%errno,$AUTOLOAD);
use Exporter ();
use Config;
use strict;

"$Config{'archname'}-$Config{'osvers'}" eq
"cygwin-thread-multi-64int-1.5.25(0.15642)" or
	die "Errno architecture (cygwin-thread-multi-64int-1.5.25(0.15642)) does not match executable architecture ($Config{'archname'}-$Config{'osvers'})";

$VERSION = "1.10";
$VERSION = eval $VERSION;
@ISA = qw(Exporter);

@EXPORT_OK = qw(EBADR EADV ENOMSG EROFS ENOTSUP ESHUTDOWN EMULTIHOP
	EPROTONOSUPPORT ENOLCK ENFILE EADDRINUSE EL3HLT ECONNABORTED EBADF
	ENOTBLK EDEADLK ECHRNG ENOLINK ESRMNT ELNRNG ETIME ENOTDIR ENOTTY
	EINVAL ENOANO EXDEV EBADE EBADSLT ELOOP ECONNREFUSED ENOSTR ENONET
	EOVERFLOW EISCONN EFBIG EPFNOSUPPORT ECONNRESET ENOENT EWOULDBLOCK
	ELIBMAX EBADMSG ENOMEDIUM EL2HLT ECASECLASH ENOPKG EBFONT EDOM ELIBSCN
	EMSGSIZE ENOCSI EL3RST ENOTSOCK EDESTADDRREQ EIDRM EIO ENOSPC
	EINPROGRESS ERANGE ENOBUFS EAFNOSUPPORT EADDRNOTAVAIL ENOSYS EINTR
	EHOSTDOWN EREMOTE EILSEQ EBADFD ENOSR ENOMEM EPIPE ENETUNREACH
	ENOTCONN ESTALE ENODATA EDQUOT EUSERS EOPNOTSUPP EPROTO EFTYPE ESPIPE
	EALREADY ENAMETOOLONG EMFILE EACCES ENOEXEC EPROCLIM EISDIR ELBIN
	EBUSY EBADRQC EPERM E2BIG ELIBEXEC EEXIST EDOTDOT ETOOMANYREFS ELIBACC
	ENOTUNIQ ECOMM ELIBBAD ENOSHARE EUNATCH ESOCKTNOSUPPORT ETIMEDOUT
	ENXIO ESRCH ETXTBSY ENODEV EFAULT EXFULL EMLINK ENMFILE EDEADLOCK
	EAGAIN ENOPROTOOPT ECHILD EHOSTUNREACH ENETDOWN EPROTOTYPE EREMCHG
	EL2NSYNC ENETRESET ENOTEMPTY);

%EXPORT_TAGS = (
    POSIX => [qw(
	E2BIG EACCES EADDRINUSE EADDRNOTAVAIL EAFNOSUPPORT EAGAIN EALREADY
	EBADF EBUSY ECHILD ECONNABORTED ECONNREFUSED ECONNRESET EDEADLK
	EDESTADDRREQ EDOM EDQUOT EEXIST EFAULT EFBIG EHOSTDOWN EHOSTUNREACH
	EINPROGRESS EINTR EINVAL EIO EISCONN EISDIR ELOOP EMFILE EMLINK
	EMSGSIZE ENAMETOOLONG ENETDOWN ENETRESET ENETUNREACH ENFILE ENOBUFS
	ENODEV ENOENT ENOEXEC ENOLCK ENOMEM ENOPROTOOPT ENOSPC ENOSYS ENOTBLK
	ENOTCONN ENOTDIR ENOTEMPTY ENOTSOCK ENOTTY ENXIO EOPNOTSUPP EPERM
	EPFNOSUPPORT EPIPE EPROCLIM EPROTONOSUPPORT EPROTOTYPE ERANGE EREMOTE
	EROFS ESHUTDOWN ESOCKTNOSUPPORT ESPIPE ESRCH ESTALE ETIMEDOUT
	ETOOMANYREFS ETXTBSY EUSERS EWOULDBLOCK EXDEV
    )]
);

sub EPERM () { 1 }
sub ENOENT () { 2 }
sub ESRCH () { 3 }
sub EINTR () { 4 }
sub EIO () { 5 }
sub ENXIO () { 6 }
sub E2BIG () { 7 }
sub ENOEXEC () { 8 }
sub EBADF () { 9 }
sub ECHILD () { 10 }
sub EWOULDBLOCK () { 11 }
sub EAGAIN () { 11 }
sub ENOMEM () { 12 }
sub EACCES () { 13 }
sub EFAULT () { 14 }
sub ENOTBLK () { 15 }
sub EBUSY () { 16 }
sub EEXIST () { 17 }
sub EXDEV () { 18 }
sub ENODEV () { 19 }
sub ENOTDIR () { 20 }
sub EISDIR () { 21 }
sub EINVAL () { 22 }
sub ENFILE () { 23 }
sub EMFILE () { 24 }
sub ENOTTY () { 25 }
sub ETXTBSY () { 26 }
sub EFBIG () { 27 }
sub ENOSPC () { 28 }
sub ESPIPE () { 29 }
sub EROFS () { 30 }
sub EMLINK () { 31 }
sub EPIPE () { 32 }
sub EDOM () { 33 }
sub ERANGE () { 34 }
sub ENOMSG () { 35 }
sub EIDRM () { 36 }
sub ECHRNG () { 37 }
sub EL2NSYNC () { 38 }
sub EL3HLT () { 39 }
sub EL3RST () { 40 }
sub ELNRNG () { 41 }
sub EUNATCH () { 42 }
sub ENOCSI () { 43 }
sub EL2HLT () { 44 }
sub EDEADLK () { 45 }
sub ENOLCK () { 46 }
sub EBADE () { 50 }
sub EBADR () { 51 }
sub EXFULL () { 52 }
sub ENOANO () { 53 }
sub EBADRQC () { 54 }
sub EBADSLT () { 55 }
sub EDEADLOCK () { 56 }
sub EBFONT () { 57 }
sub ENOSTR () { 60 }
sub ENODATA () { 61 }
sub ETIME () { 62 }
sub ENOSR () { 63 }
sub ENONET () { 64 }
sub ENOPKG () { 65 }
sub EREMOTE () { 66 }
sub ENOLINK () { 67 }
sub EADV () { 68 }
sub ESRMNT () { 69 }
sub ECOMM () { 70 }
sub EPROTO () { 71 }
sub EMULTIHOP () { 74 }
sub ELBIN () { 75 }
sub EDOTDOT () { 76 }
sub EBADMSG () { 77 }
sub EFTYPE () { 79 }
sub ENOTUNIQ () { 80 }
sub EBADFD () { 81 }
sub EREMCHG () { 82 }
sub ELIBACC () { 83 }
sub ELIBBAD () { 84 }
sub ELIBSCN () { 85 }
sub ELIBMAX () { 86 }
sub ELIBEXEC () { 87 }
sub ENOSYS () { 88 }
sub ENMFILE () { 89 }
sub ENOTEMPTY () { 90 }
sub ENAMETOOLONG () { 91 }
sub ELOOP () { 92 }
sub EOPNOTSUPP () { 95 }
sub EPFNOSUPPORT () { 96 }
sub ECONNRESET () { 104 }
sub ENOBUFS () { 105 }
sub EAFNOSUPPORT () { 106 }
sub EPROTOTYPE () { 107 }
sub ENOTSOCK () { 108 }
sub ENOPROTOOPT () { 109 }
sub ESHUTDOWN () { 110 }
sub ECONNREFUSED () { 111 }
sub EADDRINUSE () { 112 }
sub ECONNABORTED () { 113 }
sub ENETUNREACH () { 114 }
sub ENETDOWN () { 115 }
sub ETIMEDOUT () { 116 }
sub EHOSTDOWN () { 117 }
sub EHOSTUNREACH () { 118 }
sub EINPROGRESS () { 119 }
sub EALREADY () { 120 }
sub EDESTADDRREQ () { 121 }
sub EMSGSIZE () { 122 }
sub EPROTONOSUPPORT () { 123 }
sub ESOCKTNOSUPPORT () { 124 }
sub EADDRNOTAVAIL () { 125 }
sub ENETRESET () { 126 }
sub EISCONN () { 127 }
sub ENOTCONN () { 128 }
sub ETOOMANYREFS () { 129 }
sub EPROCLIM () { 130 }
sub EUSERS () { 131 }
sub EDQUOT () { 132 }
sub ESTALE () { 133 }
sub ENOTSUP () { 134 }
sub ENOMEDIUM () { 135 }
sub ENOSHARE () { 136 }
sub ECASECLASH () { 137 }
sub EILSEQ () { 138 }
sub EOVERFLOW () { 139 }

sub TIEHASH { bless [] }

sub FETCH {
    my ($self, $errname) = @_;
    my $proto = prototype("Errno::$errname");
    my $errno = "";
    if (defined($proto) && $proto eq "") {
	no strict 'refs';
	$errno = &$errname;
        $errno = 0 unless $! == $errno;
    }
    return $errno;
}

sub STORE {
    require Carp;
    Carp::confess("ERRNO hash is read only!");
}

*CLEAR = \&STORE;
*DELETE = \&STORE;

sub NEXTKEY {
    my($k,$v);
    while(($k,$v) = each %Errno::) {
	my $proto = prototype("Errno::$k");
	last if (defined($proto) && $proto eq "");
    }
    $k
}

sub FIRSTKEY {
    my $s = scalar keys %Errno::;	# initialize iterator
    goto &NEXTKEY;
}

sub EXISTS {
    my ($self, $errname) = @_;
    my $r = ref $errname;
    my $proto = !$r || $r eq 'CODE' ? prototype($errname) : undef;
    defined($proto) && $proto eq "";
}

tie %!, __PACKAGE__;

1;
__END__

=head1 NAME

Errno - System errno constants

=head1 SYNOPSIS

    use Errno qw(EINTR EIO :POSIX);

=head1 DESCRIPTION

C<Errno> defines and conditionally exports all the error constants
defined in your system C<errno.h> include file. It has a single export
tag, C<:POSIX>, which will export all POSIX defined error numbers.

C<Errno> also makes C<%!> magic such that each element of C<%!> has a
non-zero value only if C<$!> is set to that value. For example:

    use Errno;

    unless (open(FH, "/fangorn/spouse")) {
        if ($!{ENOENT}) {
            warn "Get a wife!\n";
        } else {
            warn "This path is barred: $!";
        } 
    } 

If a specified constant C<EFOO> does not exist on the system, C<$!{EFOO}>
returns C<"">.  You may use C<exists $!{EFOO}> to check whether the
constant is available on the system.

=head1 CAVEATS

Importing a particular constant may not be very portable, because the
import will fail on platforms that do not have that constant.  A more
portable way to set C<$!> to a valid value is to use:

    if (exists &Errno::EFOO) {
        $! = &Errno::EFOO;
    }

=head1 AUTHOR

Graham Barr <gbarr@pobox.com>

=head1 COPYRIGHT

Copyright (c) 1997-8 Graham Barr. All rights reserved.
This program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

=cut

