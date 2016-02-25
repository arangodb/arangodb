# -*- Mode: cperl; coding: utf-8; cperl-indent-level: 4 -*-
package CPAN::Tarzip;
use strict;
use vars qw($VERSION @ISA $BUGHUNTING);
use CPAN::Debug;
use File::Basename ();
$VERSION = sprintf "%.6f", substr(q$Rev: 2213 $,4)/1000000 + 5.4;
# module is internal to CPAN.pm

@ISA = qw(CPAN::Debug);
$BUGHUNTING ||= 0; # released code must have turned off

# it's ok if file doesn't exist, it just matters if it is .gz or .bz2
sub new {
    my($class,$file) = @_;
    $CPAN::Frontend->mydie("CPAN::Tarzip->new called without arg") unless defined $file;
    if (0) {
        # nonono, we get e.g. 01mailrc.txt uncompressed if only wget is available
        $CPAN::Frontend->mydie("file[$file] doesn't match /\\.(bz2|gz|zip|tgz)\$/")
            unless $file =~ /\.(bz2|gz|zip|tgz)$/i;
    }
    my $me = { FILE => $file };
    if (0) {
    } elsif ($file =~ /\.bz2$/i) {
        unless ($me->{UNGZIPPRG} = $CPAN::Config->{bzip2}) {
            my $bzip2;
            if ($CPAN::META->has_inst("File::Which")) {
                $bzip2 = File::Which::which("bzip2");
            }
            if ($bzip2) {
                $me->{UNGZIPPRG} = $bzip2 || "bzip2";
            } else {
                $CPAN::Frontend->mydie(qq{
CPAN.pm needs the external program bzip2 in order to handle '$file'.
Please install it now and run 'o conf init' to register it as external
program.
});
            }
        }
    } else {
        # yes, we let gzip figure it out in *any* other case
        $me->{UNGZIPPRG} = $CPAN::Config->{gzip} || "gzip";
    }
    bless $me, $class;
}

sub gzip {
    my($self,$read) = @_;
    my $write = $self->{FILE};
    if ($CPAN::META->has_inst("Compress::Zlib")) {
        my($buffer,$fhw);
        $fhw = FileHandle->new($read)
            or $CPAN::Frontend->mydie("Could not open $read: $!");
        my $cwd = `pwd`;
        my $gz = Compress::Zlib::gzopen($write, "wb")
            or $CPAN::Frontend->mydie("Cannot gzopen $write: $! (pwd is $cwd)\n");
        $gz->gzwrite($buffer)
            while read($fhw,$buffer,4096) > 0 ;
        $gz->gzclose() ;
        $fhw->close;
        return 1;
    } else {
        my $command = CPAN::HandleConfig->safe_quote($self->{UNGZIPPRG});
        system(qq{$command -c "$read" > "$write"})==0;
    }
}


sub gunzip {
    my($self,$write) = @_;
    my $read = $self->{FILE};
    if ($CPAN::META->has_inst("Compress::Zlib")) {
        my($buffer,$fhw);
        $fhw = FileHandle->new(">$write")
            or $CPAN::Frontend->mydie("Could not open >$write: $!");
        my $gz = Compress::Zlib::gzopen($read, "rb")
            or $CPAN::Frontend->mydie("Cannot gzopen $read: $!\n");
        $fhw->print($buffer)
        while $gz->gzread($buffer) > 0 ;
        $CPAN::Frontend->mydie("Error reading from $read: $!\n")
            if $gz->gzerror != Compress::Zlib::Z_STREAM_END();
        $gz->gzclose() ;
        $fhw->close;
        return 1;
    } else {
        my $command = CPAN::HandleConfig->safe_quote($self->{UNGZIPPRG});
        system(qq{$command -dc "$read" > "$write"})==0;
    }
}


sub gtest {
    my($self) = @_;
    return $self->{GTEST} if exists $self->{GTEST};
    defined $self->{FILE} or $CPAN::Frontend->mydie("gtest called but no FILE specified");
    my $read = $self->{FILE};
    my $success;
    # After I had reread the documentation in zlib.h, I discovered that
    # uncompressed files do not lead to an gzerror (anymore?).
    if ( $CPAN::META->has_inst("Compress::Zlib") ) {
        my($buffer,$len);
        $len = 0;
        my $gz = Compress::Zlib::gzopen($read, "rb")
            or $CPAN::Frontend->mydie(sprintf("Cannot gzopen %s: %s\n",
                                              $read,
                                              $Compress::Zlib::gzerrno));
        while ($gz->gzread($buffer) > 0 ) {
            $len += length($buffer);
            $buffer = "";
        }
        my $err = $gz->gzerror;
        $success = ! $err || $err == Compress::Zlib::Z_STREAM_END();
        if ($len == -s $read) {
            $success = 0;
            CPAN->debug("hit an uncompressed file") if $CPAN::DEBUG;
        }
        $gz->gzclose();
        CPAN->debug("err[$err]success[$success]") if $CPAN::DEBUG;
    } else {
        my $command = CPAN::HandleConfig->safe_quote($self->{UNGZIPPRG});
        $success = 0==system(qq{$command -qdt "$read"});
    }
    return $self->{GTEST} = $success;
}


sub TIEHANDLE {
    my($class,$file) = @_;
    my $ret;
    $class->debug("file[$file]");
    my $self = $class->new($file);
    if (0) {
    } elsif (!$self->gtest) {
        my $fh = FileHandle->new($file)
            or $CPAN::Frontend->mydie("Could not open file[$file]: $!");
        binmode $fh;
        $self->{FH} = $fh;
        $class->debug("via uncompressed FH");
    } elsif ($CPAN::META->has_inst("Compress::Zlib")) {
        my $gz = Compress::Zlib::gzopen($file,"rb") or
            $CPAN::Frontend->mydie("Could not gzopen $file");
        $self->{GZ} = $gz;
        $class->debug("via Compress::Zlib");
    } else {
        my $gzip = CPAN::HandleConfig->safe_quote($self->{UNGZIPPRG});
        my $pipe = "$gzip -dc $file |";
        my $fh = FileHandle->new($pipe) or $CPAN::Frontend->mydie("Could not pipe[$pipe]: $!");
        binmode $fh;
        $self->{FH} = $fh;
        $class->debug("via external gzip");
    }
    $self;
}


sub READLINE {
    my($self) = @_;
    if (exists $self->{GZ}) {
        my $gz = $self->{GZ};
        my($line,$bytesread);
        $bytesread = $gz->gzreadline($line);
        return undef if $bytesread <= 0;
        return $line;
    } else {
        my $fh = $self->{FH};
        return scalar <$fh>;
    }
}


sub READ {
    my($self,$ref,$length,$offset) = @_;
    $CPAN::Frontend->mydie("read with offset not implemented") if defined $offset;
    if (exists $self->{GZ}) {
        my $gz = $self->{GZ};
        my $byteread = $gz->gzread($$ref,$length);# 30eaf79e8b446ef52464b5422da328a8
        return $byteread;
    } else {
        my $fh = $self->{FH};
        return read($fh,$$ref,$length);
    }
}


sub DESTROY {
    my($self) = @_;
    if (exists $self->{GZ}) {
        my $gz = $self->{GZ};
        $gz->gzclose() if defined $gz; # hard to say if it is allowed
                                       # to be undef ever. AK, 2000-09
    } else {
        my $fh = $self->{FH};
        $fh->close if defined $fh;
    }
    undef $self;
}


sub untar {
    my($self) = @_;
    my $file = $self->{FILE};
    my($prefer) = 0;

    if (0) { # makes changing order easier
    } elsif ($BUGHUNTING) {
        $prefer=2;
    } elsif (MM->maybe_command($self->{UNGZIPPRG})
             &&
             MM->maybe_command($CPAN::Config->{tar})) {
        # should be default until Archive::Tar handles bzip2
        $prefer = 1;
    } elsif (
             $CPAN::META->has_usable("Archive::Tar")
             &&
             $CPAN::META->has_inst("Compress::Zlib") ) {
        $prefer = 2;
    } else {
        $CPAN::Frontend->mydie(qq{
CPAN.pm needs either the external programs tar, gzip and bzip2
installed. Can't continue.
});
    }
    my $tar_verb = "v";
    if (defined $CPAN::Config->{tar_verbosity}) {
        $tar_verb = $CPAN::Config->{tar_verbosity} eq "none" ? "" :
            $CPAN::Config->{tar_verbosity};
    }
    if ($prefer==1) { # 1 => external gzip+tar
        my($system);
        my $is_compressed = $self->gtest();
        my $tarcommand = CPAN::HandleConfig->safe_quote($CPAN::Config->{tar}) || "tar";
        if ($is_compressed) {
            my $command = CPAN::HandleConfig->safe_quote($self->{UNGZIPPRG});
            $system = qq{$command -dc }.
                qq{< "$file" | $tarcommand x${tar_verb}f -};
        } else {
            $system = qq{$tarcommand x${tar_verb}f "$file"};
        }
        if (system($system) != 0) {
            # people find the most curious tar binaries that cannot handle
            # pipes
            if ($is_compressed) {
                (my $ungzf = $file) =~ s/\.gz(?!\n)\Z//;
                $ungzf = File::Basename::basename($ungzf);
                my $ct = CPAN::Tarzip->new($file);
                if ($ct->gunzip($ungzf)) {
                    $CPAN::Frontend->myprint(qq{Uncompressed $file successfully\n});
                } else {
                    $CPAN::Frontend->mydie(qq{Couldn\'t uncompress $file\n});
                }
                $file = $ungzf;
            }
            $system = qq{$tarcommand x${tar_verb}f "$file"};
            $CPAN::Frontend->myprint(qq{Using Tar:$system:\n});
            if (system($system)==0) {
                $CPAN::Frontend->myprint(qq{Untarred $file successfully\n});
            } else {
                $CPAN::Frontend->mydie(qq{Couldn\'t untar $file\n});
            }
            return 1;
        } else {
            return 1;
        }
    } elsif ($prefer==2) { # 2 => modules
        unless ($CPAN::META->has_usable("Archive::Tar")) {
            $CPAN::Frontend->mydie("Archive::Tar not installed, please install it to continue");
        }
        my $tar = Archive::Tar->new($file,1);
        my $af; # archive file
        my @af;
        if ($BUGHUNTING) {
            # RCS 1.337 had this code, it turned out unacceptable slow but
            # it revealed a bug in Archive::Tar. Code is only here to hunt
            # the bug again. It should never be enabled in published code.
            # GDGraph3d-0.53 was an interesting case according to Larry
            # Virden.
            warn(">>>Bughunting code enabled<<< " x 20);
            for $af ($tar->list_files) {
                if ($af =~ m!^(/|\.\./)!) {
                    $CPAN::Frontend->mydie("ALERT: Archive contains ".
                                           "illegal member [$af]");
                }
                $CPAN::Frontend->myprint("$af\n");
                $tar->extract($af); # slow but effective for finding the bug
                return if $CPAN::Signal;
            }
        } else {
            for $af ($tar->list_files) {
                if ($af =~ m!^(/|\.\./)!) {
                    $CPAN::Frontend->mydie("ALERT: Archive contains ".
                                           "illegal member [$af]");
                }
                if ($tar_verb eq "v" || $tar_verb eq "vv") {
                    $CPAN::Frontend->myprint("$af\n");
                }
                push @af, $af;
                return if $CPAN::Signal;
            }
            $tar->extract(@af) or
                $CPAN::Frontend->mydie("Could not untar with Archive::Tar.");
        }

        Mac::BuildTools::convert_files([$tar->list_files], 1)
            if ($^O eq 'MacOS');

        return 1;
    }
}

sub unzip {
    my($self) = @_;
    my $file = $self->{FILE};
    if ($CPAN::META->has_inst("Archive::Zip")) {
        # blueprint of the code from Archive::Zip::Tree::extractTree();
        my $zip = Archive::Zip->new();
        my $status;
        $status = $zip->read($file);
        $CPAN::Frontend->mydie("Read of file[$file] failed\n")
            if $status != Archive::Zip::AZ_OK();
        $CPAN::META->debug("Successfully read file[$file]") if $CPAN::DEBUG;
        my @members = $zip->members();
        for my $member ( @members ) {
            my $af = $member->fileName();
            if ($af =~ m!^(/|\.\./)!) {
                $CPAN::Frontend->mydie("ALERT: Archive contains ".
                                       "illegal member [$af]");
            }
            $status = $member->extractToFileNamed( $af );
            $CPAN::META->debug("af[$af]status[$status]") if $CPAN::DEBUG;
            $CPAN::Frontend->mydie("Extracting of file[$af] from zipfile[$file] failed\n") if
                $status != Archive::Zip::AZ_OK();
            return if $CPAN::Signal;
        }
        return 1;
    } else {
        my $unzip = $CPAN::Config->{unzip} or
            $CPAN::Frontend->mydie("Cannot unzip, no unzip program available");
        my @system = ($unzip, $file);
        return system(@system) == 0;
    }
}

1;

__END__

=head1 LICENSE

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=cut
