# IO::Dir.pm
#
# Copyright (c) 1997-8 Graham Barr <gbarr@pobox.com>. All rights reserved.
# This program is free software; you can redistribute it and/or
# modify it under the same terms as Perl itself.

package IO::Dir;

use 5.006;

use strict;
use Carp;
use Symbol;
use Exporter;
use IO::File;
our(@ISA, $VERSION, @EXPORT_OK);
use Tie::Hash;
use File::stat;
use File::Spec;

@ISA = qw(Tie::Hash Exporter);
$VERSION = "1.06";
$VERSION = eval $VERSION;
@EXPORT_OK = qw(DIR_UNLINK);

sub DIR_UNLINK () { 1 }

sub new {
    @_ >= 1 && @_ <= 2 or croak 'usage: new IO::Dir [DIRNAME]';
    my $class = shift;
    my $dh = gensym;
    if (@_) {
	IO::Dir::open($dh, $_[0])
	    or return undef;
    }
    bless $dh, $class;
}

sub DESTROY {
    my ($dh) = @_;
    no warnings 'io';
    closedir($dh);
}

sub open {
    @_ == 2 or croak 'usage: $dh->open(DIRNAME)';
    my ($dh, $dirname) = @_;
    return undef
	unless opendir($dh, $dirname);
    # a dir name should always have a ":" in it; assume dirname is
    # in current directory
    $dirname = ':' .  $dirname if ( ($^O eq 'MacOS') && ($dirname !~ /:/) );
    ${*$dh}{io_dir_path} = $dirname;
    1;
}

sub close {
    @_ == 1 or croak 'usage: $dh->close()';
    my ($dh) = @_;
    closedir($dh);
}

sub read {
    @_ == 1 or croak 'usage: $dh->read()';
    my ($dh) = @_;
    readdir($dh);
}

sub seek {
    @_ == 2 or croak 'usage: $dh->seek(POS)';
    my ($dh,$pos) = @_;
    seekdir($dh,$pos);
}

sub tell {
    @_ == 1 or croak 'usage: $dh->tell()';
    my ($dh) = @_;
    telldir($dh);
}

sub rewind {
    @_ == 1 or croak 'usage: $dh->rewind()';
    my ($dh) = @_;
    rewinddir($dh);
}

sub TIEHASH {
    my($class,$dir,$options) = @_;

    my $dh = $class->new($dir)
	or return undef;

    $options ||= 0;

    ${*$dh}{io_dir_unlink} = $options & DIR_UNLINK;
    $dh;
}

sub FIRSTKEY {
    my($dh) = @_;
    $dh->rewind;
    scalar $dh->read;
}

sub NEXTKEY {
    my($dh) = @_;
    scalar $dh->read;
}

sub EXISTS {
    my($dh,$key) = @_;
    -e File::Spec->catfile(${*$dh}{io_dir_path}, $key);
}

sub FETCH {
    my($dh,$key) = @_;
    &lstat(File::Spec->catfile(${*$dh}{io_dir_path}, $key));
}

sub STORE {
    my($dh,$key,$data) = @_;
    my($atime,$mtime) = ref($data) ? @$data : ($data,$data);
    my $file = File::Spec->catfile(${*$dh}{io_dir_path}, $key);
    unless(-e $file) {
	my $io = IO::File->new($file,O_CREAT | O_RDWR);
	$io->close if $io;
    }
    utime($atime,$mtime, $file);
}

sub DELETE {
    my($dh,$key) = @_;

    # Only unlink if unlink-ing is enabled
    return 0
	unless ${*$dh}{io_dir_unlink};

    my $file = File::Spec->catfile(${*$dh}{io_dir_path}, $key);

    -d $file
	? rmdir($file)
	: unlink($file);
}

1;

__END__

=head1 NAME 

IO::Dir - supply object methods for directory handles

=head1 SYNOPSIS

    use IO::Dir;
    $d = IO::Dir->new(".");
    if (defined $d) {
        while (defined($_ = $d->read)) { something($_); }
        $d->rewind;
        while (defined($_ = $d->read)) { something_else($_); }
        undef $d;
    }

    tie %dir, 'IO::Dir', ".";
    foreach (keys %dir) {
	print $_, " " , $dir{$_}->size,"\n";
    }

=head1 DESCRIPTION

The C<IO::Dir> package provides two interfaces to perl's directory reading
routines.

The first interface is an object approach. C<IO::Dir> provides an object
constructor and methods, which are just wrappers around perl's built in
directory reading routines.

=over 4

=item new ( [ DIRNAME ] )

C<new> is the constructor for C<IO::Dir> objects. It accepts one optional
argument which,  if given, C<new> will pass to C<open>

=back

The following methods are wrappers for the directory related functions built
into perl (the trailing `dir' has been removed from the names). See L<perlfunc>
for details of these functions.

=over 4

=item open ( DIRNAME )

=item read ()

=item seek ( POS )

=item tell ()

=item rewind ()

=item close ()

=back

C<IO::Dir> also provides an interface to reading directories via a tied
hash. The tied hash extends the interface beyond just the directory
reading routines by the use of C<lstat>, from the C<File::stat> package,
C<unlink>, C<rmdir> and C<utime>.

=over 4

=item tie %hash, 'IO::Dir', DIRNAME [, OPTIONS ]

=back

The keys of the hash will be the names of the entries in the directory. 
Reading a value from the hash will be the result of calling
C<File::stat::lstat>.  Deleting an element from the hash will 
delete the corresponding file or subdirectory,
provided that C<DIR_UNLINK> is included in the C<OPTIONS>.

Assigning to an entry in the hash will cause the time stamps of the file
to be modified. If the file does not exist then it will be created. Assigning
a single integer to a hash element will cause both the access and 
modification times to be changed to that value. Alternatively a reference to
an array of two values can be passed. The first array element will be used to
set the access time and the second element will be used to set the modification
time.

=head1 SEE ALSO

L<File::stat>

=head1 AUTHOR

Graham Barr. Currently maintained by the Perl Porters.  Please report all
bugs to <perl5-porters@perl.org>.

=head1 COPYRIGHT

Copyright (c) 1997-2003 Graham Barr <gbarr@pobox.com>. All rights reserved.
This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=cut
