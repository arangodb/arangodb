##
# Chapter 13.3 "Methods" ISO Test

assert('The alias statement', '13.3.6 a) 4)') do
  # check aliasing in all possible ways

  def alias_test_method_original; true; end

  alias alias_test_method_a alias_test_method_original
  alias :alias_test_method_b :alias_test_method_original

  assert_true(alias_test_method_original)
  assert_true(alias_test_method_a)
  assert_true(alias_test_method_b)
end

assert('The alias statement (overwrite original)', '13.3.6 a) 4)') do
  # check that an aliased method can be overwritten
  # without side effect

  def alias_test_method_original; true; end

  alias alias_test_method_a alias_test_method_original
  alias :alias_test_method_b :alias_test_method_original

  assert_true(alias_test_method_original)

  def alias_test_method_original; false; end

  assert_false(alias_test_method_original)
  assert_true(alias_test_method_a)
  assert_true(alias_test_method_b)
end

assert('The alias statement', '13.3.6 a) 5)') do
  # check that alias is raising NameError if
  # non-existing method should be undefined

  assert_raise(NameError) do
    alias new_name_a non_existing_method
  end

  assert_raise(NameError) do
    alias :new_name_b :non_existing_method
  end
end

assert('The undef statement', '13.3.7 a) 4)') do
  # check that undef is undefining method
  # based on the method name

  def existing_method_a; true; end
  def existing_method_b; true; end
  def existing_method_c; true; end
  def existing_method_d; true; end
  def existing_method_e; true; end
  def existing_method_f; true; end

  # check that methods are defined

  assert_true(existing_method_a, 'Method should be defined')
  assert_true(existing_method_b, 'Method should be defined')
  assert_true(existing_method_c, 'Method should be defined')
  assert_true(existing_method_d, 'Method should be defined')
  assert_true(existing_method_e, 'Method should be defined')
  assert_true(existing_method_f, 'Method should be defined')

  # undefine in all possible ways and check that method 
  # is undefined

  undef existing_method_a
  assert_raise(NoMethodError) do
    existing_method_a
  end

  undef :existing_method_b
  assert_raise(NoMethodError) do
    existing_method_b
  end

  undef existing_method_c, existing_method_d
  assert_raise(NoMethodError) do
    existing_method_c
  end
  assert_raise(NoMethodError) do
    existing_method_d
  end

  undef :existing_method_e, :existing_method_f
  assert_raise(NoMethodError) do
    existing_method_e
  end
  assert_raise(NoMethodError) do
    existing_method_f
  end
end

assert('The undef statement (method undefined)', '13.3.7 a) 5)') do
  # check that undef is raising NameError if
  # non-existing method should be undefined

  assert_raise(NameError) do
    undef non_existing_method
  end

  assert_raise(NameError) do
    undef :non_existing_method
  end
end
