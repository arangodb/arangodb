package HTML::Tagset;

use strict;

=head1 NAME

HTML::Tagset - data tables useful in parsing HTML

=head1 VERSION

Version 3.20

=cut

use vars qw( $VERSION );

$VERSION = '3.20';

=head1 SYNOPSIS

  use HTML::Tagset;
  # Then use any of the items in the HTML::Tagset package
  #  as need arises

=head1 DESCRIPTION

This module contains several data tables useful in various kinds of
HTML parsing operations.

Note that all tag names used are lowercase.

In the following documentation, a "hashset" is a hash being used as a
set -- the hash conveys that its keys are there, and the actual values
associated with the keys are not significant.  (But what values are
there, are always true.)

=cut

use vars qw(
    $VERSION
    %emptyElement %optionalEndTag %linkElements %boolean_attr
    %isHeadElement %isBodyElement %isPhraseMarkup
    %is_Possible_Strict_P_Content
    %isHeadOrBodyElement
    %isList %isTableElement %isFormElement
    %isKnown %canTighten
    @p_closure_barriers
    %isCDATA_Parent
);

=head1 VARIABLES

Note that none of these variables are exported.

=head2 hashset %HTML::Tagset::emptyElement

This hashset has as values the tag-names (GIs) of elements that cannot
have content.  (For example, "base", "br", "hr".)  So
C<$HTML::Tagset::emptyElement{'hr'}> exists and is true.
C<$HTML::Tagset::emptyElement{'dl'}> does not exist, and so is not true.

=cut

%emptyElement   = map {; $_ => 1 } qw(base link meta isindex
                                     img br hr wbr
                                     input area param
                                     embed bgsound spacer
                                     basefont col frame
                                     ~comment ~literal
                                     ~declaration ~pi
                                    );
# The "~"-initial names are for pseudo-elements used by HTML::Entities
#  and TreeBuilder

=head2 hashset %HTML::Tagset::optionalEndTag

This hashset lists tag-names for elements that can have content, but whose
end-tags are generally, "safely", omissible.  Example:
C<$HTML::Tagset::emptyElement{'li'}> exists and is true.

=cut

%optionalEndTag = map {; $_ => 1 } qw(p li dt dd); # option th tr td);

=head2 hash %HTML::Tagset::linkElements

Values in this hash are tagnames for elements that might contain
links, and the value for each is a reference to an array of the names
of attributes whose values can be links.

=cut

%linkElements =
(
 'a'       => ['href'],
 'applet'  => ['archive', 'codebase', 'code'],
 'area'    => ['href'],
 'base'    => ['href'],
 'bgsound' => ['src'],
 'blockquote' => ['cite'],
 'body'    => ['background'],
 'del'     => ['cite'],
 'embed'   => ['pluginspage', 'src'],
 'form'    => ['action'],
 'frame'   => ['src', 'longdesc'],
 'iframe'  => ['src', 'longdesc'],
 'ilayer'  => ['background'],
 'img'     => ['src', 'lowsrc', 'longdesc', 'usemap'],
 'input'   => ['src', 'usemap'],
 'ins'     => ['cite'],
 'isindex' => ['action'],
 'head'    => ['profile'],
 'layer'   => ['background', 'src'],
 'link'    => ['href'],
 'object'  => ['classid', 'codebase', 'data', 'archive', 'usemap'],
 'q'       => ['cite'],
 'script'  => ['src', 'for'],
 'table'   => ['background'],
 'td'      => ['background'],
 'th'      => ['background'],
 'tr'      => ['background'],
 'xmp'     => ['href'],
);

=head2 hash %HTML::Tagset::boolean_attr

This hash (not hashset) lists what attributes of what elements can be
printed without showing the value (for example, the "noshade" attribute
of "hr" elements).  For elements with only one such attribute, its value
is simply that attribute name.  For elements with many such attributes,
the value is a reference to a hashset containing all such attributes.

=cut

%boolean_attr = (
# TODO: make these all hashes
  'area'   => 'nohref',
  'dir'    => 'compact',
  'dl'     => 'compact',
  'hr'     => 'noshade',
  'img'    => 'ismap',
  'input'  => { 'checked' => 1, 'readonly' => 1, 'disabled' => 1 },
  'menu'   => 'compact',
  'ol'     => 'compact',
  'option' => 'selected',
  'select' => 'multiple',
  'td'     => 'nowrap',
  'th'     => 'nowrap',
  'ul'     => 'compact',
);

#==========================================================================
# List of all elements from Extensible HTML version 1.0 Transitional DTD:
#
#   a abbr acronym address applet area b base basefont bdo big
#   blockquote body br button caption center cite code col colgroup
#   dd del dfn dir div dl dt em fieldset font form h1 h2 h3 h4 h5 h6
#   head hr html i iframe img input ins isindex kbd label legend li
#   link map menu meta noframes noscript object ol optgroup option p
#   param pre q s samp script select small span strike strong style
#   sub sup table tbody td textarea tfoot th thead title tr tt u ul
#   var
#
# Varia from Mozilla source internal table of tags:
#   Implemented:
#     xmp listing wbr nobr frame frameset noframes ilayer
#     layer nolayer spacer embed multicol
#   But these are unimplemented:
#     sound??  keygen??  server??
# Also seen here and there:
#     marquee??  app??  (both unimplemented)
#==========================================================================

=head2 hashset %HTML::Tagset::isPhraseMarkup

This hashset contains all phrasal-level elements.

=cut

%isPhraseMarkup = map {; $_ => 1 } qw(
  span abbr acronym q sub sup
  cite code em kbd samp strong var dfn strike
  b i u s tt small big 
  a img br
  wbr nobr blink
  font basefont bdo
  spacer embed noembed
);  # had: center, hr, table


=head2 hashset %HTML::Tagset::is_Possible_Strict_P_Content

This hashset contains all phrasal-level elements that be content of a
P element, for a strict model of HTML.

=cut

%is_Possible_Strict_P_Content = (
 %isPhraseMarkup,
 %isFormElement,
 map {; $_ => 1} qw( object script map )
  # I've no idea why there's these latter exceptions.
  # I'm just following the HTML4.01 DTD.
);

#from html4 strict:
#<!ENTITY % fontstyle "TT | I | B | BIG | SMALL">
#
#<!ENTITY % phrase "EM | STRONG | DFN | CODE |
#                   SAMP | KBD | VAR | CITE | ABBR | ACRONYM" >
#
#<!ENTITY % special
#   "A | IMG | OBJECT | BR | SCRIPT | MAP | Q | SUB | SUP | SPAN | BDO">
#
#<!ENTITY % formctrl "INPUT | SELECT | TEXTAREA | LABEL | BUTTON">
#
#<!-- %inline; covers inline or "text-level" elements -->
#<!ENTITY % inline "#PCDATA | %fontstyle; | %phrase; | %special; | %formctrl;">

=head2 hashset %HTML::Tagset::isHeadElement

This hashset contains all elements that elements that should be
present only in the 'head' element of an HTML document.

=cut

%isHeadElement = map {; $_ => 1 }
 qw(title base link meta isindex script style object bgsound);

=head2 hashset %HTML::Tagset::isList

This hashset contains all elements that can contain "li" elements.

=cut

%isList         = map {; $_ => 1 } qw(ul ol dir menu);

=head2 hashset %HTML::Tagset::isTableElement

This hashset contains all elements that are to be found only in/under
a "table" element.

=cut

%isTableElement = map {; $_ => 1 }
 qw(tr td th thead tbody tfoot caption col colgroup);

=head2 hashset %HTML::Tagset::isFormElement

This hashset contains all elements that are to be found only in/under
a "form" element.

=cut

%isFormElement  = map {; $_ => 1 }
 qw(input select option optgroup textarea button label);

=head2 hashset %HTML::Tagset::isBodyMarkup

This hashset contains all elements that are to be found only in/under
the "body" element of an HTML document.

=cut

%isBodyElement = map {; $_ => 1 } qw(
  h1 h2 h3 h4 h5 h6
  p div pre plaintext address blockquote
  xmp listing
  center

  multicol
  iframe ilayer nolayer
  bgsound

  hr
  ol ul dir menu li
  dl dt dd
  ins del
  
  fieldset legend
  
  map area
  applet param object
  isindex script noscript
  table
  center
  form
 ),
 keys %isFormElement,
 keys %isPhraseMarkup,   # And everything phrasal
 keys %isTableElement,
;


=head2 hashset %HTML::Tagset::isHeadOrBodyElement

This hashset includes all elements that I notice can fall either in
the head or in the body.

=cut

%isHeadOrBodyElement = map {; $_ => 1 }
  qw(script isindex style object map area param noscript bgsound);
  # i.e., if we find 'script' in the 'body' or the 'head', don't freak out.


=head2 hashset %HTML::Tagset::isKnown

This hashset lists all known HTML elements.

=cut

%isKnown = (%isHeadElement, %isBodyElement,
  map{; $_=>1 }
   qw( head body html
       frame frameset noframes
       ~comment ~pi ~directive ~literal
));
 # that should be all known tags ever ever


=head2 hashset %HTML::Tagset::canTighten

This hashset lists elements that might have ignorable whitespace as
children or siblings.

=cut

%canTighten = %isKnown;
delete @canTighten{
  keys(%isPhraseMarkup), 'input', 'select',
  'xmp', 'listing', 'plaintext', 'pre',
};
  # xmp, listing, plaintext, and pre  are untightenable, and
  #   in a really special way.
@canTighten{'hr','br'} = (1,1);
 # exceptional 'phrasal' things that ARE subject to tightening.

# The one case where I can think of my tightening rules failing is:
#  <p>foo bar<center> <em>baz quux</em> ...
#                    ^-- that would get deleted.
# But that's pretty gruesome code anyhow.  You gets what you pays for.

#==========================================================================

=head2 array @HTML::Tagset::p_closure_barriers

This array has a meaning that I have only seen a need for in
C<HTML::TreeBuilder>, but I include it here on the off chance that someone
might find it of use:

When we see a "E<lt>pE<gt>" token, we go lookup up the lineage for a p
element we might have to minimize.  At first sight, we might say that
if there's a p anywhere in the lineage of this new p, it should be
closed.  But that's wrong.  Consider this document:

  <html>
    <head>
      <title>foo</title>
    </head>
    <body>
      <p>foo
        <table>
          <tr>
            <td>
               foo
               <p>bar
            </td>
          </tr>
        </table>
      </p>
    </body>
  </html>

The second p is quite legally inside a much higher p.

My formalization of the reason why this is legal, but this:

  <p>foo<p>bar</p></p>

isn't, is that something about the table constitutes a "barrier" to
the application of the rule about what p must minimize.

So C<@HTML::Tagset::p_closure_barriers> is the list of all such
barrier-tags.

=cut

@p_closure_barriers = qw(
  li blockquote
  ul ol menu dir
  dl dt dd
  td th tr table caption
  div
 );

# In an ideal world (i.e., XHTML) we wouldn't have to bother with any of this
# monkey business of barriers to minimization!

=head2 hashset %isCDATA_Parent

This hashset includes all elements whose content is CDATA.

=cut

%isCDATA_Parent = map {; $_ => 1 }
  qw(script style  xmp listing plaintext);

# TODO: there's nothing else that takes CDATA children, right?

# As the HTML3 DTD (Raggett 1995-04-24) noted:
#   The XMP, LISTING and PLAINTEXT tags are incompatible with SGML
#   and derive from very early versions of HTML. They require non-
#   standard parsers and will cause problems for processing
#   documents with standard SGML tools.


=head1 CAVEATS

You may find it useful to alter the behavior of modules (like
C<HTML::Element> or C<HTML::TreeBuilder>) that use C<HTML::Tagset>'s
data tables by altering the data tables themselves.  You are welcome
to try, but be careful; and be aware that different modules may or may
react differently to the data tables being changed.

Note that it may be inappropriate to use these tables for I<producing>
HTML -- for example, C<%isHeadOrBodyElement> lists the tagnames
for all elements that can appear either in the head or in the body,
such as "script".  That doesn't mean that I am saying your code that
produces HTML should feel free to put script elements in either place!
If you are producing programs that spit out HTML, you should be
I<intimately> familiar with the DTDs for HTML or XHTML (available at
C<http://www.w3.org/>), and you should slavishly obey them, not
the data tables in this document.

=head1 SEE ALSO

L<HTML::Element>, L<HTML::TreeBuilder>, L<HTML::LinkExtor>

=head1 COPYRIGHT & LICENSE

Copyright 1995-2000 Gisle Aas.

Copyright 2000-2005 Sean M. Burke.

Copyright 2005-2008 Andy Lester.

This program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

=head1 ACKNOWLEDGEMENTS

Most of the code/data in this module was adapted from code written
by Gisle Aas for C<HTML::Element>, C<HTML::TreeBuilder>, and
C<HTML::LinkExtor>.  Then it was maintained by Sean M. Burke.

=head1 AUTHOR

Current maintainer: Andy Lester, C<< <andy at petdance.com> >>

=head1 BUGS

Please report any bugs or feature requests to
C<bug-html-tagset at rt.cpan.org>, or through the web interface at
L<http://rt.cpan.org/NoAuth/ReportBug.html?Queue=HTML-Tagset>.  I will
be notified, and then you'll automatically be notified of progress on
your bug as I make changes.

=cut

1;
