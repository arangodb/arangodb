
###
# XML::NamespaceSupport - a simple generic namespace processor
# Robin Berjon <robin@knowscape.com>
###

package XML::NamespaceSupport;
use strict;
use constant FATALS         => 0; # root object
use constant NSMAP          => 1;
use constant UNKNOWN_PREF   => 2;
use constant AUTO_PREFIX    => 3;
use constant XMLNS_11       => 4;
use constant DEFAULT        => 0; # maps
use constant PREFIX_MAP     => 1;
use constant DECLARATIONS   => 2;

use vars qw($VERSION $NS_XMLNS $NS_XML);
$VERSION    = '1.09';
$NS_XMLNS   = 'http://www.w3.org/2000/xmlns/';
$NS_XML     = 'http://www.w3.org/XML/1998/namespace';


# add the ns stuff that baud wants based on Java's xml-writer


#-------------------------------------------------------------------#
# constructor
#-------------------------------------------------------------------#
sub new {
    my $class   = ref($_[0]) ? ref(shift) : shift;
    my $options = shift;
    my $self = [
                1, # FATALS
                [[ # NSMAP
                  undef,              # DEFAULT
                  { xml => $NS_XML }, # PREFIX_MAP
                  undef,              # DECLARATIONS
                ]],
                'aaa', # UNKNOWN_PREF
                0,     # AUTO_PREFIX
                1,     # XML_11
               ];
    $self->[NSMAP]->[0]->[PREFIX_MAP]->{xmlns} = $NS_XMLNS if $options->{xmlns};
    $self->[FATALS] = $options->{fatal_errors} if defined $options->{fatal_errors};
    $self->[AUTO_PREFIX] = $options->{auto_prefix} if defined $options->{auto_prefix};
    $self->[XMLNS_11] = $options->{xmlns_11} if defined $options->{xmlns_11};
    return bless $self, $class;
}
#-------------------------------------------------------------------#

#-------------------------------------------------------------------#
# reset() - return to the original state (for reuse)
#-------------------------------------------------------------------#
sub reset {
    my $self = shift;
    $#{$self->[NSMAP]} = 0;
}
#-------------------------------------------------------------------#

#-------------------------------------------------------------------#
# push_context() - add a new empty context to the stack
#-------------------------------------------------------------------#
sub push_context {
    my $self = shift;
    push @{$self->[NSMAP]}, [
                             $self->[NSMAP]->[-1]->[DEFAULT],
                             { %{$self->[NSMAP]->[-1]->[PREFIX_MAP]} },
                             [],
                            ];
}
#-------------------------------------------------------------------#

#-------------------------------------------------------------------#
# pop_context() - remove the topmost context fromt the stack
#-------------------------------------------------------------------#
sub pop_context {
    my $self = shift;
    die 'Trying to pop context without push context' unless @{$self->[NSMAP]} > 1;
    pop @{$self->[NSMAP]};
}
#-------------------------------------------------------------------#

#-------------------------------------------------------------------#
# declare_prefix() - declare a prefix in the current scope
#-------------------------------------------------------------------#
sub declare_prefix {
    my $self    = shift;
    my $prefix  = shift;
    my $value   = shift;

    warn <<'    EOWARN' unless defined $prefix or $self->[AUTO_PREFIX];
    Prefix was undefined.
    If you wish to set the default namespace, use the empty string ''.
    If you wish to autogenerate prefixes, set the auto_prefix option
    to a true value.
    EOWARN

    no warnings 'uninitialized';
    if ($prefix eq 'xml' and $value ne $NS_XML) {
        die "The xml prefix can only be bound to the $NS_XML namespace."
    }
    elsif ($value eq $NS_XML and $prefix ne 'xml') {
        die "the $NS_XML namespace can only be bound to the xml prefix.";
    }
    elsif ($value eq $NS_XML and $prefix eq 'xml') {
        return 1;
    }
    return 0 if index(lc($prefix), 'xml') == 0;
    use warnings 'uninitialized';

    if (defined $prefix and $prefix eq '') {
        $self->[NSMAP]->[-1]->[DEFAULT] = $value;
    }
    else {
        die "Cannot undeclare prefix $prefix" if $value eq '' and not $self->[XMLNS_11];
        if (not defined $prefix and $self->[AUTO_PREFIX]) {
            while (1) {
                $prefix = $self->[UNKNOWN_PREF]++;
                last if not exists $self->[NSMAP]->[-1]->[PREFIX_MAP]->{$prefix};
            }
        }
        elsif (not defined $prefix and not $self->[AUTO_PREFIX]) {
            return 0;
        }
        $self->[NSMAP]->[-1]->[PREFIX_MAP]->{$prefix} = $value;
    }
    push @{$self->[NSMAP]->[-1]->[DECLARATIONS]}, $prefix;
    return 1;
}
#-------------------------------------------------------------------#

#-------------------------------------------------------------------#
# declare_prefixes() - declare several prefixes in the current scope
#-------------------------------------------------------------------#
sub declare_prefixes {
    my $self     = shift;
    my %prefixes = @_;
    while (my ($k,$v) = each %prefixes) {
        $self->declare_prefix($k,$v);
    }
}
#-------------------------------------------------------------------#

#-------------------------------------------------------------------#
# undeclare_prefix
#-------------------------------------------------------------------#
sub undeclare_prefix {
    my $self   = shift;
    my $prefix = shift;
    return unless not defined $prefix or $prefix eq '';
    return unless exists $self->[NSMAP]->[-1]->[PREFIX_MAP]->{$prefix};

    my ( $tfix ) = grep { $_ eq $prefix } @{$self->[NSMAP]->[-1]->[DECLARATIONS]};
    if ( not defined $tfix ) {
        die "prefix $prefix not declared in this context\n";
    }

    @{$self->[NSMAP]->[-1]->[DECLARATIONS]} = grep { $_ ne $prefix } @{$self->[NSMAP]->[-1]->[DECLARATIONS]};
    delete $self->[NSMAP]->[-1]->[PREFIX_MAP]->{$prefix};
}
#-------------------------------------------------------------------#

#-------------------------------------------------------------------#
# get_prefix() - get a (random) prefix for a given URI
#-------------------------------------------------------------------#
sub get_prefix {
    my $self    = shift;
    my $uri     = shift;

    # we have to iterate over the whole hash here because if we don't
    # the iterator isn't reset and the next pass will fail
    my $pref;
    while (my ($k, $v) = each %{$self->[NSMAP]->[-1]->[PREFIX_MAP]}) {
        $pref = $k if $v eq $uri;
    }
    return $pref;
}
#-------------------------------------------------------------------#

#-------------------------------------------------------------------#
# get_prefixes() - get all the prefixes for a given URI
#-------------------------------------------------------------------#
sub get_prefixes {
    my $self    = shift;
    my $uri     = shift;

    return keys %{$self->[NSMAP]->[-1]->[PREFIX_MAP]} unless defined $uri;
    return grep { $self->[NSMAP]->[-1]->[PREFIX_MAP]->{$_} eq $uri } keys %{$self->[NSMAP]->[-1]->[PREFIX_MAP]};
}
#-------------------------------------------------------------------#

#-------------------------------------------------------------------#
# get_declared_prefixes() - get all prefixes declared in the last context
#-------------------------------------------------------------------#
sub get_declared_prefixes {
    return @{$_[0]->[NSMAP]->[-1]->[DECLARATIONS]};
}
#-------------------------------------------------------------------#

#-------------------------------------------------------------------#
# get_uri() - get an URI given a prefix
#-------------------------------------------------------------------#
sub get_uri {
    my $self    = shift;
    my $prefix  = shift;

    warn "Prefix must not be undef in get_uri(). The emtpy prefix must be ''" unless defined $prefix;

    return $self->[NSMAP]->[-1]->[DEFAULT] if $prefix eq '';
    return $self->[NSMAP]->[-1]->[PREFIX_MAP]->{$prefix} if exists $self->[NSMAP]->[-1]->[PREFIX_MAP]->{$prefix};
    return undef;
}
#-------------------------------------------------------------------#

#-------------------------------------------------------------------#
# process_name() - provide details on a name
#-------------------------------------------------------------------#
sub process_name {
    my $self    = shift;
    my $qname   = shift;
    my $aflag   = shift;

    if ($self->[FATALS]) {
        return( ($self->_get_ns_details($qname, $aflag))[0,2], $qname );
    }
    else {
        eval { return( ($self->_get_ns_details($qname, $aflag))[0,2], $qname ); }
    }
}
#-------------------------------------------------------------------#

#-------------------------------------------------------------------#
# process_element_name() - provide details on a element's name
#-------------------------------------------------------------------#
sub process_element_name {
    my $self    = shift;
    my $qname   = shift;

    if ($self->[FATALS]) {
        return $self->_get_ns_details($qname, 0);
    }
    else {
        eval { return $self->_get_ns_details($qname, 0); }
    }
}
#-------------------------------------------------------------------#


#-------------------------------------------------------------------#
# process_attribute_name() - provide details on a attribute's name
#-------------------------------------------------------------------#
sub process_attribute_name {
    my $self    = shift;
    my $qname   = shift;

    if ($self->[FATALS]) {
        return $self->_get_ns_details($qname, 1);
    }
    else {
        eval { return $self->_get_ns_details($qname, 1); }
    }
}
#-------------------------------------------------------------------#


#-------------------------------------------------------------------#
# ($ns, $prefix, $lname) = $self->_get_ns_details($qname, $f_attr)
# returns ns, prefix, and lname for a given attribute name
# >> the $f_attr flag, if set to one, will work for an attribute
#-------------------------------------------------------------------#
sub _get_ns_details {
    my $self    = shift;
    my $qname   = shift;
    my $aflag   = shift;

    my ($ns, $prefix, $lname);
    (my ($tmp_prefix, $tmp_lname) = split /:/, $qname, 3)
                                    < 3 or die "Invalid QName: $qname";

    # no prefix
    my $cur_map = $self->[NSMAP]->[-1];
    if (not defined($tmp_lname)) {
        $prefix = undef;
        $lname = $qname;
        # attr don't have a default namespace
        $ns = ($aflag) ? undef : $cur_map->[DEFAULT];
    }

    # prefix
    else {
        if (exists $cur_map->[PREFIX_MAP]->{$tmp_prefix}) {
            $prefix = $tmp_prefix;
            $lname  = $tmp_lname;
            $ns     = $cur_map->[PREFIX_MAP]->{$prefix}
        }
        else { # no ns -> lname == name, all rest undef
            die "Undeclared prefix: $tmp_prefix";
        }
    }

    return ($ns, $prefix, $lname);
}
#-------------------------------------------------------------------#

#-------------------------------------------------------------------#
# parse_jclark_notation() - parse the Clarkian notation
#-------------------------------------------------------------------#
sub parse_jclark_notation {
    shift;
    my $jc = shift;
    $jc =~ m/^\{(.*)\}([^}]+)$/;
    return $1, $2;
}
#-------------------------------------------------------------------#


#-------------------------------------------------------------------#
# Java names mapping
#-------------------------------------------------------------------#
*XML::NamespaceSupport::pushContext          = \&push_context;
*XML::NamespaceSupport::popContext           = \&pop_context;
*XML::NamespaceSupport::declarePrefix        = \&declare_prefix;
*XML::NamespaceSupport::declarePrefixes      = \&declare_prefixes;
*XML::NamespaceSupport::getPrefix            = \&get_prefix;
*XML::NamespaceSupport::getPrefixes          = \&get_prefixes;
*XML::NamespaceSupport::getDeclaredPrefixes  = \&get_declared_prefixes;
*XML::NamespaceSupport::getURI               = \&get_uri;
*XML::NamespaceSupport::processName          = \&process_name;
*XML::NamespaceSupport::processElementName   = \&process_element_name;
*XML::NamespaceSupport::processAttributeName = \&process_attribute_name;
*XML::NamespaceSupport::parseJClarkNotation  = \&parse_jclark_notation;
*XML::NamespaceSupport::undeclarePrefix      = \&undeclare_prefix;
#-------------------------------------------------------------------#


1;
#,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,#
#`,`, Documentation `,`,`,`,`,`,`,`,`,`,`,`,`,`,`,`,`,`,`,`,`,`,`,`,#
#```````````````````````````````````````````````````````````````````#

=pod

=head1 NAME

XML::NamespaceSupport - a simple generic namespace support class

=head1 SYNOPSIS

  use XML::NamespaceSupport;
  my $nsup = XML::NamespaceSupport->new;

  # add a new empty context
  $nsup->push_context;
  # declare a few prefixes
  $nsup->declare_prefix($prefix1, $uri1);
  $nsup->declare_prefix($prefix2, $uri2);
  # the same shorter
  $nsup->declare_prefixes($prefix1 => $uri1, $prefix2 => $uri2);

  # get a single prefix for a URI (randomly)
  $prefix = $nsup->get_prefix($uri);
  # get all prefixes for a URI (probably better)
  @prefixes = $nsup->get_prefixes($uri);
  # get all prefixes in scope
  @prefixes = $nsup->get_prefixes();
  # get all prefixes that were declared for the current scope
  @prefixes = $nsup->get_declared_prefixes;
  # get a URI for a given prefix
  $uri = $nsup->get_uri($prefix);

  # get info on a qname (java-ish way, it's a bit weird)
  ($ns_uri, $local_name, $qname) = $nsup->process_name($qname, $is_attr);
  # the same, more perlish
  ($ns_uri, $prefix, $local_name) = $nsup->process_element_name($qname);
  ($ns_uri, $prefix, $local_name) = $nsup->process_attribute_name($qname);

  # remove the current context
  $nsup->pop_context;

  # reset the object for reuse in another document
  $nsup->reset;

  # a simple helper to process Clarkian Notation
  my ($ns, $lname) = $nsup->parse_jclark_notation('{http://foo}bar');
  # or (given that it doesn't care about the object
  my ($ns, $lname) = XML::NamespaceSupport->parse_jclark_notation('{http://foo}bar');


=head1 DESCRIPTION

This module offers a simple to process namespaced XML names (unames)
from within any application that may need them. It also helps maintain
a prefix to namespace URI map, and provides a number of basic checks.

The model for this module is SAX2's NamespaceSupport class, readable at
http://www.megginson.com/SAX/Java/javadoc/org/xml/sax/helpers/NamespaceSupport.html.
It adds a few perlisations where we thought it appropriate.

=head1 METHODS

=over 4

=item * XML::NamespaceSupport->new(\%options)

A simple constructor.

The options are C<xmlns>, C<fatal_errors>, and C<auto_prefix>

If C<xmlns> is turned on (it is off by default) the mapping from the
xmlns prefix to the URI defined for it in DOM level 2 is added to the
list of predefined mappings (which normally only contains the xml
prefix mapping).

If C<fatal_errors> is turned off (it is on by default) a number of
validity errors will simply be flagged as failures, instead of
die()ing.

If C<auto_prefix> is turned on (it is off by default) when one
provides a prefix of C<undef> to C<declare_prefix> it will generate a
random prefix mapped to that namespace. Otherwise an undef prefix will
trigger a warning (you should probably know what you're doing if you
turn this option on).

If C<xmlns_11> us turned off, it becomes illegal to undeclare namespace
prefixes. It is on by default. This behaviour is compliant with Namespaces
in XML 1.1, turning it off reverts you to version 1.0.

=item * $nsup->push_context

Adds a new empty context to the stack. You can then populate it with
new prefixes defined at this level.

=item * $nsup->pop_context

Removes the topmost context in the stack and reverts to the previous
one. It will die() if you try to pop more than you have pushed.

=item * $nsup->declare_prefix($prefix, $uri)

Declares a mapping of $prefix to $uri, at the current level.

Note that with C<auto_prefix> turned on, if you declare a prefix
mapping in which $prefix is undef(), you will get an automatic prefix
selected for you. If it is off you will get a warning.

This is useful when you deal with code that hasn't kept prefixes around
and need to reserialize the nodes. It also means that if you want to
set the default namespace (ie with an empty prefix) you must use the
empty string instead of undef. This behaviour is consistent with the
SAX 2.0 specification.

=item * $nsup->declare_prefixes(%prefixes2uris)

Declares a mapping of several prefixes to URIs, at the current level.

=item * $nsup->get_prefix($uri)

Returns a prefix given an URI. Note that as several prefixes may be
mapped to the same URI, it returns an arbitrary one. It'll return
undef on failure.

=item * $nsup->get_prefixes($uri)

Returns an array of prefixes given an URI. It'll return all the
prefixes if the uri is undef.

=item * $nsup->get_declared_prefixes

Returns an array of all the prefixes that have been declared within
this context, ie those that were declared on the last element, not
those that were declared above and are simply in scope.

=item * $nsup->get_uri($prefix)

Returns a URI for a given prefix. Returns undef on failure.

=item * $nsup->process_name($qname, $is_attr)

Given a qualified name and a boolean indicating whether this is an
attribute or another type of name (those are differently affected by
default namespaces), it returns a namespace URI, local name, qualified
name tuple. I know that that is a rather abnormal list to return, but
it is so for compatibility with the Java spec. See below for more
Perlish alternatives.

If the prefix is not declared, or if the name is not valid, it'll
either die or return undef depending on the current setting of
C<fatal_errors>.

=item * $nsup->undeclare_prefix($prefix);

Removes a namespace prefix from the current context. This function may
be used in SAX's end_prefix_mapping when there is fear that a namespace
declaration might be available outside their scope (which shouldn't
normally happen, but you never know ;). This may be needed in order to
properly support Namespace 1.1.

=item * $nsup->process_element_name($qname)

Given a qualified name, it returns a namespace URI, prefix, and local
name tuple. This method applies to element names.

If the prefix is not declared, or if the name is not valid, it'll
either die or return undef depending on the current setting of
C<fatal_errors>.

=item * $nsup->process_attribute_name($qname)

Given a qualified name, it returns a namespace URI, prefix, and local
name tuple. This method applies to attribute names.

If the prefix is not declared, or if the name is not valid, it'll
either die or return undef depending on the current setting of
C<fatal_errors>.

=item * $nsup->reset

Resets the object so that it can be reused on another document.

=back

All methods of the interface have an alias that is the name used in
the original Java specification. You can use either name
interchangeably. Here is the mapping:

  Java name                 Perl name
  ---------------------------------------------------
  pushContext               push_context
  popContext                pop_context
  declarePrefix             declare_prefix
  declarePrefixes           declare_prefixes
  getPrefix                 get_prefix
  getPrefixes               get_prefixes
  getDeclaredPrefixes       get_declared_prefixes
  getURI                    get_uri
  processName               process_name
  processElementName        process_element_name
  processAttributeName      process_attribute_name
  parseJClarkNotation       parse_jclark_notation
  undeclarePrefix           undeclare_prefix

=head1 VARIABLES

Two global variables are made available to you. They used to be constants but
simple scalars are easier to use in a number of contexts. They are not
exported but can easily be accessed from any package, or copied into it.

=over 4

=item * C<$NS_XMLNS>

The namespace for xmlns prefixes, http://www.w3.org/2000/xmlns/.

=item * C<$NS_XML>

The namespace for xml prefixes, http://www.w3.org/XML/1998/namespace.

=back

=head1 TODO

 - add more tests
 - optimise here and there

=head1 AUTHOR

Robin Berjon, robin@knowscape.com, with lots of it having been done
by Duncan Cameron, and a number of suggestions from the perl-xml
list.

=head1 COPYRIGHT

Copyright (c) 2001-2005 Robin Berjon. All rights reserved. This program is
free software; you can redistribute it and/or modify it under the same terms
as Perl itself.

=head1 SEE ALSO

XML::Parser::PerlSAX

=cut

