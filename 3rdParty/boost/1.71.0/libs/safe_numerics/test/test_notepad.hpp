// this is a hack to workaround the limititation of template
// expansion depth in the MSVC compiler.
template<class L, class F>
BOOST_MP11_CONSTEXPR mp_if_c<mp_size<L>::value <= 1024, F> mp_for_each_1( F && f ){
    return detail::mp_for_each_impl( mp_rename<L, mp_list>(), std::forward<F>(f) );
}
template<class L, class F>
BOOST_MP11_CONSTEXPR mp_if_c<mp_size<L>::value >= 1025, F> mp_for_each_1( F && f ){
    mp_for_each<mp_take_c<L, 1024>>( std::forward<F>(f) );
    return mp_for_each_1<mp_drop_c<L, 1024>>( std::forward<F>(f) );
}

