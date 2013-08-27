##
# Bootstrap test for literals

assert('BS Literal 1') do
  assert_true true
end

assert('BS Literal 2') do
  assert_equal TrueClass, true.class
end

assert('BS Literal 3') do
  assert_false false
end

assert('BS Literal 4') do
  assert_equal FalseClass, false.class
end

assert('BS Literal 5') do
  assert_equal 'nil', nil.inspect
end

assert('BS Literal 6') do
  assert_equal NilClass, nil.class
end

assert('BS Literal 7') do
  assert_equal Symbol, :sym.class
end

assert('BS Literal 8') do
  assert_equal 1234, 1234
end

assert('BS Literal 9') do
  assert_equal Fixnum, 1234.class
end
