package HTML::Form;

use strict;
use URI;
use Carp ();

use vars qw($VERSION);
$VERSION = "5.813";

my %form_tags = map {$_ => 1} qw(input textarea button select option);

my %type2class = (
 text     => "TextInput",
 password => "TextInput",
 hidden   => "TextInput",
 textarea => "TextInput",

 "reset"  => "IgnoreInput",

 radio    => "ListInput",
 checkbox => "ListInput",
 option   => "ListInput",

 button   => "SubmitInput",
 submit   => "SubmitInput",
 image    => "ImageInput",
 file     => "FileInput",

 keygen   => "KeygenInput",
);

=head1 NAME

HTML::Form - Class that represents an HTML form element

=head1 SYNOPSIS

 use HTML::Form;
 $form = HTML::Form->parse($html, $base_uri);
 $form->value(query => "Perl");

 use LWP::UserAgent;
 $ua = LWP::UserAgent->new;
 $response = $ua->request($form->click);

=head1 DESCRIPTION

Objects of the C<HTML::Form> class represents a single HTML
C<E<lt>formE<gt> ... E<lt>/formE<gt>> instance.  A form consists of a
sequence of inputs that usually have names, and which can take on
various values.  The state of a form can be tweaked and it can then be
asked to provide C<HTTP::Request> objects that can be passed to the
request() method of C<LWP::UserAgent>.

The following methods are available:

=over 4

=item @forms = HTML::Form->parse( $response )

=item @forms = HTML::Form->parse( $html_document, $base )

=item @forms = HTML::Form->parse( $html_document, %opt )

The parse() class method will parse an HTML document and build up
C<HTML::Form> objects for each <form> element found.  If called in scalar
context only returns the first <form>.  Returns an empty list if there
are no forms to be found.

The $base is the URI used to retrieve the $html_document.  It is
needed to resolve relative action URIs.  If the document was retrieved
with LWP then this this parameter is obtained from the
$response->base() method, as shown by the following example:

    my $ua = LWP::UserAgent->new;
    my $response = $ua->get("http://www.example.com/form.html");
    my @forms = HTML::Form->parse($response->decoded_content,
				  $response->base);

The parse() method can parse from an C<HTTP::Response> object
directly, so the example above can be more conveniently written as:

    my $ua = LWP::UserAgent->new;
    my $response = $ua->get("http://www.example.com/form.html");
    my @forms = HTML::Form->parse($response);

Note that any object that implements a decoded_content() and base() method
with similar behaviour as C<HTTP::Response> will do.

Finally options might be passed in to control how the parse method
behaves.  The following options are currently recognized:

=over

=item C<base>

Another way to provide the base URI.

=item C<verbose>

Print messages to STDERR about any bad HTML form constructs found.

=back

=cut

sub parse
{
    my $class = shift;
    my $html = shift;
    unshift(@_, "base") if @_ == 1;
    my %opt = @_;

    require HTML::TokeParser;
    my $p = HTML::TokeParser->new(ref($html) ? $html->decoded_content(ref => 1) : \$html);
    die "Failed to create HTML::TokeParser object" unless $p;
    eval {
	# optimization
	$p->report_tags(qw(form input textarea select optgroup option keygen label button));
    };

    my $base_uri = delete $opt{base};
    my $verbose = delete $opt{verbose};

    if ($^W) {
	Carp::carp("Unrecognized option $_ in HTML::Form->parse") for sort keys %opt;
    }

    unless (defined $base_uri) {
	if (ref($html)) {
	    $base_uri = $html->base;
	}
	else {
	    Carp::croak("HTML::Form::parse: No \$base_uri provided");
	}
    }

    my @forms;
    my $f;  # current form

    my %openselect; # index to the open instance of a select

    while (my $t = $p->get_tag) {
	my($tag,$attr) = @$t;
	if ($tag eq "form") {
	    my $action = delete $attr->{'action'};
	    $action = "" unless defined $action;
	    $action = URI->new_abs($action, $base_uri);
	    $f = $class->new($attr->{'method'},
			     $action,
			     $attr->{'enctype'});
	    $f->{attr} = $attr;
            %openselect = ();
	    push(@forms, $f);
	    my(%labels, $current_label);
	    while (my $t = $p->get_tag) {
		my($tag, $attr) = @$t;
		last if $tag eq "/form";

		# if we are inside a label tag, then keep
		# appending any text to the current label
		if(defined $current_label) {
		    $current_label = join " ",
		        grep { defined and length }
		        $current_label,
		        $p->get_phrase;
		}

		if ($tag eq "input") {
		    $attr->{value_name} =
		        exists $attr->{id} && exists $labels{$attr->{id}} ? $labels{$attr->{id}} :
			defined $current_label                            ?  $current_label      :
		        $p->get_phrase;
		}

		if ($tag eq "label") {
		    $current_label = $p->get_phrase;
		    $labels{ $attr->{for} } = $current_label
		        if exists $attr->{for};
		}
		elsif ($tag eq "/label") {
		    $current_label = undef;
		}
		elsif ($tag eq "input") {
		    my $type = delete $attr->{type} || "text";
		    $f->push_input($type, $attr);
		}
                elsif ($tag eq "button") {
                    my $type = delete $attr->{type} || "submit";
                    $f->push_input($type, $attr);
                }
		elsif ($tag eq "textarea") {
		    $attr->{textarea_value} = $attr->{value}
		        if exists $attr->{value};
		    my $text = $p->get_text("/textarea");
		    $attr->{value} = $text;
		    $f->push_input("textarea", $attr);
		}
		elsif ($tag eq "select") {
		    # rename attributes reserved to come for the option tag
		    for ("value", "value_name") {
			$attr->{"select_$_"} = delete $attr->{$_}
			    if exists $attr->{$_};
		    }
		    # count this new select option separately
		    $openselect{$attr->{name}}++;

		    while ($t = $p->get_tag) {
			my $tag = shift @$t;
			last if $tag eq "/select";
			next if $tag =~ m,/?optgroup,;
			next if $tag eq "/option";
			if ($tag eq "option") {
			    my %a = %{$t->[0]};
			    # rename keys so they don't clash with %attr
			    for (keys %a) {
				next if $_ eq "value";
				$a{"option_$_"} = delete $a{$_};
			    }
			    while (my($k,$v) = each %$attr) {
				$a{$k} = $v;
			    }
			    $a{value_name} = $p->get_trimmed_text;
			    $a{value} = delete $a{value_name}
				unless defined $a{value};
			    $a{idx} = $openselect{$attr->{name}};
			    $f->push_input("option", \%a);
			}
			else {
			    warn("Bad <select> tag '$tag' in $base_uri\n") if $verbose;
			    if ($tag eq "/form" ||
				$tag eq "input" ||
				$tag eq "textarea" ||
				$tag eq "select" ||
				$tag eq "keygen")
			    {
				# MSIE implictly terminate the <select> here, so we
				# try to do the same.  Actually the MSIE behaviour
				# appears really strange:  <input> and <textarea>
				# do implictly close, but not <select>, <keygen> or
				# </form>.
				my $type = ($tag =~ s,^/,,) ? "E" : "S";
				$p->unget_token([$type, $tag, @$t]);
				last;
			    }
			}
		    }
		}
		elsif ($tag eq "keygen") {
		    $f->push_input("keygen", $attr);
		}
	    }
	}
	elsif ($form_tags{$tag}) {
	    warn("<$tag> outside <form> in $base_uri\n") if $verbose;
	}
    }
    for (@forms) {
	$_->fixup;
    }

    wantarray ? @forms : $forms[0];
}

sub new {
    my $class = shift;
    my $self = bless {}, $class;
    $self->{method} = uc(shift  || "GET");
    $self->{action} = shift  || Carp::croak("No action defined");
    $self->{enctype} = lc(shift || "application/x-www-form-urlencoded");
    $self->{inputs} = [@_];
    $self;
}


sub push_input
{
    my($self, $type, $attr) = @_;
    $type = lc $type;
    my $class = $type2class{$type};
    unless ($class) {
	Carp::carp("Unknown input type '$type'") if $^W;
	$class = "TextInput";
    }
    $class = "HTML::Form::$class";
    my @extra;
    push(@extra, readonly => 1) if $type eq "hidden";

    delete $attr->{type}; # don't confuse the type argument
    my $input = $class->new(type => $type, %$attr, @extra);
    $input->add_to_form($self);
}


=item $method = $form->method

=item $form->method( $new_method )

This method is gets/sets the I<method> name used for the
C<HTTP::Request> generated.  It is a string like "GET" or "POST".

=item $action = $form->action

=item $form->action( $new_action )

This method gets/sets the URI which we want to apply the request
I<method> to.

=item $enctype = $form->enctype

=item $form->enctype( $new_enctype )

This method gets/sets the encoding type for the form data.  It is a
string like "application/x-www-form-urlencoded" or "multipart/form-data".

=cut

BEGIN {
    # Set up some accesor
    for (qw(method action enctype)) {
	my $m = $_;
	no strict 'refs';
	*{$m} = sub {
	    my $self = shift;
	    my $old = $self->{$m};
	    $self->{$m} = shift if @_;
	    $old;
	};
    }
    *uri = \&action;  # alias
}

=item $value = $form->attr( $name )

=item $form->attr( $name, $new_value )

This method give access to the original HTML attributes of the <form> tag.
The $name should always be passed in lower case.

Example:

   @f = HTML::Form->parse( $html, $foo );
   @f = grep $_->attr("id") eq "foo", @f;
   die "No form named 'foo' found" unless @f;
   $foo = shift @f;

=cut

sub attr {
    my $self = shift;
    my $name = shift;
    return undef unless defined $name;

    my $old = $self->{attr}{$name};
    $self->{attr}{$name} = shift if @_;
    return $old;
}

=item @inputs = $form->inputs

This method returns the list of inputs in the form.  If called in
scalar context it returns the number of inputs contained in the form.
See L</INPUTS> for what methods are available for the input objects
returned.

=cut

sub inputs
{
    my $self = shift;
    @{$self->{'inputs'}};
}


=item $input = $form->find_input( $name )

=item $input = $form->find_input( $name, $type )

=item $input = $form->find_input( $name, $type, $index )

This method is used to locate specific inputs within the form.  All
inputs that match the arguments given are returned.  In scalar context
only the first is returned, or C<undef> if none match.

If $name is specified, then the input must have the indicated name.

If $type is specified, then the input must have the specified type.
The following type names are used: "text", "password", "hidden",
"textarea", "file", "image", "submit", "radio", "checkbox" and "option".

The $index is the sequence number of the input matched where 1 is the
first.  If combined with $name and/or $type then it select the I<n>th
input with the given name and/or type.

=cut

sub find_input
{
    my($self, $name, $type, $no) = @_;
    if (wantarray) {
	my @res;
	my $c;
	for (@{$self->{'inputs'}}) {
	    if (defined $name) {
		next unless exists $_->{name};
		next if $name ne $_->{name};
	    }
	    next if $type && $type ne $_->{type};
	    $c++;
	    next if $no && $no != $c;
	    push(@res, $_);
	}
	return @res;
	
    }
    else {
	$no ||= 1;
	for (@{$self->{'inputs'}}) {
	    if (defined $name) {
		next unless exists $_->{name};
		next if $name ne $_->{name};
	    }
	    next if $type && $type ne $_->{type};
	    next if --$no;
	    return $_;
	}
	return undef;
    }
}

sub fixup
{
    my $self = shift;
    for (@{$self->{'inputs'}}) {
	$_->fixup;
    }
}


=item $value = $form->value( $name )

=item $form->value( $name, $new_value )

The value() method can be used to get/set the value of some input.  If
no input has the indicated name, then this method will croak.

If multiple inputs have the same name, only the first one will be
affected.

The call:

    $form->value('foo')

is a short-hand for:

    $form->find_input('foo')->value;

=cut

sub value
{
    my $self = shift;
    my $key  = shift;
    my $input = $self->find_input($key);
    Carp::croak("No such field '$key'") unless $input;
    local $Carp::CarpLevel = 1;
    $input->value(@_);
}

=item @names = $form->param

=item @values = $form->param( $name )

=item $form->param( $name, $value, ... )

=item $form->param( $name, \@values )

Alternative interface to examining and setting the values of the form.

If called without arguments then it returns the names of all the
inputs in the form.  The names will not repeat even if multiple inputs
have the same name.  In scalar context the number of different names
is returned.

If called with a single argument then it returns the value or values
of inputs with the given name.  If called in scalar context only the
first value is returned.  If no input exists with the given name, then
C<undef> is returned.

If called with 2 or more arguments then it will set values of the
named inputs.  This form will croak if no inputs have the given name
or if any of the values provided does not fit.  Values can also be
provided as a reference to an array.  This form will allow unsetting
all values with the given name as well.

This interface resembles that of the param() function of the CGI
module.

=cut

sub param {
    my $self = shift;
    if (@_) {
        my $name = shift;
        my @inputs;
        for ($self->inputs) {
            my $n = $_->name;
            next if !defined($n) || $n ne $name;
            push(@inputs, $_);
        }

        if (@_) {
            # set
            die "No '$name' parameter exists" unless @inputs;
	    my @v = @_;
	    @v = @{$v[0]} if @v == 1 && ref($v[0]);
            while (@v) {
                my $v = shift @v;
                my $err;
                for my $i (0 .. @inputs-1) {
                    eval {
                        $inputs[$i]->value($v);
                    };
                    unless ($@) {
                        undef($err);
                        splice(@inputs, $i, 1);
                        last;
                    }
                    $err ||= $@;
                }
                die $err if $err;
            }

	    # the rest of the input should be cleared
	    for (@inputs) {
		$_->value(undef);
	    }
        }
        else {
            # get
            my @v;
            for (@inputs) {
		if (defined(my $v = $_->value)) {
		    push(@v, $v);
		}
            }
            return wantarray ? @v : $v[0];
        }
    }
    else {
        # list parameter names
        my @n;
        my %seen;
        for ($self->inputs) {
            my $n = $_->name;
            next if !defined($n) || $seen{$n}++;
            push(@n, $n);
        }
        return @n;
    }
}


=item $form->try_others( \&callback )

This method will iterate over all permutations of unvisited enumerated
values (<select>, <radio>, <checkbox>) and invoke the callback for
each.  The callback is passed the $form as argument.  The return value
from the callback is ignored and the try_others() method itself does
not return anything.

=cut

sub try_others
{
    my($self, $cb) = @_;
    my @try;
    for (@{$self->{'inputs'}}) {
	my @not_tried_yet = $_->other_possible_values;
	next unless @not_tried_yet;
	push(@try, [\@not_tried_yet, $_]);
    }
    return unless @try;
    $self->_try($cb, \@try, 0);
}

sub _try
{
    my($self, $cb, $try, $i) = @_;
    for (@{$try->[$i][0]}) {
	$try->[$i][1]->value($_);
	&$cb($self);
	$self->_try($cb, $try, $i+1) if $i+1 < @$try;
    }
}


=item $request = $form->make_request

Will return an C<HTTP::Request> object that reflects the current setting
of the form.  You might want to use the click() method instead.

=cut

sub make_request
{
    my $self = shift;
    my $method  = uc $self->{'method'};
    my $uri     = $self->{'action'};
    my $enctype = $self->{'enctype'};
    my @form    = $self->form;

    if ($method eq "GET") {
	require HTTP::Request;
	$uri = URI->new($uri, "http");
	$uri->query_form(@form);
	return HTTP::Request->new(GET => $uri);
    }
    elsif ($method eq "POST") {
	require HTTP::Request::Common;
	return HTTP::Request::Common::POST($uri, \@form,
					   Content_Type => $enctype);
    }
    else {
	Carp::croak("Unknown method '$method'");
    }
}


=item $request = $form->click

=item $request = $form->click( $name )

=item $request = $form->click( $x, $y )

=item $request = $form->click( $name, $x, $y )

Will "click" on the first clickable input (which will be of type
C<submit> or C<image>).  The result of clicking is an C<HTTP::Request>
object that can then be passed to C<LWP::UserAgent> if you want to
obtain the server response.

If a $name is specified, we will click on the first clickable input
with the given name, and the method will croak if no clickable input
with the given name is found.  If $name is I<not> specified, then it
is ok if the form contains no clickable inputs.  In this case the
click() method returns the same request as the make_request() method
would do.

If there are multiple clickable inputs with the same name, then there
is no way to get the click() method of the C<HTML::Form> to click on
any but the first.  If you need this you would have to locate the
input with find_input() and invoke the click() method on the given
input yourself.

A click coordinate pair can also be provided, but this only makes a
difference if you clicked on an image.  The default coordinate is
(1,1).  The upper-left corner of the image is (0,0), but some badly
coded CGI scripts are known to not recognize this.  Therefore (1,1) was
selected as a safer default.

=cut

sub click
{
    my $self = shift;
    my $name;
    $name = shift if (@_ % 2) == 1;  # odd number of arguments

    # try to find first submit button to activate
    for (@{$self->{'inputs'}}) {
        next unless $_->can("click");
        next if $name && $_->name ne $name;
	next if $_->disabled;
	return $_->click($self, @_);
    }
    Carp::croak("No clickable input with name $name") if $name;
    $self->make_request;
}


=item @kw = $form->form

Returns the current setting as a sequence of key/value pairs.  Note
that keys might be repeated, which means that some values might be
lost if the return values are assigned to a hash.

In scalar context this method returns the number of key/value pairs
generated.

=cut

sub form
{
    my $self = shift;
    map { $_->form_name_value($self) } @{$self->{'inputs'}};
}


=item $form->dump

Returns a textual representation of current state of the form.  Mainly
useful for debugging.  If called in void context, then the dump is
printed on STDERR.

=cut

sub dump
{
    my $self = shift;
    my $method  = $self->{'method'};
    my $uri     = $self->{'action'};
    my $enctype = $self->{'enctype'};
    my $dump = "$method $uri";
    $dump .= " ($enctype)"
	if $enctype ne "application/x-www-form-urlencoded";
    $dump .= " [$self->{attr}{name}]"
    	if exists $self->{attr}{name};
    $dump .= "\n";
    for ($self->inputs) {
	$dump .= "  " . $_->dump . "\n";
    }
    print STDERR $dump unless defined wantarray;
    $dump;
}


#---------------------------------------------------
package HTML::Form::Input;

=back

=head1 INPUTS

An C<HTML::Form> objects contains a sequence of I<inputs>.  References to
the inputs can be obtained with the $form->inputs or $form->find_input
methods.

Note that there is I<not> a one-to-one correspondence between input
I<objects> and E<lt>inputE<gt> I<elements> in the HTML document.  An
input object basically represents a name/value pair, so when multiple
HTML elements contribute to the same name/value pair in the submitted
form they are combined.

The input elements that are mapped one-to-one are "text", "textarea",
"password", "hidden", "file", "image", "submit" and "checkbox".  For
the "radio" and "option" inputs the story is not as simple: All
E<lt>input type="radio"E<gt> elements with the same name will
contribute to the same input radio object.  The number of radio input
objects will be the same as the number of distinct names used for the
E<lt>input type="radio"E<gt> elements.  For a E<lt>selectE<gt> element
without the C<multiple> attribute there will be one input object of
type of "option".  For a E<lt>select multipleE<gt> element there will
be one input object for each contained E<lt>optionE<gt> element.  Each
one of these option objects will have the same name.

The following methods are available for the I<input> objects:

=over 4

=cut

sub new
{
    my $class = shift;
    my $self = bless {@_}, $class;
    $self;
}

sub add_to_form
{
    my($self, $form) = @_;
    push(@{$form->{'inputs'}}, $self);
    $self;
}

sub fixup {}


=item $input->type

Returns the type of this input.  The type is one of the following
strings: "text", "password", "hidden", "textarea", "file", "image", "submit",
"radio", "checkbox" or "option".

=cut

sub type
{
    shift->{type};
}

=item $name = $input->name

=item $input->name( $new_name )

This method can be used to get/set the current name of the input.

=item $value = $input->value

=item $input->value( $new_value )

This method can be used to get/set the current value of an
input.

If the input only can take an enumerated list of values, then it is an
error to try to set it to something else and the method will croak if
you try.

You will also be able to set the value of read-only inputs, but a
warning will be generated if running under C<perl -w>.

=cut

sub name
{
    my $self = shift;
    my $old = $self->{name};
    $self->{name} = shift if @_;
    $old;
}

sub value
{
    my $self = shift;
    my $old = $self->{value};
    $self->{value} = shift if @_;
    $old;
}

=item $input->possible_values

Returns a list of all values that an input can take.  For inputs that
do not have discrete values, this returns an empty list.

=cut

sub possible_values
{
    return;
}

=item $input->other_possible_values

Returns a list of all values not tried yet.

=cut

sub other_possible_values
{
    return;
}

=item $input->value_names

For some inputs the values can have names that are different from the
values themselves.  The number of names returned by this method will
match the number of values reported by $input->possible_values.

When setting values using the value() method it is also possible to
use the value names in place of the value itself.

=cut

sub value_names {
    return
}

=item $bool = $input->readonly

=item $input->readonly( $bool )

This method is used to get/set the value of the readonly attribute.
You are allowed to modify the value of readonly inputs, but setting
the value will generate some noise when warnings are enabled.  Hidden
fields always start out readonly.

=cut

sub readonly {
    my $self = shift;
    my $old = $self->{readonly};
    $self->{readonly} = shift if @_;
    $old;
}

=item $bool = $input->disabled

=item $input->disabled( $bool )

This method is used to get/set the value of the disabled attribute.
Disabled inputs do not contribute any key/value pairs for the form
value.

=cut

sub disabled {
    my $self = shift;
    my $old = $self->{disabled};
    $self->{disabled} = shift if @_;
    $old;
}

=item $input->form_name_value

Returns a (possible empty) list of key/value pairs that should be
incorporated in the form value from this input.

=cut

sub form_name_value
{
    my $self = shift;
    my $name = $self->{'name'};
    return unless defined $name;
    return if $self->disabled;
    my $value = $self->value;
    return unless defined $value;
    return ($name => $value);
}

sub dump
{
    my $self = shift;
    my $name = $self->name;
    $name = "<NONAME>" unless defined $name;
    my $value = $self->value;
    $value = "<UNDEF>" unless defined $value;
    my $dump = "$name=$value";

    my $type = $self->type;

    $type .= " disabled" if $self->disabled;
    $type .= " readonly" if $self->readonly;
    return sprintf "%-30s %s", $dump, "($type)" unless $self->{menu};

    my @menu;
    my $i = 0;
    for (@{$self->{menu}}) {
	my $opt = $_->{value};
	$opt = "<UNDEF>" unless defined $opt;
	$opt .= "/$_->{name}"
	    if defined $_->{name} && length $_->{name} && $_->{name} ne $opt;
	substr($opt,0,0) = "-" if $_->{disabled};
	if (exists $self->{current} && $self->{current} == $i) {
	    substr($opt,0,0) = "!" unless $_->{seen};
	    substr($opt,0,0) = "*";
	}
	else {
	    substr($opt,0,0) = ":" if $_->{seen};
	}
	push(@menu, $opt);
	$i++;
    }

    return sprintf "%-30s %-10s %s", $dump, "($type)", "[" . join("|", @menu) . "]";
}


#---------------------------------------------------
package HTML::Form::TextInput;
@HTML::Form::TextInput::ISA=qw(HTML::Form::Input);

#input/text
#input/password
#input/hidden
#textarea

sub value
{
    my $self = shift;
    my $old = $self->{value};
    $old = "" unless defined $old;
    if (@_) {
        Carp::carp("Input '$self->{name}' is readonly")
	    if $^W && $self->{readonly};
        my $new = shift;
        my $n = exists $self->{maxlength} ? $self->{maxlength} : undef;
        Carp::carp("Input '$self->{name}' has maxlength '$n'")
	    if $^W && defined($n) && defined($new) && length($new) > $n;
	$self->{value} = $new;
    }
    $old;
}

#---------------------------------------------------
package HTML::Form::IgnoreInput;
@HTML::Form::IgnoreInput::ISA=qw(HTML::Form::Input);

#input/button
#input/reset

sub value { return }


#---------------------------------------------------
package HTML::Form::ListInput;
@HTML::Form::ListInput::ISA=qw(HTML::Form::Input);

#select/option   (val1, val2, ....)
#input/radio     (undef, val1, val2,...)
#input/checkbox  (undef, value)
#select-multiple/option (undef, value)

sub new
{
    my $class = shift;
    my $self = $class->SUPER::new(@_);

    my $value = delete $self->{value};
    my $value_name = delete $self->{value_name};
    my $type = $self->{type};

    if ($type eq "checkbox") {
	$value = "on" unless defined $value;
	$self->{menu} = [
	    { value => undef, name => "off", },
            { value => $value, name => $value_name, },
        ];
	$self->{current} = (delete $self->{checked}) ? 1 : 0;
	;
    }
    else {
	$self->{option_disabled}++
	    if $type eq "radio" && delete $self->{disabled};
	$self->{menu} = [
            {value => $value, name => $value_name},
        ];
	my $checked = $self->{checked} || $self->{option_selected};
	delete $self->{checked};
	delete $self->{option_selected};
	if (exists $self->{multiple}) {
	    unshift(@{$self->{menu}}, { value => undef, name => "off"});
	    $self->{current} = $checked ? 1 : 0;
	}
	else {
	    $self->{current} = 0 if $checked;
	}
    }
    $self;
}

sub add_to_form
{
    my($self, $form) = @_;
    my $type = $self->type;

    return $self->SUPER::add_to_form($form)
	if $type eq "checkbox";

    if ($type eq "option" && exists $self->{multiple}) {
	$self->{disabled} ||= delete $self->{option_disabled};
	return $self->SUPER::add_to_form($form);
    }

    die "Assert" if @{$self->{menu}} != 1;
    my $m = $self->{menu}[0];
    $m->{disabled}++ if delete $self->{option_disabled};

    my $prev = $form->find_input($self->{name}, $self->{type}, $self->{idx});
    return $self->SUPER::add_to_form($form) unless $prev;

    # merge menues
    $prev->{current} = @{$prev->{menu}} if exists $self->{current};
    push(@{$prev->{menu}}, $m);
}

sub fixup
{
    my $self = shift;
    if ($self->{type} eq "option" && !(exists $self->{current})) {
	$self->{current} = 0;
    }
    $self->{menu}[$self->{current}]{seen}++ if exists $self->{current};
}

sub disabled
{
    my $self = shift;
    my $type = $self->type;

    my $old = $self->{disabled} || _menu_all_disabled(@{$self->{menu}});
    if (@_) {
	my $v = shift;
	$self->{disabled} = $v;
        for (@{$self->{menu}}) {
            $_->{disabled} = $v;
        }
    }
    return $old;
}

sub _menu_all_disabled {
    for (@_) {
	return 0 unless $_->{disabled};
    }
    return 1;
}

sub value
{
    my $self = shift;
    my $old;
    $old = $self->{menu}[$self->{current}]{value} if exists $self->{current};
    if (@_) {
	my $i = 0;
	my $val = shift;
	my $cur;
	my $disabled;
	for (@{$self->{menu}}) {
	    if ((defined($val) && defined($_->{value}) && $val eq $_->{value}) ||
		(!defined($val) && !defined($_->{value}))
	       )
	    {
		$cur = $i;
		$disabled = $_->{disabled};
		last unless $disabled;
	    }
	    $i++;
	}
	if (!(defined $cur) || $disabled) {
	    if (defined $val) {
		# try to search among the alternative names as well
		my $i = 0;
		my $cur_ignorecase;
		my $lc_val = lc($val);
		for (@{$self->{menu}}) {
		    if (defined $_->{name}) {
			if ($val eq $_->{name}) {
			    $disabled = $_->{disabled};
			    $cur = $i;
			    last unless $disabled;
			}
			if (!defined($cur_ignorecase) && $lc_val eq lc($_->{name})) {
			    $cur_ignorecase = $i;
			}
		    }
		    $i++;
		}
		unless (defined $cur) {
		    $cur = $cur_ignorecase;
		    if (defined $cur) {
			$disabled = $self->{menu}[$cur]{disabled};
		    }
		    else {
			my $n = $self->name;
		        Carp::croak("Illegal value '$val' for field '$n'");
		    }
		}
	    }
	    else {
		my $n = $self->name;
	        Carp::croak("The '$n' field can't be unchecked");
	    }
	}
	if ($disabled) {
	    my $n = $self->name;
	    Carp::croak("The value '$val' has been disabled for field '$n'");
	}
	$self->{current} = $cur;
	$self->{menu}[$cur]{seen}++;
    }
    $old;
}

=item $input->check

Some input types represent toggles that can be turned on/off.  This
includes "checkbox" and "option" inputs.  Calling this method turns
this input on without having to know the value name.  If the input is
already on, then nothing happens.

This has the same effect as:

    $input->value($input->possible_values[1]);

The input can be turned off with:

    $input->value(undef);

=cut

sub check
{
    my $self = shift;
    $self->{current} = 1;
    $self->{menu}[1]{seen}++;
}

sub possible_values
{
    my $self = shift;
    map $_->{value}, @{$self->{menu}};
}

sub other_possible_values
{
    my $self = shift;
    map $_->{value}, grep !$_->{seen}, @{$self->{menu}};
}

sub value_names {
    my $self = shift;
    my @names;
    for (@{$self->{menu}}) {
	my $n = $_->{name};
	$n = $_->{value} unless defined $n;
	push(@names, $n);
    }
    @names;
}


#---------------------------------------------------
package HTML::Form::SubmitInput;
@HTML::Form::SubmitInput::ISA=qw(HTML::Form::Input);

#input/image
#input/submit

=item $input->click($form, $x, $y)

Some input types (currently "submit" buttons and "images") can be
clicked to submit the form.  The click() method returns the
corresponding C<HTTP::Request> object.

=cut

sub click
{
    my($self,$form,$x,$y) = @_;
    for ($x, $y) { $_ = 1 unless defined; }
    local($self->{clicked}) = [$x,$y];
    return $form->make_request;
}

sub form_name_value
{
    my $self = shift;
    return unless $self->{clicked};
    return $self->SUPER::form_name_value(@_);
}


#---------------------------------------------------
package HTML::Form::ImageInput;
@HTML::Form::ImageInput::ISA=qw(HTML::Form::SubmitInput);

sub form_name_value
{
    my $self = shift;
    my $clicked = $self->{clicked};
    return unless $clicked;
    return if $self->{disabled};
    my $name = $self->{name};
    $name = (defined($name) && length($name)) ? "$name." : "";
    return ("${name}x" => $clicked->[0],
	    "${name}y" => $clicked->[1]
	   );
}

#---------------------------------------------------
package HTML::Form::FileInput;
@HTML::Form::FileInput::ISA=qw(HTML::Form::TextInput);

=back

If the input is of type C<file>, then it has these additional methods:

=over 4

=item $input->file

This is just an alias for the value() method.  It sets the filename to
read data from.

=cut

sub file {
    my $self = shift;
    $self->value(@_);
}

=item $filename = $input->filename

=item $input->filename( $new_filename )

This get/sets the filename reported to the server during file upload.
This attribute defaults to the value reported by the file() method.

=cut

sub filename {
    my $self = shift;
    my $old = $self->{filename};
    $self->{filename} = shift if @_;
    $old = $self->file unless defined $old;
    $old;
}

=item $content = $input->content

=item $input->content( $new_content )

This get/sets the file content provided to the server during file
upload.  This method can be used if you do not want the content to be
read from an actual file.

=cut

sub content {
    my $self = shift;
    my $old = $self->{content};
    $self->{content} = shift if @_;
    $old;
}

=item @headers = $input->headers

=item input->headers($key => $value, .... )

This get/set additional header fields describing the file uploaded.
This can for instance be used to set the C<Content-Type> reported for
the file.

=cut

sub headers {
    my $self = shift;
    my $old = $self->{headers} || [];
    $self->{headers} = [@_] if @_;
    @$old;
}

sub form_name_value {
    my($self, $form) = @_;
    return $self->SUPER::form_name_value($form)
	if $form->method ne "POST" ||
	   $form->enctype ne "multipart/form-data";

    my $name = $self->name;
    return unless defined $name;
    return if $self->{disabled};

    my $file = $self->file;
    my $filename = $self->filename;
    my @headers = $self->headers;
    my $content = $self->content;
    if (defined $content) {
	$filename = $file unless defined $filename;
	$file = undef;
	unshift(@headers, "Content" => $content);
    }
    elsif (!defined($file) || length($file) == 0) {
	return;
    }

    # legacy (this used to be the way to do it)
    if (ref($file) eq "ARRAY") {
	my $f = shift @$file;
	my $fn = shift @$file;
	push(@headers, @$file);
	$file = $f;
	$filename = $fn unless defined $filename;
    }

    return ($name => [$file, $filename, @headers]);
}

package HTML::Form::KeygenInput;
@HTML::Form::KeygenInput::ISA=qw(HTML::Form::Input);

sub challenge {
    my $self = shift;
    return $self->{challenge};
}

sub keytype {
    my $self = shift;
    return lc($self->{keytype} || 'rsa');
}

1;

__END__

=back

=head1 SEE ALSO

L<LWP>, L<LWP::UserAgent>, L<HTML::Parser>

=head1 COPYRIGHT

Copyright 1998-2005 Gisle Aas.

This library is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=cut
