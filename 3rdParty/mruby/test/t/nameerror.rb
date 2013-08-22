##
# NameError ISO Test

assert('NameError', '15.2.31') do
  assert_equal Class, NameError.class
end

assert('NameError superclass', '15.2.31.2') do
  assert_equal StandardError, NameError.superclass
end

assert('NameError#name', '15.2.31.2.1') do

  # This check is not duplicate with 15.2.31.2.2 check.
  # Because the NameError in this test is generated in
  # C API.
  class TestDummy
    alias foo bar
  rescue NameError => e
    $test_dummy_result = e.name
  end

  assert_equal $test_dummy_result, :bar
end

assert('NameError#initialize', '15.2.31.2.2') do
   e = NameError.new('a', :foo)

   assert_equal NameError, e.class
   assert_equal 'a', e.message
   assert_equal :foo, e.name
end
