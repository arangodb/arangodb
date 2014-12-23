package Filter::Simple;

use Text::Balanced ':ALL';

use vars qw{ $VERSION @EXPORT };

$VERSION = '0.82';

use Filter::Util::Call;
use Carp;

@EXPORT = qw( FILTER FILTER_ONLY );


sub import {
    if (@_>1) { shift; goto &FILTER }
    else      { *{caller()."::$_"} = \&$_ foreach @EXPORT }
}

sub fail {
    croak "FILTER_ONLY: ", @_;
}

my $exql = sub {
    my @bits = extract_quotelike $_[0], qr//;
    return unless $bits[0];
    return \@bits;
};

my $ncws = qr/\s+/;
my $comment = qr/(?<![\$\@%])#.*/;
my $ws = qr/(?:$ncws|$comment)+/;
my $id = qr/\b(?!([ysm]|q[rqxw]?|tr)\b)\w+/;
my $EOP = qr/\n\n|\Z/;
my $CUT = qr/\n=cut.*$EOP/;
my $pod_or_DATA = qr/
              ^=(?:head[1-4]|item) .*? $CUT
            | ^=pod .*? $CUT
            | ^=for .*? $EOP
            | ^=begin \s* (\S+) .*? \n=end \s* \1 .*? $EOP
            | ^__(DATA|END)__\r?\n.*
            /smx;

my %extractor_for = (
    quotelike  => [ $ws,  \&extract_variable, $id, { MATCH  => \&extract_quotelike } ],
    regex      => [ $ws,  $pod_or_DATA, $id, $exql           ],
    string     => [ $ws,  $pod_or_DATA, $id, $exql           ],
    code       => [ $ws, { DONT_MATCH => $pod_or_DATA },
    		        \&extract_variable,
                    $id, { DONT_MATCH => \&extract_quotelike }   ],
    code_no_comments
               => [ { DONT_MATCH => $comment },
                    $ncws, { DONT_MATCH => $pod_or_DATA },
    		        \&extract_variable,
                    $id, { DONT_MATCH => \&extract_quotelike }   ],
    executable => [ $ws, { DONT_MATCH => $pod_or_DATA }      ],
    executable_no_comments
               => [ { DONT_MATCH => $comment },
                    $ncws, { DONT_MATCH => $pod_or_DATA }      ],
    all        => [        { MATCH  => qr/(?s:.*)/         } ],
);

my %selector_for = (
    all   => sub { my ($t)=@_; sub{ $_=$$_; $t->(@_); $_} },
    executable=> sub { my ($t)=@_; sub{ref() ? $_=$$_ : $t->(@_); $_} }, 
    quotelike => sub { my ($t)=@_; sub{ref() && do{$_=$$_; $t->(@_)}; $_} },
    regex     => sub { my ($t)=@_;
               sub{ref() or return $_;
                   my ($ql,undef,$pre,$op,$ld,$pat) = @$_;
                   return $_->[0] unless $op =~ /^(qr|m|s)/
                         || !$op && ($ld eq '/' || $ld eq '?');
                   $_ = $pat;
                   $t->(@_);
                   $ql =~ s/^(\s*\Q$op\E\s*\Q$ld\E)\Q$pat\E/$1$_/;
                   return "$pre$ql";
                  };
            },
    string     => sub { my ($t)=@_;
               sub{ref() or return $_;
                   local *args = \@_;
                   my ($pre,$op,$ld1,$str1,$rd1,$ld2,$str2,$rd2,$flg) = @{$_}[2..10];
                   return $_->[0] if $op =~ /^(qr|m)/
                         || !$op && ($ld1 eq '/' || $ld1 eq '?');
                   if (!$op || $op eq 'tr' || $op eq 'y') {
                       local *_ = \$str1;
                       $t->(@args);
                   }
                   if ($op =~ /^(tr|y|s)/) {
                       local *_ = \$str2;
                       $t->(@args);
                   }
                   my $result = "$pre$op$ld1$str1$rd1";
                   $result .= $ld2 if $ld1 =~ m/[[({<]/; #])}>
                   $result .= "$str2$rd2$flg";
                   return $result;
                  };
              },
);


sub gen_std_filter_for {
    my ($type, $transform) = @_;
    return sub {
        my $instr;
        local @components;
		for (extract_multiple($_,$extractor_for{$type})) {
            if (ref())     { push @components, $_; $instr=0 }
            elsif ($instr) { $components[-1] .= $_ }
            else           { push @components, $_; $instr=1 }
        }
        if ($type =~ /^code/) {
            my $count = 0;
            local $placeholder = qr/\Q$;\E(\C{4})\Q$;\E/;
            my $extractor =      qr/\Q$;\E(\C{4})\Q$;\E/;
            $_ = join "",
                  map { ref $_ ? $;.pack('N',$count++).$; : $_ }
                      @components;
            @components = grep { ref $_ } @components;
            $transform->(@_);
            s/$extractor/${$components[unpack('N',$1)]}/g;
        }
        else {
            my $selector = $selector_for{$type}->($transform);
            $_ = join "", map $selector->(@_), @components;
        }
    }
};

sub FILTER (&;$) {
    my $caller = caller;
    my ($filter, $terminator) = @_;
    no warnings 'redefine';
    *{"${caller}::import"} = gen_filter_import($caller,$filter,$terminator);
    *{"${caller}::unimport"} = gen_filter_unimport($caller);
}

sub FILTER_ONLY {
    my $caller = caller;
    while (@_ > 1) {
        my ($what, $how) = splice(@_, 0, 2);
        fail "Unknown selector: $what"
            unless exists $extractor_for{$what};
        fail "Filter for $what is not a subroutine reference"
            unless ref $how eq 'CODE';
        push @transforms, gen_std_filter_for($what,$how);
    }
    my $terminator = shift;

    my $multitransform = sub {
        foreach my $transform ( @transforms ) {
            $transform->(@_);
        }
    };
    no warnings 'redefine';
    *{"${caller}::import"} =
        gen_filter_import($caller,$multitransform,$terminator);
    *{"${caller}::unimport"} = gen_filter_unimport($caller);
}

my $ows    = qr/(?:[ \t]+|#[^\n]*)*/;

sub gen_filter_import {
    my ($class, $filter, $terminator) = @_;
    my %terminator;
    my $prev_import = *{$class."::import"}{CODE};
    return sub {
        my ($imported_class, @args) = @_;
        my $def_terminator =
            qr/^(?:\s*no\s+$imported_class\s*;$ows|__(?:END|DATA)__)\r?$/;
        if (!defined $terminator) {
            $terminator{terminator} = $def_terminator;
        }
        elsif (!ref $terminator || ref $terminator eq 'Regexp') {
            $terminator{terminator} = $terminator;
        }
        elsif (ref $terminator ne 'HASH') {
            croak "Terminator must be specified as scalar or hash ref"
        }
        elsif (!exists $terminator->{terminator}) {
            $terminator{terminator} = $def_terminator;
        }
        filter_add(
            sub {
                my ($status, $lastline);
                my $count = 0;
                my $data = "";
                while ($status = filter_read()) {
                    return $status if $status < 0;
                    if ($terminator{terminator} &&
                        m/$terminator{terminator}/) {
                        $lastline = $_;
                        last;
                    }
                    $data .= $_;
                    $count++;
                    $_ = "";
                }
                return $count if not $count;
                $_ = $data;
                $filter->($imported_class, @args) unless $status < 0;
                if (defined $lastline) {
                    if (defined $terminator{becomes}) {
                        $_ .= $terminator{becomes};
                    }
                    elsif ($lastline =~ $def_terminator) {
                        $_ .= $lastline;
                    }
                }
                return $count;
            }
        );
        if ($prev_import) {
            goto &$prev_import;
        }
        elsif ($class->isa('Exporter')) {
            $class->export_to_level(1,@_);
        }
    }
}

sub gen_filter_unimport {
    my ($class) = @_;
    return sub {
        filter_del();
        goto &$prev_unimport if $prev_unimport;
    }
}

1;

__END__

=head1 NAME

Filter::Simple - Simplified source filtering


=head1 SYNOPSIS

 # in MyFilter.pm:

     package MyFilter;

     use Filter::Simple;
     
     FILTER { ... };

     # or just:
     #
     # use Filter::Simple sub { ... };

 # in user's code:

     use MyFilter;

     # this code is filtered

     no MyFilter;

     # this code is not


=head1 DESCRIPTION

=head2 The Problem

Source filtering is an immensely powerful feature of recent versions of Perl.
It allows one to extend the language itself (e.g. the Switch module), to 
simplify the language (e.g. Language::Pythonesque), or to completely recast the
language (e.g. Lingua::Romana::Perligata). Effectively, it allows one to use
the full power of Perl as its own, recursively applied, macro language.

The excellent Filter::Util::Call module (by Paul Marquess) provides a
usable Perl interface to source filtering, but it is often too powerful
and not nearly as simple as it could be.

To use the module it is necessary to do the following:

=over 4

=item 1.

Download, build, and install the Filter::Util::Call module.
(If you have Perl 5.7.1 or later, this is already done for you.)

=item 2.

Set up a module that does a C<use Filter::Util::Call>.

=item 3.

Within that module, create an C<import> subroutine.

=item 4.

Within the C<import> subroutine do a call to C<filter_add>, passing
it either a subroutine reference.

=item 5.

Within the subroutine reference, call C<filter_read> or C<filter_read_exact>
to "prime" $_ with source code data from the source file that will
C<use> your module. Check the status value returned to see if any
source code was actually read in.

=item 6.

Process the contents of $_ to change the source code in the desired manner.

=item 7.

Return the status value.

=item 8.

If the act of unimporting your module (via a C<no>) should cause source
code filtering to cease, create an C<unimport> subroutine, and have it call
C<filter_del>. Make sure that the call to C<filter_read> or
C<filter_read_exact> in step 5 will not accidentally read past the
C<no>. Effectively this limits source code filters to line-by-line
operation, unless the C<import> subroutine does some fancy
pre-pre-parsing of the source code it's filtering.

=back

For example, here is a minimal source code filter in a module named
BANG.pm. It simply converts every occurrence of the sequence C<BANG\s+BANG>
to the sequence C<die 'BANG' if $BANG> in any piece of code following a
C<use BANG;> statement (until the next C<no BANG;> statement, if any):

    package BANG;
 
    use Filter::Util::Call ;

    sub import {
        filter_add( sub {
        my $caller = caller;
        my ($status, $no_seen, $data);
        while ($status = filter_read()) {
            if (/^\s*no\s+$caller\s*;\s*?$/) {
                $no_seen=1;
                last;
            }
            $data .= $_;
            $_ = "";
        }
        $_ = $data;
        s/BANG\s+BANG/die 'BANG' if \$BANG/g
            unless $status < 0;
        $_ .= "no $class;\n" if $no_seen;
        return 1;
        })
    }

    sub unimport {
        filter_del();
    }

    1 ;

This level of sophistication puts filtering out of the reach of
many programmers.


=head2 A Solution

The Filter::Simple module provides a simplified interface to
Filter::Util::Call; one that is sufficient for most common cases.

Instead of the above process, with Filter::Simple the task of setting up
a source code filter is reduced to:

=over 4

=item 1.

Download and install the Filter::Simple module.
(If you have Perl 5.7.1 or later, this is already done for you.)

=item 2.

Set up a module that does a C<use Filter::Simple> and then
calls C<FILTER { ... }>.

=item 3.

Within the anonymous subroutine or block that is passed to
C<FILTER>, process the contents of $_ to change the source code in
the desired manner.

=back

In other words, the previous example, would become:

    package BANG;
    use Filter::Simple;
    
    FILTER {
        s/BANG\s+BANG/die 'BANG' if \$BANG/g;
    };

    1 ;

Note that the source code is passed as a single string, so any regex that
uses C<^> or C<$> to detect line boundaries will need the C</m> flag.

=head2 Disabling or changing <no> behaviour

By default, the installed filter only filters up to a line consisting of one of
the three standard source "terminators":

    no ModuleName;  # optional comment

or:

    __END__

or:

    __DATA__

but this can be altered by passing a second argument to C<use Filter::Simple>
or C<FILTER> (just remember: there's I<no> comma after the initial block when
you use C<FILTER>).

That second argument may be either a C<qr>'d regular expression (which is then
used to match the terminator line), or a defined false value (which indicates
that no terminator line should be looked for), or a reference to a hash
(in which case the terminator is the value associated with the key
C<'terminator'>.

For example, to cause the previous filter to filter only up to a line of the
form:

    GNAB esu;

you would write:

    package BANG;
    use Filter::Simple;
    
    FILTER {
        s/BANG\s+BANG/die 'BANG' if \$BANG/g;
    }
    qr/^\s*GNAB\s+esu\s*;\s*?$/;

or:

    FILTER {
        s/BANG\s+BANG/die 'BANG' if \$BANG/g;
    }
    { terminator => qr/^\s*GNAB\s+esu\s*;\s*?$/ };

and to prevent the filter's being turned off in any way:

    package BANG;
    use Filter::Simple;
    
    FILTER {
        s/BANG\s+BANG/die 'BANG' if \$BANG/g;
    }
    "";    # or: 0

or:

    FILTER {
        s/BANG\s+BANG/die 'BANG' if \$BANG/g;
    }
    { terminator => "" };

B<Note that, no matter what you set the terminator pattern to,
the actual terminator itself I<must> be contained on a single source line.>


=head2 All-in-one interface

Separating the loading of Filter::Simple:

    use Filter::Simple;

from the setting up of the filtering:

    FILTER { ... };

is useful because it allows other code (typically parser support code
or caching variables) to be defined before the filter is invoked.
However, there is often no need for such a separation.

In those cases, it is easier to just append the filtering subroutine and
any terminator specification directly to the C<use> statement that loads
Filter::Simple, like so:

    use Filter::Simple sub {
        s/BANG\s+BANG/die 'BANG' if \$BANG/g;
    };

This is exactly the same as:

    use Filter::Simple;
    BEGIN {
        Filter::Simple::FILTER {
            s/BANG\s+BANG/die 'BANG' if \$BANG/g;
        };
    }

except that the C<FILTER> subroutine is not exported by Filter::Simple.


=head2 Filtering only specific components of source code

One of the problems with a filter like:

    use Filter::Simple;

    FILTER { s/BANG\s+BANG/die 'BANG' if \$BANG/g };

is that it indiscriminately applies the specified transformation to
the entire text of your source program. So something like:

    warn 'BANG BANG, YOU'RE DEAD';
    BANG BANG;

will become:

    warn 'die 'BANG' if $BANG, YOU'RE DEAD';
    die 'BANG' if $BANG;

It is very common when filtering source to only want to apply the filter
to the non-character-string parts of the code, or alternatively to I<only>
the character strings.

Filter::Simple supports this type of filtering by automatically
exporting the C<FILTER_ONLY> subroutine.

C<FILTER_ONLY> takes a sequence of specifiers that install separate
(and possibly multiple) filters that act on only parts of the source code.
For example:

    use Filter::Simple;

    FILTER_ONLY
        code      => sub { s/BANG\s+BANG/die 'BANG' if \$BANG/g },
        quotelike => sub { s/BANG\s+BANG/CHITTY CHITTY/g };

The C<"code"> subroutine will only be used to filter parts of the source
code that are not quotelikes, POD, or C<__DATA__>. The C<quotelike>
subroutine only filters Perl quotelikes (including here documents).

The full list of alternatives is:

=over

=item C<"code">

Filters only those sections of the source code that are not quotelikes, POD, or
C<__DATA__>.

=item C<"code_no_comments">

Filters only those sections of the source code that are not quotelikes, POD,
comments, or C<__DATA__>.

=item C<"executable">

Filters only those sections of the source code that are not POD or C<__DATA__>.

=item C<"executable_no_comments">

Filters only those sections of the source code that are not POD, comments, or C<__DATA__>.

=item C<"quotelike">

Filters only Perl quotelikes (as interpreted by
C<&Text::Balanced::extract_quotelike>).

=item C<"string">

Filters only the string literal parts of a Perl quotelike (i.e. the 
contents of a string literal, either half of a C<tr///>, the second
half of an C<s///>).

=item C<"regex">

Filters only the pattern literal parts of a Perl quotelike (i.e. the 
contents of a C<qr//> or an C<m//>, the first half of an C<s///>).

=item C<"all">

Filters everything. Identical in effect to C<FILTER>.

=back

Except for C<< FILTER_ONLY code => sub {...} >>, each of
the component filters is called repeatedly, once for each component
found in the source code.

Note that you can also apply two or more of the same type of filter in
a single C<FILTER_ONLY>. For example, here's a simple 
macro-preprocessor that is only applied within regexes,
with a final debugging pass that prints the resulting source code:

    use Regexp::Common;
    FILTER_ONLY
        regex => sub { s/!\[/[^/g },
        regex => sub { s/%d/$RE{num}{int}/g },
        regex => sub { s/%f/$RE{num}{real}/g },
        all   => sub { print if $::DEBUG };



=head2 Filtering only the code parts of source code
 
Most source code ceases to be grammatically correct when it is broken up
into the pieces between string literals and regexes. So the C<'code'>
and C<'code_no_comments'> component filter behave slightly differently
from the other partial filters described in the previous section.

Rather than calling the specified processor on each individual piece of
code (i.e. on the bits between quotelikes), the C<'code...'> partial
filters operate on the entire source code, but with the quotelike bits
(and, in the case of C<'code_no_comments'>, the comments) "blanked out".

That is, a C<'code...'> filter I<replaces> each quoted string, quotelike,
regex, POD, and __DATA__ section with a placeholder. The
delimiters of this placeholder are the contents of the C<$;> variable
at the time the filter is applied (normally C<"\034">). The remaining
four bytes are a unique identifier for the component being replaced.

This approach makes it comparatively easy to write code preprocessors
without worrying about the form or contents of strings, regexes, etc.

For convenience, during a C<'code...'> filtering operation, Filter::Simple
provides a package variable (C<$Filter::Simple::placeholder>) that
contains a pre-compiled regex that matches any placeholder...and
captures the identifier within the placeholder. Placeholders can be
moved and re-ordered within the source code as needed.

In addition, a second package variable (C<@Filter::Simple::components>)
contains a list of the various pieces of C<$_>, as they were originally split
up to allow placeholders to be inserted.

Once the filtering has been applied, the original strings, regexes, POD,
etc. are re-inserted into the code, by replacing each placeholder with
the corresponding original component (from C<@components>). Note that
this means that the C<@components> variable must be treated with extreme
care within the filter. The C<@components> array stores the "back-
translations" of each placeholder inserted into C<$_>, as well as the
interstitial source code between placeholders. If the placeholder
backtranslations are altered in C<@components>, they will be similarly
changed when the placeholders are removed from C<$_> after the filter
is complete.

For example, the following filter detects concatenated pairs of
strings/quotelikes and reverses the order in which they are
concatenated:

    package DemoRevCat;
    use Filter::Simple;

    FILTER_ONLY code => sub {
        my $ph = $Filter::Simple::placeholder;
        s{ ($ph) \s* [.] \s* ($ph) }{ $2.$1 }gx
    };

Thus, the following code:

    use DemoRevCat;

    my $str = "abc" . q(def);

    print "$str\n";

would become:

    my $str = q(def)."abc";

    print "$str\n";

and hence print:

    defabc


=head2 Using Filter::Simple with an explicit C<import> subroutine

Filter::Simple generates a special C<import> subroutine for
your module (see L<"How it works">) which would normally replace any
C<import> subroutine you might have explicitly declared.

However, Filter::Simple is smart enough to notice your existing
C<import> and Do The Right Thing with it.
That is, if you explicitly define an C<import> subroutine in a package
that's using Filter::Simple, that C<import> subroutine will still
be invoked immediately after any filter you install.

The only thing you have to remember is that the C<import> subroutine
I<must> be declared I<before> the filter is installed. If you use C<FILTER>
to install the filter:

    package Filter::TurnItUpTo11;

    use Filter::Simple;

    FILTER { s/(\w+)/\U$1/ };
    
that will almost never be a problem, but if you install a filtering
subroutine by passing it directly to the C<use Filter::Simple>
statement:

    package Filter::TurnItUpTo11;

    use Filter::Simple sub{ s/(\w+)/\U$1/ };

then you must make sure that your C<import> subroutine appears before
that C<use> statement.


=head2 Using Filter::Simple and Exporter together

Likewise, Filter::Simple is also smart enough
to Do The Right Thing if you use Exporter:

    package Switch;
    use base Exporter;
    use Filter::Simple;

    @EXPORT    = qw(switch case);
    @EXPORT_OK = qw(given  when);

    FILTER { $_ = magic_Perl_filter($_) }

Immediately after the filter has been applied to the source,
Filter::Simple will pass control to Exporter, so it can do its magic too.

Of course, here too, Filter::Simple has to know you're using Exporter
before it applies the filter. That's almost never a problem, but if you're
nervous about it, you can guarantee that things will work correctly by
ensuring that your C<use base Exporter> always precedes your
C<use Filter::Simple>.


=head2 How it works

The Filter::Simple module exports into the package that calls C<FILTER>
(or C<use>s it directly) -- such as package "BANG" in the above example --
two automagically constructed
subroutines -- C<import> and C<unimport> -- which take care of all the
nasty details.

In addition, the generated C<import> subroutine passes its own argument
list to the filtering subroutine, so the BANG.pm filter could easily 
be made parametric:

    package BANG;
 
    use Filter::Simple;
    
    FILTER {
        my ($die_msg, $var_name) = @_;
        s/BANG\s+BANG/die '$die_msg' if \${$var_name}/g;
    };

    # and in some user code:

    use BANG "BOOM", "BAM";  # "BANG BANG" becomes: die 'BOOM' if $BAM


The specified filtering subroutine is called every time a C<use BANG> is
encountered, and passed all the source code following that call, up to
either the next C<no BANG;> (or whatever terminator you've set) or the
end of the source file, whichever occurs first. By default, any C<no
BANG;> call must appear by itself on a separate line, or it is ignored.


=head1 AUTHOR

Damian Conway (damian@conway.org)

=head1 COPYRIGHT

    Copyright (c) 2000-2001, Damian Conway. All Rights Reserved.
    This module is free software. It may be used, redistributed
    and/or modified under the same terms as Perl itself.
