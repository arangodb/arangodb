##
# NilClass ISO Test

assert('NilClass', '15.2.4') do
  assert_equal Class, NilClass.class
end

assert('NilClass#&', '15.2.4.3.1') do
  assert_false nil.&(true)
  assert_false nil.&(nil)
end

assert('NilClass#^', '15.2.4.3.2') do
  assert_true nil.^(true)
  assert_false nil.^(false)
end

assert('NilClass#|', '15.2.4.3.3') do
  assert_true nil.|(true)
  assert_false nil.|(false)
end

assert('NilClass#nil?', '15.2.4.3.4') do
  assert_true nil.nil?
end

assert('NilClass#to_s', '15.2.4.3.5') do
  assert_equal '', nil.to_s
end
