##
# Range(Ext) Test

assert('Range#cover?') do
  assert_true ("a".."z").cover?("c")
  assert_true !("a".."z").cover?("5")
  assert_true ("a".."z").cover?("cc")
end

assert('Range#first') do
  assert_equal 10, (10..20).first
  assert_equal [10, 11, 12], (10..20).first(3)
end

assert('Range#last') do
  assert_equal 20, (10..20).last
  assert_equal 20, (10...20).last
  assert_equal [18, 19, 20], (10..20).last(3)
  assert_equal [17, 18, 19], (10...20).last(3)
end
