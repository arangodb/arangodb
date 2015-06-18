package bytes;

sub length (_) {
    BEGIN { bytes::import() }
    return CORE::length($_[0]);
}

sub substr ($$;$$) {
    BEGIN { bytes::import() }
    return
	@_ == 2 ? CORE::substr($_[0], $_[1]) :
	@_ == 3 ? CORE::substr($_[0], $_[1], $_[2]) :
	          CORE::substr($_[0], $_[1], $_[2], $_[3]) ;
}

sub ord (_) {
    BEGIN { bytes::import() }
    return CORE::ord($_[0]);
}

sub chr (_) {
    BEGIN { bytes::import() }
    return CORE::chr($_[0]);
}

sub index ($$;$) {
    BEGIN { bytes::import() }
    return
	@_ == 2 ? CORE::index($_[0], $_[1]) :
	          CORE::index($_[0], $_[1], $_[2]) ;
}

sub rindex ($$;$) {
    BEGIN { bytes::import() }
    return
	@_ == 2 ? CORE::rindex($_[0], $_[1]) :
	          CORE::rindex($_[0], $_[1], $_[2]) ;
}

1;
