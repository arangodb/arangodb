##
# RuntimeError ISO Test

assert('RuntimeError', '15.2.28') do
  assert_equal Class, RuntimeError.class
end
