package Hash::Util;

require 5.007003;
use strict;
use Carp;
use warnings;
use warnings::register;
use Scalar::Util qw(reftype);

require Exporter;
our @ISA        = qw(Exporter);
our @EXPORT_OK  = qw(
                     fieldhash fieldhashes

                     all_keys
                     lock_keys unlock_keys
                     lock_value unlock_value
                     lock_hash unlock_hash
                     lock_keys_plus hash_locked
                     hidden_keys legal_keys

                     lock_ref_keys unlock_ref_keys
                     lock_ref_value unlock_ref_value
                     lock_hashref unlock_hashref
                     lock_ref_keys_plus hashref_locked
                     hidden_ref_keys legal_ref_keys

                     hash_seed hv_store

                    );
our $VERSION    = 0.07;
require DynaLoader;
local @ISA = qw(DynaLoader);
bootstrap Hash::Util $VERSION;

sub import {
    my $class = shift;
    if ( grep /fieldhash/, @_ ) {
        require Hash::Util::FieldHash;
        Hash::Util::FieldHash->import(':all'); # for re-export
    }
    unshift @_, $class;
    goto &Exporter::import;
}


=head1 NAME

Hash::Util - A selection of general-utility hash subroutines

=head1 SYNOPSIS

  # Restricted hashes

  use Hash::Util qw(
                     hash_seed all_keys
                     lock_keys unlock_keys
                     lock_value unlock_value
                     lock_hash unlock_hash
                     lock_keys_plus hash_locked
                     hidden_keys legal_keys
                   );

  %hash = (foo => 42, bar => 23);
  # Ways to restrict a hash
  lock_keys(%hash);
  lock_keys(%hash, @keyset);
  lock_keys_plus(%hash, @additional_keys);

  # Ways to inspect the properties of a restricted hash
  my @legal = legal_keys(%hash);
  my @hidden = hidden_keys(%hash);
  my $ref = all_keys(%hash,@keys,@hidden);
  my $is_locked = hash_locked(%hash);

  # Remove restrictions on the hash
  unlock_keys(%hash);

  # Lock individual values in a hash
  lock_value  (%hash, 'foo');
  unlock_value(%hash, 'foo');

  # Ways to change the restrictions on both keys and values
  lock_hash  (%hash);
  unlock_hash(%hash);

  my $hashes_are_randomised = hash_seed() != 0;

=head1 DESCRIPTION

C<Hash::Util> and C<Hash::Util::FieldHash> contain special functions
for manipulating hashes that don't really warrant a keyword.

C<Hash::Util> contains a set of functions that support
L<restricted hashes|/"Restricted hashes">. These are described in
this document.  C<Hash::Util::FieldHash> contains an (unrelated)
set of functions that support the use of hashes in
I<inside-out classes>, described in L<Hash::Util::FieldHash>.

By default C<Hash::Util> does not export anything.

=head2 Restricted hashes

5.8.0 introduces the ability to restrict a hash to a certain set of
keys.  No keys outside of this set can be added.  It also introduces
the ability to lock an individual key so it cannot be deleted and the
ability to ensure that an individual value cannot be changed.

This is intended to largely replace the deprecated pseudo-hashes.

=over 4

=item B<lock_keys>

=item B<unlock_keys>

  lock_keys(%hash);
  lock_keys(%hash, @keys);

Restricts the given %hash's set of keys to @keys.  If @keys is not
given it restricts it to its current keyset.  No more keys can be
added. delete() and exists() will still work, but will not alter
the set of allowed keys. B<Note>: the current implementation prevents
the hash from being bless()ed while it is in a locked state. Any attempt
to do so will raise an exception. Of course you can still bless()
the hash before you call lock_keys() so this shouldn't be a problem.

  unlock_keys(%hash);

Removes the restriction on the %hash's keyset.

B<Note> that if any of the values of the hash have been locked they will not be unlocked
after this sub executes.

Both routines return a reference to the hash operated on.

=cut

sub lock_ref_keys {
    my($hash, @keys) = @_;

    Internals::hv_clear_placeholders %$hash;
    if( @keys ) {
        my %keys = map { ($_ => 1) } @keys;
        my %original_keys = map { ($_ => 1) } keys %$hash;
        foreach my $k (keys %original_keys) {
            croak "Hash has key '$k' which is not in the new key set"
              unless $keys{$k};
        }

        foreach my $k (@keys) {
            $hash->{$k} = undef unless exists $hash->{$k};
        }
        Internals::SvREADONLY %$hash, 1;

        foreach my $k (@keys) {
            delete $hash->{$k} unless $original_keys{$k};
        }
    }
    else {
        Internals::SvREADONLY %$hash, 1;
    }

    return $hash;
}

sub unlock_ref_keys {
    my $hash = shift;

    Internals::SvREADONLY %$hash, 0;
    return $hash;
}

sub   lock_keys (\%;@) {   lock_ref_keys(@_) }
sub unlock_keys (\%)   { unlock_ref_keys(@_) }

=item B<lock_keys_plus>

  lock_keys_plus(%hash,@additional_keys)

Similar to C<lock_keys()>, with the difference being that the optional key list
specifies keys that may or may not be already in the hash. Essentially this is
an easier way to say

  lock_keys(%hash,@additional_keys,keys %hash);

Returns a reference to %hash

=cut


sub lock_ref_keys_plus {
    my ($hash,@keys)=@_;
    my @delete;
    Internals::hv_clear_placeholders(%$hash);
    foreach my $key (@keys) {
        unless (exists($hash->{$key})) {
            $hash->{$key}=undef;
            push @delete,$key;
        }
    }
    Internals::SvREADONLY(%$hash,1);
    delete @{$hash}{@delete};
    return $hash
}

sub lock_keys_plus(\%;@) { lock_ref_keys_plus(@_) }


=item B<lock_value>

=item B<unlock_value>

  lock_value  (%hash, $key);
  unlock_value(%hash, $key);

Locks and unlocks the value for an individual key of a hash.  The value of a
locked key cannot be changed.

Unless %hash has already been locked the key/value could be deleted
regardless of this setting.

Returns a reference to the %hash.

=cut

sub lock_ref_value {
    my($hash, $key) = @_;
    # I'm doubtful about this warning, as it seems not to be true.
    # Marking a value in the hash as RO is useful, regardless
    # of the status of the hash itself.
    carp "Cannot usefully lock values in an unlocked hash"
      if !Internals::SvREADONLY(%$hash) && warnings::enabled;
    Internals::SvREADONLY $hash->{$key}, 1;
    return $hash
}

sub unlock_ref_value {
    my($hash, $key) = @_;
    Internals::SvREADONLY $hash->{$key}, 0;
    return $hash
}

sub   lock_value (\%$) {   lock_ref_value(@_) }
sub unlock_value (\%$) { unlock_ref_value(@_) }


=item B<lock_hash>

=item B<unlock_hash>

    lock_hash(%hash);

lock_hash() locks an entire hash, making all keys and values read-only.
No value can be changed, no keys can be added or deleted.

    unlock_hash(%hash);

unlock_hash() does the opposite of lock_hash().  All keys and values
are made writable.  All values can be changed and keys can be added
and deleted.

Returns a reference to the %hash.

=cut

sub lock_hashref {
    my $hash = shift;

    lock_ref_keys($hash);

    foreach my $value (values %$hash) {
        Internals::SvREADONLY($value,1);
    }

    return $hash;
}

sub unlock_hashref {
    my $hash = shift;

    foreach my $value (values %$hash) {
        Internals::SvREADONLY($value, 0);
    }

    unlock_ref_keys($hash);

    return $hash;
}

sub   lock_hash (\%) {   lock_hashref(@_) }
sub unlock_hash (\%) { unlock_hashref(@_) }

=item B<lock_hash_recurse>

=item B<unlock_hash_recurse>

    lock_hash_recurse(%hash);

lock_hash() locks an entire hash and any hashes it references recursively,
making all keys and values read-only. No value can be changed, no keys can
be added or deleted.

B<Only> recurses into hashes that are referenced by another hash. Thus a
Hash of Hashes (HoH) will all be restricted, but a Hash of Arrays of Hashes
(HoAoH) will only have the top hash restricted.

    unlock_hash_recurse(%hash);

unlock_hash_recurse() does the opposite of lock_hash_recurse().  All keys and
values are made writable.  All values can be changed and keys can be added
and deleted. Identical recursion restrictions apply as to lock_hash_recurse().

Returns a reference to the %hash.

=cut

sub lock_hashref_recurse {
    my $hash = shift;

    lock_ref_keys($hash);
    foreach my $value (values %$hash) {
        if (reftype($value) eq 'HASH') {
            lock_hashref_recurse($value);
        }
        Internals::SvREADONLY($value,1);
    }
    return $hash
}

sub unlock_hashref_recurse {
    my $hash = shift;

    foreach my $value (values %$hash) {
        if (reftype($value) eq 'HASH') {
            unlock_hashref_recurse($value);
        }
        Internals::SvREADONLY($value,1);
    }
    unlock_ref_keys($hash);
    return $hash;
}

sub   lock_hash_recurse (\%) {   lock_hashref_recurse(@_) }
sub unlock_hash_recurse (\%) { unlock_hashref_recurse(@_) }


=item B<hash_unlocked>

  hash_unlocked(%hash) and print "Hash is unlocked!\n";

Returns true if the hash and its keys are unlocked.

=cut

sub hashref_unlocked {
    my $hash=shift;
    return Internals::SvREADONLY($hash)
}

sub hash_unlocked(\%) { hashref_unlocked(@_) }

=for demerphqs_editor
sub legal_ref_keys{}
sub hidden_ref_keys{}
sub all_keys{}

=cut

sub legal_keys(\%) { legal_ref_keys(@_)  }
sub hidden_keys(\%){ hidden_ref_keys(@_) }

=item B<legal_keys>

  my @keys = legal_keys(%hash);

Returns the list of the keys that are legal in a restricted hash.
In the case of an unrestricted hash this is identical to calling
keys(%hash).

=item B<hidden_keys>

  my @keys = hidden_keys(%hash);

Returns the list of the keys that are legal in a restricted hash but
do not have a value associated to them. Thus if 'foo' is a
"hidden" key of the %hash it will return false for both C<defined>
and C<exists> tests.

In the case of an unrestricted hash this will return an empty list.

B<NOTE> this is an experimental feature that is heavily dependent
on the current implementation of restricted hashes. Should the
implementation change, this routine may become meaningless, in which
case it will return an empty list.

=item B<all_keys>

  all_keys(%hash,@keys,@hidden);

Populates the arrays @keys with the all the keys that would pass
an C<exists> tests, and populates @hidden with the remaining legal
keys that have not been utilized.

Returns a reference to the hash.

In the case of an unrestricted hash this will be equivalent to

  $ref = do {
      @keys = keys %hash;
      @hidden = ();
      \%hash
  };

B<NOTE> this is an experimental feature that is heavily dependent
on the current implementation of restricted hashes. Should the
implementation change this routine may become meaningless in which
case it will behave identically to how it would behave on an
unrestricted hash.

=item B<hash_seed>

    my $hash_seed = hash_seed();

hash_seed() returns the seed number used to randomise hash ordering.
Zero means the "traditional" random hash ordering, non-zero means the
new even more random hash ordering introduced in Perl 5.8.1.

B<Note that the hash seed is sensitive information>: by knowing it one
can craft a denial-of-service attack against Perl code, even remotely,
see L<perlsec/"Algorithmic Complexity Attacks"> for more information.
B<Do not disclose the hash seed> to people who don't need to know it.
See also L<perlrun/PERL_HASH_SEED_DEBUG>.

=cut

sub hash_seed () {
    Internals::rehash_seed();
}

=item B<hv_store>

  my $sv = 0;
  hv_store(%hash,$key,$sv) or die "Failed to alias!";
  $hash{$key} = 1;
  print $sv; # prints 1

Stores an alias to a variable in a hash instead of copying the value.

=back

=head2 Operating on references to hashes.

Most subroutines documented in this module have equivalent versions
that operate on references to hashes instead of native hashes.
The following is a list of these subs. They are identical except
in name and in that instead of taking a %hash they take a $hashref,
and additionally are not prototyped.

=over 4

=item lock_ref_keys

=item unlock_ref_keys

=item lock_ref_keys_plus

=item lock_ref_value

=item unlock_ref_value

=item lock_hashref

=item unlock_hashref

=item lock_hashref_recurse

=item unlock_hashref_recurse

=item hash_ref_unlocked

=item legal_ref_keys

=item hidden_ref_keys

=back

=head1 CAVEATS

Note that the trapping of the restricted operations is not atomic:
for example

    eval { %hash = (illegal_key => 1) }

leaves the C<%hash> empty rather than with its original contents.

=head1 BUGS

The interface exposed by this module is very close to the current
implementation of restricted hashes. Over time it is expected that
this behavior will be extended and the interface abstracted further.

=head1 AUTHOR

Michael G Schwern <schwern@pobox.com> on top of code by Nick
Ing-Simmons and Jeffrey Friedl.

hv_store() is from Array::RefElem, Copyright 2000 Gisle Aas.

Additional code by Yves Orton.

=head1 SEE ALSO

L<Scalar::Util>, L<List::Util> and L<perlsec/"Algorithmic Complexity Attacks">.

L<Hash::Util::FieldHash>.

=cut

1;
