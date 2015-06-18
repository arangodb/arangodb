package ExtUtils::Constant::ProxySubs;

use strict;
use vars qw($VERSION @ISA %type_to_struct %type_from_struct %type_to_sv
	    %type_to_C_value %type_is_a_problem %type_num_args
	    %type_temporary);
use Carp;
require ExtUtils::Constant::XS;
use ExtUtils::Constant::Utils qw(C_stringify);
use ExtUtils::Constant::XS qw(%XS_TypeSet);

$VERSION = '0.06';
@ISA = 'ExtUtils::Constant::XS';

%type_to_struct =
    (
     IV => '{const char *name; I32 namelen; IV value;}',
     NV => '{const char *name; I32 namelen; NV value;}',
     UV => '{const char *name; I32 namelen; UV value;}',
     PV => '{const char *name; I32 namelen; const char *value;}',
     PVN => '{const char *name; I32 namelen; const char *value; STRLEN len;}',
     YES => '{const char *name; I32 namelen;}',
     NO => '{const char *name; I32 namelen;}',
     UNDEF => '{const char *name; I32 namelen;}',
     '' => '{const char *name; I32 namelen;} ',
     );

%type_from_struct =
    (
     IV => sub { $_[0] . '->value' },
     NV => sub { $_[0] . '->value' },
     UV => sub { $_[0] . '->value' },
     PV => sub { $_[0] . '->value' },
     PVN => sub { $_[0] . '->value', $_[0] . '->len' },
     YES => sub {},
     NO => sub {},
     UNDEF => sub {},
     '' => sub {},
    );

%type_to_sv = 
    (
     IV => sub { "newSViv($_[0])" },
     NV => sub { "newSVnv($_[0])" },
     UV => sub { "newSVuv($_[0])" },
     PV => sub { "newSVpv($_[0], 0)" },
     PVN => sub { "newSVpvn($_[0], $_[1])" },
     YES => sub { '&PL_sv_yes' },
     NO => sub { '&PL_sv_no' },
     UNDEF => sub { '&PL_sv_undef' },
     '' => sub { '&PL_sv_yes' },
     SV => sub {"SvREFCNT_inc($_[0])"},
     );

%type_to_C_value = 
    (
     YES => sub {},
     NO => sub {},
     UNDEF => sub {},
     '' => sub {},
     );

sub type_to_C_value {
    my ($self, $type) = @_;
    return $type_to_C_value{$type} || sub {return map {ref $_ ? @$_ : $_} @_};
}

# TODO - figure out if there is a clean way for the type_to_sv code to
# attempt s/sv_2mortal// and if it succeeds tell type_to_sv not to add
# SvREFCNT_inc
%type_is_a_problem =
    (
     # The documentation says *mortal SV*, but we now need a non-mortal copy.
     SV => 1,
     );

%type_temporary =
    (
     SV => ['SV *'],
     PV => ['const char *'],
     PVN => ['const char *', 'STRLEN'],
     );
$type_temporary{$_} = [$_] foreach qw(IV UV NV);
     
while (my ($type, $value) = each %XS_TypeSet) {
    $type_num_args{$type}
	= defined $value ? ref $value ? scalar @$value : 1 : 0;
}
$type_num_args{''} = 0;

sub partition_names {
    my ($self, $default_type, @items) = @_;
    my (%found, @notfound, @trouble);

    while (my $item = shift @items) {
	my $default = delete $item->{default};
	if ($default) {
	    # If we find a default value, convert it into a regular item and
	    # append it to the queue of items to process
	    my $default_item = {%$item};
	    $default_item->{invert_macro} = 1;
	    $default_item->{pre} = delete $item->{def_pre};
	    $default_item->{post} = delete $item->{def_post};
	    $default_item->{type} = shift @$default;
	    $default_item->{value} = $default;
	    push @items, $default_item;
	} else {
	    # It can be "not found" unless it's the default (invert the macro)
	    # or the "macro" is an empty string (ie no macro)
	    push @notfound, $item unless $item->{invert_macro}
		or !$self->macro_to_ifdef($self->macro_from_item($item));
	}

	if ($item->{pre} or $item->{post} or $item->{not_constant}
	    or $type_is_a_problem{$item->{type}}) {
	    push @trouble, $item;
	} else {
	    push @{$found{$item->{type}}}, $item;
	}
    }
    # use Data::Dumper; print Dumper \%found;
    (\%found, \@notfound, \@trouble);
}

sub boottime_iterator {
    my ($self, $type, $iterator, $hash, $subname) = @_;
    my $extractor = $type_from_struct{$type};
    die "Can't find extractor code for type $type"
	unless defined $extractor;
    my $generator = $type_to_sv{$type};
    die "Can't find generator code for type $type"
	unless defined $generator;

    my $athx = $self->C_constant_prefix_param();

    return sprintf <<"EOBOOT", &$generator(&$extractor($iterator));
        while ($iterator->name) {
	    $subname($athx $hash, $iterator->name,
				$iterator->namelen, %s);
	    ++$iterator;
	}
EOBOOT
}

sub name_len_value_macro {
    my ($self, $item) = @_;
    my $name = $item->{name};
    my $value = $item->{value};
    $value = $item->{name} unless defined $value;

    my $namelen = length $name;
    if ($name =~ tr/\0-\377// != $namelen) {
	# the hash API signals UTF-8 by passing the length negated.
	utf8::encode($name);
	$namelen = -length $name;
    }
    $name = C_stringify($name);

    my $macro = $self->macro_from_item($item);
    ($name, $namelen, $value, $macro);
}

sub WriteConstants {
    my $self = shift;
    my $ARGS = {@_};

    my ($c_fh, $xs_fh, $c_subname, $xs_subname, $default_type, $package)
	= @{$ARGS}{qw(C_FH XS_FH C_SUBNAME XS_SUBNAME DEFAULT_TYPE NAME)};

    my $options = $ARGS->{PROXYSUBS};
    $options = {} unless ref $options;
    my $explosives = $options->{croak_on_read};

    $xs_subname ||= 'constant';

    # If anyone is insane enough to suggest a package name containing %
    my $package_sprintf_safe = $package;
    $package_sprintf_safe =~ s/%/%%/g;

    # All the types we see
    my $what = {};
    # A hash to lookup items with.
    my $items = {};

    my @items = $self->normalise_items ({disable_utf8_duplication => 1},
					$default_type, $what, $items,
					@{$ARGS->{NAMES}});

    # Partition the values by type. Also include any defaults in here
    # Everything that doesn't have a default needs alternative code for
    # "I'm missing"
    # And everything that has pre or post code ends up in a private block
    my ($found, $notfound, $trouble)
	= $self->partition_names($default_type, @items);

    my $pthx = $self->C_constant_prefix_param_defintion();
    my $athx = $self->C_constant_prefix_param();
    my $symbol_table = C_stringify($package) . '::';

    my $can_do_pcs = $] >= 5.009;
    my $cast_CONSTSUB = $] < 5.010 ? '(char *)' : '';

    print $c_fh $self->header(), <<"EOADD";
static void
${c_subname}_add_symbol($pthx HV *hash, const char *name, I32 namelen, SV *value) {
EOADD
    if (!$can_do_pcs) {
	print $c_fh <<'EO_NOPCS';
    if (namelen == namelen) {
EO_NOPCS
    } else {
	print $c_fh <<"EO_PCS";
    SV **sv = hv_fetch(hash, name, namelen, TRUE);
    if (!sv) {
        Perl_croak($athx "Couldn't add key '%s' to %%$package_sprintf_safe\::",
		   name);
    }
    if (SvOK(*sv) || SvTYPE(*sv) == SVt_PVGV) {
	/* Someone has been here before us - have to make a real sub.  */
EO_PCS
    }
    # This piece of code is common to both
    print $c_fh <<"EOADD";
	newCONSTSUB(hash, ${cast_CONSTSUB}name, value);
EOADD
    if ($can_do_pcs) {
	print $c_fh <<'EO_PCS';
    } else {
	SvUPGRADE(*sv, SVt_RV);
	SvRV_set(*sv, value);
	SvROK_on(*sv);
	SvREADONLY_on(value);
    }
EO_PCS
    } else {
	print $c_fh <<'EO_NOPCS';
    }
EO_NOPCS
    }
    print $c_fh <<'EOADD';
}

EOADD

    print $c_fh $explosives ? <<"EXPLODE" : "\n";

static int
Im_sorry_Dave(pTHX_ SV *sv, MAGIC *mg)
{
    PERL_UNUSED_ARG(mg);
    Perl_croak(aTHX_
	       "Your vendor has not defined $package_sprintf_safe macro %"SVf
	       " used", sv);
    NORETURN_FUNCTION_END;
}

static MGVTBL not_defined_vtbl = {
 Im_sorry_Dave, /* get - I'm afraid I can't do that */
 Im_sorry_Dave, /* set */
 0, /* len */
 0, /* clear */
 0, /* free */
 0, /* copy */
 0, /* dup */
};

EXPLODE

{
    my $key = $symbol_table;
    # Just seems tidier (and slightly more space efficient) not to have keys
    # such as Fcntl::
    $key =~ s/::$//;
    my $key_len = length $key;

    print $c_fh <<"MISSING";

#ifndef SYMBIAN

/* Store a hash of all symbols missing from the package. To avoid trampling on
   the package namespace (uninvited) put each package's hash in our namespace.
   To avoid creating lots of typeblogs and symbol tables for sub-packages, put
   each package's hash into one hash in our namespace.  */

static HV *
get_missing_hash(pTHX) {
    HV *const parent
	= get_hv("ExtUtils::Constant::ProxySubs::Missing", GVf_MULTI);
    /* We could make a hash of hashes directly, but this would confuse anything
	at Perl space that looks at us, and as we're visible in Perl space,
	best to play nice. */
    SV *const *const ref
	= hv_fetch(parent, "$key", $key_len, TRUE);
    HV *new_hv;

    if (!ref)
	return NULL;

    if (SvROK(*ref))
	return (HV*) SvRV(*ref);

    new_hv = newHV();
    SvUPGRADE(*ref, SVt_RV);
    SvRV_set(*ref, (SV *)new_hv);
    SvROK_on(*ref);
    return new_hv;
}

#endif

MISSING

}

    print $xs_fh <<"EOBOOT";
BOOT:
  {
#ifdef dTHX
    dTHX;
#endif
    HV *symbol_table = get_hv("$symbol_table", TRUE);
#ifndef SYMBIAN
    HV *${c_subname}_missing;
#endif
EOBOOT

    my %iterator;

    $found->{''}
        = [map {{%$_, type=>'', invert_macro => 1}} @$notfound];

    foreach my $type (sort keys %$found) {
	my $struct = $type_to_struct{$type};
	my $type_to_value = $self->type_to_C_value($type);
	my $number_of_args = $type_num_args{$type};
	die "Can't find structure definition for type $type"
	    unless defined $struct;

	my $struct_type = $type ? lc($type) . '_s' : 'notfound_s';
	print $c_fh "struct $struct_type $struct;\n";

	my $array_name = 'values_for_' . ($type ? lc $type : 'notfound');
	print $xs_fh <<"EOBOOT";

    static const struct $struct_type $array_name\[] =
      {
EOBOOT


	foreach my $item (@{$found->{$type}}) {
            my ($name, $namelen, $value, $macro)
                 = $self->name_len_value_macro($item);

	    my $ifdef = $self->macro_to_ifdef($macro);
	    if (!$ifdef && $item->{invert_macro}) {
		carp("Attempting to supply a default for '$name' which has no conditional macro");
		next;
	    }
	    print $xs_fh $ifdef;
	    if ($item->{invert_macro}) {
		print $xs_fh
		    "        /* This is the default value: */\n" if $type;
		print $xs_fh "#else\n";
	    }
	    print $xs_fh "        { ", join (', ', "\"$name\"", $namelen,
					     &$type_to_value($value)), " },\n",
						 $self->macro_to_endif($macro);
	}


    # Terminate the list with a NULL
	print $xs_fh "        { NULL, 0", (", 0" x $number_of_args), " } };\n";

	$iterator{$type} = "value_for_" . ($type ? lc $type : 'notfound');

	print $xs_fh <<"EOBOOT";
	const struct $struct_type *$iterator{$type} = $array_name;
EOBOOT
    }

    delete $found->{''};

    print $xs_fh <<"EOBOOT";
#ifndef SYMBIAN
	${c_subname}_missing = get_missing_hash(aTHX);
#endif
EOBOOT

    my $add_symbol_subname = $c_subname . '_add_symbol';
    foreach my $type (sort keys %$found) {
	print $xs_fh $self->boottime_iterator($type, $iterator{$type}, 
					      'symbol_table',
					      $add_symbol_subname);
    }

    print $xs_fh <<"EOBOOT";
	while (value_for_notfound->name) {
EOBOOT

    print $xs_fh $explosives ? <<"EXPLODE" : << "DONT";
	    SV *tripwire = newSV(0);
	    
	    sv_magicext(tripwire, 0, PERL_MAGIC_ext, &not_defined_vtbl, 0, 0);
	    SvPV_set(tripwire, (char *)value_for_notfound->name);
	    if(value_for_notfound->namelen >= 0) {
		SvCUR_set(tripwire, value_for_notfound->namelen);
	    } else {
		SvCUR_set(tripwire, -value_for_notfound->namelen);
		SvUTF8_on(tripwire);
	    }
	    SvPOKp_on(tripwire);
	    SvREADONLY_on(tripwire);
	    assert(SvLEN(tripwire) == 0);

	    $add_symbol_subname($athx symbol_table, value_for_notfound->name,
				value_for_notfound->namelen, tripwire);
EXPLODE

	    /* Need to add prototypes, else parsing will vary by platform.  */
	    SV **sv = hv_fetch(symbol_table, value_for_notfound->name,
			       value_for_notfound->namelen, TRUE);
	    if (!sv) {
		Perl_croak($athx
			   "Couldn't add key '%s' to %%$package_sprintf_safe\::",
			   value_for_notfound->name);
	    }
	    if (!SvOK(*sv) && SvTYPE(*sv) != SVt_PVGV) {
		/* Nothing was here before, so mark a prototype of ""  */
		sv_setpvn(*sv, "", 0);
	    } else if (SvPOK(*sv) && SvCUR(*sv) == 0) {
		/* There is already a prototype of "" - do nothing  */
	    } else {
		/* Someone has been here before us - have to make a real
		   typeglob.  */
		/* It turns out to be incredibly hard to deal with all the
		   corner cases of sub foo (); and reporting errors correctly,
		   so lets cheat a bit.  Start with a constant subroutine  */
		CV *cv = newCONSTSUB(symbol_table,
				     ${cast_CONSTSUB}value_for_notfound->name,
				     &PL_sv_yes);
		/* and then turn it into a non constant declaration only.  */
		SvREFCNT_dec(CvXSUBANY(cv).any_ptr);
		CvCONST_off(cv);
		CvXSUB(cv) = NULL;
		CvXSUBANY(cv).any_ptr = NULL;
	    }
#ifndef SYMBIAN
	    if (!hv_store(${c_subname}_missing, value_for_notfound->name,
			  value_for_notfound->namelen, &PL_sv_yes, 0))
		Perl_croak($athx "Couldn't add key '%s' to missing_hash",
			   value_for_notfound->name);
#endif
DONT

    print $xs_fh <<"EOBOOT";

	    ++value_for_notfound;
	}
EOBOOT

    foreach my $item (@$trouble) {
        my ($name, $namelen, $value, $macro)
	    = $self->name_len_value_macro($item);
        my $ifdef = $self->macro_to_ifdef($macro);
        my $type = $item->{type};
	my $type_to_value = $self->type_to_C_value($type);

        print $xs_fh $ifdef;
	if ($item->{invert_macro}) {
	    print $xs_fh
		 "        /* This is the default value: */\n" if $type;
	    print $xs_fh "#else\n";
	}
	my $generator = $type_to_sv{$type};
	die "Can't find generator code for type $type"
	    unless defined $generator;

	print $xs_fh "        {\n";
	# We need to use a temporary value because some really troublesome
	# items use C pre processor directives in their values, and in turn
	# these don't fit nicely in the macro-ised generator functions
	my $counter = 0;
	printf $xs_fh "            %s temp%d;\n", $_, $counter++
	    foreach @{$type_temporary{$type}};

	print $xs_fh "            $item->{pre}\n" if $item->{pre};

	# And because the code in pre might be both declarations and
	# statements, we can't declare and assign to the temporaries in one.
	$counter = 0;
	printf $xs_fh "            temp%d = %s;\n", $counter++, $_
	    foreach &$type_to_value($value);

	my @tempvarnames = map {sprintf 'temp%d', $_} 0 .. $counter - 1;
	printf $xs_fh <<"EOBOOT", $name, &$generator(@tempvarnames);
	    ${c_subname}_add_symbol($athx symbol_table, "%s",
				    $namelen, %s);
EOBOOT
	print $xs_fh "        $item->{post}\n" if $item->{post};
	print $xs_fh "        }\n";

        print $xs_fh $self->macro_to_endif($macro);
    }

    print $xs_fh <<EOBOOT;
    /* As we've been creating subroutines, we better invalidate any cached
       methods  */
    ++PL_sub_generation;
  }
EOBOOT

    print $xs_fh $explosives ? <<"EXPLODE" : <<"DONT";

void
$xs_subname(sv)
    INPUT:
	SV *		sv;
    PPCODE:
	sv = newSVpvf("Your vendor has not defined $package_sprintf_safe macro %" SVf
			  ", used", sv);
        PUSHs(sv_2mortal(sv));
EXPLODE

void
$xs_subname(sv)
    PREINIT:
	STRLEN		len;
    INPUT:
	SV *		sv;
        const char *	s = SvPV(sv, len);
    PPCODE:
#ifdef SYMBIAN
	sv = newSVpvf("%"SVf" is not a valid $package_sprintf_safe macro", sv);
#else
	HV *${c_subname}_missing = get_missing_hash(aTHX);
	if (hv_exists(${c_subname}_missing, s, SvUTF8(sv) ? -(I32)len : (I32)len)) {
	    sv = newSVpvf("Your vendor has not defined $package_sprintf_safe macro %" SVf
			  ", used", sv);
	} else {
	    sv = newSVpvf("%"SVf" is not a valid $package_sprintf_safe macro",
			  sv);
	}
#endif
	PUSHs(sv_2mortal(sv));
DONT

}

1;
