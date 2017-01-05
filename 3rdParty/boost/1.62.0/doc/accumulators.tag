<?xml version='1.0' encoding='UTF-8' standalone='yes' ?>
<tagfile>
  <compound kind="struct">
    <name>boost::accumulators::as_feature</name>
    <filename>structboost_1_1accumulators_1_1as__feature.html</filename>
    <templarg>Feature</templarg>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::as_weighted_feature</name>
    <filename>structboost_1_1accumulators_1_1as__weighted__feature.html</filename>
    <templarg>Feature</templarg>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::feature_of</name>
    <filename>structboost_1_1accumulators_1_1feature__of.html</filename>
    <templarg>Feature</templarg>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::depends_on</name>
    <filename>structboost_1_1accumulators_1_1depends__on.html</filename>
    <templarg></templarg>
    <templarg></templarg>
    <templarg></templarg>
    <base>depends_on_base&lt; mpl::transform&lt; mpl::vector&lt; Feature1, Feature2,...&gt;, as_feature&lt; mpl::_1 &gt; &gt;::type &gt;</base>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::extractor</name>
    <filename>structboost_1_1accumulators_1_1extractor.html</filename>
    <templarg></templarg>
    <class kind="struct">boost::accumulators::extractor::result</class>
    <class kind="struct">boost::accumulators::extractor::result&lt; this_type(A1)&gt;</class>
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
    <name>boost::accumulators::extractor::result</name>
    <filename>structboost_1_1accumulators_1_1extractor_1_1result.html</filename>
    <templarg></templarg>
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
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::detail::undroppable</name>
    <filename>structboost_1_1accumulators_1_1detail_1_1undroppable.html</filename>
    <templarg></templarg>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::detail::undroppable&lt; tag::droppable&lt; Feature &gt; &gt;</name>
    <filename>structboost_1_1accumulators_1_1detail_1_1undroppable_3_01tag_1_1droppable_3_01Feature_01_4_01_4.html</filename>
    <templarg></templarg>
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
    <name>boost::accumulators::detail::build_acc_list</name>
    <filename>structboost_1_1accumulators_1_1detail_1_1build__acc__list.html</filename>
    <templarg></templarg>
    <templarg></templarg>
    <templarg>is_empty</templarg>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::detail::build_acc_list&lt; First, Last, true &gt;</name>
    <filename>structboost_1_1accumulators_1_1detail_1_1build__acc__list_3_01First_00_01Last_00_01true_01_4.html</filename>
    <templarg></templarg>
    <templarg></templarg>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::detail::build_acc_list&lt; First, Last, false &gt;</name>
    <filename>structboost_1_1accumulators_1_1detail_1_1build__acc__list_3_01First_00_01Last_00_01false_01_4.html</filename>
    <templarg></templarg>
    <templarg></templarg>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::detail::checked_as_weighted_feature</name>
    <filename>structboost_1_1accumulators_1_1detail_1_1checked__as__weighted__feature.html</filename>
    <templarg></templarg>
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
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::detail::to_accumulator</name>
    <filename>structboost_1_1accumulators_1_1detail_1_1to__accumulator.html</filename>
    <templarg></templarg>
    <templarg></templarg>
    <templarg></templarg>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::detail::to_accumulator&lt; Feature, Sample, tag::external&lt; Weight, Tag, AccumulatorSet &gt; &gt;</name>
    <filename>structboost_1_1accumulators_1_1detail_1_1to__accumulator_3_01Feature_00_01Sample_00_01tag_1_1ext0c26d8c3c18bcca084cb467b003ed836.html</filename>
    <templarg></templarg>
    <templarg></templarg>
    <templarg></templarg>
    <templarg></templarg>
    <templarg></templarg>
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::detail::insert_feature</name>
    <filename>structboost_1_1accumulators_1_1detail_1_1insert__feature.html</filename>
    <templarg></templarg>
    <templarg></templarg>
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
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::detail::accumulator_set_result</name>
    <filename>structboost_1_1accumulators_1_1detail_1_1accumulator__set__result.html</filename>
    <templarg>AccumulatorSet</templarg>
    <templarg>Feature</templarg>
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
  </compound>
  <compound kind="struct">
    <name>boost::accumulators::detail::meta::make_acc_list</name>
    <filename>structboost_1_1accumulators_1_1detail_1_1meta_1_1make__acc__list.html</filename>
    <templarg></templarg>
    <base>build_acc_list&lt; fusion::result_of::begin&lt; Sequence &gt;::type, fusion::result_of::end&lt; Sequence &gt;::type &gt;</base>
  </compound>
  <compound kind="dir">
    <name>/home/travis/build/boostorg/boost/boost/accumulators</name>
    <path>/home/travis/build/boostorg/boost/boost/accumulators/</path>
    <filename>dir_66af1ff6c588fbffd50b9a38d3ada08d.html</filename>
    <dir>/home/travis/build/boostorg/boost/boost/accumulators/framework</dir>
  </compound>
  <compound kind="dir">
    <name>/home/travis/build/boostorg/boost/boost</name>
    <path>/home/travis/build/boostorg/boost/boost/</path>
    <filename>dir_c8984f1860c11f62f47abb6761e46c1e.html</filename>
    <dir>/home/travis/build/boostorg/boost/boost/accumulators</dir>
  </compound>
  <compound kind="dir">
    <name>/home/travis/build/boostorg/boost/boost/accumulators/framework</name>
    <path>/home/travis/build/boostorg/boost/boost/accumulators/framework/</path>
    <filename>dir_9590459749c6d0e896222db774211cbc.html</filename>
    <file>depends_on.hpp</file>
    <file>extractor.hpp</file>
  </compound>
</tagfile>
