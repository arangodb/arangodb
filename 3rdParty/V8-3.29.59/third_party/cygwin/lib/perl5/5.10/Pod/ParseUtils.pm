#############################################################################
# Pod/ParseUtils.pm -- helpers for POD parsing and conversion
#
# Copyright (C) 1999-2000 by Marek Rouchal. All rights reserved.
# This file is part of "PodParser". PodParser is free software;
# you can redistribute it and/or modify it under the same terms
# as Perl itself.
#############################################################################

package Pod::ParseUtils;

use vars qw($VERSION);
$VERSION = 1.35;   ## Current version of this package
require  5.005;    ## requires this Perl version or later

=head1 NAME

Pod::ParseUtils - helpers for POD parsing and conversion

=head1 SYNOPSIS

  use Pod::ParseUtils;

  my $list = new Pod::List;
  my $link = Pod::Hyperlink->new('Pod::Parser');

=head1 DESCRIPTION

B<Pod::ParseUtils> contains a few object-oriented helper packages for
POD parsing and processing (i.e. in POD formatters and translators).

=cut

#-----------------------------------------------------------------------------
# Pod::List
#
# class to hold POD list info (=over, =item, =back)
#-----------------------------------------------------------------------------

package Pod::List;

use Carp;

=head2 Pod::List

B<Pod::List> can be used to hold information about POD lists
(written as =over ... =item ... =back) for further processing.
The following methods are available:

=over 4

=item Pod::List-E<gt>new()

Create a new list object. Properties may be specified through a hash
reference like this:

  my $list = Pod::List->new({ -start => $., -indent => 4 });

See the individual methods/properties for details.

=cut

sub new {
    my $this = shift;
    my $class = ref($this) || $this;
    my %params = @_;
    my $self = {%params};
    bless $self, $class;
    $self->initialize();
    return $self;
}

sub initialize {
    my $self = shift;
    $self->{-file} ||= 'unknown';
    $self->{-start} ||= 'unknown';
    $self->{-indent} ||= 4; # perlpod: "should be the default"
    $self->{_items} = [];
    $self->{-type} ||= '';
}

=item $list-E<gt>file()

Without argument, retrieves the file name the list is in. This must
have been set before by either specifying B<-file> in the B<new()>
method or by calling the B<file()> method with a scalar argument.

=cut

# The POD file name the list appears in
sub file {
   return (@_ > 1) ? ($_[0]->{-file} = $_[1]) : $_[0]->{-file};
}

=item $list-E<gt>start()

Without argument, retrieves the line number where the list started.
This must have been set before by either specifying B<-start> in the
B<new()> method or by calling the B<start()> method with a scalar
argument.

=cut

# The line in the file the node appears
sub start {
   return (@_ > 1) ? ($_[0]->{-start} = $_[1]) : $_[0]->{-start};
}

=item $list-E<gt>indent()

Without argument, retrieves the indent level of the list as specified
in C<=over n>. This must have been set before by either specifying
B<-indent> in the B<new()> method or by calling the B<indent()> method
with a scalar argument.

=cut

# indent level
sub indent {
   return (@_ > 1) ? ($_[0]->{-indent} = $_[1]) : $_[0]->{-indent};
}

=item $list-E<gt>type()

Without argument, retrieves the list type, which can be an arbitrary value,
e.g. C<OL>, C<UL>, ... when thinking the HTML way.
This must have been set before by either specifying
B<-type> in the B<new()> method or by calling the B<type()> method
with a scalar argument.

=cut

# The type of the list (UL, OL, ...)
sub type {
   return (@_ > 1) ? ($_[0]->{-type} = $_[1]) : $_[0]->{-type};
}

=item $list-E<gt>rx()

Without argument, retrieves a regular expression for simplifying the 
individual item strings once the list type has been determined. Usage:
E.g. when converting to HTML, one might strip the leading number in
an ordered list as C<E<lt>OLE<gt>> already prints numbers itself.
This must have been set before by either specifying
B<-rx> in the B<new()> method or by calling the B<rx()> method
with a scalar argument.

=cut

# The regular expression to simplify the items
sub rx {
   return (@_ > 1) ? ($_[0]->{-rx} = $_[1]) : $_[0]->{-rx};
}

=item $list-E<gt>item()

Without argument, retrieves the array of the items in this list.
The items may be represented by any scalar.
If an argument has been given, it is pushed on the list of items.

=cut

# The individual =items of this list
sub item {
    my ($self,$item) = @_;
    if(defined $item) {
        push(@{$self->{_items}}, $item);
        return $item;
    }
    else {
        return @{$self->{_items}};
    }
}

=item $list-E<gt>parent()

Without argument, retrieves information about the parent holding this
list, which is represented as an arbitrary scalar.
This must have been set before by either specifying
B<-parent> in the B<new()> method or by calling the B<parent()> method
with a scalar argument.

=cut

# possibility for parsers/translators to store information about the
# lists's parent object
sub parent {
   return (@_ > 1) ? ($_[0]->{-parent} = $_[1]) : $_[0]->{-parent};
}

=item $list-E<gt>tag()

Without argument, retrieves information about the list tag, which can be
any scalar.
This must have been set before by either specifying
B<-tag> in the B<new()> method or by calling the B<tag()> method
with a scalar argument.

=back

=cut

# possibility for parsers/translators to store information about the
# list's object
sub tag {
   return (@_ > 1) ? ($_[0]->{-tag} = $_[1]) : $_[0]->{-tag};
}

#-----------------------------------------------------------------------------
# Pod::Hyperlink
#
# class to manipulate POD hyperlinks (L<>)
#-----------------------------------------------------------------------------

package Pod::Hyperlink;

=head2 Pod::Hyperlink

B<Pod::Hyperlink> is a class for manipulation of POD hyperlinks. Usage:

  my $link = Pod::Hyperlink->new('alternative text|page/"section in page"');

The B<Pod::Hyperlink> class is mainly designed to parse the contents of the
C<LE<lt>...E<gt>> sequence, providing a simple interface for accessing the
different parts of a POD hyperlink for further processing. It can also be
used to construct hyperlinks.

=over 4

=item Pod::Hyperlink-E<gt>new()

The B<new()> method can either be passed a set of key/value pairs or a single
scalar value, namely the contents of a C<LE<lt>...E<gt>> sequence. An object
of the class C<Pod::Hyperlink> is returned. The value C<undef> indicates a
failure, the error message is stored in C<$@>.

=cut

use Carp;

sub new {
    my $this = shift;
    my $class = ref($this) || $this;
    my $self = +{};
    bless $self, $class;
    $self->initialize();
    if(defined $_[0]) {
        if(ref($_[0])) {
            # called with a list of parameters
            %$self = %{$_[0]};
            $self->_construct_text();
        }
        else {
            # called with L<> contents
            return undef unless($self->parse($_[0]));
        }
    }
    return $self;
}

sub initialize {
    my $self = shift;
    $self->{-line} ||= 'undef';
    $self->{-file} ||= 'undef';
    $self->{-page} ||= '';
    $self->{-node} ||= '';
    $self->{-alttext} ||= '';
    $self->{-type} ||= 'undef';
    $self->{_warnings} = [];
}

=item $link-E<gt>parse($string)

This method can be used to (re)parse a (new) hyperlink, i.e. the contents
of a C<LE<lt>...E<gt>> sequence. The result is stored in the current object.
Warnings are stored in the B<warnings> property.
E.g. sections like C<LE<lt>open(2)E<gt>> are deprecated, as they do not point
to Perl documents. C<LE<lt>DBI::foo(3p)E<gt>> is wrong as well, the manpage
section can simply be dropped.

=cut

sub parse {
    my $self = shift;
    local($_) = $_[0];
    # syntax check the link and extract destination
    my ($alttext,$page,$node,$type,$quoted) = (undef,'','','',0);

    $self->{_warnings} = [];

    # collapse newlines with whitespace
    s/\s*\n+\s*/ /g;

    # strip leading/trailing whitespace
    if(s/^[\s\n]+//) {
        $self->warning("ignoring leading whitespace in link");
    }
    if(s/[\s\n]+$//) {
        $self->warning("ignoring trailing whitespace in link");
    }
    unless(length($_)) {
        _invalid_link("empty link");
        return undef;
    }

    ## Check for different possibilities. This is tedious and error-prone
    # we match all possibilities (alttext, page, section/item)
    #warn "DEBUG: link=$_\n";

    # only page
    # problem: a lot of people use (), or (1) or the like to indicate
    # man page sections. But this collides with L<func()> that is supposed
    # to point to an internal funtion...
    my $page_rx = '[\w.-]+(?:::[\w.-]+)*(?:[(](?:\d\w*|)[)]|)';
    # page name only
    if(m!^($page_rx)$!o) {
        $page = $1;
        $type = 'page';
    }
    # alttext, page and "section"
    elsif(m!^(.*?)\s*[|]\s*($page_rx)\s*/\s*"(.+)"$!o) {
        ($alttext, $page, $node) = ($1, $2, $3);
        $type = 'section';
        $quoted = 1; #... therefore | and / are allowed
    }
    # alttext and page
    elsif(m!^(.*?)\s*[|]\s*($page_rx)$!o) {
        ($alttext, $page) = ($1, $2);
        $type = 'page';
    }
    # alttext and "section"
    elsif(m!^(.*?)\s*[|]\s*(?:/\s*|)"(.+)"$!) {
        ($alttext, $node) = ($1,$2);
        $type = 'section';
        $quoted = 1;
    }
    # page and "section"
    elsif(m!^($page_rx)\s*/\s*"(.+)"$!o) {
        ($page, $node) = ($1, $2);
        $type = 'section';
        $quoted = 1;
    }
    # page and item
    elsif(m!^($page_rx)\s*/\s*(.+)$!o) {
        ($page, $node) = ($1, $2);
        $type = 'item';
    }
    # only "section"
    elsif(m!^/?"(.+)"$!) {
        $node = $1;
        $type = 'section';
        $quoted = 1;
    }
    # only item
    elsif(m!^\s*/(.+)$!) {
        $node = $1;
        $type = 'item';
    }

    # non-standard: Hyperlink with alt-text - doesn't remove protocol prefix, maybe it should?
    elsif(m!^ \s* (.*?) \s* [|] \s* (\w+:[^:\s] [^\s|]*?) \s* $!ix) {
      ($alttext,$node) = ($1,$2);
      $type = 'hyperlink';
    }

    # non-standard: Hyperlink
    elsif(m!^(\w+:[^:\s]\S*)$!i) {
        $node = $1;
        $type = 'hyperlink';
    }
    # alttext, page and item
    elsif(m!^(.*?)\s*[|]\s*($page_rx)\s*/\s*(.+)$!o) {
        ($alttext, $page, $node) = ($1, $2, $3);
        $type = 'item';
    }
    # alttext and item
    elsif(m!^(.*?)\s*[|]\s*/(.+)$!) {
        ($alttext, $node) = ($1,$2);
    }
    # must be an item or a "malformed" section (without "")
    else {
        $node = $_;
        $type = 'item';
    }
    # collapse whitespace in nodes
    $node =~ s/\s+/ /gs;

    # empty alternative text expands to node name
    if(defined $alttext) {
        if(!length($alttext)) {
          $alttext = $node | $page;
        }
    }
    else {
        $alttext = '';
    }

    if($page =~ /[(]\w*[)]$/) {
        $self->warning("(section) in '$page' deprecated");
    }
    if(!$quoted && $node =~ m:[|/]: && $type ne 'hyperlink') {
        $self->warning("node '$node' contains non-escaped | or /");
    }
    if($alttext =~ m:[|/]:) {
        $self->warning("alternative text '$node' contains non-escaped | or /");
    }
    $self->{-page} = $page;
    $self->{-node} = $node;
    $self->{-alttext} = $alttext;
    #warn "DEBUG: page=$page section=$section item=$item alttext=$alttext\n";
    $self->{-type} = $type;
    $self->_construct_text();
    1;
}

sub _construct_text {
    my $self = shift;
    my $alttext = $self->alttext();
    my $type = $self->type();
    my $section = $self->node();
    my $page = $self->page();
    my $page_ext = '';
    $page =~ s/([(]\w*[)])$// && ($page_ext = $1);
    if($alttext) {
        $self->{_text} = $alttext;
    }
    elsif($type eq 'hyperlink') {
        $self->{_text} = $section;
    }
    else {
        $self->{_text} = ($section || '') .
            (($page && $section) ? ' in ' : '') .
            "$page$page_ext";
    }
    # for being marked up later
    # use the non-standard markers P<> and Q<>, so that the resulting
    # text can be parsed by the translators. It's their job to put
    # the correct hypertext around the linktext
    if($alttext) {
        $self->{_markup} = "Q<$alttext>";
    }
    elsif($type eq 'hyperlink') {
        $self->{_markup} = "Q<$section>";
    }
    else {
        $self->{_markup} = (!$section ? '' : "Q<$section>") .
            ($page ? ($section ? ' in ':'') . "P<$page>$page_ext" : '');
    }
}

=item $link-E<gt>markup($string)

Set/retrieve the textual value of the link. This string contains special
markers C<PE<lt>E<gt>> and C<QE<lt>E<gt>> that should be expanded by the
translator's interior sequence expansion engine to the
formatter-specific code to highlight/activate the hyperlink. The details
have to be implemented in the translator.

=cut

#' retrieve/set markuped text
sub markup {
    return (@_ > 1) ? ($_[0]->{_markup} = $_[1]) : $_[0]->{_markup};
}

=item $link-E<gt>text()

This method returns the textual representation of the hyperlink as above,
but without markers (read only). Depending on the link type this is one of
the following alternatives (the + and * denote the portions of the text
that are marked up):

  +perl+                    L<perl>
  *$|* in +perlvar+         L<perlvar/$|>
  *OPTIONS* in +perldoc+    L<perldoc/"OPTIONS">
  *DESCRIPTION*             L<"DESCRIPTION">

=cut

# The complete link's text
sub text {
    $_[0]->{_text};
}

=item $link-E<gt>warning()

After parsing, this method returns any warnings encountered during the
parsing process.

=cut

# Set/retrieve warnings
sub warning {
    my $self = shift;
    if(@_) {
        push(@{$self->{_warnings}}, @_);
        return @_;
    }
    return @{$self->{_warnings}};
}

=item $link-E<gt>file()

=item $link-E<gt>line()

Just simple slots for storing information about the line and the file
the link was encountered in. Has to be filled in manually.

=cut

# The line in the file the link appears
sub line {
    return (@_ > 1) ? ($_[0]->{-line} = $_[1]) : $_[0]->{-line};
}

# The POD file name the link appears in
sub file {
    return (@_ > 1) ? ($_[0]->{-file} = $_[1]) : $_[0]->{-file};
}

=item $link-E<gt>page()

This method sets or returns the POD page this link points to.

=cut

# The POD page the link appears on
sub page {
    if (@_ > 1) {
        $_[0]->{-page} = $_[1];
        $_[0]->_construct_text();
    }
    $_[0]->{-page};
}

=item $link-E<gt>node()

As above, but the destination node text of the link.

=cut

# The link destination
sub node {
    if (@_ > 1) {
        $_[0]->{-node} = $_[1];
        $_[0]->_construct_text();
    }
    $_[0]->{-node};
}

=item $link-E<gt>alttext()

Sets or returns an alternative text specified in the link.

=cut

# Potential alternative text
sub alttext {
    if (@_ > 1) {
        $_[0]->{-alttext} = $_[1];
        $_[0]->_construct_text();
    }
    $_[0]->{-alttext};
}

=item $link-E<gt>type()

The node type, either C<section> or C<item>. As an unofficial type,
there is also C<hyperlink>, derived from e.g. C<LE<lt>http://perl.comE<gt>>

=cut

# The type: item or headn
sub type {
    return (@_ > 1) ? ($_[0]->{-type} = $_[1]) : $_[0]->{-type};
}

=item $link-E<gt>link()

Returns the link as contents of C<LE<lt>E<gt>>. Reciprocal to B<parse()>.

=back

=cut

# The link itself
sub link {
    my $self = shift;
    my $link = $self->page() || '';
    if($self->node()) {
        my $node = $self->node();
        $text =~ s/\|/E<verbar>/g;
        $text =~ s:/:E<sol>:g;
        if($self->type() eq 'section') {
            $link .= ($link ? '/' : '') . '"' . $node . '"';
        }
        elsif($self->type() eq 'hyperlink') {
            $link = $self->node();
        }
        else { # item
            $link .= '/' . $node;
        }
    }
    if($self->alttext()) {
        my $text = $self->alttext();
        $text =~ s/\|/E<verbar>/g;
        $text =~ s:/:E<sol>:g;
        $link = "$text|$link";
    }
    $link;
}

sub _invalid_link {
    my ($msg) = @_;
    # this sets @_
    #eval { die "$msg\n" };
    #chomp $@;
    $@ = $msg; # this seems to work, too!
    undef;
}

#-----------------------------------------------------------------------------
# Pod::Cache
#
# class to hold POD page details
#-----------------------------------------------------------------------------

package Pod::Cache;

=head2 Pod::Cache

B<Pod::Cache> holds information about a set of POD documents,
especially the nodes for hyperlinks.
The following methods are available:

=over 4

=item Pod::Cache-E<gt>new()

Create a new cache object. This object can hold an arbitrary number of
POD documents of class Pod::Cache::Item.

=cut

sub new {
    my $this = shift;
    my $class = ref($this) || $this;
    my $self = [];
    bless $self, $class;
    return $self;
}

=item $cache-E<gt>item()

Add a new item to the cache. Without arguments, this method returns a
list of all cache elements.

=cut

sub item {
    my ($self,%param) = @_;
    if(%param) {
        my $item = Pod::Cache::Item->new(%param);
        push(@$self, $item);
        return $item;
    }
    else {
        return @{$self};
    }
}

=item $cache-E<gt>find_page($name)

Look for a POD document named C<$name> in the cache. Returns the
reference to the corresponding Pod::Cache::Item object or undef if
not found.

=back

=cut

sub find_page {
    my ($self,$page) = @_;
    foreach(@$self) {
        if($_->page() eq $page) {
            return $_;
        }
    }
    undef;
}

package Pod::Cache::Item;

=head2 Pod::Cache::Item

B<Pod::Cache::Item> holds information about individual POD documents,
that can be grouped in a Pod::Cache object.
It is intended to hold information about the hyperlink nodes of POD
documents.
The following methods are available:

=over 4

=item Pod::Cache::Item-E<gt>new()

Create a new object.

=cut

sub new {
    my $this = shift;
    my $class = ref($this) || $this;
    my %params = @_;
    my $self = {%params};
    bless $self, $class;
    $self->initialize();
    return $self;
}

sub initialize {
    my $self = shift;
    $self->{-nodes} = [] unless(defined $self->{-nodes});
}

=item $cacheitem-E<gt>page()

Set/retrieve the POD document name (e.g. "Pod::Parser").

=cut

# The POD page
sub page {
   return (@_ > 1) ? ($_[0]->{-page} = $_[1]) : $_[0]->{-page};
}

=item $cacheitem-E<gt>description()

Set/retrieve the POD short description as found in the C<=head1 NAME>
section.

=cut

# The POD description, taken out of NAME if present
sub description {
   return (@_ > 1) ? ($_[0]->{-description} = $_[1]) : $_[0]->{-description};
}

=item $cacheitem-E<gt>path()

Set/retrieve the POD file storage path.

=cut

# The file path
sub path {
   return (@_ > 1) ? ($_[0]->{-path} = $_[1]) : $_[0]->{-path};
}

=item $cacheitem-E<gt>file()

Set/retrieve the POD file name.

=cut

# The POD file name
sub file {
   return (@_ > 1) ? ($_[0]->{-file} = $_[1]) : $_[0]->{-file};
}

=item $cacheitem-E<gt>nodes()

Add a node (or a list of nodes) to the document's node list. Note that
the order is kept, i.e. start with the first node and end with the last.
If no argument is given, the current list of nodes is returned in the
same order the nodes have been added.
A node can be any scalar, but usually is a pair of node string and
unique id for the C<find_node> method to work correctly.

=cut

# The POD nodes
sub nodes {
    my ($self,@nodes) = @_;
    if(@nodes) {
        push(@{$self->{-nodes}}, @nodes);
        return @nodes;
    }
    else {
        return @{$self->{-nodes}};
    }
}

=item $cacheitem-E<gt>find_node($name)

Look for a node or index entry named C<$name> in the object.
Returns the unique id of the node (i.e. the second element of the array
stored in the node array) or undef if not found.

=cut

sub find_node {
    my ($self,$node) = @_;
    my @search;
    push(@search, @{$self->{-nodes}}) if($self->{-nodes});
    push(@search, @{$self->{-idx}}) if($self->{-idx});
    foreach(@search) {
        if($_->[0] eq $node) {
            return $_->[1]; # id
        }
    }
    undef;
}

=item $cacheitem-E<gt>idx()

Add an index entry (or a list of them) to the document's index list. Note that
the order is kept, i.e. start with the first node and end with the last.
If no argument is given, the current list of index entries is returned in the
same order the entries have been added.
An index entry can be any scalar, but usually is a pair of string and
unique id.

=back

=cut

# The POD index entries
sub idx {
    my ($self,@idx) = @_;
    if(@idx) {
        push(@{$self->{-idx}}, @idx);
        return @idx;
    }
    else {
        return @{$self->{-idx}};
    }
}

=head1 AUTHOR

Please report bugs using L<http://rt.cpan.org>.

Marek Rouchal E<lt>marekr@cpan.orgE<gt>, borrowing
a lot of things from L<pod2man> and L<pod2roff> as well as other POD
processing tools by Tom Christiansen, Brad Appleton and Russ Allbery.

=head1 SEE ALSO

L<pod2man>, L<pod2roff>, L<Pod::Parser>, L<Pod::Checker>,
L<pod2html>

=cut

1;
