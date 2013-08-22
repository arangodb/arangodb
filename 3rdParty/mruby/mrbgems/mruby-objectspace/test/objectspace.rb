assert('ObjectSpace.count_objects') do
  h = {}
  ObjectSpace.count_objects(h)
  assert_kind_of(Hash, h)
  assert_true(h.keys.all? {|x| x.is_a?(Symbol) || x.is_a?(Integer) })
  assert_true(h.values.all? {|x| x.is_a?(Integer) })

  assert_true(h.has_key?(:TOTAL))
  assert_true(h.has_key?(:FREE))

  h = ObjectSpace.count_objects
  assert_kind_of(Hash, h)
  assert_true(h.keys.all? {|x| x.is_a?(Symbol) || x.is_a?(Integer) })
  assert_true(h.values.all? {|x| x.is_a?(Integer) })

  assert_raise(TypeError) { ObjectSpace.count_objects(1) }

  h0 = {:MRB_TT_FOO=>1000}
  h = ObjectSpace.count_objects(h0)
  assert_false(h0.has_key?(:MRB_TT_FOO))

  GC.start
  h_after = {}
  h_before = ObjectSpace.count_objects

  objs = []
  1000.times do
    objs << {}
  end
  objs = nil
  ObjectSpace.count_objects(h)
  GC.start
  ObjectSpace.count_objects(h_after)

  assert_equal(h[:MRB_TT_HASH], h_before[:MRB_TT_HASH] + 1000)
  assert_equal(h_after[:MRB_TT_HASH], h_before[:MRB_TT_HASH])
end
