### the gnu tar specification:
### http://www.gnu.org/software/tar/manual/tar.html
###
### and the pax format spec, which tar derives from:
### http://www.opengroup.org/onlinepubs/007904975/utilities/pax.html

package Archive::Tar;
require 5.005_03;

use strict;
use vars qw[$DEBUG $error $VERSION $WARN $FOLLOW_SYMLINK $CHOWN $CHMOD
            $DO_NOT_USE_PREFIX $HAS_PERLIO $HAS_IO_STRING
            $INSECURE_EXTRACT_MODE
         ];

$DEBUG                  = 0;
$WARN                   = 1;
$FOLLOW_SYMLINK         = 0;
$VERSION                = "1.38";
$CHOWN                  = 1;
$CHMOD                  = 1;
$DO_NOT_USE_PREFIX      = 0;
$INSECURE_EXTRACT_MODE  = 0;

BEGIN {
    use Config;
    $HAS_PERLIO = $Config::Config{useperlio};

    ### try and load IO::String anyway, so you can dynamically
    ### switch between perlio and IO::String
    eval {
        require IO::String;
        import IO::String;
    };
    $HAS_IO_STRING = $@ ? 0 : 1;

}

use Cwd;
use IO::File;
use Carp                qw(carp croak);
use File::Spec          ();
use File::Spec::Unix    ();
use File::Path          ();

use Archive::Tar::File;
use Archive::Tar::Constant;

=head1 NAME

Archive::Tar - module for manipulations of tar archives

=head1 SYNOPSIS

    use Archive::Tar;
    my $tar = Archive::Tar->new;

    $tar->read('origin.tgz',1);
    $tar->extract();

    $tar->add_files('file/foo.pl', 'docs/README');
    $tar->add_data('file/baz.txt', 'This is the contents now');

    $tar->rename('oldname', 'new/file/name');

    $tar->write('files.tar');

=head1 DESCRIPTION

Archive::Tar provides an object oriented mechanism for handling tar
files.  It provides class methods for quick and easy files handling
while also allowing for the creation of tar file objects for custom
manipulation.  If you have the IO::Zlib module installed,
Archive::Tar will also support compressed or gzipped tar files.

An object of class Archive::Tar represents a .tar(.gz) archive full
of files and things.

=head1 Object Methods

=head2 Archive::Tar->new( [$file, $compressed] )

Returns a new Tar object. If given any arguments, C<new()> calls the
C<read()> method automatically, passing on the arguments provided to
the C<read()> method.

If C<new()> is invoked with arguments and the C<read()> method fails
for any reason, C<new()> returns undef.

=cut

my $tmpl = {
    _data   => [ ],
    _file   => 'Unknown',
};

### install get/set accessors for this object.
for my $key ( keys %$tmpl ) {
    no strict 'refs';
    *{__PACKAGE__."::$key"} = sub {
        my $self = shift;
        $self->{$key} = $_[0] if @_;
        return $self->{$key};
    }
}

sub new {
    my $class = shift;
    $class = ref $class if ref $class;

    ### copying $tmpl here since a shallow copy makes it use the
    ### same aref, causing for files to remain in memory always.
    my $obj = bless { _data => [ ], _file => 'Unknown' }, $class;

    if (@_) {
        unless ( $obj->read( @_ ) ) {
            $obj->_error(qq[No data could be read from file]);
            return;
        }
    }

    return $obj;
}

=head2 $tar->read ( $filename|$handle, $compressed, {opt => 'val'} )

Read the given tar file into memory.
The first argument can either be the name of a file or a reference to
an already open filehandle (or an IO::Zlib object if it's compressed)
The second argument indicates whether the file referenced by the first
argument is compressed.

The C<read> will I<replace> any previous content in C<$tar>!

The second argument may be considered optional if IO::Zlib is
installed, since it will transparently Do The Right Thing.
Archive::Tar will warn if you try to pass a compressed file if
IO::Zlib is not available and simply return.

Note that you can currently B<not> pass a C<gzip> compressed
filehandle, which is not opened with C<IO::Zlib>, nor a string
containing the full archive information (either compressed or
uncompressed). These are worth while features, but not currently
implemented. See the C<TODO> section.

The third argument can be a hash reference with options. Note that
all options are case-sensitive.

=over 4

=item limit

Do not read more than C<limit> files. This is useful if you have
very big archives, and are only interested in the first few files.

=item extract

If set to true, immediately extract entries when reading them. This
gives you the same memory break as the C<extract_archive> function.
Note however that entries will not be read into memory, but written
straight to disk.

=back

All files are stored internally as C<Archive::Tar::File> objects.
Please consult the L<Archive::Tar::File> documentation for details.

Returns the number of files read in scalar context, and a list of
C<Archive::Tar::File> objects in list context.

=cut

sub read {
    my $self = shift;
    my $file = shift;
    my $gzip = shift || 0;
    my $opts = shift || {};

    unless( defined $file ) {
        $self->_error( qq[No file to read from!] );
        return;
    } else {
        $self->_file( $file );
    }

    my $handle = $self->_get_handle($file, $gzip, READ_ONLY->( ZLIB ) )
                    or return;

    my $data = $self->_read_tar( $handle, $opts ) or return;

    $self->_data( $data );

    return wantarray ? @$data : scalar @$data;
}

sub _get_handle {
    my $self = shift;
    my $file = shift;   return unless defined $file;
                        return $file if ref $file;

    my $gzip = shift || 0;
    my $mode = shift || READ_ONLY->( ZLIB ); # default to read only

    my $fh; my $bin;

    ### only default to ZLIB if we're not trying to /write/ to a handle ###
    if( ZLIB and $gzip || MODE_READ->( $mode ) ) {

        ### IO::Zlib will Do The Right Thing, even when passed
        ### a plain file ###
        $fh = new IO::Zlib;

    } else {
        if( $gzip ) {
            $self->_error(qq[Compression not available - Install IO::Zlib!]);
            return;

        } else {
            $fh = new IO::File;
            $bin++;
        }
    }

    unless( $fh->open( $file, $mode ) ) {
        $self->_error( qq[Could not create filehandle for '$file': $!!] );
        return;
    }

    binmode $fh if $bin;

    return $fh;
}

sub _read_tar {
    my $self    = shift;
    my $handle  = shift or return;
    my $opts    = shift || {};

    my $count   = $opts->{limit}    || 0;
    my $extract = $opts->{extract}  || 0;

    ### set a cap on the amount of files to extract ###
    my $limit   = 0;
    $limit = 1 if $count > 0;

    my $tarfile = [ ];
    my $chunk;
    my $read = 0;
    my $real_name;  # to set the name of a file when
                    # we're encountering @longlink
    my $data;

    LOOP:
    while( $handle->read( $chunk, HEAD ) ) {
        ### IO::Zlib doesn't support this yet
        my $offset = eval { tell $handle } || 'unknown';

        unless( $read++ ) {
            my $gzip = GZIP_MAGIC_NUM;
            if( $chunk =~ /$gzip/ ) {
                $self->_error( qq[Cannot read compressed format in tar-mode] );
                return;
            }
        }

        ### if we can't read in all bytes... ###
        last if length $chunk != HEAD;

        ### Apparently this should really be two blocks of 512 zeroes,
        ### but GNU tar sometimes gets it wrong. See comment in the
        ### source code (tar.c) to GNU cpio.
        next if $chunk eq TAR_END;

        ### according to the posix spec, the last 12 bytes of the header are
        ### null bytes, to pad it to a 512 byte block. That means if these
        ### bytes are NOT null bytes, it's a corrrupt header. See:
        ### www.koders.com/c/fidCE473AD3D9F835D690259D60AD5654591D91D5BA.aspx
        ### line 111
        {   my $nulls = join '', "\0" x 12;
            unless( $nulls eq substr( $chunk, 500, 12 ) ) {
                $self->_error( qq[Invalid header block at offset $offset] );
                next LOOP;
            }
        }

        ### pass the realname, so we can set it 'proper' right away
        ### some of the heuristics are done on the name, so important
        ### to set it ASAP
        my $entry;
        {   my %extra_args = ();
            $extra_args{'name'} = $$real_name if defined $real_name;
            
            unless( $entry = Archive::Tar::File->new(   chunk => $chunk, 
                                                        %extra_args ) 
            ) {
                $self->_error( qq[Couldn't read chunk at offset $offset] );
                next LOOP;
            }
        }

        ### ignore labels:
        ### http://www.gnu.org/manual/tar/html_node/tar_139.html
        next if $entry->is_label;

        if( length $entry->type and ($entry->is_file || $entry->is_longlink) ) {

            if ( $entry->is_file && !$entry->validate ) {
                ### sometimes the chunk is rather fux0r3d and a whole 512
                ### bytes ends up in the ->name area.
                ### clean it up, if need be
                my $name = $entry->name;
                $name = substr($name, 0, 100) if length $name > 100;
                $name =~ s/\n/ /g;

                $self->_error( $name . qq[: checksum error] );
                next LOOP;
            }

            my $block = BLOCK_SIZE->( $entry->size );

            $data = $entry->get_content_by_ref;

            ### just read everything into memory
            ### can't do lazy loading since IO::Zlib doesn't support 'seek'
            ### this is because Compress::Zlib doesn't support it =/
            ### this reads in the whole data in one read() call.
            if( $handle->read( $$data, $block ) < $block ) {
                $self->_error( qq[Read error on tarfile (missing data) '].
                                    $entry->full_path ."' at offset $offset" );
                next LOOP;
            }

            ### throw away trailing garbage ###
            substr ($$data, $entry->size) = "" if defined $$data;

            ### part II of the @LongLink munging -- need to do /after/
            ### the checksum check.
            if( $entry->is_longlink ) {
                ### weird thing in tarfiles -- if the file is actually a
                ### @LongLink, the data part seems to have a trailing ^@
                ### (unprintable) char. to display, pipe output through less.
                ### but that doesn't *always* happen.. so check if the last
                ### character is a control character, and if so remove it
                ### at any rate, we better remove that character here, or tests
                ### like 'eq' and hashlook ups based on names will SO not work
                ### remove it by calculating the proper size, and then
                ### tossing out everything that's longer than that size.

                ### count number of nulls
                my $nulls = $$data =~ tr/\0/\0/;

                ### cut data + size by that many bytes
                $entry->size( $entry->size - $nulls );
                substr ($$data, $entry->size) = "";
            }
        }

        ### clean up of the entries.. posix tar /apparently/ has some
        ### weird 'feature' that allows for filenames > 255 characters
        ### they'll put a header in with as name '././@LongLink' and the
        ### contents will be the name of the /next/ file in the archive
        ### pretty crappy and kludgy if you ask me

        ### set the name for the next entry if this is a @LongLink;
        ### this is one ugly hack =/ but needed for direct extraction
        if( $entry->is_longlink ) {
            $real_name = $data;
            next LOOP;
        } elsif ( defined $real_name ) {
            $entry->name( $$real_name );
            $entry->prefix('');
            undef $real_name;
        }

        $self->_extract_file( $entry ) if $extract
                                            && !$entry->is_longlink
                                            && !$entry->is_unknown
                                            && !$entry->is_label;

        ### Guard against tarfiles with garbage at the end
	    last LOOP if $entry->name eq '';

        ### push only the name on the rv if we're extracting
        ### -- for extract_archive
        push @$tarfile, ($extract ? $entry->name : $entry);

        if( $limit ) {
            $count-- unless $entry->is_longlink || $entry->is_dir;
            last LOOP unless $count;
        }
    } continue {
        undef $data;
    }

    return $tarfile;
}

=head2 $tar->contains_file( $filename )

Check if the archive contains a certain file.
It will return true if the file is in the archive, false otherwise.

Note however, that this function does an exact match using C<eq>
on the full path. So it cannot compensate for case-insensitive file-
systems or compare 2 paths to see if they would point to the same
underlying file.

=cut

sub contains_file {
    my $self = shift;
    my $full = shift;
    
    return unless defined $full;

    ### don't warn if the entry isn't there.. that's what this function
    ### is for after all.
    local $WARN = 0;
    return 1 if $self->_find_entry($full);
    return;
}

=head2 $tar->extract( [@filenames] )

Write files whose names are equivalent to any of the names in
C<@filenames> to disk, creating subdirectories as necessary. This
might not work too well under VMS.
Under MacPerl, the file's modification time will be converted to the
MacOS zero of time, and appropriate conversions will be done to the
path.  However, the length of each element of the path is not
inspected to see whether it's longer than MacOS currently allows (32
characters).

If C<extract> is called without a list of file names, the entire
contents of the archive are extracted.

Returns a list of filenames extracted.

=cut

sub extract {
    my $self    = shift;
    my @args    = @_;
    my @files;

    # use the speed optimization for all extracted files
    local($self->{cwd}) = cwd() unless $self->{cwd};

    ### you requested the extraction of only certian files
    if( @args ) {
        for my $file ( @args ) {
            
            ### it's already an object?
            if( UNIVERSAL::isa( $file, 'Archive::Tar::File' ) ) {
                push @files, $file;
                next;

            ### go find it then
            } else {
            
                my $found;
                for my $entry ( @{$self->_data} ) {
                    next unless $file eq $entry->full_path;
    
                    ### we found the file you're looking for
                    push @files, $entry;
                    $found++;
                }
    
                unless( $found ) {
                    return $self->_error( 
                        qq[Could not find '$file' in archive] );
                }
            }
        }

    ### just grab all the file items
    } else {
        @files = $self->get_files;
    }

    ### nothing found? that's an error
    unless( scalar @files ) {
        $self->_error( qq[No files found for ] . $self->_file );
        return;
    }

    ### now extract them
    for my $entry ( @files ) {
        unless( $self->_extract_file( $entry ) ) {
            $self->_error(q[Could not extract ']. $entry->full_path .q['] );
            return;
        }
    }

    return @files;
}

=head2 $tar->extract_file( $file, [$extract_path] )

Write an entry, whose name is equivalent to the file name provided to
disk. Optionally takes a second parameter, which is the full native
path (including filename) the entry will be written to.

For example:

    $tar->extract_file( 'name/in/archive', 'name/i/want/to/give/it' );

    $tar->extract_file( $at_file_object,   'name/i/want/to/give/it' );

Returns true on success, false on failure.

=cut

sub extract_file {
    my $self = shift;
    my $file = shift;   return unless defined $file;
    my $alt  = shift;

    my $entry = $self->_find_entry( $file )
        or $self->_error( qq[Could not find an entry for '$file'] ), return;

    return $self->_extract_file( $entry, $alt );
}

sub _extract_file {
    my $self    = shift;
    my $entry   = shift or return;
    my $alt     = shift;

    ### you wanted an alternate extraction location ###
    my $name = defined $alt ? $alt : $entry->full_path;

                            ### splitpath takes a bool at the end to indicate
                            ### that it's splitting a dir
    my ($vol,$dirs,$file);
    if ( defined $alt ) { # It's a local-OS path
        ($vol,$dirs,$file) = File::Spec->splitpath(       $alt,
                                                          $entry->is_dir );
    } else {
        ($vol,$dirs,$file) = File::Spec::Unix->splitpath( $name,
                                                          $entry->is_dir );
    }

    my $dir;
    ### is $name an absolute path? ###
    if( File::Spec->file_name_is_absolute( $dirs ) ) {

        ### absolute names are not allowed to be in tarballs under
        ### strict mode, so only allow it if a user tells us to do it
        if( not defined $alt and not $INSECURE_EXTRACT_MODE ) {
            $self->_error( 
                q[Entry ']. $entry->full_path .q[' is an absolute path. ].
                q[Not extracting absolute paths under SECURE EXTRACT MODE]
            );  
            return;
        }
        
        ### user asked us to, it's fine.
        $dir = $dirs;

    ### it's a relative path ###
    } else {
        my $cwd     = (defined $self->{cwd} ? $self->{cwd} : cwd());

        my @dirs = defined $alt
            ? File::Spec->splitdir( $dirs )         # It's a local-OS path
            : File::Spec::Unix->splitdir( $dirs );  # it's UNIX-style, likely
                                                    # straight from the tarball

        ### paths that leave the current directory are not allowed under
        ### strict mode, so only allow it if a user tells us to do this.
        if( not defined $alt            and 
            not $INSECURE_EXTRACT_MODE  and 
            grep { $_ eq '..' } @dirs
        ) {
            $self->_error(
                q[Entry ']. $entry->full_path .q[' is attempting to leave the ].
                q[current working directory. Not extracting under SECURE ].
                q[EXTRACT MODE]
            );
            return;
        }            
        
        ### '.' is the directory delimiter, of which the first one has to
        ### be escaped/changed.
        map tr/\./_/, @dirs if ON_VMS;        

        my ($cwd_vol,$cwd_dir,$cwd_file) 
                    = File::Spec->splitpath( $cwd );
        my @cwd     = File::Spec->splitdir( $cwd_dir );
        push @cwd, $cwd_file if length $cwd_file;

        ### We need to pass '' as the last elemant to catpath. Craig Berry
        ### explains why (msgid <p0624083dc311ae541393@[172.16.52.1]>):
        ### The root problem is that splitpath on UNIX always returns the 
        ### final path element as a file even if it is a directory, and of
        ### course there is no way it can know the difference without checking
        ### against the filesystem, which it is documented as not doing.  When
        ### you turn around and call catpath, on VMS you have to know which bits
        ### are directory bits and which bits are file bits.  In this case we
        ### know the result should be a directory.  I had thought you could omit
        ### the file argument to catpath in such a case, but apparently on UNIX
        ### you can't.
        $dir        = File::Spec->catpath( 
                            $cwd_vol, File::Spec->catdir( @cwd, @dirs ), '' 
                        );

        ### catdir() returns undef if the path is longer than 255 chars on VMS
        unless ( defined $dir ) {
            $^W && $self->_error( qq[Could not compose a path for '$dirs'\n] );
            return;
        }

    }

    if( -e $dir && !-d _ ) {
        $^W && $self->_error( qq['$dir' exists, but it's not a directory!\n] );
        return;
    }

    unless ( -d _ ) {
        eval { File::Path::mkpath( $dir, 0, 0777 ) };
        if( $@ ) {
            $self->_error( qq[Could not create directory '$dir': $@] );
            return;
        }
        
        ### XXX chown here? that might not be the same as in the archive
        ### as we're only chown'ing to the owner of the file we're extracting
        ### not to the owner of the directory itself, which may or may not
        ### be another entry in the archive
        ### Answer: no, gnu tar doesn't do it either, it'd be the wrong
        ### way to go.
        #if( $CHOWN && CAN_CHOWN ) {
        #    chown $entry->uid, $entry->gid, $dir or
        #        $self->_error( qq[Could not set uid/gid on '$dir'] );
        #}
    }

    ### we're done if we just needed to create a dir ###
    return 1 if $entry->is_dir;

    my $full = File::Spec->catfile( $dir, $file );

    if( $entry->is_unknown ) {
        $self->_error( qq[Unknown file type for file '$full'] );
        return;
    }

    if( length $entry->type && $entry->is_file ) {
        my $fh = IO::File->new;
        $fh->open( '>' . $full ) or (
            $self->_error( qq[Could not open file '$full': $!] ),
            return
        );

        if( $entry->size ) {
            binmode $fh;
            syswrite $fh, $entry->data or (
                $self->_error( qq[Could not write data to '$full'] ),
                return
            );
        }

        close $fh or (
            $self->_error( qq[Could not close file '$full'] ),
            return
        );

    } else {
        $self->_make_special_file( $entry, $full ) or return;
    }

    utime time, $entry->mtime - TIME_OFFSET, $full or
        $self->_error( qq[Could not update timestamp] );

    if( $CHOWN && CAN_CHOWN ) {
        chown $entry->uid, $entry->gid, $full or
            $self->_error( qq[Could not set uid/gid on '$full'] );
    }

    ### only chmod if we're allowed to, but never chmod symlinks, since they'll
    ### change the perms on the file they're linking too...
    if( $CHMOD and not -l $full ) {
        chmod $entry->mode, $full or
            $self->_error( qq[Could not chown '$full' to ] . $entry->mode );
    }

    return 1;
}

sub _make_special_file {
    my $self    = shift;
    my $entry   = shift     or return;
    my $file    = shift;    return unless defined $file;

    my $err;

    if( $entry->is_symlink ) {
        my $fail;
        if( ON_UNIX ) {
            symlink( $entry->linkname, $file ) or $fail++;

        } else {
            $self->_extract_special_file_as_plain_file( $entry, $file )
                or $fail++;
        }

        $err =  qq[Making symbolink link from '] . $entry->linkname .
                qq[' to '$file' failed] if $fail;

    } elsif ( $entry->is_hardlink ) {
        my $fail;
        if( ON_UNIX ) {
            link( $entry->linkname, $file ) or $fail++;

        } else {
            $self->_extract_special_file_as_plain_file( $entry, $file )
                or $fail++;
        }

        $err =  qq[Making hard link from '] . $entry->linkname .
                qq[' to '$file' failed] if $fail;

    } elsif ( $entry->is_fifo ) {
        ON_UNIX && !system('mknod', $file, 'p') or
            $err = qq[Making fifo ']. $entry->name .qq[' failed];

    } elsif ( $entry->is_blockdev or $entry->is_chardev ) {
        my $mode = $entry->is_blockdev ? 'b' : 'c';

        ON_UNIX && !system('mknod', $file, $mode,
                            $entry->devmajor, $entry->devminor) or
            $err =  qq[Making block device ']. $entry->name .qq[' (maj=] .
                    $entry->devmajor . qq[ min=] . $entry->devminor .
                    qq[) failed.];

    } elsif ( $entry->is_socket ) {
        ### the original doesn't do anything special for sockets.... ###
        1;
    }

    return $err ? $self->_error( $err ) : 1;
}

### don't know how to make symlinks, let's just extract the file as
### a plain file
sub _extract_special_file_as_plain_file {
    my $self    = shift;
    my $entry   = shift     or return;
    my $file    = shift;    return unless defined $file;

    my $err;
    TRY: {
        my $orig = $self->_find_entry( $entry->linkname );

        unless( $orig ) {
            $err =  qq[Could not find file '] . $entry->linkname .
                    qq[' in memory.];
            last TRY;
        }

        ### clone the entry, make it appear as a normal file ###
        my $clone = $entry->clone;
        $clone->_downgrade_to_plainfile;
        $self->_extract_file( $clone, $file ) or last TRY;

        return 1;
    }

    return $self->_error($err);
}

=head2 $tar->list_files( [\@properties] )

Returns a list of the names of all the files in the archive.

If C<list_files()> is passed an array reference as its first argument
it returns a list of hash references containing the requested
properties of each file.  The following list of properties is
supported: name, size, mtime (last modified date), mode, uid, gid,
linkname, uname, gname, devmajor, devminor, prefix.

Passing an array reference containing only one element, 'name', is
special cased to return a list of names rather than a list of hash
references, making it equivalent to calling C<list_files> without
arguments.

=cut

sub list_files {
    my $self = shift;
    my $aref = shift || [ ];

    unless( $self->_data ) {
        $self->read() or return;
    }

    if( @$aref == 0 or ( @$aref == 1 and $aref->[0] eq 'name' ) ) {
        return map { $_->full_path } @{$self->_data};
    } else {

        #my @rv;
        #for my $obj ( @{$self->_data} ) {
        #    push @rv, { map { $_ => $obj->$_() } @$aref };
        #}
        #return @rv;

        ### this does the same as the above.. just needs a +{ }
        ### to make sure perl doesn't confuse it for a block
        return map {    my $o=$_;
                        +{ map { $_ => $o->$_() } @$aref }
                    } @{$self->_data};
    }
}

sub _find_entry {
    my $self = shift;
    my $file = shift;

    unless( defined $file ) {
        $self->_error( qq[No file specified] );
        return;
    }

    ### it's an object already
    return $file if UNIVERSAL::isa( $file, 'Archive::Tar::File' );

    for my $entry ( @{$self->_data} ) {
        my $path = $entry->full_path;
        return $entry if $path eq $file;
    }

    $self->_error( qq[No such file in archive: '$file'] );
    return;
}

=head2 $tar->get_files( [@filenames] )

Returns the C<Archive::Tar::File> objects matching the filenames
provided. If no filename list was passed, all C<Archive::Tar::File>
objects in the current Tar object are returned.

Please refer to the C<Archive::Tar::File> documentation on how to
handle these objects.

=cut

sub get_files {
    my $self = shift;

    return @{ $self->_data } unless @_;

    my @list;
    for my $file ( @_ ) {
        push @list, grep { defined } $self->_find_entry( $file );
    }

    return @list;
}

=head2 $tar->get_content( $file )

Return the content of the named file.

=cut

sub get_content {
    my $self = shift;
    my $entry = $self->_find_entry( shift ) or return;

    return $entry->data;
}

=head2 $tar->replace_content( $file, $content )

Make the string $content be the content for the file named $file.

=cut

sub replace_content {
    my $self = shift;
    my $entry = $self->_find_entry( shift ) or return;

    return $entry->replace_content( shift );
}

=head2 $tar->rename( $file, $new_name )

Rename the file of the in-memory archive to $new_name.

Note that you must specify a Unix path for $new_name, since per tar
standard, all files in the archive must be Unix paths.

Returns true on success and false on failure.

=cut

sub rename {
    my $self = shift;
    my $file = shift; return unless defined $file;
    my $new  = shift; return unless defined $new;

    my $entry = $self->_find_entry( $file ) or return;

    return $entry->rename( $new );
}

=head2 $tar->remove (@filenamelist)

Removes any entries with names matching any of the given filenames
from the in-memory archive. Returns a list of C<Archive::Tar::File>
objects that remain.

=cut

sub remove {
    my $self = shift;
    my @list = @_;

    my %seen = map { $_->full_path => $_ } @{$self->_data};
    delete $seen{ $_ } for @list;

    $self->_data( [values %seen] );

    return values %seen;
}

=head2 $tar->clear

C<clear> clears the current in-memory archive. This effectively gives
you a 'blank' object, ready to be filled again. Note that C<clear>
only has effect on the object, not the underlying tarfile.

=cut

sub clear {
    my $self = shift or return;

    $self->_data( [] );
    $self->_file( '' );

    return 1;
}


=head2 $tar->write ( [$file, $compressed, $prefix] )

Write the in-memory archive to disk.  The first argument can either
be the name of a file or a reference to an already open filehandle (a
GLOB reference). If the second argument is true, the module will use
IO::Zlib to write the file in a compressed format.  If IO::Zlib is
not available, the C<write> method will fail and return.

Note that when you pass in a filehandle, the compression argument
is ignored, as all files are printed verbatim to your filehandle.
If you wish to enable compression with filehandles, use an
C<IO::Zlib> filehandle instead.

Specific levels of compression can be chosen by passing the values 2
through 9 as the second parameter.

The third argument is an optional prefix. All files will be tucked
away in the directory you specify as prefix. So if you have files
'a' and 'b' in your archive, and you specify 'foo' as prefix, they
will be written to the archive as 'foo/a' and 'foo/b'.

If no arguments are given, C<write> returns the entire formatted
archive as a string, which could be useful if you'd like to stuff the
archive into a socket or a pipe to gzip or something.

=cut

sub write {
    my $self        = shift;
    my $file        = shift; $file = '' unless defined $file;
    my $gzip        = shift || 0;
    my $ext_prefix  = shift; $ext_prefix = '' unless defined $ext_prefix;
    my $dummy       = '';
    
    ### only need a handle if we have a file to print to ###
    my $handle = length($file)
                    ? ( $self->_get_handle($file, $gzip, WRITE_ONLY->($gzip) )
                        or return )
                    : $HAS_PERLIO    ? do { open my $h, '>', \$dummy; $h }
                    : $HAS_IO_STRING ? IO::String->new 
                    : __PACKAGE__->no_string_support();



    for my $entry ( @{$self->_data} ) {
        ### entries to be written to the tarfile ###
        my @write_me;

        ### only now will we change the object to reflect the current state
        ### of the name and prefix fields -- this needs to be limited to
        ### write() only!
        my $clone = $entry->clone;


        ### so, if you don't want use to use the prefix, we'll stuff 
        ### everything in the name field instead
        if( $DO_NOT_USE_PREFIX ) {

            ### you might have an extended prefix, if so, set it in the clone
            ### XXX is ::Unix right?
            $clone->name( length $ext_prefix
                            ? File::Spec::Unix->catdir( $ext_prefix,
                                                        $clone->full_path)
                            : $clone->full_path );
            $clone->prefix( '' );

        ### otherwise, we'll have to set it properly -- prefix part in the
        ### prefix and name part in the name field.
        } else {

            ### split them here, not before!
            my ($prefix,$name) = $clone->_prefix_and_file( $clone->full_path );

            ### you might have an extended prefix, if so, set it in the clone
            ### XXX is ::Unix right?
            $prefix = File::Spec::Unix->catdir( $ext_prefix, $prefix )
                if length $ext_prefix;

            $clone->prefix( $prefix );
            $clone->name( $name );
        }

        ### names are too long, and will get truncated if we don't add a
        ### '@LongLink' file...
        my $make_longlink = (   length($clone->name)    > NAME_LENGTH or
                                length($clone->prefix)  > PREFIX_LENGTH
                            ) || 0;

        ### perhaps we need to make a longlink file?
        if( $make_longlink ) {
            my $longlink = Archive::Tar::File->new(
                            data => LONGLINK_NAME,
                            $clone->full_path,
                            { type => LONGLINK }
                        );

            unless( $longlink ) {
                $self->_error(  qq[Could not create 'LongLink' entry for ] .
                                qq[oversize file '] . $clone->full_path ."'" );
                return;
            };

            push @write_me, $longlink;
        }

        push @write_me, $clone;

        ### write the one, optionally 2 a::t::file objects to the handle
        for my $clone (@write_me) {

            ### if the file is a symlink, there are 2 options:
            ### either we leave the symlink intact, but then we don't write any
            ### data OR we follow the symlink, which means we actually make a
            ### copy. if we do the latter, we have to change the TYPE of the
            ### clone to 'FILE'
            my $link_ok =  $clone->is_symlink && $Archive::Tar::FOLLOW_SYMLINK;
            my $data_ok = !$clone->is_symlink && $clone->has_content;

            ### downgrade to a 'normal' file if it's a symlink we're going to
            ### treat as a regular file
            $clone->_downgrade_to_plainfile if $link_ok;

            ### get the header for this block
            my $header = $self->_format_tar_entry( $clone );
            unless( $header ) {
                $self->_error(q[Could not format header for: ] .
                                    $clone->full_path );
                return;
            }

            unless( print $handle $header ) {
                $self->_error(q[Could not write header for: ] .
                                    $clone->full_path);
                return;
            }

            if( $link_ok or $data_ok ) {
                unless( print $handle $clone->data ) {
                    $self->_error(q[Could not write data for: ] .
                                    $clone->full_path);
                    return;
                }

                ### pad the end of the clone if required ###
                print $handle TAR_PAD->( $clone->size ) if $clone->size % BLOCK
            }

        } ### done writing these entries
    }

    ### write the end markers ###
    print $handle TAR_END x 2 or
            return $self->_error( qq[Could not write tar end markers] );

    ### did you want it written to a file, or returned as a string? ###
    my $rv =  length($file) ? 1
                        : $HAS_PERLIO ? $dummy
                        : do { seek $handle, 0, 0; local $/; <$handle> };

    ### make sure to close the handle;
    close $handle;
    
    return $rv;
}

sub _format_tar_entry {
    my $self        = shift;
    my $entry       = shift or return;
    my $ext_prefix  = shift; $ext_prefix = '' unless defined $ext_prefix;
    my $no_prefix   = shift || 0;

    my $file    = $entry->name;
    my $prefix  = $entry->prefix; $prefix = '' unless defined $prefix;

    ### remove the prefix from the file name
    ### not sure if this is still neeeded --kane
    ### no it's not -- Archive::Tar::File->_new_from_file will take care of
    ### this for us. Even worse, this would break if we tried to add a file
    ### like x/x.
    #if( length $prefix ) {
    #    $file =~ s/^$match//;
    #}

    $prefix = File::Spec::Unix->catdir($ext_prefix, $prefix)
                if length $ext_prefix;

    ### not sure why this is... ###
    my $l = PREFIX_LENGTH; # is ambiguous otherwise...
    substr ($prefix, 0, -$l) = "" if length $prefix >= PREFIX_LENGTH;

    my $f1 = "%06o"; my $f2  = "%11o";

    ### this might be optimizable with a 'changed' flag in the file objects ###
    my $tar = pack (
                PACK,
                $file,

                (map { sprintf( $f1, $entry->$_() ) } qw[mode uid gid]),
                (map { sprintf( $f2, $entry->$_() ) } qw[size mtime]),

                "",  # checksum field - space padded a bit down

                (map { $entry->$_() }                 qw[type linkname magic]),

                $entry->version || TAR_VERSION,

                (map { $entry->$_() }                 qw[uname gname]),
                (map { sprintf( $f1, $entry->$_() ) } qw[devmajor devminor]),

                ($no_prefix ? '' : $prefix)
    );

    ### add the checksum ###
    substr($tar,148,7) = sprintf("%6o\0", unpack("%16C*",$tar));

    return $tar;
}

=head2 $tar->add_files( @filenamelist )

Takes a list of filenames and adds them to the in-memory archive.

The path to the file is automatically converted to a Unix like
equivalent for use in the archive, and, if on MacOS, the file's
modification time is converted from the MacOS epoch to the Unix epoch.
So tar archives created on MacOS with B<Archive::Tar> can be read
both with I<tar> on Unix and applications like I<suntar> or
I<Stuffit Expander> on MacOS.

Be aware that the file's type/creator and resource fork will be lost,
which is usually what you want in cross-platform archives.

Returns a list of C<Archive::Tar::File> objects that were just added.

=cut

sub add_files {
    my $self    = shift;
    my @files   = @_ or return;

    my @rv;
    for my $file ( @files ) {
        unless( -e $file || -l $file ) {
            $self->_error( qq[No such file: '$file'] );
            next;
        }

        my $obj = Archive::Tar::File->new( file => $file );
        unless( $obj ) {
            $self->_error( qq[Unable to add file: '$file'] );
            next;
        }

        push @rv, $obj;
    }

    push @{$self->{_data}}, @rv;

    return @rv;
}

=head2 $tar->add_data ( $filename, $data, [$opthashref] )

Takes a filename, a scalar full of data and optionally a reference to
a hash with specific options.

Will add a file to the in-memory archive, with name C<$filename> and
content C<$data>. Specific properties can be set using C<$opthashref>.
The following list of properties is supported: name, size, mtime
(last modified date), mode, uid, gid, linkname, uname, gname,
devmajor, devminor, prefix, type.  (On MacOS, the file's path and
modification times are converted to Unix equivalents.)

Valid values for the file type are the following constants defined in
Archive::Tar::Constants:

=over 4

=item FILE

Regular file.

=item HARDLINK

=item SYMLINK

Hard and symbolic ("soft") links; linkname should specify target.

=item CHARDEV

=item BLOCKDEV

Character and block devices. devmajor and devminor should specify the major
and minor device numbers.

=item DIR

Directory.

=item FIFO

FIFO (named pipe).

=item SOCKET

Socket.

=back

Returns the C<Archive::Tar::File> object that was just added, or
C<undef> on failure.

=cut

sub add_data {
    my $self    = shift;
    my ($file, $data, $opt) = @_;

    my $obj = Archive::Tar::File->new( data => $file, $data, $opt );
    unless( $obj ) {
        $self->_error( qq[Unable to add file: '$file'] );
        return;
    }

    push @{$self->{_data}}, $obj;

    return $obj;
}

=head2 $tar->error( [$BOOL] )

Returns the current errorstring (usually, the last error reported).
If a true value was specified, it will give the C<Carp::longmess>
equivalent of the error, in effect giving you a stacktrace.

For backwards compatibility, this error is also available as
C<$Archive::Tar::error> although it is much recommended you use the
method call instead.

=cut

{
    $error = '';
    my $longmess;

    sub _error {
        my $self    = shift;
        my $msg     = $error = shift;
        $longmess   = Carp::longmess($error);

        ### set Archive::Tar::WARN to 0 to disable printing
        ### of errors
        if( $WARN ) {
            carp $DEBUG ? $longmess : $msg;
        }

        return;
    }

    sub error {
        my $self = shift;
        return shift() ? $longmess : $error;
    }
}

=head2 $tar->setcwd( $cwd );

C<Archive::Tar> needs to know the current directory, and it will run
C<Cwd::cwd()> I<every> time it extracts a I<relative> entry from the 
tarfile and saves it in the file system. (As of version 1.30, however,
C<Archive::Tar> will use the speed optimization described below 
automatically, so it's only relevant if you're using C<extract_file()>).

Since C<Archive::Tar> doesn't change the current directory internally
while it is extracting the items in a tarball, all calls to C<Cwd::cwd()>
can be avoided if we can guarantee that the current directory doesn't
get changed externally.

To use this performance boost, set the current directory via

    use Cwd;
    $tar->setcwd( cwd() );

once before calling a function like C<extract_file> and
C<Archive::Tar> will use the current directory setting from then on
and won't call C<Cwd::cwd()> internally. 

To switch back to the default behaviour, use

    $tar->setcwd( undef );

and C<Archive::Tar> will call C<Cwd::cwd()> internally again.

If you're using C<Archive::Tar>'s C<exract()> method, C<setcwd()> will
be called for you.

=cut 

sub setcwd {
    my $self     = shift;
    my $cwd      = shift;

    $self->{cwd} = $cwd;
}

=head2 $bool = $tar->has_io_string

Returns true if we currently have C<IO::String> support loaded.

Either C<IO::String> or C<perlio> support is needed to support writing 
stringified archives. Currently, C<perlio> is the preferred method, if
available.

See the C<GLOBAL VARIABLES> section to see how to change this preference.

=cut

sub has_io_string { return $HAS_IO_STRING; }

=head2 $bool = $tar->has_perlio

Returns true if we currently have C<perlio> support loaded.

This requires C<perl-5.8> or higher, compiled with C<perlio> 

Either C<IO::String> or C<perlio> support is needed to support writing 
stringified archives. Currently, C<perlio> is the preferred method, if
available.

See the C<GLOBAL VARIABLES> section to see how to change this preference.

=cut

sub has_perlio { return $HAS_PERLIO; }


=head1 Class Methods

=head2 Archive::Tar->create_archive($file, $compression, @filelist)

Creates a tar file from the list of files provided.  The first
argument can either be the name of the tar file to create or a
reference to an open file handle (e.g. a GLOB reference).

The second argument specifies the level of compression to be used, if
any.  Compression of tar files requires the installation of the
IO::Zlib module.  Specific levels of compression may be
requested by passing a value between 2 and 9 as the second argument.
Any other value evaluating as true will result in the default
compression level being used.

Note that when you pass in a filehandle, the compression argument
is ignored, as all files are printed verbatim to your filehandle.
If you wish to enable compression with filehandles, use an
C<IO::Zlib> filehandle instead.

The remaining arguments list the files to be included in the tar file.
These files must all exist. Any files which don't exist or can't be
read are silently ignored.

If the archive creation fails for any reason, C<create_archive> will
return false. Please use the C<error> method to find the cause of the
failure.

Note that this method does not write C<on the fly> as it were; it
still reads all the files into memory before writing out the archive.
Consult the FAQ below if this is a problem.

=cut

sub create_archive {
    my $class = shift;

    my $file    = shift; return unless defined $file;
    my $gzip    = shift || 0;
    my @files   = @_;

    unless( @files ) {
        return $class->_error( qq[Cowardly refusing to create empty archive!] );
    }

    my $tar = $class->new;
    $tar->add_files( @files );
    return $tar->write( $file, $gzip );
}

=head2 Archive::Tar->list_archive ($file, $compressed, [\@properties])

Returns a list of the names of all the files in the archive.  The
first argument can either be the name of the tar file to list or a
reference to an open file handle (e.g. a GLOB reference).

If C<list_archive()> is passed an array reference as its third
argument it returns a list of hash references containing the requested
properties of each file.  The following list of properties is
supported: full_path, name, size, mtime (last modified date), mode, 
uid, gid, linkname, uname, gname, devmajor, devminor, prefix.

See C<Archive::Tar::File> for details about supported properties.

Passing an array reference containing only one element, 'name', is
special cased to return a list of names rather than a list of hash
references.

=cut

sub list_archive {
    my $class   = shift;
    my $file    = shift; return unless defined $file;
    my $gzip    = shift || 0;

    my $tar = $class->new($file, $gzip);
    return unless $tar;

    return $tar->list_files( @_ );
}

=head2 Archive::Tar->extract_archive ($file, $gzip)

Extracts the contents of the tar file.  The first argument can either
be the name of the tar file to create or a reference to an open file
handle (e.g. a GLOB reference).  All relative paths in the tar file will
be created underneath the current working directory.

C<extract_archive> will return a list of files it extracted.
If the archive extraction fails for any reason, C<extract_archive>
will return false.  Please use the C<error> method to find the cause
of the failure.

=cut

sub extract_archive {
    my $class   = shift;
    my $file    = shift; return unless defined $file;
    my $gzip    = shift || 0;

    my $tar = $class->new( ) or return;

    return $tar->read( $file, $gzip, { extract => 1 } );
}

=head2 Archive::Tar->can_handle_compressed_files

A simple checking routine, which will return true if C<Archive::Tar>
is able to uncompress compressed archives on the fly with C<IO::Zlib>,
or false if C<IO::Zlib> is not installed.

You can use this as a shortcut to determine whether C<Archive::Tar>
will do what you think before passing compressed archives to its
C<read> method.

=cut

sub can_handle_compressed_files { return ZLIB ? 1 : 0 }

sub no_string_support {
    croak("You have to install IO::String to support writing archives to strings");
}

1;

__END__

=head1 GLOBAL VARIABLES

=head2 $Archive::Tar::FOLLOW_SYMLINK

Set this variable to C<1> to make C<Archive::Tar> effectively make a
copy of the file when extracting. Default is C<0>, which
means the symlink stays intact. Of course, you will have to pack the
file linked to as well.

This option is checked when you write out the tarfile using C<write>
or C<create_archive>.

This works just like C</bin/tar>'s C<-h> option.

=head2 $Archive::Tar::CHOWN

By default, C<Archive::Tar> will try to C<chown> your files if it is
able to. In some cases, this may not be desired. In that case, set
this variable to C<0> to disable C<chown>-ing, even if it were
possible.

The default is C<1>.

=head2 $Archive::Tar::CHMOD

By default, C<Archive::Tar> will try to C<chmod> your files to
whatever mode was specified for the particular file in the archive.
In some cases, this may not be desired. In that case, set this
variable to C<0> to disable C<chmod>-ing.

The default is C<1>.

=head2 $Archive::Tar::DO_NOT_USE_PREFIX

By default, C<Archive::Tar> will try to put paths that are over 
100 characters in the C<prefix> field of your tar header, as
defined per POSIX-standard. However, some (older) tar programs 
do not implement this spec. To retain compatibility with these older 
or non-POSIX compliant versions, you can set the C<$DO_NOT_USE_PREFIX> 
variable to a true value, and C<Archive::Tar> will use an alternate 
way of dealing with paths over 100 characters by using the 
C<GNU Extended Header> feature.

Note that clients who do not support the C<GNU Extended Header>
feature will not be able to read these archives. Such clients include
tars on C<Solaris>, C<Irix> and C<AIX>.

The default is C<0>.

=head2 $Archive::Tar::DEBUG

Set this variable to C<1> to always get the C<Carp::longmess> output
of the warnings, instead of the regular C<carp>. This is the same
message you would get by doing:

    $tar->error(1);

Defaults to C<0>.

=head2 $Archive::Tar::WARN

Set this variable to C<0> if you do not want any warnings printed.
Personally I recommend against doing this, but people asked for the
option. Also, be advised that this is of course not threadsafe.

Defaults to C<1>.

=head2 $Archive::Tar::error

Holds the last reported error. Kept for historical reasons, but its
use is very much discouraged. Use the C<error()> method instead:

    warn $tar->error unless $tar->extract;

=head2 $Archive::Tar::INSECURE_EXTRACT_MODE

This variable indicates whether C<Archive::Tar> should allow
files to be extracted outside their current working directory.

Allowing this could have security implications, as a malicious
tar archive could alter or replace any file the extracting user
has permissions to. Therefor, the default is to not allow 
insecure extractions. 

If you trust the archive, or have other reasons to allow the 
archive to write files outside your current working directory, 
set this variable to C<true>.

Note that this is a backwards incompatible change from version
C<1.36> and before.

=head2 $Archive::Tar::HAS_PERLIO

This variable holds a boolean indicating if we currently have 
C<perlio> support loaded. This will be enabled for any perl
greater than C<5.8> compiled with C<perlio>. 

If you feel strongly about disabling it, set this variable to
C<false>. Note that you will then need C<IO::String> installed
to support writing stringified archives.

Don't change this variable unless you B<really> know what you're
doing.

=head2 $Archive::Tar::HAS_IO_STRING

This variable holds a boolean indicating if we currently have 
C<IO::String> support loaded. This will be enabled for any perl
that has a loadable C<IO::String> module.

If you feel strongly about disabling it, set this variable to
C<false>. Note that you will then need C<perlio> support from
your perl to be able to  write stringified archives.

Don't change this variable unless you B<really> know what you're
doing.

=head1 FAQ

=over 4

=item What's the minimum perl version required to run Archive::Tar?

You will need perl version 5.005_03 or newer.

=item Isn't Archive::Tar slow?

Yes it is. It's pure perl, so it's a lot slower then your C</bin/tar>
However, it's very portable. If speed is an issue, consider using
C</bin/tar> instead.

=item Isn't Archive::Tar heavier on memory than /bin/tar?

Yes it is, see previous answer. Since C<Compress::Zlib> and therefore
C<IO::Zlib> doesn't support C<seek> on their filehandles, there is little
choice but to read the archive into memory.
This is ok if you want to do in-memory manipulation of the archive.
If you just want to extract, use the C<extract_archive> class method
instead. It will optimize and write to disk immediately.

=item Can't you lazy-load data instead?

No, not easily. See previous question.

=item How much memory will an X kb tar file need?

Probably more than X kb, since it will all be read into memory. If
this is a problem, and you don't need to do in memory manipulation
of the archive, consider using C</bin/tar> instead.

=item What do you do with unsupported filetypes in an archive?

C<Unix> has a few filetypes that aren't supported on other platforms,
like C<Win32>. If we encounter a C<hardlink> or C<symlink> we'll just
try to make a copy of the original file, rather than throwing an error.

This does require you to read the entire archive in to memory first,
since otherwise we wouldn't know what data to fill the copy with.
(This means that you cannot use the class methods on archives that
have incompatible filetypes and still expect things to work).

For other filetypes, like C<chardevs> and C<blockdevs> we'll warn that
the extraction of this particular item didn't work.

=item I'm using WinZip, or some other non-POSIX client, and files are not being extracted properly!

By default, C<Archive::Tar> is in a completely POSIX-compatible
mode, which uses the POSIX-specification of C<tar> to store files.
For paths greather than 100 characters, this is done using the
C<POSIX header prefix>. Non-POSIX-compatible clients may not support
this part of the specification, and may only support the C<GNU Extended
Header> functionality. To facilitate those clients, you can set the
C<$Archive::Tar::DO_NOT_USE_PREFIX> variable to C<true>. See the 
C<GLOBAL VARIABLES> section for details on this variable.

Note that GNU tar earlier than version 1.14 does not cope well with
the C<POSIX header prefix>. If you use such a version, consider setting
the C<$Archive::Tar::DO_NOT_USE_PREFIX> variable to C<true>.

=item How do I extract only files that have property X from an archive?

Sometimes, you might not wish to extract a complete archive, just
the files that are relevant to you, based on some criteria.

You can do this by filtering a list of C<Archive::Tar::File> objects
based on your criteria. For example, to extract only files that have
the string C<foo> in their title, you would use:

    $tar->extract( 
        grep { $_->full_path =~ /foo/ } $tar->get_files
    ); 

This way, you can filter on any attribute of the files in the archive.
Consult the C<Archive::Tar::File> documentation on how to use these
objects.

=item How do I access .tar.Z files?

The C<Archive::Tar> module can optionally use C<Compress::Zlib> (via
the C<IO::Zlib> module) to access tar files that have been compressed
with C<gzip>. Unfortunately tar files compressed with the Unix C<compress>
utility cannot be read by C<Compress::Zlib> and so cannot be directly
accesses by C<Archive::Tar>.

If the C<uncompress> or C<gunzip> programs are available, you can use
one of these workarounds to read C<.tar.Z> files from C<Archive::Tar>

Firstly with C<uncompress>

    use Archive::Tar;

    open F, "uncompress -c $filename |";
    my $tar = Archive::Tar->new(*F);
    ...

and this with C<gunzip>

    use Archive::Tar;

    open F, "gunzip -c $filename |";
    my $tar = Archive::Tar->new(*F);
    ...

Similarly, if the C<compress> program is available, you can use this to
write a C<.tar.Z> file

    use Archive::Tar;
    use IO::File;

    my $fh = new IO::File "| compress -c >$filename";
    my $tar = Archive::Tar->new();
    ...
    $tar->write($fh);
    $fh->close ;

=item How do I handle Unicode strings?

C<Archive::Tar> uses byte semantics for any files it reads from or writes
to disk. This is not a problem if you only deal with files and never
look at their content or work solely with byte strings. But if you use
Unicode strings with character semantics, some additional steps need
to be taken.

For example, if you add a Unicode string like

    # Problem
    $tar->add_data('file.txt', "Euro: \x{20AC}");

then there will be a problem later when the tarfile gets written out
to disk via C<$tar->write()>:

    Wide character in print at .../Archive/Tar.pm line 1014.

The data was added as a Unicode string and when writing it out to disk,
the C<:utf8> line discipline wasn't set by C<Archive::Tar>, so Perl
tried to convert the string to ISO-8859 and failed. The written file
now contains garbage.

For this reason, Unicode strings need to be converted to UTF-8-encoded
bytestrings before they are handed off to C<add_data()>:

    use Encode;
    my $data = "Accented character: \x{20AC}";
    $data = encode('utf8', $data);

    $tar->add_data('file.txt', $data);

A opposite problem occurs if you extract a UTF8-encoded file from a 
tarball. Using C<get_content()> on the C<Archive::Tar::File> object
will return its content as a bytestring, not as a Unicode string.

If you want it to be a Unicode string (because you want character
semantics with operations like regular expression matching), you need
to decode the UTF8-encoded content and have Perl convert it into 
a Unicode string:

    use Encode;
    my $data = $tar->get_content();
    
    # Make it a Unicode string
    $data = decode('utf8', $data);

There is no easy way to provide this functionality in C<Archive::Tar>, 
because a tarball can contain many files, and each of which could be
encoded in a different way.

=back

=head1 TODO

=over 4

=item Check if passed in handles are open for read/write

Currently I don't know of any portable pure perl way to do this.
Suggestions welcome.

=item Allow archives to be passed in as string

Currently, we only allow opened filehandles or filenames, but
not strings. The internals would need some reworking to facilitate
stringified archives.

=item Facilitate processing an opened filehandle of a compressed archive

Currently, we only support this if the filehandle is an IO::Zlib object.
Environments, like apache, will present you with an opened filehandle
to an uploaded file, which might be a compressed archive.

=back

=head1 SEE ALSO

=over 4

=item The GNU tar specification

C<http://www.gnu.org/software/tar/manual/tar.html>

=item The PAX format specication

The specifcation which tar derives from; C< http://www.opengroup.org/onlinepubs/007904975/utilities/pax.html>

=item A comparison of GNU and POSIX tar standards; C<http://www.delorie.com/gnu/docs/tar/tar_114.html>

=item GNU tar intends to switch to POSIX compatibility

GNU Tar authors have expressed their intention to become completely
POSIX-compatible; C<http://www.gnu.org/software/tar/manual/html_node/Formats.html>

=item A Comparison between various tar implementations

Lists known issues and incompatibilities; C<http://gd.tuwien.ac.at/utils/archivers/star/README.otherbugs>

=back

=head1 AUTHOR

This module by Jos Boumans E<lt>kane@cpan.orgE<gt>.

Please reports bugs to E<lt>bug-archive-tar@rt.cpan.orgE<gt>.

=head1 ACKNOWLEDGEMENTS

Thanks to Sean Burke, Chris Nandor, Chip Salzenberg, Tim Heaney and
especially Andrew Savige for their help and suggestions.

=head1 COPYRIGHT

This module is copyright (c) 2002 - 2007 Jos Boumans 
E<lt>kane@cpan.orgE<gt>. All rights reserved.

This library is free software; you may redistribute and/or modify 
it under the same terms as Perl itself.

=cut
