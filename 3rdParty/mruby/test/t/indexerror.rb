##
# IndexError ISO Test

assert('IndexError', '15.2.33') do
  assert_equal Class, IndexError.class
end

assert('IndexError superclass', '15.2.33.2') do
  assert_equal StandardError, IndexError.superclass
end
