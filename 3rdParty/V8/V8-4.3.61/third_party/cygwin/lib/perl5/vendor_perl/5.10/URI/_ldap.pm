# Copyright (c) 1998 Graham Barr <gbarr@pobox.com>. All rights reserved.
# This program is free software; you can redistribute it and/or
# modify it under the same terms as Perl itself.

package URI::_ldap;

use strict;

use vars qw($VERSION);
$VERSION = "1.10";

use URI::Escape qw(uri_unescape);

sub _ldap_elem {
  my $self  = shift;
  my $elem  = shift;
  my $query = $self->query;
  my @bits  = (split(/\?/,defined($query) ? $query : ""),("")x4);
  my $old   = $bits[$elem];

  if (@_) {
    my $new = shift;
    $new =~ s/\?/%3F/g;
    $bits[$elem] = $new;
    $query = join("?",@bits);
    $query =~ s/\?+$//;
    $query = undef unless length($query);
    $self->query($query);
  }

  $old;
}

sub dn {
  my $old = shift->path(@_);
  $old =~ s:^/::;
  uri_unescape($old);
}

sub attributes {
  my $self = shift;
  my $old = _ldap_elem($self,0, @_ ? join(",", map { my $tmp = $_; $tmp =~ s/,/%2C/g; $tmp } @_) : ());
  return $old unless wantarray;
  map { uri_unescape($_) } split(/,/,$old);
}

sub _scope {
  my $self = shift;
  my $old = _ldap_elem($self,1, @_);
  return unless defined wantarray && defined $old;
  uri_unescape($old);
}

sub scope {
  my $old = &_scope;
  $old = "base" unless length $old;
  $old;
}

sub _filter {
  my $self = shift;
  my $old = _ldap_elem($self,2, @_);
  return unless defined wantarray && defined $old;
  uri_unescape($old); # || "(objectClass=*)";
}

sub filter {
  my $old = &_filter;
  $old = "(objectClass=*)" unless length $old;
  $old;
}

sub extensions {
  my $self = shift;
  my @ext;
  while (@_) {
    my $key = shift;
    my $value = shift;
    push(@ext, join("=", map { $_="" unless defined; s/,/%2C/g; $_ } $key, $value));
  }
  @ext = join(",", @ext) if @ext;
  my $old = _ldap_elem($self,3, @ext);
  return $old unless wantarray;
  map { uri_unescape($_) } map { /^([^=]+)=(.*)$/ } split(/,/,$old);
}

sub canonical
{
    my $self = shift;
    my $other = $self->_nonldap_canonical;

    # The stuff below is not as efficient as one might hope...

    $other = $other->clone if $other == $self;

    $other->dn(_normalize_dn($other->dn));

    # Should really know about mixed case "postalAddress", etc...
    $other->attributes(map lc, $other->attributes);

    # Lowecase scope, remove default
    my $old_scope = $other->scope;
    my $new_scope = lc($old_scope);
    $new_scope = "" if $new_scope eq "base";
    $other->scope($new_scope) if $new_scope ne $old_scope;

    # Remove filter if default
    my $old_filter = $other->filter;
    $other->filter("") if lc($old_filter) eq "(objectclass=*)" ||
	                  lc($old_filter) eq "objectclass=*";

    # Lowercase extensions types and deal with known extension values
    my @ext = $other->extensions;
    for (my $i = 0; $i < @ext; $i += 2) {
	my $etype = $ext[$i] = lc($ext[$i]);
	if ($etype =~ /^!?bindname$/) {
	    $ext[$i+1] = _normalize_dn($ext[$i+1]);
	}
    }
    $other->extensions(@ext) if @ext;
    
    $other;
}

sub _normalize_dn  # RFC 2253
{
    my $dn = shift;

    return $dn;
    # The code below will fail if the "+" or "," is embedding in a quoted
    # string or simply escaped...

    my @dn = split(/([+,])/, $dn);
    for (@dn) {
	s/^([a-zA-Z]+=)/lc($1)/e;
    }
    join("", @dn);
}

1;
