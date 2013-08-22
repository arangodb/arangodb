##
# FalseClass ISO Test

assert('FalseClass', '15.2.6') do
  assert_equal Class, FalseClass.class
end

assert('FalseClass superclass', '15.2.6.2') do
  assert_equal Object, FalseClass.superclass
end

assert('FalseClass false', '15.2.6.1') do
  assert_false false
end

assert('FalseClass#&', '15.2.6.3.1') do
  assert_false false.&(true)
  assert_false false.&(false)
end

assert('FalseClass#^', '15.2.6.3.2') do
  assert_true false.^(true)
  assert_false false.^(false)
end

assert('FalseClass#to_s', '15.2.6.3.3') do
  assert_equal 'false', false.to_s
end

assert('FalseClass#|', '15.2.6.3.4') do
  assert_true false.|(true)
  assert_false false.|(false)
end
