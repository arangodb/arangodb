package Archive::Tar::File;
use strict;

use IO::File;
use File::Spec::Unix    ();
use File::Spec          ();
use File::Basename      ();

use Archive::Tar::Constant;

use vars qw[@ISA $VERSION];
@ISA        = qw[Archive::Tar];
$VERSION    = '0.02';

### set value to 1 to oct() it during the unpack ###
my $tmpl = [
        name        => 0,   # string
        mode        => 1,   # octal
        uid         => 1,   # octal
        gid         => 1,   # octal
        size        => 1,   # octal
        mtime       => 1,   # octal
        chksum      => 1,   # octal
        type        => 0,   # character
        linkname    => 0,   # string
        magic       => 0,   # string
        version     => 0,   # 2 bytes
        uname       => 0,   # string
        gname       => 0,   # string
        devmajor    => 1,   # octal
        devminor    => 1,   # octal
        prefix      => 0,

### end UNPACK items ###
        raw         => 0,   # the raw data chunk
        data        => 0,   # the data associated with the file --
                            # This  might be very memory intensive
];

### install get/set accessors for this object.
for ( my $i=0; $i<scalar @$tmpl ; $i+=2 ) {
    my $key = $tmpl->[$i];
    no strict 'refs';
    *{__PACKAGE__."::$key"} = sub {
        my $self = shift;
        $self->{$key} = $_[0] if @_;

        ### just in case the key is not there or undef or something ###
        {   local $^W = 0;
            return $self->{$key};
        }
    }
}

=head1 NAME

Archive::Tar::File - a subclass for in-memory extracted file from Archive::Tar

=head1 SYNOPSIS

    my @items = $tar->get_files;

    print $_->name, ' ', $_->size, "\n" for @items;

    print $object->get_content;
    $object->replace_content('new content');

    $object->rename( 'new/full/path/to/file.c' );

=head1 DESCRIPTION

Archive::Tar::Files provides a neat little object layer for in-memory
extracted files. It's mostly used internally in Archive::Tar to tidy
up the code, but there's no reason users shouldn't use this API as
well.

=head2 Accessors

A lot of the methods in this package are accessors to the various
fields in the tar header:

=over 4

=item name

The file's name

=item mode

The file's mode

=item uid

The user id owning the file

=item gid

The group id owning the file

=item size

File size in bytes

=item mtime

Modification time. Adjusted to mac-time on MacOS if required

=item chksum

Checksum field for the tar header

=item type

File type -- numeric, but comparable to exported constants -- see
Archive::Tar's documentation

=item linkname

If the file is a symlink, the file it's pointing to

=item magic

Tar magic string -- not useful for most users

=item version

Tar version string -- not useful for most users

=item uname

The user name that owns the file

=item gname

The group name that owns the file

=item devmajor

Device major number in case of a special file

=item devminor

Device minor number in case of a special file

=item prefix

Any directory to prefix to the extraction path, if any

=item raw

Raw tar header -- not useful for most users

=back

=head1 Methods

=head2 new( file => $path )

Returns a new Archive::Tar::File object from an existing file.

Returns undef on failure.

=head2 new( data => $path, $data, $opt )

Returns a new Archive::Tar::File object from data.

C<$path> defines the file name (which need not exist), C<$data> the
file contents, and C<$opt> is a reference to a hash of attributes
which may be used to override the default attributes (fields in the
tar header), which are described above in the Accessors section.

Returns undef on failure.

=head2 new( chunk => $chunk )

Returns a new Archive::Tar::File object from a raw 512-byte tar
archive chunk.

Returns undef on failure.

=cut

sub new {
    my $class   = shift;
    my $what    = shift;

    my $obj =   ($what eq 'chunk') ? __PACKAGE__->_new_from_chunk( @_ ) :
                ($what eq 'file' ) ? __PACKAGE__->_new_from_file( @_ ) :
                ($what eq 'data' ) ? __PACKAGE__->_new_from_data( @_ ) :
                undef;

    return $obj;
}

### copies the data, creates a clone ###
sub clone {
    my $self = shift;
    return bless { %$self }, ref $self;
}

sub _new_from_chunk {
    my $class = shift;
    my $chunk = shift or return;    # 512 bytes of tar header
    my %hash  = @_;

    ### filter any arguments on defined-ness of values.
    ### this allows overriding from what the tar-header is saying
    ### about this tar-entry. Particularly useful for @LongLink files
    my %args  = map { $_ => $hash{$_} } grep { defined $hash{$_} } keys %hash;

    ### makes it start at 0 actually... :) ###
    my $i = -1;
    my %entry = map {
        $tmpl->[++$i] => $tmpl->[++$i] ? oct $_ : $_
    } map { /^([^\0]*)/ } unpack( UNPACK, $chunk );

    my $obj = bless { %entry, %args }, $class;

	### magic is a filetype string.. it should have something like 'ustar' or
	### something similar... if the chunk is garbage, skip it
	return unless $obj->magic !~ /\W/;

    ### store the original chunk ###
    $obj->raw( $chunk );

    $obj->type(FILE) if ( (!length $obj->type) or ($obj->type =~ /\W/) );
    $obj->type(DIR)  if ( ($obj->is_file) && ($obj->name =~ m|/$|) );


    return $obj;

}

sub _new_from_file {
    my $class       = shift;
    my $path        = shift;        
    
    ### path has to at least exist
    return unless defined $path;
    
    my $type        = __PACKAGE__->_filetype($path);
    my $data        = '';

    READ: { 
        unless ($type == DIR ) {
            my $fh = IO::File->new;
        
            unless( $fh->open($path) ) {
                ### dangling symlinks are fine, stop reading but continue
                ### creating the object
                last READ if $type == SYMLINK;
                
                ### otherwise, return from this function --
                ### anything that's *not* a symlink should be
                ### resolvable
                return;
            }

            ### binmode needed to read files properly on win32 ###
            binmode $fh;
            $data = do { local $/; <$fh> };
            close $fh;
        }
    }

    my @items       = qw[mode uid gid size mtime];
    my %hash        = map { shift(@items), $_ } (lstat $path)[2,4,5,7,9];

    ### you *must* set size == 0 on symlinks, or the next entry will be
    ### though of as the contents of the symlink, which is wrong.
    ### this fixes bug #7937
    $hash{size}     = 0 if ($type == DIR or $type == SYMLINK);
    $hash{mtime}    -= TIME_OFFSET;

    ### strip the high bits off the mode, which we don't need to store
    $hash{mode}     = STRIP_MODE->( $hash{mode} );


    ### probably requires some file path munging here ... ###
    ### name and prefix are set later
    my $obj = {
        %hash,
        name        => '',
        chksum      => CHECK_SUM,
        type        => $type,
        linkname    => ($type == SYMLINK and CAN_READLINK)
                            ? readlink $path
                            : '',
        magic       => MAGIC,
        version     => TAR_VERSION,
        uname       => UNAME->( $hash{uid} ),
        gname       => GNAME->( $hash{gid} ),
        devmajor    => 0,   # not handled
        devminor    => 0,   # not handled
        prefix      => '',
        data        => $data,
    };

    bless $obj, $class;

    ### fix up the prefix and file from the path
    my($prefix,$file) = $obj->_prefix_and_file( $path );
    $obj->prefix( $prefix );
    $obj->name( $file );

    return $obj;
}

sub _new_from_data {
    my $class   = shift;
    my $path    = shift;    return unless defined $path;
    my $data    = shift;    return unless defined $data;
    my $opt     = shift;

    my $obj = {
        data        => $data,
        name        => '',
        mode        => MODE,
        uid         => UID,
        gid         => GID,
        size        => length $data,
        mtime       => time - TIME_OFFSET,
        chksum      => CHECK_SUM,
        type        => FILE,
        linkname    => '',
        magic       => MAGIC,
        version     => TAR_VERSION,
        uname       => UNAME->( UID ),
        gname       => GNAME->( GID ),
        devminor    => 0,
        devmajor    => 0,
        prefix      => '',
    };

    ### overwrite with user options, if provided ###
    if( $opt and ref $opt eq 'HASH' ) {
        for my $key ( keys %$opt ) {

            ### don't write bogus options ###
            next unless exists $obj->{$key};
            $obj->{$key} = $opt->{$key};
        }
    }

    bless $obj, $class;

    ### fix up the prefix and file from the path
    my($prefix,$file) = $obj->_prefix_and_file( $path );
    $obj->prefix( $prefix );
    $obj->name( $file );

    return $obj;
}

sub _prefix_and_file {
    my $self = shift;
    my $path = shift;

    my ($vol, $dirs, $file) = File::Spec->splitpath( $path, $self->is_dir );
    my @dirs = File::Spec->splitdir( $dirs );

    ### so sometimes the last element is '' -- probably when trailing
    ### dir slashes are encountered... this is is of course pointless,
    ### so remove it
    pop @dirs while @dirs and not length $dirs[-1];

    ### if it's a directory, then $file might be empty
    $file = pop @dirs if $self->is_dir and not length $file;

    my $prefix = File::Spec::Unix->catdir(
                        grep { length } $vol, @dirs
                    );
    return( $prefix, $file );
}

sub _filetype {
    my $self = shift;
    my $file = shift;
    
    return unless defined $file;

    return SYMLINK  if (-l $file);	# Symlink

    return FILE     if (-f _);		# Plain file

    return DIR      if (-d _);		# Directory

    return FIFO     if (-p _);		# Named pipe

    return SOCKET   if (-S _);		# Socket

    return BLOCKDEV if (-b _);		# Block special

    return CHARDEV  if (-c _);		# Character special

    ### shouldn't happen, this is when making archives, not reading ###
    return LONGLINK if ( $file eq LONGLINK_NAME );

    return UNKNOWN;		            # Something else (like what?)

}

### this method 'downgrades' a file to plain file -- this is used for
### symlinks when FOLLOW_SYMLINKS is true.
sub _downgrade_to_plainfile {
    my $entry = shift;
    $entry->type( FILE );
    $entry->mode( MODE );
    $entry->linkname('');

    return 1;
}

=head2 full_path

Returns the full path from the tar header; this is basically a
concatenation of the C<prefix> and C<name> fields.

=cut

sub full_path {
    my $self = shift;

    ### if prefix field is emtpy
    return $self->name unless defined $self->prefix and length $self->prefix;

    ### or otherwise, catfile'd
    return File::Spec::Unix->catfile( $self->prefix, $self->name );
}


=head2 validate

Done by Archive::Tar internally when reading the tar file:
validate the header against the checksum to ensure integer tar file.

Returns true on success, false on failure

=cut

sub validate {
    my $self = shift;

    my $raw = $self->raw;

    ### don't know why this one is different from the one we /write/ ###
    substr ($raw, 148, 8) = "        ";
	return unpack ("%16C*", $raw) == $self->chksum ? 1 : 0;
}

=head2 has_content

Returns a boolean to indicate whether the current object has content.
Some special files like directories and so on never will have any
content. This method is mainly to make sure you don't get warnings
for using uninitialized values when looking at an object's content.

=cut

sub has_content {
    my $self = shift;
    return defined $self->data() && length $self->data() ? 1 : 0;
}

=head2 get_content

Returns the current content for the in-memory file

=cut

sub get_content {
    my $self = shift;
    $self->data( );
}

=head2 get_content_by_ref

Returns the current content for the in-memory file as a scalar
reference. Normal users won't need this, but it will save memory if
you are dealing with very large data files in your tar archive, since
it will pass the contents by reference, rather than make a copy of it
first.

=cut

sub get_content_by_ref {
    my $self = shift;

    return \$self->{data};
}

=head2 replace_content( $content )

Replace the current content of the file with the new content. This
only affects the in-memory archive, not the on-disk version until
you write it.

Returns true on success, false on failure.

=cut

sub replace_content {
    my $self = shift;
    my $data = shift || '';

    $self->data( $data );
    $self->size( length $data );
    return 1;
}

=head2 rename( $new_name )

Rename the current file to $new_name.

Note that you must specify a Unix path for $new_name, since per tar
standard, all files in the archive must be Unix paths.

Returns true on success and false on failure.

=cut

sub rename {
    my $self = shift;
    my $path = shift;
    
    return unless defined $path;

    my ($prefix,$file) = $self->_prefix_and_file( $path );

    $self->name( $file );
    $self->prefix( $prefix );

	return 1;
}

=head1 Convenience methods

To quickly check the type of a C<Archive::Tar::File> object, you can
use the following methods:

=over 4

=item is_file

Returns true if the file is of type C<file>

=item is_dir

Returns true if the file is of type C<dir>

=item is_hardlink

Returns true if the file is of type C<hardlink>

=item is_symlink

Returns true if the file is of type C<symlink>

=item is_chardev

Returns true if the file is of type C<chardev>

=item is_blockdev

Returns true if the file is of type C<blockdev>

=item is_fifo

Returns true if the file is of type C<fifo>

=item is_socket

Returns true if the file is of type C<socket>

=item is_longlink

Returns true if the file is of type C<LongLink>.
Should not happen after a successful C<read>.

=item is_label

Returns true if the file is of type C<Label>.
Should not happen after a successful C<read>.

=item is_unknown

Returns true if the file type is C<unknown>

=back

=cut

#stupid perl5.5.3 needs to warn if it's not numeric
sub is_file     { local $^W;    FILE      == $_[0]->type }
sub is_dir      { local $^W;    DIR       == $_[0]->type }
sub is_hardlink { local $^W;    HARDLINK  == $_[0]->type }
sub is_symlink  { local $^W;    SYMLINK   == $_[0]->type }
sub is_chardev  { local $^W;    CHARDEV   == $_[0]->type }
sub is_blockdev { local $^W;    BLOCKDEV  == $_[0]->type }
sub is_fifo     { local $^W;    FIFO      == $_[0]->type }
sub is_socket   { local $^W;    SOCKET    == $_[0]->type }
sub is_unknown  { local $^W;    UNKNOWN   == $_[0]->type }
sub is_longlink { local $^W;    LONGLINK  eq $_[0]->type }
sub is_label    { local $^W;    LABEL     eq $_[0]->type }

1;
