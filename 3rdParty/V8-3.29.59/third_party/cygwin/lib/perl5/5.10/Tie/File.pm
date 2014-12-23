
package Tie::File;
require 5.005;
use Carp ':DEFAULT', 'confess';
use POSIX 'SEEK_SET';
use Fcntl 'O_CREAT', 'O_RDWR', 'LOCK_EX', 'LOCK_SH', 'O_WRONLY', 'O_RDONLY';
sub O_ACCMODE () { O_RDONLY | O_RDWR | O_WRONLY }


$VERSION = "0.97_02";
my $DEFAULT_MEMORY_SIZE = 1<<21;    # 2 megabytes
my $DEFAULT_AUTODEFER_THRESHHOLD = 3; # 3 records
my $DEFAULT_AUTODEFER_FILELEN_THRESHHOLD = 65536; # 16 disk blocksful

my %good_opt = map {$_ => 1, "-$_" => 1}
                 qw(memory dw_size mode recsep discipline 
                    autodefer autochomp autodefer_threshhold concurrent);

sub TIEARRAY {
  if (@_ % 2 != 0) {
    croak "usage: tie \@array, $_[0], filename, [option => value]...";
  }
  my ($pack, $file, %opts) = @_;

  # transform '-foo' keys into 'foo' keys
  for my $key (keys %opts) {
    unless ($good_opt{$key}) {
      croak("$pack: Unrecognized option '$key'\n");
    }
    my $okey = $key;
    if ($key =~ s/^-+//) {
      $opts{$key} = delete $opts{$okey};
    }
  }

  if ($opts{concurrent}) {
    croak("$pack: concurrent access not supported yet\n");
  }

  unless (defined $opts{memory}) {
    # default is the larger of the default cache size and the 
    # deferred-write buffer size (if specified)
    $opts{memory} = $DEFAULT_MEMORY_SIZE;
    $opts{memory} = $opts{dw_size}
      if defined $opts{dw_size} && $opts{dw_size} > $DEFAULT_MEMORY_SIZE;
    # Dora Winifred Read
  }
  $opts{dw_size} = $opts{memory} unless defined $opts{dw_size};
  if ($opts{dw_size} > $opts{memory}) {
      croak("$pack: dw_size may not be larger than total memory allocation\n");
  }
  # are we in deferred-write mode?
  $opts{defer} = 0 unless defined $opts{defer};
  $opts{deferred} = {};         # no records are presently deferred
  $opts{deferred_s} = 0;        # count of total bytes in ->{deferred}
  $opts{deferred_max} = -1;     # empty

  # What's a good way to arrange that this class can be overridden?
  $opts{cache} = Tie::File::Cache->new($opts{memory});

  # autodeferment is enabled by default
  $opts{autodefer} = 1 unless defined $opts{autodefer};
  $opts{autodeferring} = 0;     # but is not initially active
  $opts{ad_history} = [];
  $opts{autodefer_threshhold} = $DEFAULT_AUTODEFER_THRESHHOLD
    unless defined $opts{autodefer_threshhold};
  $opts{autodefer_filelen_threshhold} = $DEFAULT_AUTODEFER_FILELEN_THRESHHOLD
    unless defined $opts{autodefer_filelen_threshhold};

  $opts{offsets} = [0];
  $opts{filename} = $file;
  unless (defined $opts{recsep}) { 
    $opts{recsep} = _default_recsep();
  }
  $opts{recseplen} = length($opts{recsep});
  if ($opts{recseplen} == 0) {
    croak "Empty record separator not supported by $pack";
  }

  $opts{autochomp} = 1 unless defined $opts{autochomp};

  $opts{mode} = O_CREAT|O_RDWR unless defined $opts{mode};
  $opts{rdonly} = (($opts{mode} & O_ACCMODE) == O_RDONLY);
  $opts{sawlastrec} = undef;

  my $fh;

  if (UNIVERSAL::isa($file, 'GLOB')) {
    # We use 1 here on the theory that some systems 
    # may not indicate failure if we use 0.
    # MSWin32 does not indicate failure with 0, but I don't know if
    # it will indicate failure with 1 or not.
    unless (seek $file, 1, SEEK_SET) {
      croak "$pack: your filehandle does not appear to be seekable";
    }
    seek $file, 0, SEEK_SET;    # put it back
    $fh = $file;                # setting binmode is the user's problem
  } elsif (ref $file) {
    croak "usage: tie \@array, $pack, filename, [option => value]...";
  } else {
    # $fh = \do { local *FH };  # XXX this is buggy
    if ($] < 5.006) {
	# perl 5.005 and earlier don't autovivify filehandles
	require Symbol;
	$fh = Symbol::gensym();
    }
    sysopen $fh, $file, $opts{mode}, 0666 or return;
    binmode $fh;
    ++$opts{ourfh};
  }
  { my $ofh = select $fh; $| = 1; select $ofh } # autoflush on write
  if (defined $opts{discipline} && $] >= 5.006) {
    # This avoids a compile-time warning under 5.005
    eval 'binmode($fh, $opts{discipline})';
    croak $@ if $@ =~ /unknown discipline/i;
    die if $@;
  }
  $opts{fh} = $fh;

  bless \%opts => $pack;
}

sub FETCH {
  my ($self, $n) = @_;
  my $rec;

  # check the defer buffer
  $rec = $self->{deferred}{$n} if exists $self->{deferred}{$n};
  $rec = $self->_fetch($n) unless defined $rec;

  # inlined _chomp1
  substr($rec, - $self->{recseplen}) = ""
    if defined $rec && $self->{autochomp};
  $rec;
}

# Chomp many records in-place; return nothing useful
sub _chomp {
  my $self = shift;
  return unless $self->{autochomp};
  if ($self->{autochomp}) {
    for (@_) {
      next unless defined;
      substr($_, - $self->{recseplen}) = "";
    }
  }
}

# Chomp one record in-place; return modified record
sub _chomp1 {
  my ($self, $rec) = @_;
  return $rec unless $self->{autochomp};
  return unless defined $rec;
  substr($rec, - $self->{recseplen}) = "";
  $rec;
}

sub _fetch {
  my ($self, $n) = @_;

  # check the record cache
  { my $cached = $self->{cache}->lookup($n);
    return $cached if defined $cached;
  }

  if ($#{$self->{offsets}} < $n) {
    return if $self->{eof};  # request for record beyond end of file
    my $o = $self->_fill_offsets_to($n);
    # If it's still undefined, there is no such record, so return 'undef'
    return unless defined $o;
  }

  my $fh = $self->{FH};
  $self->_seek($n);             # we can do this now that offsets is populated
  my $rec = $self->_read_record;

# If we happen to have just read the first record, check to see if
# the length of the record matches what 'tell' says.  If not, Tie::File
# won't work, and should drop dead.
#
#  if ($n == 0 && defined($rec) && tell($self->{fh}) != length($rec)) {
#    if (defined $self->{discipline}) {
#      croak "I/O discipline $self->{discipline} not supported";
#    } else {
#      croak "File encoding not supported";
#    }
#  }

  $self->{cache}->insert($n, $rec) if defined $rec && not $self->{flushing};
  $rec;
}

sub STORE {
  my ($self, $n, $rec) = @_;
  die "STORE called from _check_integrity!" if $DIAGNOSTIC;

  $self->_fixrecs($rec);

  if ($self->{autodefer}) {
    $self->_annotate_ad_history($n);
  }

  return $self->_store_deferred($n, $rec) if $self->_is_deferring;


  # We need this to decide whether the new record will fit
  # It incidentally populates the offsets table 
  # Note we have to do this before we alter the cache
  # 20020324 Wait, but this DOES alter the cache.  TODO BUG?
  my $oldrec = $self->_fetch($n);

  if (not defined $oldrec) {
    # We're storing a record beyond the end of the file
    $self->_extend_file_to($n+1);
    $oldrec = $self->{recsep};
  }
#  return if $oldrec eq $rec;    # don't bother
  my $len_diff = length($rec) - length($oldrec);

  # length($oldrec) here is not consistent with text mode  TODO XXX BUG
  $self->_mtwrite($rec, $self->{offsets}[$n], length($oldrec));
  $self->_oadjust([$n, 1, $rec]);
  $self->{cache}->update($n, $rec);
}

sub _store_deferred {
  my ($self, $n, $rec) = @_;
  $self->{cache}->remove($n);
  my $old_deferred = $self->{deferred}{$n};

  if (defined $self->{deferred_max} && $n > $self->{deferred_max}) {
    $self->{deferred_max} = $n;
  }
  $self->{deferred}{$n} = $rec;

  my $len_diff = length($rec);
  $len_diff -= length($old_deferred) if defined $old_deferred;
  $self->{deferred_s} += $len_diff;
  $self->{cache}->adj_limit(-$len_diff);
  if ($self->{deferred_s} > $self->{dw_size}) {
    $self->_flush;
  } elsif ($self->_cache_too_full) {
    $self->_cache_flush;
  }
}

# Remove a single record from the deferred-write buffer without writing it
# The record need not be present
sub _delete_deferred {
  my ($self, $n) = @_;
  my $rec = delete $self->{deferred}{$n};
  return unless defined $rec;

  if (defined $self->{deferred_max} 
      && $n == $self->{deferred_max}) {
    undef $self->{deferred_max};
  }

  $self->{deferred_s} -= length $rec;
  $self->{cache}->adj_limit(length $rec);
}

sub FETCHSIZE {
  my $self = shift;
  my $n = $self->{eof} ? $#{$self->{offsets}} : $self->_fill_offsets;

  my $top_deferred = $self->_defer_max;
  $n = $top_deferred+1 if defined $top_deferred && $n < $top_deferred+1;
  $n;
}

sub STORESIZE {
  my ($self, $len) = @_;

  if ($self->{autodefer}) {
    $self->_annotate_ad_history('STORESIZE');
  }

  my $olen = $self->FETCHSIZE;
  return if $len == $olen;      # Woo-hoo!

  # file gets longer
  if ($len > $olen) {
    if ($self->_is_deferring) {
      for ($olen .. $len-1) {
        $self->_store_deferred($_, $self->{recsep});
      }
    } else {
      $self->_extend_file_to($len);
    }
    return;
  }

  # file gets shorter
  if ($self->_is_deferring) {
    # TODO maybe replace this with map-plus-assignment?
    for (grep $_ >= $len, keys %{$self->{deferred}}) {
      $self->_delete_deferred($_);
    }
    $self->{deferred_max} = $len-1;
  }

  $self->_seek($len);
  $self->_chop_file;
  $#{$self->{offsets}} = $len;
#  $self->{offsets}[0] = 0;      # in case we just chopped this

  $self->{cache}->remove(grep $_ >= $len, $self->{cache}->ckeys);
}

### OPTIMIZE ME
### It should not be necessary to do FETCHSIZE
### Just seek to the end of the file.
sub PUSH {
  my $self = shift;
  $self->SPLICE($self->FETCHSIZE, scalar(@_), @_);

  # No need to return:
  #  $self->FETCHSIZE;  # because av.c takes care of this for me
}

sub POP {
  my $self = shift;
  my $size = $self->FETCHSIZE;
  return if $size == 0;
#  print STDERR "# POPPITY POP POP POP\n";
  scalar $self->SPLICE($size-1, 1);
}

sub SHIFT {
  my $self = shift;
  scalar $self->SPLICE(0, 1);
}

sub UNSHIFT {
  my $self = shift;
  $self->SPLICE(0, 0, @_);
  # $self->FETCHSIZE; # av.c takes care of this for me
}

sub CLEAR {
  my $self = shift;

  if ($self->{autodefer}) {
    $self->_annotate_ad_history('CLEAR');
  }

  $self->_seekb(0);
  $self->_chop_file;
    $self->{cache}->set_limit($self->{memory});
    $self->{cache}->empty;
  @{$self->{offsets}} = (0);
  %{$self->{deferred}}= ();
    $self->{deferred_s} = 0;
    $self->{deferred_max} = -1;
}

sub EXTEND {
  my ($self, $n) = @_;

  # No need to pre-extend anything in this case
  return if $self->_is_deferring;

  $self->_fill_offsets_to($n);
  $self->_extend_file_to($n);
}

sub DELETE {
  my ($self, $n) = @_;

  if ($self->{autodefer}) {
    $self->_annotate_ad_history('DELETE');
  }

  my $lastrec = $self->FETCHSIZE-1;
  my $rec = $self->FETCH($n);
  $self->_delete_deferred($n) if $self->_is_deferring;
  if ($n == $lastrec) {
    $self->_seek($n);
    $self->_chop_file;
    $#{$self->{offsets}}--;
    $self->{cache}->remove($n);
    # perhaps in this case I should also remove trailing null records?
    # 20020316
    # Note that delete @a[-3..-1] deletes the records in the wrong order,
    # so we only chop the very last one out of the file.  We could repair this
    # by tracking deleted records inside the object.
  } elsif ($n < $lastrec) {
    $self->STORE($n, "");
  }
  $rec;
}

sub EXISTS {
  my ($self, $n) = @_;
  return 1 if exists $self->{deferred}{$n};
  $n < $self->FETCHSIZE;
}

sub SPLICE {
  my $self = shift;

  if ($self->{autodefer}) {
    $self->_annotate_ad_history('SPLICE');
  }

  $self->_flush if $self->_is_deferring; # move this up?
  if (wantarray) {
    $self->_chomp(my @a = $self->_splice(@_));
    @a;
  } else {
    $self->_chomp1(scalar $self->_splice(@_));
  }
}

sub DESTROY {
  my $self = shift;
  $self->flush if $self->_is_deferring;
  $self->{cache}->delink if defined $self->{cache}; # break circular link
  if ($self->{fh} and $self->{ourfh}) {
      delete $self->{ourfh};
      close delete $self->{fh};
  }
}

sub _splice {
  my ($self, $pos, $nrecs, @data) = @_;
  my @result;

  $pos = 0 unless defined $pos;

  # Deal with negative and other out-of-range positions
  # Also set default for $nrecs 
  {
    my $oldsize = $self->FETCHSIZE;
    $nrecs = $oldsize unless defined $nrecs;
    my $oldpos = $pos;

    if ($pos < 0) {
      $pos += $oldsize;
      if ($pos < 0) {
        croak "Modification of non-creatable array value attempted, subscript $oldpos";
      }
    }

    if ($pos > $oldsize) {
      return unless @data;
      $pos = $oldsize;          # This is what perl does for normal arrays
    }

    # The manual is very unclear here
    if ($nrecs < 0) {
      $nrecs = $oldsize - $pos + $nrecs;
      $nrecs = 0 if $nrecs < 0;
    }

    # nrecs is too big---it really means "until the end"
    # 20030507
    if ($nrecs + $pos > $oldsize) {
      $nrecs = $oldsize - $pos;
    }
  }

  $self->_fixrecs(@data);
  my $data = join '', @data;
  my $datalen = length $data;
  my $oldlen = 0;

  # compute length of data being removed
  for ($pos .. $pos+$nrecs-1) {
    last unless defined $self->_fill_offsets_to($_);
    my $rec = $self->_fetch($_);
    last unless defined $rec;
    push @result, $rec;

    # Why don't we just use length($rec) here?
    # Because that record might have come from the cache.  _splice
    # might have been called to flush out the deferred-write records,
    # and in this case length($rec) is the length of the record to be
    # *written*, not the length of the actual record in the file.  But
    # the offsets are still true. 20020322
    $oldlen += $self->{offsets}[$_+1] - $self->{offsets}[$_]
      if defined $self->{offsets}[$_+1];
  }
  $self->_fill_offsets_to($pos+$nrecs);

  # Modify the file
  $self->_mtwrite($data, $self->{offsets}[$pos], $oldlen);
  # Adjust the offsets table
  $self->_oadjust([$pos, $nrecs, @data]);

  { # Take this read cache stuff out into a separate function
    # You made a half-attempt to put it into _oadjust.  
    # Finish something like that up eventually.
    # STORE also needs to do something similarish

    # update the read cache, part 1
    # modified records
    for ($pos .. $pos+$nrecs-1) {
      my $new = $data[$_-$pos];
      if (defined $new) {
        $self->{cache}->update($_, $new);
      } else {
        $self->{cache}->remove($_);
      }
    }
    
    # update the read cache, part 2
    # moved records - records past the site of the change
    # need to be renumbered
    # Maybe merge this with the previous block?
    {
      my @oldkeys = grep $_ >= $pos + $nrecs, $self->{cache}->ckeys;
      my @newkeys = map $_-$nrecs+@data, @oldkeys;
      $self->{cache}->rekey(\@oldkeys, \@newkeys);
    }

    # Now there might be too much data in the cache, if we spliced out
    # some short records and spliced in some long ones.  If so, flush
    # the cache.
    $self->_cache_flush;
  }

  # Yes, the return value of 'splice' *is* actually this complicated
  wantarray ? @result : @result ? $result[-1] : undef;
}


# write data into the file
# $data is the data to be written.
# it should be written at position $pos, and should overwrite
# exactly $len of the following bytes.  
# Note that if length($data) > $len, the subsequent bytes will have to 
# be moved up, and if length($data) < $len, they will have to
# be moved down
sub _twrite {
  my ($self, $data, $pos, $len) = @_;

  unless (defined $pos) {
    die "\$pos was undefined in _twrite";
  }

  my $len_diff = length($data) - $len;

  if ($len_diff == 0) {          # Woo-hoo!
    my $fh = $self->{fh};
    $self->_seekb($pos);
    $self->_write_record($data);
    return;                     # well, that was easy.
  }

  # the two records are of different lengths
  # our strategy here: rewrite the tail of the file,
  # reading ahead one buffer at a time
  # $bufsize is required to be at least as large as the data we're overwriting
  my $bufsize = _bufsize($len_diff);
  my ($writepos, $readpos) = ($pos, $pos+$len);
  my $next_block;
  my $more_data;

  # Seems like there ought to be a way to avoid the repeated code
  # and the special case here.  The read(1) is also a little weird.
  # Think about this.
  do {
    $self->_seekb($readpos);
    my $br = read $self->{fh}, $next_block, $bufsize;
    $more_data = read $self->{fh}, my($dummy), 1;
    $self->_seekb($writepos);
    $self->_write_record($data);
    $readpos += $br;
    $writepos += length $data;
    $data = $next_block;
  } while $more_data;
  $self->_seekb($writepos);
  $self->_write_record($next_block);

  # There might be leftover data at the end of the file
  $self->_chop_file if $len_diff < 0;
}

# _iwrite(D, S, E)
# Insert text D at position S.
# Let C = E-S-|D|.  If C < 0; die.  
# Data in [S,S+C) is copied to [S+D,S+D+C) = [S+D,E).
# Data in [S+C = E-D, E) is returned.  Data in [E, oo) is untouched.
#
# In a later version, don't read the entire intervening area into
# memory at once; do the copying block by block.
sub _iwrite {
  my $self = shift;
  my ($D, $s, $e) = @_;
  my $d = length $D;
  my $c = $e-$s-$d;
  local *FH = $self->{fh};
  confess "Not enough space to insert $d bytes between $s and $e"
    if $c < 0;
  confess "[$s,$e) is an invalid insertion range" if $e < $s;

  $self->_seekb($s);
  read FH, my $buf, $e-$s;

  $D .= substr($buf, 0, $c, "");

  $self->_seekb($s);
  $self->_write_record($D);

  return $buf;
}

# Like _twrite, but the data-pos-len triple may be repeated; you may
# write several chunks.  All the writing will be done in
# one pass.   Chunks SHALL be in ascending order and SHALL NOT overlap.
sub _mtwrite {
  my $self = shift;
  my $unwritten = "";
  my $delta = 0;

  @_ % 3 == 0 
    or die "Arguments to _mtwrite did not come in groups of three";

  while (@_) {
    my ($data, $pos, $len) = splice @_, 0, 3;
    my $end = $pos + $len;  # The OLD end of the segment to be replaced
    $data = $unwritten . $data;
    $delta -= length($unwritten);
    $unwritten  = "";
    $pos += $delta;             # This is where the data goes now
    my $dlen = length $data;
    $self->_seekb($pos);
    if ($len >= $dlen) {        # the data will fit
      $self->_write_record($data);
      $delta += ($dlen - $len); # everything following moves down by this much
      $data = ""; # All the data in the buffer has been written
    } else {                    # won't fit
      my $writable = substr($data, 0, $len - $delta, "");
      $self->_write_record($writable);
      $delta += ($dlen - $len); # everything following moves down by this much
    } 

    # At this point we've written some but maybe not all of the data.
    # There might be a gap to close up, or $data might still contain a
    # bunch of unwritten data that didn't fit.
    my $ndlen = length $data;
    if ($delta == 0) {
      $self->_write_record($data);
    } elsif ($delta < 0) {
      # upcopy (close up gap)
      if (@_) {
        $self->_upcopy($end, $end + $delta, $_[1] - $end);  
      } else {
        $self->_upcopy($end, $end + $delta);  
      }
    } else {
      # downcopy (insert data that didn't fit; replace this data in memory
      # with _later_ data that doesn't fit)
      if (@_) {
        $unwritten = $self->_downcopy($data, $end, $_[1] - $end);
      } else {
        # Make the file longer to accommodate the last segment that doesn'
        $unwritten = $self->_downcopy($data, $end);
      }
    }
  }
}

# Copy block of data of length $len from position $spos to position $dpos
# $dpos must be <= $spos
#
# If $len is undefined, go all the way to the end of the file
# and then truncate it ($spos - $dpos bytes will be removed)
sub _upcopy {
  my $blocksize = 8192;
  my ($self, $spos, $dpos, $len) = @_;
  if ($dpos > $spos) {
    die "source ($spos) was upstream of destination ($dpos) in _upcopy";
  } elsif ($dpos == $spos) {
    return;
  }
  
  while (! defined ($len) || $len > 0) {
    my $readsize = ! defined($len) ? $blocksize
               : $len > $blocksize ? $blocksize
               : $len;
      
    my $fh = $self->{fh};
    $self->_seekb($spos);
    my $bytes_read = read $fh, my($data), $readsize;
    $self->_seekb($dpos);
    if ($data eq "") { 
      $self->_chop_file;
      last;
    }
    $self->_write_record($data);
    $spos += $bytes_read;
    $dpos += $bytes_read;
    $len -= $bytes_read if defined $len;
  }
}

# Write $data into a block of length $len at position $pos,
# moving everything in the block forwards to make room.
# Instead of writing the last length($data) bytes from the block
# (because there isn't room for them any longer) return them.
#
# Undefined $len means 'until the end of the file'
sub _downcopy {
  my $blocksize = 8192;
  my ($self, $data, $pos, $len) = @_;
  my $fh = $self->{fh};

  while (! defined $len || $len > 0) {
    my $readsize = ! defined($len) ? $blocksize 
      : $len > $blocksize? $blocksize : $len;
    $self->_seekb($pos);
    read $fh, my($old), $readsize;
    my $last_read_was_short = length($old) < $readsize;
    $data .= $old;
    my $writable;
    if ($last_read_was_short) {
      # If last read was short, then $data now contains the entire rest
      # of the file, so there's no need to write only one block of it
      $writable = $data;
      $data = "";
    } else {
      $writable = substr($data, 0, $readsize, "");
    }
    last if $writable eq "";
    $self->_seekb($pos);
    $self->_write_record($writable);
    last if $last_read_was_short && $data eq "";
    $len -= $readsize if defined $len;
    $pos += $readsize;
  }
  return $data;
}

# Adjust the object data structures following an '_mtwrite'
# Arguments are
#  [$pos, $nrecs, @length]  items
# indicating that $nrecs records were removed at $recpos (a record offset)
# and replaced with records of length @length...
# Arguments guarantee that $recpos is strictly increasing.
# No return value
sub _oadjust {
  my $self = shift;
  my $delta = 0;
  my $delta_recs = 0;
  my $prev_end = -1;
  my %newkeys;

  for (@_) {
    my ($pos, $nrecs, @data) = @$_;
    $pos += $delta_recs;

    # Adjust the offsets of the records after the previous batch up
    # to the first new one of this batch
    for my $i ($prev_end+2 .. $pos - 1) {
      $self->{offsets}[$i] += $delta;
      $newkey{$i} = $i + $delta_recs;
    }

    $prev_end = $pos + @data - 1; # last record moved on this pass 

    # Remove the offsets for the removed records;
    # replace with the offsets for the inserted records
    my @newoff = ($self->{offsets}[$pos] + $delta);
    for my $i (0 .. $#data) {
      my $newlen = length $data[$i];
      push @newoff, $newoff[$i] + $newlen;
      $delta += $newlen;
    }

    for my $i ($pos .. $pos+$nrecs-1) {
      last if $i+1 > $#{$self->{offsets}};
      my $oldlen = $self->{offsets}[$i+1] - $self->{offsets}[$i];
      $delta -= $oldlen;
    }

#    # also this data has changed, so update it in the cache
#    for (0 .. $#data) {
#      $self->{cache}->update($pos + $_, $data[$_]);
#    }
#    if ($delta_recs) {
#      my @oldkeys = grep $_ >= $pos + @data, $self->{cache}->ckeys;
#      my @newkeys = map $_ + $delta_recs, @oldkeys;
#      $self->{cache}->rekey(\@oldkeys, \@newkeys);
#    }

    # replace old offsets with new
    splice @{$self->{offsets}}, $pos, $nrecs+1, @newoff;
    # What if we just spliced out the end of the offsets table?
    # shouldn't we clear $self->{eof}?   Test for this XXX BUG TODO

    $delta_recs += @data - $nrecs; # net change in total number of records
  }

  # The trailing records at the very end of the file
  if ($delta) {
    for my $i ($prev_end+2 .. $#{$self->{offsets}}) {
      $self->{offsets}[$i] += $delta;
    }
  }

  # If we scrubbed out all known offsets, regenerate the trivial table
  # that knows that the file does indeed start at 0.
  $self->{offsets}[0] = 0 unless @{$self->{offsets}};
  # If the file got longer, the offsets table is no longer complete
  # $self->{eof} = 0 if $delta_recs > 0;

  # Now there might be too much data in the cache, if we spliced out
  # some short records and spliced in some long ones.  If so, flush
  # the cache.
  $self->_cache_flush;
}

# If a record does not already end with the appropriate terminator
# string, append one.
sub _fixrecs {
  my $self = shift;
  for (@_) {
    $_ = "" unless defined $_;
    $_ .= $self->{recsep}
      unless substr($_, - $self->{recseplen}) eq $self->{recsep};
  }
}


################################################################
#
# Basic read, write, and seek
#

# seek to the beginning of record #$n
# Assumes that the offsets table is already correctly populated
#
# Note that $n=-1 has a special meaning here: It means the start of
# the last known record; this may or may not be the very last record
# in the file, depending on whether the offsets table is fully populated.
#
sub _seek {
  my ($self, $n) = @_;
  my $o = $self->{offsets}[$n];
  defined($o)
    or confess("logic error: undefined offset for record $n");
  seek $self->{fh}, $o, SEEK_SET
    or confess "Couldn't seek filehandle: $!";  # "Should never happen."
}

# seek to byte $b in the file
sub _seekb {
  my ($self, $b) = @_;
  seek $self->{fh}, $b, SEEK_SET
    or die "Couldn't seek filehandle: $!";  # "Should never happen."
}

# populate the offsets table up to the beginning of record $n
# return the offset of record $n
sub _fill_offsets_to {
  my ($self, $n) = @_;

  return $self->{offsets}[$n] if $self->{eof};

  my $fh = $self->{fh};
  local *OFF = $self->{offsets};
  my $rec;

  until ($#OFF >= $n) {
    $self->_seek(-1);           # tricky -- see comment at _seek
    $rec = $self->_read_record;
    if (defined $rec) {
      push @OFF, int(tell $fh);  # Tels says that int() saves memory here
    } else {
      $self->{eof} = 1;
      return;                   # It turns out there is no such record
    }
  }

  # we have now read all the records up to record n-1,
  # so we can return the offset of record n
  $OFF[$n];
}

sub _fill_offsets {
  my ($self) = @_;

  my $fh = $self->{fh};
  local *OFF = $self->{offsets};
  
  $self->_seek(-1);           # tricky -- see comment at _seek

  # Tels says that inlining read_record() would make this loop
  # five times faster. 20030508
  while ( defined $self->_read_record()) {
    # int() saves us memory here
    push @OFF, int(tell $fh);
  }

  $self->{eof} = 1;
  $#OFF;
}

# assumes that $rec is already suitably terminated
sub _write_record {
  my ($self, $rec) = @_;
  my $fh = $self->{fh};
  local $\ = "";
  print $fh $rec
    or die "Couldn't write record: $!";  # "Should never happen."
#  $self->{_written} += length($rec);
}

sub _read_record {
  my $self = shift;
  my $rec;
  { local $/ = $self->{recsep};
    my $fh = $self->{fh};
    $rec = <$fh>;
  }
  return unless defined $rec;
  if (substr($rec, -$self->{recseplen}) ne $self->{recsep}) {
    # improperly terminated final record --- quietly fix it.
#    my $ac = substr($rec, -$self->{recseplen});
#    $ac =~ s/\n/\\n/g;
    $self->{sawlastrec} = 1;
    unless ($self->{rdonly}) {
      local $\ = "";
      my $fh = $self->{fh};
      print $fh $self->{recsep};
    }
    $rec .= $self->{recsep};
  }
#  $self->{_read} += length($rec) if defined $rec;
  $rec;
}

sub _rw_stats {
  my $self = shift;
  @{$self}{'_read', '_written'};
}

################################################################
#
# Read cache management

sub _cache_flush {
  my ($self) = @_;
  $self->{cache}->reduce_size_to($self->{memory} - $self->{deferred_s});
}

sub _cache_too_full {
  my $self = shift;
  $self->{cache}->bytes + $self->{deferred_s} >= $self->{memory};
}

################################################################
#
# File custodial services
#


# We have read to the end of the file and have the offsets table
# entirely populated.  Now we need to write a new record beyond
# the end of the file.  We prepare for this by writing
# empty records into the file up to the position we want
#
# assumes that the offsets table already contains the offset of record $n,
# if it exists, and extends to the end of the file if not.
sub _extend_file_to {
  my ($self, $n) = @_;
  $self->_seek(-1);             # position after the end of the last record
  my $pos = $self->{offsets}[-1];

  # the offsets table has one entry more than the total number of records
  my $extras = $n - $#{$self->{offsets}};

  # Todo : just use $self->{recsep} x $extras here?
  while ($extras-- > 0) {
    $self->_write_record($self->{recsep});
    push @{$self->{offsets}}, int(tell $self->{fh});
  }
}

# Truncate the file at the current position
sub _chop_file {
  my $self = shift;
  truncate $self->{fh}, tell($self->{fh});
}


# compute the size of a buffer suitable for moving
# all the data in a file forward $n bytes
# ($n may be negative)
# The result should be at least $n.
sub _bufsize {
  my $n = shift;
  return 8192 if $n <= 0;
  my $b = $n & ~8191;
  $b += 8192 if $n & 8191;
  $b;
}

################################################################
#
# Miscellaneous public methods
#

# Lock the file
sub flock {
  my ($self, $op) = @_;
  unless (@_ <= 3) {
    my $pack = ref $self;
    croak "Usage: $pack\->flock([OPERATION])";
  }
  my $fh = $self->{fh};
  $op = LOCK_EX unless defined $op;
  my $locked = flock $fh, $op;
  
  if ($locked && ($op & (LOCK_EX | LOCK_SH))) {
    # If you're locking the file, then presumably it's because
    # there might have been a write access by another process.
    # In that case, the read cache contents and the offsets table
    # might be invalid, so discard them.  20030508
    $self->{offsets} = [0];
    $self->{cache}->empty;
  }

  $locked;
}

# Get/set autochomp option
sub autochomp {
  my $self = shift;
  if (@_) {
    my $old = $self->{autochomp};
    $self->{autochomp} = shift;
    $old;
  } else {
    $self->{autochomp};
  }
}

# Get offset table entries; returns offset of nth record
sub offset {
  my ($self, $n) = @_;

  if ($#{$self->{offsets}} < $n) {
    return if $self->{eof};     # request for record beyond the end of file
    my $o = $self->_fill_offsets_to($n);
    # If it's still undefined, there is no such record, so return 'undef'
    return unless defined $o;
   }
 
  $self->{offsets}[$n];
}

sub discard_offsets {
  my $self = shift;
  $self->{offsets} = [0];
}

################################################################
#
# Matters related to deferred writing
#

# Defer writes
sub defer {
  my $self = shift;
  $self->_stop_autodeferring;
  @{$self->{ad_history}} = ();
  $self->{defer} = 1;
}

# Flush deferred writes
#
# This could be better optimized to write the file in one pass, instead
# of one pass per block of records.  But that will require modifications
# to _twrite, so I should have a good _twrite test suite first.
sub flush {
  my $self = shift;

  $self->_flush;
  $self->{defer} = 0;
}

sub _old_flush {
  my $self = shift;
  my @writable = sort {$a<=>$b} (keys %{$self->{deferred}});

  while (@writable) {
    # gather all consecutive records from the front of @writable
    my $first_rec = shift @writable;
    my $last_rec = $first_rec+1;
    ++$last_rec, shift @writable while @writable && $last_rec == $writable[0];
    --$last_rec;
    $self->_fill_offsets_to($last_rec);
    $self->_extend_file_to($last_rec);
    $self->_splice($first_rec, $last_rec-$first_rec+1, 
                   @{$self->{deferred}}{$first_rec .. $last_rec});
  }

  $self->_discard;               # clear out defered-write-cache
}

sub _flush {
  my $self = shift;
  my @writable = sort {$a<=>$b} (keys %{$self->{deferred}});
  my @args;
  my @adjust;

  while (@writable) {
    # gather all consecutive records from the front of @writable
    my $first_rec = shift @writable;
    my $last_rec = $first_rec+1;
    ++$last_rec, shift @writable while @writable && $last_rec == $writable[0];
    --$last_rec;
    my $end = $self->_fill_offsets_to($last_rec+1);
    if (not defined $end) {
      $self->_extend_file_to($last_rec);
      $end = $self->{offsets}[$last_rec];
    }
    my ($start) = $self->{offsets}[$first_rec];
    push @args,
         join("", @{$self->{deferred}}{$first_rec .. $last_rec}), # data
         $start,                                                  # position
         $end-$start;                                             # length
    push @adjust, [$first_rec, # starting at this position...
                   $last_rec-$first_rec+1,  # this many records...
                   # are replaced with these...
                   @{$self->{deferred}}{$first_rec .. $last_rec},
                  ];
  }

  $self->_mtwrite(@args);  # write multiple record groups
  $self->_discard;               # clear out defered-write-cache
  $self->_oadjust(@adjust);
}

# Discard deferred writes and disable future deferred writes
sub discard {
  my $self = shift;
  $self->_discard;
  $self->{defer} = 0;
}

# Discard deferred writes, but retain old deferred writing mode
sub _discard {
  my $self = shift;
  %{$self->{deferred}} = ();
  $self->{deferred_s}  = 0;
  $self->{deferred_max}  = -1;
  $self->{cache}->set_limit($self->{memory});
}

# Deferred writing is enabled, either explicitly ($self->{defer})
# or automatically ($self->{autodeferring})
sub _is_deferring {
  my $self = shift;
  $self->{defer} || $self->{autodeferring};
}

# The largest record number of any deferred record
sub _defer_max {
  my $self = shift;
  return $self->{deferred_max} if defined $self->{deferred_max};
  my $max = -1;
  for my $key (keys %{$self->{deferred}}) {
    $max = $key if $key > $max;
  }
  $self->{deferred_max} = $max;
  $max;
}

################################################################
#
# Matters related to autodeferment
#

# Get/set autodefer option
sub autodefer {
  my $self = shift;
  if (@_) {
    my $old = $self->{autodefer};
    $self->{autodefer} = shift;
    if ($old) {
      $self->_stop_autodeferring;
      @{$self->{ad_history}} = ();
    }
    $old;
  } else {
    $self->{autodefer};
  }
}

# The user is trying to store record #$n Record that in the history,
# and then enable (or disable) autodeferment if that seems useful.
# Note that it's OK for $n to be a non-number, as long as the function
# is prepared to deal with that.  Nobody else looks at the ad_history.
#
# Now, what does the ad_history mean, and what is this function doing?
# Essentially, the idea is to enable autodeferring when we see that the
# user has made three consecutive STORE calls to three consecutive records.
# ("Three" is actually ->{autodefer_threshhold}.)
# A STORE call for record #$n inserts $n into the autodefer history,
# and if the history contains three consecutive records, we enable 
# autodeferment.  An ad_history of [X, Y] means that the most recent
# STOREs were for records X, X+1, ..., Y, in that order.  
#
# Inserting a nonconsecutive number erases the history and starts over.
#
# Performing a special operation like SPLICE erases the history.
#
# There's one special case: CLEAR means that CLEAR was just called.
# In this case, we prime the history with [-2, -1] so that if the next
# write is for record 0, autodeferring goes on immediately.  This is for
# the common special case of "@a = (...)".
#
sub _annotate_ad_history {
  my ($self, $n) = @_;
  return unless $self->{autodefer}; # feature is disabled
  return if $self->{defer};     # already in explicit defer mode
  return unless $self->{offsets}[-1] >= $self->{autodefer_filelen_threshhold};

  local *H = $self->{ad_history};
  if ($n eq 'CLEAR') {
    @H = (-2, -1);              # prime the history with fake records
    $self->_stop_autodeferring;
  } elsif ($n =~ /^\d+$/) {
    if (@H == 0) {
      @H =  ($n, $n);
    } else {                    # @H == 2
      if ($H[1] == $n-1) {      # another consecutive record
        $H[1]++;
        if ($H[1] - $H[0] + 1 >= $self->{autodefer_threshhold}) {
          $self->{autodeferring} = 1;
        }
      } else {                  # nonconsecutive- erase and start over
        @H = ($n, $n);
        $self->_stop_autodeferring;
      }
    }
  } else {                      # SPLICE or STORESIZE or some such
    @H = ();
    $self->_stop_autodeferring;
  }
}

# If autodeferring was enabled, cut it out and discard the history
sub _stop_autodeferring {
  my $self = shift;
  if ($self->{autodeferring}) {
    $self->_flush;
  }
  $self->{autodeferring} = 0;
}

################################################################


# This is NOT a method.  It is here for two reasons:
#  1. To factor a fairly complicated block out of the constructor
#  2. To provide access for the test suite, which need to be sure
#     files are being written properly.
sub _default_recsep {
  my $recsep = $/;
  if ($^O eq 'MSWin32') {       # Dos too?
    # Windows users expect files to be terminated with \r\n
    # But $/ is set to \n instead
    # Note that this also transforms \n\n into \r\n\r\n.
    # That is a feature.
    $recsep =~ s/\n/\r\n/g;
  }
  $recsep;
}

# Utility function for _check_integrity
sub _ci_warn {
  my $msg = shift;
  $msg =~ s/\n/\\n/g;
  $msg =~ s/\r/\\r/g;
  print "# $msg\n";
}

# Given a file, make sure the cache is consistent with the
# file contents and the internal data structures are consistent with
# each other.  Returns true if everything checks out, false if not
#
# The $file argument is no longer used.  It is retained for compatibility
# with the existing test suite.
sub _check_integrity {
  my ($self, $file, $warn) = @_;
  my $rsl = $self->{recseplen};
  my $rs  = $self->{recsep};
  my $good = 1; 
  local *_;                     # local $_ does not work here
  local $DIAGNOSTIC = 1;

  if (not defined $rs) {
    _ci_warn("recsep is undef!");
    $good = 0;
  } elsif ($rs eq "") {
    _ci_warn("recsep is empty!");
    $good = 0;
  } elsif ($rsl != length $rs) {
    my $ln = length $rs;
    _ci_warn("recsep <$rs> has length $ln, should be $rsl");
    $good = 0;
  }

  if (not defined $self->{offsets}[0]) {
    _ci_warn("offset 0 is missing!");
    $good = 0;

  } elsif ($self->{offsets}[0] != 0) {
    _ci_warn("rec 0: offset <$self->{offsets}[0]> s/b 0!");
    $good = 0;
  }

  my $cached = 0;
  {
    local *F = $self->{fh};
    seek F, 0, SEEK_SET;
    local $. = 0;
    local $/ = $rs;

    while (<F>) {
      my $n = $. - 1;
      my $cached = $self->{cache}->_produce($n);
      my $offset = $self->{offsets}[$.];
      my $ao = tell F;
      if (defined $offset && $offset != $ao) {
        _ci_warn("rec $n: offset <$offset> actual <$ao>");
        $good = 0;
      }
      if (defined $cached && $_ ne $cached && ! $self->{deferred}{$n}) {
        $good = 0;
        _ci_warn("rec $n: cached <$cached> actual <$_>");
      }
      if (defined $cached && substr($cached, -$rsl) ne $rs) {
        $good = 0;
        _ci_warn("rec $n in the cache is missing the record separator");
      }
      if (! defined $offset && $self->{eof}) {
        $good = 0;
        _ci_warn("The offset table was marked complete, but it is missing element $.");
      }
    }
    if (@{$self->{offsets}} > $.+1) {
        $good = 0;
        my $n = @{$self->{offsets}};
        _ci_warn("The offset table has $n items, but the file has only $.");
    }

    my $deferring = $self->_is_deferring;
    for my $n ($self->{cache}->ckeys) {
      my $r = $self->{cache}->_produce($n);
      $cached += length($r);
      next if $n+1 <= $.;         # checked this already
      _ci_warn("spurious caching of record $n");
      $good = 0;
    }
    my $b = $self->{cache}->bytes;
    if ($cached != $b) {
      _ci_warn("cache size is $b, should be $cached");
      $good = 0;
    }
  }

  # That cache has its own set of tests
  $good = 0 unless $self->{cache}->_check_integrity;

  # Now let's check the deferbuffer
  # Unless deferred writing is enabled, it should be empty
  if (! $self->_is_deferring && %{$self->{deferred}}) {
    _ci_warn("deferred writing disabled, but deferbuffer nonempty");
    $good = 0;
  }

  # Any record in the deferbuffer should *not* be present in the readcache
  my $deferred_s = 0;
  while (my ($n, $r) = each %{$self->{deferred}}) {
    $deferred_s += length($r);
    if (defined $self->{cache}->_produce($n)) {
      _ci_warn("record $n is in the deferbuffer *and* the readcache");
      $good = 0;
    }
    if (substr($r, -$rsl) ne $rs) {
      _ci_warn("rec $n in the deferbuffer is missing the record separator");
      $good = 0;
    }
  }

  # Total size of deferbuffer should match internal total
  if ($deferred_s != $self->{deferred_s}) {
    _ci_warn("buffer size is $self->{deferred_s}, should be $deferred_s");
    $good = 0;
  }

  # Total size of deferbuffer should not exceed the specified limit
  if ($deferred_s > $self->{dw_size}) {
    _ci_warn("buffer size is $self->{deferred_s} which exceeds the limit of $self->{dw_size}");
    $good = 0;
  }

  # Total size of cached data should not exceed the specified limit
  if ($deferred_s + $cached > $self->{memory}) {
    my $total = $deferred_s + $cached;
    _ci_warn("total stored data size is $total which exceeds the limit of $self->{memory}");
    $good = 0;
  }

  # Stuff related to autodeferment
  if (!$self->{autodefer} && @{$self->{ad_history}}) {
    _ci_warn("autodefer is disabled, but ad_history is nonempty");
    $good = 0;
  }
  if ($self->{autodeferring} && $self->{defer}) {
    _ci_warn("both autodeferring and explicit deferring are active");
    $good = 0;
  }
  if (@{$self->{ad_history}} == 0) {
    # That's OK, no additional tests required
  } elsif (@{$self->{ad_history}} == 2) {
    my @non_number = grep !/^-?\d+$/, @{$self->{ad_history}};
    if (@non_number) {
      my $msg;
      { local $" = ')(';
        $msg = "ad_history contains non-numbers (@{$self->{ad_history}})";
      }
      _ci_warn($msg);
      $good = 0;
    } elsif ($self->{ad_history}[1] < $self->{ad_history}[0]) {
      _ci_warn("ad_history has nonsensical values @{$self->{ad_history}}");
      $good = 0;
    }
  } else {
    _ci_warn("ad_history has bad length <@{$self->{ad_history}}>");
    $good = 0;
  }

  $good;
}

################################################################
#
# Tie::File::Cache
#
# Read cache

package Tie::File::Cache;
$Tie::File::Cache::VERSION = $Tie::File::VERSION;
use Carp ':DEFAULT', 'confess';

sub HEAP () { 0 }
sub HASH () { 1 }
sub MAX  () { 2 }
sub BYTES() { 3 }
#sub STAT () { 4 } # Array with request statistics for each record
#sub MISS () { 5 } # Total number of cache misses
#sub REQ  () { 6 } # Total number of cache requests 
use strict 'vars';

sub new {
  my ($pack, $max) = @_;
  local *_;
  croak "missing argument to ->new" unless defined $max;
  my $self = [];
  bless $self => $pack;
  @$self = (Tie::File::Heap->new($self), {}, $max, 0);
  $self;
}

sub adj_limit {
  my ($self, $n) = @_;
  $self->[MAX] += $n;
}

sub set_limit {
  my ($self, $n) = @_;
  $self->[MAX] = $n;
}

# For internal use only
# Will be called by the heap structure to notify us that a certain 
# piece of data has moved from one heap element to another.
# $k is the hash key of the item
# $n is the new index into the heap at which it is stored
# If $n is undefined, the item has been removed from the heap.
sub _heap_move {
  my ($self, $k, $n) = @_;
  if (defined $n) {
    $self->[HASH]{$k} = $n;
  } else {
    delete $self->[HASH]{$k};
  }
}

sub insert {
  my ($self, $key, $val) = @_;
  local *_;
  croak "missing argument to ->insert" unless defined $key;
  unless (defined $self->[MAX]) {
    confess "undefined max" ;
  }
  confess "undefined val" unless defined $val;
  return if length($val) > $self->[MAX];

#  if ($self->[STAT]) {
#    $self->[STAT][$key] = 1;
#    return;
#  }

  my $oldnode = $self->[HASH]{$key};
  if (defined $oldnode) {
    my $oldval = $self->[HEAP]->set_val($oldnode, $val);
    $self->[BYTES] -= length($oldval);
  } else {
    $self->[HEAP]->insert($key, $val);
  }
  $self->[BYTES] += length($val);
  $self->flush if $self->[BYTES] > $self->[MAX];
}

sub expire {
  my $self = shift;
  my $old_data = $self->[HEAP]->popheap;
  return unless defined $old_data;
  $self->[BYTES] -= length $old_data;
  $old_data;
}

sub remove {
  my ($self, @keys) = @_;
  my @result;

#  if ($self->[STAT]) {
#    for my $key (@keys) {
#      $self->[STAT][$key] = 0;
#    }
#    return;
#  }

  for my $key (@keys) {
    next unless exists $self->[HASH]{$key};
    my $old_data = $self->[HEAP]->remove($self->[HASH]{$key});
    $self->[BYTES] -= length $old_data;
    push @result, $old_data;
  }
  @result;
}

sub lookup {
  my ($self, $key) = @_;
  local *_;
  croak "missing argument to ->lookup" unless defined $key;

#  if ($self->[STAT]) {
#    $self->[MISS]++  if $self->[STAT][$key]++ == 0;
#    $self->[REQ]++;
#    my $hit_rate = 1 - $self->[MISS] / $self->[REQ];
#    # Do some testing to determine this threshhold
#    $#$self = STAT - 1 if $hit_rate > 0.20; 
#  }

  if (exists $self->[HASH]{$key}) {
    $self->[HEAP]->lookup($self->[HASH]{$key});
  } else {
    return;
  }
}

# For internal use only
sub _produce {
  my ($self, $key) = @_;
  my $loc = $self->[HASH]{$key};
  return unless defined $loc;
  $self->[HEAP][$loc][2];
}

# For internal use only
sub _promote {
  my ($self, $key) = @_;
  $self->[HEAP]->promote($self->[HASH]{$key});
}

sub empty {
  my ($self) = @_;
  %{$self->[HASH]} = ();
    $self->[BYTES] = 0;
    $self->[HEAP]->empty;
#  @{$self->[STAT]} = ();
#    $self->[MISS] = 0;
#    $self->[REQ] = 0;
}

sub is_empty {
  my ($self) = @_;
  keys %{$self->[HASH]} == 0;
}

sub update {
  my ($self, $key, $val) = @_;
  local *_;
  croak "missing argument to ->update" unless defined $key;
  if (length($val) > $self->[MAX]) {
    my ($oldval) = $self->remove($key);
    $self->[BYTES] -= length($oldval) if defined $oldval;
  } elsif (exists $self->[HASH]{$key}) {
    my $oldval = $self->[HEAP]->set_val($self->[HASH]{$key}, $val);
    $self->[BYTES] += length($val);
    $self->[BYTES] -= length($oldval) if defined $oldval;
  } else {
    $self->[HEAP]->insert($key, $val);
    $self->[BYTES] += length($val);
  }
  $self->flush;
}

sub rekey {
  my ($self, $okeys, $nkeys) = @_;
  local *_;
  my %map;
  @map{@$okeys} = @$nkeys;
  croak "missing argument to ->rekey" unless defined $nkeys;
  croak "length mismatch in ->rekey arguments" unless @$nkeys == @$okeys;
  my %adjusted;                 # map new keys to heap indices
  # You should be able to cut this to one loop TODO XXX
  for (0 .. $#$okeys) {
    $adjusted{$nkeys->[$_]} = delete $self->[HASH]{$okeys->[$_]};
  }
  while (my ($nk, $ix) = each %adjusted) {
    # @{$self->[HASH]}{keys %adjusted} = values %adjusted;
    $self->[HEAP]->rekey($ix, $nk);
    $self->[HASH]{$nk} = $ix;
  }
}

sub ckeys {
  my $self = shift;
  my @a = keys %{$self->[HASH]};
  @a;
}

# Return total amount of cached data
sub bytes {
  my $self = shift;
  $self->[BYTES];
}

# Expire oldest item from cache until cache size is smaller than $max
sub reduce_size_to {
  my ($self, $max) = @_;
  until ($self->[BYTES] <= $max) {
    # Note that Tie::File::Cache::expire has been inlined here
    my $old_data = $self->[HEAP]->popheap;
    return unless defined $old_data;
    $self->[BYTES] -= length $old_data;
  }
}

# Why not just $self->reduce_size_to($self->[MAX])?
# Try this when things stabilize   TODO XXX
# If the cache is too full, expire the oldest records
sub flush {
  my $self = shift;
  $self->reduce_size_to($self->[MAX]) if $self->[BYTES] > $self->[MAX];
}

# For internal use only
sub _produce_lru {
  my $self = shift;
  $self->[HEAP]->expire_order;
}

BEGIN { *_ci_warn = \&Tie::File::_ci_warn }

sub _check_integrity {          # For CACHE
  my $self = shift;
  my $good = 1;

  # Test HEAP
  $self->[HEAP]->_check_integrity or $good = 0;

  # Test HASH
  my $bytes = 0;
  for my $k (keys %{$self->[HASH]}) {
    if ($k ne '0' && $k !~ /^[1-9][0-9]*$/) {
      $good = 0;
      _ci_warn "Cache hash key <$k> is non-numeric";
    }

    my $h = $self->[HASH]{$k};
    if (! defined $h) {
      $good = 0;
      _ci_warn "Heap index number for key $k is undefined";
    } elsif ($h == 0) {
      $good = 0;
      _ci_warn "Heap index number for key $k is zero";
    } else {
      my $j = $self->[HEAP][$h];
      if (! defined $j) {
        $good = 0;
        _ci_warn "Heap contents key $k (=> $h) are undefined";
      } else {
        $bytes += length($j->[2]);
        if ($k ne $j->[1]) {
          $good = 0;
          _ci_warn "Heap contents key $k (=> $h) is $j->[1], should be $k";
        }
      }
    }
  }

  # Test BYTES
  if ($bytes != $self->[BYTES]) {
    $good = 0;
    _ci_warn "Total data in cache is $bytes, expected $self->[BYTES]";
  }

  # Test MAX
  if ($bytes > $self->[MAX]) {
    $good = 0;
    _ci_warn "Total data in cache is $bytes, exceeds maximum $self->[MAX]";
  }

  return $good;
}

sub delink {
  my $self = shift;
  $self->[HEAP] = undef;        # Bye bye heap
}

################################################################
#
# Tie::File::Heap
#
# Heap data structure for use by cache LRU routines

package Tie::File::Heap;
use Carp ':DEFAULT', 'confess';
$Tie::File::Heap::VERSION = $Tie::File::Cache::VERSION;
sub SEQ () { 0 };
sub KEY () { 1 };
sub DAT () { 2 };

sub new {
  my ($pack, $cache) = @_;
  die "$pack: Parent cache object $cache does not support _heap_move method"
    unless eval { $cache->can('_heap_move') };
  my $self = [[0,$cache,0]];
  bless $self => $pack;
}

# Allocate a new sequence number, larger than all previously allocated numbers
sub _nseq {
  my $self = shift;
  $self->[0][0]++;
}

sub _cache {
  my $self = shift;
  $self->[0][1];
}

sub _nelts {
  my $self = shift;
  $self->[0][2];
}

sub _nelts_inc {
  my $self = shift;
  ++$self->[0][2];
}  

sub _nelts_dec {
  my $self = shift;
  --$self->[0][2];
}  

sub is_empty {
  my $self = shift;
  $self->_nelts == 0;
}

sub empty {
  my $self = shift;
  $#$self = 0;
  $self->[0][2] = 0;
  $self->[0][0] = 0;            # might as well reset the sequence numbers
}

# notify the parent cache object that we moved something
sub _heap_move {
  my $self = shift;
  $self->_cache->_heap_move(@_);
}

# Insert a piece of data into the heap with the indicated sequence number.
# The item with the smallest sequence number is always at the top.
# If no sequence number is specified, allocate a new one and insert the
# item at the bottom.
sub insert {
  my ($self, $key, $data, $seq) = @_;
  $seq = $self->_nseq unless defined $seq;
  $self->_insert_new([$seq, $key, $data]);
}

# Insert a new, fresh item at the bottom of the heap
sub _insert_new {
  my ($self, $item) = @_;
  my $i = @$self;
  $i = int($i/2) until defined $self->[$i/2];
  $self->[$i] = $item;
  $self->[0][1]->_heap_move($self->[$i][KEY], $i);
  $self->_nelts_inc;
}

# Insert [$data, $seq] pair at or below item $i in the heap.
# If $i is omitted, default to 1 (the top element.)
sub _insert {
  my ($self, $item, $i) = @_;
#  $self->_check_loc($i) if defined $i;
  $i = 1 unless defined $i;
  until (! defined $self->[$i]) {
    if ($self->[$i][SEQ] > $item->[SEQ]) { # inserted item is older
      ($self->[$i], $item) = ($item, $self->[$i]);
      $self->[0][1]->_heap_move($self->[$i][KEY], $i);
    }
    # If either is undefined, go that way.  Otherwise, choose at random
    my $dir;
    $dir = 0 if !defined $self->[2*$i];
    $dir = 1 if !defined $self->[2*$i+1];
    $dir = int(rand(2)) unless defined $dir;
    $i = 2*$i + $dir;
  }
  $self->[$i] = $item;
  $self->[0][1]->_heap_move($self->[$i][KEY], $i);
  $self->_nelts_inc;
}

# Remove the item at node $i from the heap, moving child items upwards.
# The item with the smallest sequence number is always at the top.
# Moving items upwards maintains this condition.
# Return the removed item.  Return undef if there was no item at node $i.
sub remove {
  my ($self, $i) = @_;
  $i = 1 unless defined $i;
  my $top = $self->[$i];
  return unless defined $top;
  while (1) {
    my $ii;
    my ($L, $R) = (2*$i, 2*$i+1);

    # If either is undefined, go the other way.
    # Otherwise, go towards the smallest.
    last unless defined $self->[$L] || defined $self->[$R];
    $ii = $R if not defined $self->[$L];
    $ii = $L if not defined $self->[$R];
    unless (defined $ii) {
      $ii = $self->[$L][SEQ] < $self->[$R][SEQ] ? $L : $R;
    }

    $self->[$i] = $self->[$ii]; # Promote child to fill vacated spot
    $self->[0][1]->_heap_move($self->[$i][KEY], $i);
    $i = $ii; # Fill new vacated spot
  }
  $self->[0][1]->_heap_move($top->[KEY], undef);
  undef $self->[$i];
  $self->_nelts_dec;
  return $top->[DAT];
}

sub popheap {
  my $self = shift;
  $self->remove(1);
}

# set the sequence number of the indicated item to a higher number
# than any other item in the heap, and bubble the item down to the
# bottom.
sub promote {
  my ($self, $n) = @_;
#  $self->_check_loc($n);
  $self->[$n][SEQ] = $self->_nseq;
  my $i = $n;
  while (1) {
    my ($L, $R) = (2*$i, 2*$i+1);
    my $dir;
    last unless defined $self->[$L] || defined $self->[$R];
    $dir = $R unless defined $self->[$L];
    $dir = $L unless defined $self->[$R];
    unless (defined $dir) {
      $dir = $self->[$L][SEQ] < $self->[$R][SEQ] ? $L : $R;
    }
    @{$self}[$i, $dir] = @{$self}[$dir, $i];
    for ($i, $dir) {
      $self->[0][1]->_heap_move($self->[$_][KEY], $_) if defined $self->[$_];
    }
    $i = $dir;
  }
}

# Return item $n from the heap, promoting its LRU status
sub lookup {
  my ($self, $n) = @_;
#  $self->_check_loc($n);
  my $val = $self->[$n];
  $self->promote($n);
  $val->[DAT];
}


# Assign a new value for node $n, promoting it to the bottom of the heap
sub set_val {
  my ($self, $n, $val) = @_;
#  $self->_check_loc($n);
  my $oval = $self->[$n][DAT];
  $self->[$n][DAT] = $val;
  $self->promote($n);
  return $oval;
}

# The hask key has changed for an item;
# alter the heap's record of the hash key
sub rekey {
  my ($self, $n, $new_key) = @_;
#  $self->_check_loc($n);
  $self->[$n][KEY] = $new_key;
}

sub _check_loc {
  my ($self, $n) = @_;
  unless (1 || defined $self->[$n]) {
    confess "_check_loc($n) failed";
  }
}

BEGIN { *_ci_warn = \&Tie::File::_ci_warn }

sub _check_integrity {
  my $self = shift;
  my $good = 1;
  my %seq;

  unless (eval {$self->[0][1]->isa("Tie::File::Cache")}) {
    _ci_warn "Element 0 of heap corrupt";
    $good = 0;
  }
  $good = 0 unless $self->_satisfies_heap_condition(1);
  for my $i (2 .. $#{$self}) {
    my $p = int($i/2);          # index of parent node
    if (defined $self->[$i] && ! defined $self->[$p]) {
      _ci_warn "Element $i of heap defined, but parent $p isn't";
      $good = 0;
    }

    if (defined $self->[$i]) {
      if ($seq{$self->[$i][SEQ]}) {
        my $seq = $self->[$i][SEQ];
        _ci_warn "Nodes $i and $seq{$seq} both have SEQ=$seq";
        $good = 0;
      } else {
        $seq{$self->[$i][SEQ]} = $i;
      }
    }
  }

  return $good;
}

sub _satisfies_heap_condition {
  my $self = shift;
  my $n = shift || 1;
  my $good = 1;
  for (0, 1) {
    my $c = $n*2 + $_;
    next unless defined $self->[$c];
    if ($self->[$n][SEQ] >= $self->[$c]) {
      _ci_warn "Node $n of heap does not predate node $c";
      $good = 0 ;
    }
    $good = 0 unless $self->_satisfies_heap_condition($c);
  }
  return $good;
}

# Return a list of all the values, sorted by expiration order
sub expire_order {
  my $self = shift;
  my @nodes = sort {$a->[SEQ] <=> $b->[SEQ]} $self->_nodes;
  map { $_->[KEY] } @nodes;
}

sub _nodes {
  my $self = shift;
  my $i = shift || 1;
  return unless defined $self->[$i];
  ($self->[$i], $self->_nodes($i*2), $self->_nodes($i*2+1));
}

"Cogito, ergo sum.";  # don't forget to return a true value from the file

__END__

=head1 NAME

Tie::File - Access the lines of a disk file via a Perl array

=head1 SYNOPSIS

	# This file documents Tie::File version 0.97
	use Tie::File;

	tie @array, 'Tie::File', filename or die ...;

	$array[13] = 'blah';     # line 13 of the file is now 'blah'
	print $array[42];        # display line 42 of the file

	$n_recs = @array;        # how many records are in the file?
	$#array -= 2;            # chop two records off the end


	for (@array) {
	  s/PERL/Perl/g;         # Replace PERL with Perl everywhere in the file
	}

	# These are just like regular push, pop, unshift, shift, and splice
	# Except that they modify the file in the way you would expect

	push @array, new recs...;
	my $r1 = pop @array;
	unshift @array, new recs...;
	my $r2 = shift @array;
	@old_recs = splice @array, 3, 7, new recs...;

	untie @array;            # all finished


=head1 DESCRIPTION

C<Tie::File> represents a regular text file as a Perl array.  Each
element in the array corresponds to a record in the file.  The first
line of the file is element 0 of the array; the second line is element
1, and so on.

The file is I<not> loaded into memory, so this will work even for
gigantic files.

Changes to the array are reflected in the file immediately.

Lazy people and beginners may now stop reading the manual.

=head2 C<recsep>

What is a 'record'?  By default, the meaning is the same as for the
C<E<lt>...E<gt>> operator: It's a string terminated by C<$/>, which is
probably C<"\n">.  (Minor exception: on DOS and Win32 systems, a
'record' is a string terminated by C<"\r\n">.)  You may change the
definition of "record" by supplying the C<recsep> option in the C<tie>
call:

	tie @array, 'Tie::File', $file, recsep => 'es';

This says that records are delimited by the string C<es>.  If the file
contained the following data:

	Curse these pesky flies!\n

then the C<@array> would appear to have four elements:

	"Curse th"
	"e p"
	"ky fli"
	"!\n"

An undefined value is not permitted as a record separator.  Perl's
special "paragraph mode" semantics (E<agrave> la C<$/ = "">) are not
emulated.

Records read from the tied array do not have the record separator
string on the end; this is to allow

	$array[17] .= "extra";

to work as expected.

(See L<"autochomp">, below.)  Records stored into the array will have
the record separator string appended before they are written to the
file, if they don't have one already.  For example, if the record
separator string is C<"\n">, then the following two lines do exactly
the same thing:

	$array[17] = "Cherry pie";
	$array[17] = "Cherry pie\n";

The result is that the contents of line 17 of the file will be
replaced with "Cherry pie"; a newline character will separate line 17
from line 18.  This means that this code will do nothing:

	chomp $array[17];

Because the C<chomp>ed value will have the separator reattached when
it is written back to the file.  There is no way to create a file
whose trailing record separator string is missing.

Inserting records that I<contain> the record separator string is not
supported by this module.  It will probably produce a reasonable
result, but what this result will be may change in a future version.
Use 'splice' to insert records or to replace one record with several.

=head2 C<autochomp>

Normally, array elements have the record separator removed, so that if
the file contains the text

	Gold
	Frankincense
	Myrrh

the tied array will appear to contain C<("Gold", "Frankincense",
"Myrrh")>.  If you set C<autochomp> to a false value, the record
separator will not be removed.  If the file above was tied with

	tie @gifts, "Tie::File", $gifts, autochomp => 0;

then the array C<@gifts> would appear to contain C<("Gold\n",
"Frankincense\n", "Myrrh\n")>, or (on Win32 systems) C<("Gold\r\n",
"Frankincense\r\n", "Myrrh\r\n")>.

=head2 C<mode>

Normally, the specified file will be opened for read and write access,
and will be created if it does not exist.  (That is, the flags
C<O_RDWR | O_CREAT> are supplied in the C<open> call.)  If you want to
change this, you may supply alternative flags in the C<mode> option.
See L<Fcntl> for a listing of available flags.
For example:

	# open the file if it exists, but fail if it does not exist
	use Fcntl 'O_RDWR';
	tie @array, 'Tie::File', $file, mode => O_RDWR;

	# create the file if it does not exist
	use Fcntl 'O_RDWR', 'O_CREAT';
	tie @array, 'Tie::File', $file, mode => O_RDWR | O_CREAT;

	# open an existing file in read-only mode
	use Fcntl 'O_RDONLY';
	tie @array, 'Tie::File', $file, mode => O_RDONLY;

Opening the data file in write-only or append mode is not supported.

=head2 C<memory>

This is an upper limit on the amount of memory that C<Tie::File> will
consume at any time while managing the file.  This is used for two
things: managing the I<read cache> and managing the I<deferred write
buffer>.

Records read in from the file are cached, to avoid having to re-read
them repeatedly.  If you read the same record twice, the first time it
will be stored in memory, and the second time it will be fetched from
the I<read cache>.  The amount of data in the read cache will not
exceed the value you specified for C<memory>.  If C<Tie::File> wants
to cache a new record, but the read cache is full, it will make room
by expiring the least-recently visited records from the read cache.

The default memory limit is 2Mib.  You can adjust the maximum read
cache size by supplying the C<memory> option.  The argument is the
desired cache size, in bytes.

	# I have a lot of memory, so use a large cache to speed up access
	tie @array, 'Tie::File', $file, memory => 20_000_000;

Setting the memory limit to 0 will inhibit caching; records will be
fetched from disk every time you examine them.

The C<memory> value is not an absolute or exact limit on the memory
used.  C<Tie::File> objects contains some structures besides the read
cache and the deferred write buffer, whose sizes are not charged
against C<memory>. 

The cache itself consumes about 310 bytes per cached record, so if
your file has many short records, you may want to decrease the cache
memory limit, or else the cache overhead may exceed the size of the
cached data.


=head2 C<dw_size>

(This is an advanced feature.  Skip this section on first reading.)

If you use deferred writing (See L<"Deferred Writing">, below) then
data you write into the array will not be written directly to the
file; instead, it will be saved in the I<deferred write buffer> to be
written out later.  Data in the deferred write buffer is also charged
against the memory limit you set with the C<memory> option.

You may set the C<dw_size> option to limit the amount of data that can
be saved in the deferred write buffer.  This limit may not exceed the
total memory limit.  For example, if you set C<dw_size> to 1000 and
C<memory> to 2500, that means that no more than 1000 bytes of deferred
writes will be saved up.  The space available for the read cache will
vary, but it will always be at least 1500 bytes (if the deferred write
buffer is full) and it could grow as large as 2500 bytes (if the
deferred write buffer is empty.)

If you don't specify a C<dw_size>, it defaults to the entire memory
limit.

=head2 Option Format

C<-mode> is a synonym for C<mode>.  C<-recsep> is a synonym for
C<recsep>.  C<-memory> is a synonym for C<memory>.  You get the
idea.

=head1 Public Methods

The C<tie> call returns an object, say C<$o>.  You may call

	$rec = $o->FETCH($n);
	$o->STORE($n, $rec);

to fetch or store the record at line C<$n>, respectively; similarly
the other tied array methods.  (See L<perltie> for details.)  You may
also call the following methods on this object:

=head2 C<flock>

	$o->flock(MODE)

will lock the tied file.  C<MODE> has the same meaning as the second
argument to the Perl built-in C<flock> function; for example
C<LOCK_SH> or C<LOCK_EX | LOCK_NB>.  (These constants are provided by
the C<use Fcntl ':flock'> declaration.)

C<MODE> is optional; the default is C<LOCK_EX>.

C<Tie::File> maintains an internal table of the byte offset of each
record it has seen in the file.  

When you use C<flock> to lock the file, C<Tie::File> assumes that the
read cache is no longer trustworthy, because another process might
have modified the file since the last time it was read.  Therefore, a
successful call to C<flock> discards the contents of the read cache
and the internal record offset table.

C<Tie::File> promises that the following sequence of operations will
be safe:

	my $o = tie @array, "Tie::File", $filename;
	$o->flock;

In particular, C<Tie::File> will I<not> read or write the file during
the C<tie> call.  (Exception: Using C<mode =E<gt> O_TRUNC> will, of
course, erase the file during the C<tie> call.  If you want to do this
safely, then open the file without C<O_TRUNC>, lock the file, and use
C<@array = ()>.)

The best way to unlock a file is to discard the object and untie the
array.  It is probably unsafe to unlock the file without also untying
it, because if you do, changes may remain unwritten inside the object.
That is why there is no shortcut for unlocking.  If you really want to
unlock the file prematurely, you know what to do; if you don't know
what to do, then don't do it.

All the usual warnings about file locking apply here.  In particular,
note that file locking in Perl is B<advisory>, which means that
holding a lock will not prevent anyone else from reading, writing, or
erasing the file; it only prevents them from getting another lock at
the same time.  Locks are analogous to green traffic lights: If you
have a green light, that does not prevent the idiot coming the other
way from plowing into you sideways; it merely guarantees to you that
the idiot does not also have a green light at the same time.

=head2 C<autochomp>

	my $old_value = $o->autochomp(0);    # disable autochomp option
	my $old_value = $o->autochomp(1);    #  enable autochomp option

	my $ac = $o->autochomp();   # recover current value

See L<"autochomp">, above.

=head2 C<defer>, C<flush>, C<discard>, and C<autodefer>

See L<"Deferred Writing">, below.

=head2 C<offset>

	$off = $o->offset($n);

This method returns the byte offset of the start of the C<$n>th record
in the file.  If there is no such record, it returns an undefined
value.

=head1 Tying to an already-opened filehandle

If C<$fh> is a filehandle, such as is returned by C<IO::File> or one
of the other C<IO> modules, you may use:

	tie @array, 'Tie::File', $fh, ...;

Similarly if you opened that handle C<FH> with regular C<open> or
C<sysopen>, you may use:

	tie @array, 'Tie::File', \*FH, ...;

Handles that were opened write-only won't work.  Handles that were
opened read-only will work as long as you don't try to modify the
array.  Handles must be attached to seekable sources of data---that
means no pipes or sockets.  If C<Tie::File> can detect that you
supplied a non-seekable handle, the C<tie> call will throw an
exception.  (On Unix systems, it can detect this.)

Note that Tie::File will only close any filehandles that it opened
internally.  If you passed it a filehandle as above, you "own" the
filehandle, and are responsible for closing it after you have untied
the @array.

=head1 Deferred Writing

(This is an advanced feature.  Skip this section on first reading.)

Normally, modifying a C<Tie::File> array writes to the underlying file
immediately.  Every assignment like C<$a[3] = ...> rewrites as much of
the file as is necessary; typically, everything from line 3 through
the end will need to be rewritten.  This is the simplest and most
transparent behavior.  Performance even for large files is reasonably
good.

However, under some circumstances, this behavior may be excessively
slow.  For example, suppose you have a million-record file, and you
want to do:

	for (@FILE) {
	  $_ = "> $_";
	}

The first time through the loop, you will rewrite the entire file,
from line 0 through the end.  The second time through the loop, you
will rewrite the entire file from line 1 through the end.  The third
time through the loop, you will rewrite the entire file from line 2 to
the end.  And so on.

If the performance in such cases is unacceptable, you may defer the
actual writing, and then have it done all at once.  The following loop
will perform much better for large files:

	(tied @a)->defer;
	for (@a) {
	  $_ = "> $_";
	}
	(tied @a)->flush;

If C<Tie::File>'s memory limit is large enough, all the writing will
done in memory.  Then, when you call C<-E<gt>flush>, the entire file
will be rewritten in a single pass.

(Actually, the preceding discussion is something of a fib.  You don't
need to enable deferred writing to get good performance for this
common case, because C<Tie::File> will do it for you automatically
unless you specifically tell it not to.  See L<"autodeferring">,
below.)

Calling C<-E<gt>flush> returns the array to immediate-write mode.  If
you wish to discard the deferred writes, you may call C<-E<gt>discard>
instead of C<-E<gt>flush>.  Note that in some cases, some of the data
will have been written already, and it will be too late for
C<-E<gt>discard> to discard all the changes.  Support for
C<-E<gt>discard> may be withdrawn in a future version of C<Tie::File>.

Deferred writes are cached in memory up to the limit specified by the
C<dw_size> option (see above).  If the deferred-write buffer is full
and you try to write still more deferred data, the buffer will be
flushed.  All buffered data will be written immediately, the buffer
will be emptied, and the now-empty space will be used for future
deferred writes.

If the deferred-write buffer isn't yet full, but the total size of the
buffer and the read cache would exceed the C<memory> limit, the oldest
records will be expired from the read cache until the total size is
under the limit.

C<push>, C<pop>, C<shift>, C<unshift>, and C<splice> cannot be
deferred.  When you perform one of these operations, any deferred data
is written to the file and the operation is performed immediately.
This may change in a future version.

If you resize the array with deferred writing enabled, the file will
be resized immediately, but deferred records will not be written.
This has a surprising consequence: C<@a = (...)> erases the file
immediately, but the writing of the actual data is deferred.  This
might be a bug.  If it is a bug, it will be fixed in a future version.

=head2 Autodeferring

C<Tie::File> tries to guess when deferred writing might be helpful,
and to turn it on and off automatically. 

	for (@a) {
	  $_ = "> $_";
	}

In this example, only the first two assignments will be done
immediately; after this, all the changes to the file will be deferred
up to the user-specified memory limit.

You should usually be able to ignore this and just use the module
without thinking about deferring.  However, special applications may
require fine control over which writes are deferred, or may require
that all writes be immediate.  To disable the autodeferment feature,
use

	(tied @o)->autodefer(0);

or

       	tie @array, 'Tie::File', $file, autodefer => 0;


Similarly, C<-E<gt>autodefer(1)> re-enables autodeferment, and 
C<-E<gt>autodefer()> recovers the current value of the autodefer setting.


=head1 CONCURRENT ACCESS TO FILES

Caching and deferred writing are inappropriate if you want the same
file to be accessed simultaneously from more than one process.  Other
optimizations performed internally by this module are also
incompatible with concurrent access.  A future version of this module will
support a C<concurrent =E<gt> 1> option that enables safe concurrent access.

Previous versions of this documentation suggested using C<memory
=E<gt> 0> for safe concurrent access.  This was mistaken.  Tie::File
will not support safe concurrent access before version 0.98.

=head1 CAVEATS

(That's Latin for 'warnings'.)

=over 4

=item *

Reasonable effort was made to make this module efficient.  Nevertheless,
changing the size of a record in the middle of a large file will
always be fairly slow, because everything after the new record must be
moved.

=item *

The behavior of tied arrays is not precisely the same as for regular
arrays.  For example:

	# This DOES print "How unusual!"
	undef $a[10];  print "How unusual!\n" if defined $a[10];

C<undef>-ing a C<Tie::File> array element just blanks out the
corresponding record in the file.  When you read it back again, you'll
get the empty string, so the supposedly-C<undef>'ed value will be
defined.  Similarly, if you have C<autochomp> disabled, then

	# This DOES print "How unusual!" if 'autochomp' is disabled
	undef $a[10];
        print "How unusual!\n" if $a[10];

Because when C<autochomp> is disabled, C<$a[10]> will read back as
C<"\n"> (or whatever the record separator string is.)  

There are other minor differences, particularly regarding C<exists>
and C<delete>, but in general, the correspondence is extremely close.

=item *

I have supposed that since this module is concerned with file I/O,
almost all normal use of it will be heavily I/O bound.  This means
that the time to maintain complicated data structures inside the
module will be dominated by the time to actually perform the I/O.
When there was an opportunity to spend CPU time to avoid doing I/O, I
usually tried to take it.

=item *

You might be tempted to think that deferred writing is like
transactions, with C<flush> as C<commit> and C<discard> as
C<rollback>, but it isn't, so don't.

=item *

There is a large memory overhead for each record offset and for each
cache entry: about 310 bytes per cached data record, and about 21 bytes per offset table entry.

The per-record overhead will limit the maximum number of records you
can access per file. Note that I<accessing> the length of the array
via C<$x = scalar @tied_file> accesses B<all> records and stores their
offsets.  The same for C<foreach (@tied_file)>, even if you exit the
loop early.

=back

=head1 SUBCLASSING

This version promises absolutely nothing about the internals, which
may change without notice.  A future version of the module will have a
well-defined and stable subclassing API.

=head1 WHAT ABOUT C<DB_File>?

People sometimes point out that L<DB_File> will do something similar,
and ask why C<Tie::File> module is necessary.

There are a number of reasons that you might prefer C<Tie::File>.
A list is available at C<http://perl.plover.com/TieFile/why-not-DB_File>.

=head1 AUTHOR

Mark Jason Dominus

To contact the author, send email to: C<mjd-perl-tiefile+@plover.com>

To receive an announcement whenever a new version of this module is
released, send a blank email message to
C<mjd-perl-tiefile-subscribe@plover.com>.

The most recent version of this module, including documentation and
any news of importance, will be available at

	http://perl.plover.com/TieFile/


=head1 LICENSE

C<Tie::File> version 0.97 is copyright (C) 2003 Mark Jason Dominus.

This library is free software; you may redistribute it and/or modify
it under the same terms as Perl itself.

These terms are your choice of any of (1) the Perl Artistic Licence,
or (2) version 2 of the GNU General Public License as published by the
Free Software Foundation, or (3) any later version of the GNU General
Public License.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this library program; it should be in the file C<COPYING>.
If not, write to the Free Software Foundation, Inc., 51 Franklin Street,
Fifth Floor, Boston, MA  02110-1301, USA

For licensing inquiries, contact the author at:

	Mark Jason Dominus
	255 S. Warnock St.
	Philadelphia, PA 19107

=head1 WARRANTY

C<Tie::File> version 0.97 comes with ABSOLUTELY NO WARRANTY.
For details, see the license.

=head1 THANKS

Gigantic thanks to Jarkko Hietaniemi, for agreeing to put this in the
core when I hadn't written it yet, and for generally being helpful,
supportive, and competent.  (Usually the rule is "choose any one.")
Also big thanks to Abhijit Menon-Sen for all of the same things.

Special thanks to Craig Berry and Peter Prymmer (for VMS portability
help), Randy Kobes (for Win32 portability help), Clinton Pierce and
Autrijus Tang (for heroic eleventh-hour Win32 testing above and beyond
the call of duty), Michael G Schwern (for testing advice), and the
rest of the CPAN testers (for testing generally).

Special thanks to Tels for suggesting several speed and memory
optimizations.

Additional thanks to:
Edward Avis /
Mattia Barbon /
Tom Christiansen /
Gerrit Haase /
Gurusamy Sarathy /
Jarkko Hietaniemi (again) /
Nikola Knezevic /
John Kominetz /
Nick Ing-Simmons /
Tassilo von Parseval /
H. Dieter Pearcey /
Slaven Rezic /
Eric Roode /
Peter Scott /
Peter Somu /
Autrijus Tang (again) /
Tels (again) /
Juerd Waalboer

=head1 TODO

More tests.  (Stuff I didn't think of yet.)

Paragraph mode?

Fixed-length mode.  Leave-blanks mode.

Maybe an autolocking mode?

For many common uses of the module, the read cache is a liability.
For example, a program that inserts a single record, or that scans the
file once, will have a cache hit rate of zero.  This suggests a major
optimization: The cache should be initially disabled.  Here's a hybrid
approach: Initially, the cache is disabled, but the cache code
maintains statistics about how high the hit rate would be *if* it were
enabled.  When it sees the hit rate get high enough, it enables
itself.  The STAT comments in this code are the beginning of an
implementation of this.

Record locking with fcntl()?  Then the module might support an undo
log and get real transactions.  What a tour de force that would be.

Keeping track of the highest cached record. This would allow reads-in-a-row
to skip the cache lookup faster (if reading from 1..N with empty cache at
start, the last cached value will be always N-1).

More tests.

=cut

