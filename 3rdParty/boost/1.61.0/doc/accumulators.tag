<?xml version='1.0' encoding='ISO-8859-1' standalone='yes' ?>
<tagfile>
  <compound kind="class">
    <name>mpl::eval_if</name>
    <filename>classmpl_1_1eval__if.html</filename>
  </compound>
  <compound kind="class">
    <name>mpl::inherit_linearly::type</name>
    <filename>classmpl_1_1inherit__linearly_1_1type.html</filename>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::as_feature</name>
    <filename>structboost_1_1accumulators_1_1as__feature.html</filename>
    <templarg>Feature</templarg>
    <member kind="typedef">
      <type>Feature</type>
      <name>type</name>
      <anchorfile>structboost_1_1accumulators_1_1as__feature.html</anchorfile>
      <anchor>a7394e83f03a0e8eb86fcdc63fa9cda94</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::as_weighted_feature</name>
    <filename>structboost_1_1accumulators_1_1as__weighted__feature.html</filename>
    <templarg>Feature</templarg>
    <member kind="typedef">
      <type>Feature</type>
      <name>type</name>
      <anchorfile>structboost_1_1accumulators_1_1as__weighted__feature.html</anchorfile>
      <anchor>ab3f196e87e3dcfaa9a3b0158fea60dbe</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::feature_of</name>
    <filename>structboost_1_1accumulators_1_1feature__of.html</filename>
    <templarg>Feature</templarg>
    <member kind="typedef">
      <type>Feature</type>
      <name>type</name>
      <anchorfile>structboost_1_1accumulators_1_1feature__of.html</anchorfile>
      <anchor>a9db0cf9fc26431bea0cf190ba5dab74b</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::depends_on</name>
    <filename>structboost_1_1accumulators_1_1depends__on.html</filename>
    <templarg></templarg>
    <templarg></templarg>
    <templarg></templarg>
    <base>depends_on_base&lt; mpl::transform&lt; mpl::vector&lt; Feature1, Feature2,...&gt;, as_feature&lt; mpl::_1 &gt; &gt;::type &gt;</base>
    <member kind="typedef">
      <type>mpl::false_</type>
      <name>is_weight_accumulator</name>
      <anchorfile>structboost_1_1accumulators_1_1depends__on.html</anchorfile>
      <anchor>a985b688e224c7ee167d28f42d930a257</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>mpl::transform&lt; mpl::vector&lt; Feature1, Feature2,...&gt;, as_feature&lt; mpl::_1 &gt; &gt;::type</type>
      <name>dependencies</name>
      <anchorfile>structboost_1_1accumulators_1_1depends__on.html</anchorfile>
      <anchor>ac27dba2ed95ab1c424261048a4604b22</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::extractor</name>
    <filename>structboost_1_1accumulators_1_1extractor.html</filename>
    <templarg></templarg>
    <class kind="struct">boost::accumulators::extractor::result&lt; this_type(A1)&gt;</class>
    <member kind="typedef">
      <type>extractor&lt; Feature &gt;</type>
      <name>this_type</name>
      <anchorfile>structboost_1_1accumulators_1_1extractor.html</anchorfile>
      <anchor>a554f534b7b0d9cb5d8235b35180991a4</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>detail::extractor_result&lt; Arg1, Feature &gt;::type</type>
      <name>operator()</name>
      <anchorfile>structboost_1_1accumulators_1_1extractor.html</anchorfile>
      <anchor>a2dead1ae2c017a80a765fd0f636597ae</anchor>
      <arglist>(Arg1 const &amp;arg1) const </arglist>
    </member>
    <member kind="function">
      <type>detail::extractor_result&lt; AccumulatorSet, Feature &gt;::type</type>
      <name>operator()</name>
      <anchorfile>structboost_1_1accumulators_1_1extractor.html</anchorfile>
      <anchor>a93d3942e8ad550f925c95122d8035bdd</anchor>
      <arglist>(AccumulatorSet const &amp;acc, A1 const &amp;a1) const </arglist>
    </member>
    <member kind="function">
      <type>detail::extractor_result&lt; AccumulatorSet, Feature &gt;::type</type>
      <name>operator()</name>
      <anchorfile>structboost_1_1accumulators_1_1extractor.html</anchorfile>
      <anchor>aa2d708d723b762ee86ef3e03f79aec25</anchor>
      <arglist>(AccumulatorSet const &amp;acc, A1 const &amp;a1, A2 const &amp;a2,...)</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::extractor::result&lt; this_type(A1)&gt;</name>
    <filename>structboost_1_1accumulators_1_1extractor_1_1result_3_01this__type_07A1_08_4.html</filename>
    <templarg></templarg>
    <base>extractor_result&lt; A1, Feature &gt;</base>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::detail::feature_tag</name>
    <filename>structboost_1_1accumulators_1_1detail_1_1feature__tag.html</filename>
    <templarg></templarg>
    <member kind="typedef">
      <type>Accumulator::feature_tag</type>
      <name>type</name>
      <anchorfile>structboost_1_1accumulators_1_1detail_1_1feature__tag.html</anchorfile>
      <anchor>acebd4d1a9eca307d968048169f5adac7</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::detail::undroppable</name>
    <filename>structboost_1_1accumulators_1_1detail_1_1undroppable.html</filename>
    <templarg></templarg>
    <member kind="typedef">
      <type>Feature</type>
      <name>type</name>
      <anchorfile>structboost_1_1accumulators_1_1detail_1_1undroppable.html</anchorfile>
      <anchor>a43b6e09cb47cd4783f1d8cd3f756c5d0</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::detail::undroppable&lt; tag::droppable&lt; Feature &gt; &gt;</name>
    <filename>structboost_1_1accumulators_1_1detail_1_1undroppable_3_01tag_1_1droppable_3_01Feature_01_4_01_4.html</filename>
    <templarg></templarg>
    <member kind="typedef">
      <type>Feature</type>
      <name>type</name>
      <anchorfile>structboost_1_1accumulators_1_1detail_1_1undroppable_3_01tag_1_1droppable_3_01Feature_01_4_01_4.html</anchorfile>
      <anchor>a8e557ec3e6aa77c641fe73d8268755aa</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::detail::is_dependent_on</name>
    <filename>structboost_1_1accumulators_1_1detail_1_1is__dependent__on.html</filename>
    <templarg></templarg>
    <templarg></templarg>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::detail::dependencies_of</name>
    <filename>structboost_1_1accumulators_1_1detail_1_1dependencies__of.html</filename>
    <templarg></templarg>
    <member kind="typedef">
      <type>Feature::dependencies</type>
      <name>type</name>
      <anchorfile>structboost_1_1accumulators_1_1detail_1_1dependencies__of.html</anchorfile>
      <anchor>aba1354bd1d35369d6f446a6ab0f1e168</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::detail::set_insert_range</name>
    <filename>structboost_1_1accumulators_1_1detail_1_1set__insert__range.html</filename>
    <templarg></templarg>
    <templarg></templarg>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::detail::collect_abstract_features</name>
    <filename>structboost_1_1accumulators_1_1detail_1_1collect__abstract__features.html</filename>
    <templarg></templarg>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::detail::depends_on_base</name>
    <filename>structboost_1_1accumulators_1_1detail_1_1depends__on__base.html</filename>
    <templarg>Features</templarg>
    <base>mpl::inherit_linearly::type</base>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::detail::matches_feature</name>
    <filename>structboost_1_1accumulators_1_1detail_1_1matches__feature.html</filename>
    <templarg></templarg>
    <class kind="struct">boost::accumulators::detail::matches_feature::apply</class>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::detail::matches_feature::apply</name>
    <filename>structboost_1_1accumulators_1_1detail_1_1matches__feature_1_1apply.html</filename>
    <templarg></templarg>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::detail::contains_feature_of</name>
    <filename>structboost_1_1accumulators_1_1detail_1_1contains__feature__of.html</filename>
    <templarg></templarg>
    <templarg></templarg>
    <member kind="typedef">
      <type>mpl::transform_view&lt; Features, feature_of&lt; as_feature&lt; mpl::_ &gt; &gt; &gt;</type>
      <name>features_list</name>
      <anchorfile>structboost_1_1accumulators_1_1detail_1_1contains__feature__of.html</anchorfile>
      <anchor>a6675c38642f7cfb450bfdbe3d9aab9ff</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>feature_of&lt; typename feature_tag&lt; Accumulator &gt;::type &gt;::type</type>
      <name>the_feature</name>
      <anchorfile>structboost_1_1accumulators_1_1detail_1_1contains__feature__of.html</anchorfile>
      <anchor>a392167c13bf85669509fb810eef5ba00</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>mpl::contains&lt; features_list, the_feature &gt;::type</type>
      <name>type</name>
      <anchorfile>structboost_1_1accumulators_1_1detail_1_1contains__feature__of.html</anchorfile>
      <anchor>af86f5781e8418e5933877b8e9c7c7508</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::detail::contains_feature_of_</name>
    <filename>structboost_1_1accumulators_1_1detail_1_1contains__feature__of__.html</filename>
    <templarg></templarg>
    <class kind="struct">boost::accumulators::detail::contains_feature_of_::apply</class>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::detail::contains_feature_of_::apply</name>
    <filename>structboost_1_1accumulators_1_1detail_1_1contains__feature__of___1_1apply.html</filename>
    <templarg></templarg>
    <base>boost::accumulators::detail::contains_feature_of</base>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::detail::build_acc_list&lt; First, Last, true &gt;</name>
    <filename>structboost_1_1accumulators_1_1detail_1_1build__acc__list_3_01First_00_01Last_00_01true_01_4.html</filename>
    <templarg></templarg>
    <templarg></templarg>
    <member kind="typedef">
      <type>fusion::nil_</type>
      <name>type</name>
      <anchorfile>structboost_1_1accumulators_1_1detail_1_1build__acc__list_3_01First_00_01Last_00_01true_01_4.html</anchorfile>
      <anchor>a1346268175b0ed535d69299544747de8</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" static="yes">
      <type>static fusion::nil_</type>
      <name>call</name>
      <anchorfile>structboost_1_1accumulators_1_1detail_1_1build__acc__list_3_01First_00_01Last_00_01true_01_4.html</anchorfile>
      <anchor>ad4a1da6a98761bc08cbda5d53fb446c2</anchor>
      <arglist>(Args const &amp;, First const &amp;, Last const &amp;)</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::detail::build_acc_list&lt; First, Last, false &gt;</name>
    <filename>structboost_1_1accumulators_1_1detail_1_1build__acc__list_3_01First_00_01Last_00_01false_01_4.html</filename>
    <templarg></templarg>
    <templarg></templarg>
    <member kind="typedef">
      <type>build_acc_list&lt; typename fusion::result_of::next&lt; First &gt;::type, Last &gt;</type>
      <name>next_build_acc_list</name>
      <anchorfile>structboost_1_1accumulators_1_1detail_1_1build__acc__list_3_01First_00_01Last_00_01false_01_4.html</anchorfile>
      <anchor>aa002fffefd5a6438cdb17a1d16f5acd4</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>fusion::cons&lt; typename fusion::result_of::value_of&lt; First &gt;::type, typename next_build_acc_list::type &gt;</type>
      <name>type</name>
      <anchorfile>structboost_1_1accumulators_1_1detail_1_1build__acc__list_3_01First_00_01Last_00_01false_01_4.html</anchorfile>
      <anchor>a1844abec1e2df128389721be1c8b1934</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" static="yes">
      <type>static type</type>
      <name>call</name>
      <anchorfile>structboost_1_1accumulators_1_1detail_1_1build__acc__list_3_01First_00_01Last_00_01false_01_4.html</anchorfile>
      <anchor>aa7a80abb1d9867376060a7350c4ee4af</anchor>
      <arglist>(Args const &amp;args, First const &amp;f, Last const &amp;l)</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::detail::checked_as_weighted_feature</name>
    <filename>structboost_1_1accumulators_1_1detail_1_1checked__as__weighted__feature.html</filename>
    <templarg></templarg>
    <member kind="typedef">
      <type>as_feature&lt; Feature &gt;::type</type>
      <name>feature_type</name>
      <anchorfile>structboost_1_1accumulators_1_1detail_1_1checked__as__weighted__feature.html</anchorfile>
      <anchor>af81b9803990774a1d26fd5e4f6c08399</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>as_weighted_feature&lt; feature_type &gt;::type</type>
      <name>type</name>
      <anchorfile>structboost_1_1accumulators_1_1detail_1_1checked__as__weighted__feature.html</anchorfile>
      <anchor>a1068d76183c160715203c41b05ba639d</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>BOOST_MPL_ASSERT</name>
      <anchorfile>structboost_1_1accumulators_1_1detail_1_1checked__as__weighted__feature.html</anchorfile>
      <anchor>afc95cfea055ba0e66c529b4c63e960eb</anchor>
      <arglist>((is_same&lt; typename feature_of&lt; feature_type &gt;::type, typename feature_of&lt; type &gt;::type &gt;))</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::detail::as_feature_list</name>
    <filename>structboost_1_1accumulators_1_1detail_1_1as__feature__list.html</filename>
    <templarg></templarg>
    <templarg></templarg>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::detail::as_feature_list&lt; Features, void &gt;</name>
    <filename>structboost_1_1accumulators_1_1detail_1_1as__feature__list_3_01Features_00_01void_01_4.html</filename>
    <templarg></templarg>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::detail::accumulator_wrapper</name>
    <filename>structboost_1_1accumulators_1_1detail_1_1accumulator__wrapper.html</filename>
    <templarg></templarg>
    <templarg></templarg>
    <member kind="typedef">
      <type>Feature</type>
      <name>feature_tag</name>
      <anchorfile>structboost_1_1accumulators_1_1detail_1_1accumulator__wrapper.html</anchorfile>
      <anchor>afce3db206ac219e19bea65cca4c26749</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>accumulator_wrapper</name>
      <anchorfile>structboost_1_1accumulators_1_1detail_1_1accumulator__wrapper.html</anchorfile>
      <anchor>a32ea11da8b6370acb823ed97b4fac408</anchor>
      <arglist>(accumulator_wrapper const &amp;that)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>accumulator_wrapper</name>
      <anchorfile>structboost_1_1accumulators_1_1detail_1_1accumulator__wrapper.html</anchorfile>
      <anchor>add73eb03a35d564e5964cdff0ad20376</anchor>
      <arglist>(Args const &amp;args)</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::detail::to_accumulator</name>
    <filename>structboost_1_1accumulators_1_1detail_1_1to__accumulator.html</filename>
    <templarg></templarg>
    <templarg></templarg>
    <templarg></templarg>
    <member kind="typedef">
      <type>accumulator_wrapper&lt; typename mpl::apply2&lt; typename Feature::impl, Sample, Weight &gt;::type, Feature &gt;</type>
      <name>type</name>
      <anchorfile>structboost_1_1accumulators_1_1detail_1_1to__accumulator.html</anchorfile>
      <anchor>a9e943bc5323fb078de1d1d3c8ab33ffb</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::detail::to_accumulator&lt; Feature, Sample, tag::external&lt; Weight, Tag, AccumulatorSet &gt; &gt;</name>
    <filename>structboost_1_1accumulators_1_1detail_1_1to__accumulator_3_01Feature_00_01Sample_00_01tag_1_1ext0c26d8c3c18bcca084cb467b003ed836.html</filename>
    <templarg></templarg>
    <templarg></templarg>
    <templarg></templarg>
    <templarg></templarg>
    <templarg></templarg>
    <member kind="typedef">
      <type>accumulator_wrapper&lt; typename mpl::apply2&lt; typename Feature::impl, Sample, Weight &gt;::type, Feature &gt;</type>
      <name>accumulator_type</name>
      <anchorfile>structboost_1_1accumulators_1_1detail_1_1to__accumulator_3_01Feature_00_01Sample_00_01tag_1_1ext0c26d8c3c18bcca084cb467b003ed836.html</anchorfile>
      <anchor>ae092c4e99a42ca3461933e1af7cb74aa</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>mpl::if_&lt; typename Feature::is_weight_accumulator, accumulator_wrapper&lt; impl::external_impl&lt; accumulator_type, tag::weights &gt;, Feature &gt;, accumulator_type &gt;::type</type>
      <name>type</name>
      <anchorfile>structboost_1_1accumulators_1_1detail_1_1to__accumulator_3_01Feature_00_01Sample_00_01tag_1_1ext0c26d8c3c18bcca084cb467b003ed836.html</anchorfile>
      <anchor>a4e218668889e529539672bc1e027f299</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>BOOST_MPL_ASSERT</name>
      <anchorfile>structboost_1_1accumulators_1_1detail_1_1to__accumulator_3_01Feature_00_01Sample_00_01tag_1_1ext0c26d8c3c18bcca084cb467b003ed836.html</anchorfile>
      <anchor>ab84e97a817dfbca46fcf3e17ba0c5d0f</anchor>
      <arglist>((is_same&lt; Tag, void &gt;))</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>BOOST_MPL_ASSERT</name>
      <anchorfile>structboost_1_1accumulators_1_1detail_1_1to__accumulator_3_01Feature_00_01Sample_00_01tag_1_1ext0c26d8c3c18bcca084cb467b003ed836.html</anchorfile>
      <anchor>a840711e0f3df48ac7a440650e7a82ee0</anchor>
      <arglist>((is_same&lt; AccumulatorSet, void &gt;))</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::detail::insert_feature</name>
    <filename>structboost_1_1accumulators_1_1detail_1_1insert__feature.html</filename>
    <templarg></templarg>
    <templarg></templarg>
    <base>mpl::eval_if</base>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::detail::insert_dependencies</name>
    <filename>structboost_1_1accumulators_1_1detail_1_1insert__dependencies.html</filename>
    <templarg></templarg>
    <templarg></templarg>
    <templarg></templarg>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::detail::insert_sequence</name>
    <filename>structboost_1_1accumulators_1_1detail_1_1insert__sequence.html</filename>
    <templarg></templarg>
    <templarg></templarg>
    <templarg></templarg>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::detail::make_accumulator_tuple</name>
    <filename>structboost_1_1accumulators_1_1detail_1_1make__accumulator__tuple.html</filename>
    <templarg></templarg>
    <templarg></templarg>
    <templarg></templarg>
    <member kind="typedef">
      <type>mpl::fold&lt; as_feature_list&lt; Features, Weight &gt;, mpl::map0&lt;&gt;, mpl::if_&lt; mpl::is_sequence&lt; mpl::_2 &gt;, insert_sequence&lt; mpl::_1, mpl::_2, Weight &gt;, insert_feature&lt; mpl::_1, mpl::_2 &gt; &gt; &gt;::type</type>
      <name>feature_map</name>
      <anchorfile>structboost_1_1accumulators_1_1detail_1_1make__accumulator__tuple.html</anchorfile>
      <anchor>a44f1beb32e4c56227e7035b39bb04cfd</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>mpl::fold&lt; feature_map, feature_map, insert_dependencies&lt; mpl::_1, mpl::second&lt; mpl::_2 &gt;, Weight &gt; &gt;::type</type>
      <name>feature_map_with_dependencies</name>
      <anchorfile>structboost_1_1accumulators_1_1detail_1_1make__accumulator__tuple.html</anchorfile>
      <anchor>ae6fd9412b5538e2fdc7995116621c54b</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>mpl::insert_range&lt; mpl::vector&lt;&gt;, mpl::end&lt; mpl::vector&lt;&gt; &gt;::type, mpl::transform_view&lt; feature_map_with_dependencies, mpl::second&lt; mpl::_1 &gt; &gt; &gt;::type</type>
      <name>feature_vector_with_dependencies</name>
      <anchorfile>structboost_1_1accumulators_1_1detail_1_1make__accumulator__tuple.html</anchorfile>
      <anchor>ac71d803bbf582ed80647b9d968ed8aa6</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>mpl::sort&lt; feature_vector_with_dependencies, is_dependent_on&lt; mpl::_2, mpl::_1 &gt; &gt;::type</type>
      <name>sorted_feature_vector</name>
      <anchorfile>structboost_1_1accumulators_1_1detail_1_1make__accumulator__tuple.html</anchorfile>
      <anchor>ae06bc572d8d0942333d455a28c107db7</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>mpl::transform&lt; sorted_feature_vector, to_accumulator&lt; mpl::_1, Sample, Weight &gt; &gt;::type</type>
      <name>type</name>
      <anchorfile>structboost_1_1accumulators_1_1detail_1_1make__accumulator__tuple.html</anchorfile>
      <anchor>ac0cc446f96a3ab6f6d8aca2b211aea57</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::detail::accumulator_set_result</name>
    <filename>structboost_1_1accumulators_1_1detail_1_1accumulator__set__result.html</filename>
    <templarg>AccumulatorSet</templarg>
    <templarg>Feature</templarg>
    <member kind="typedef">
      <type>as_feature&lt; Feature &gt;::type</type>
      <name>feature_type</name>
      <anchorfile>structboost_1_1accumulators_1_1detail_1_1accumulator__set__result.html</anchorfile>
      <anchor>a1b81c544ebcad329cdd7b04adbf3c163</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>mpl::apply&lt; AccumulatorSet, feature_type &gt;::type::result_type</type>
      <name>type</name>
      <anchorfile>structboost_1_1accumulators_1_1detail_1_1accumulator__set__result.html</anchorfile>
      <anchor>a195bd4fb7b955f4f63432d93635be4d1</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::detail::argument_pack_result</name>
    <filename>structboost_1_1accumulators_1_1detail_1_1argument__pack__result.html</filename>
    <templarg></templarg>
    <templarg></templarg>
    <base>accumulator_set_result&lt; remove_reference&lt; parameter::binding&lt; Args, tag::accumulator &gt;::type &gt;::type, Feature &gt;</base>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::detail::extractor_result</name>
    <filename>structboost_1_1accumulators_1_1detail_1_1extractor__result.html</filename>
    <templarg>A</templarg>
    <templarg>Feature</templarg>
    <base>mpl::eval_if</base>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::detail::meta::make_acc_list</name>
    <filename>structboost_1_1accumulators_1_1detail_1_1meta_1_1make__acc__list.html</filename>
    <templarg></templarg>
  </compound>
</tagfile>
