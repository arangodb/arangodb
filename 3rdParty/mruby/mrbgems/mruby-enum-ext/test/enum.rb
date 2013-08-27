##
# Enumerable(Ext) Test

assert("Enumerable#drop") do
  a = [1, 2, 3, 4, 5, 0]

  assert_equal [4, 5, 0], a.drop(3)
  assert_equal [], a.drop(6)
end

assert("Enumerable#drop_while") do
  a = [1, 2, 3, 4, 5, 0]
  assert_equal [3, 4, 5, 0], a.drop_while {|i| i < 3 }
end

assert("Enumerable#take") do
  a = [1, 2, 3, 4, 5, 0]
  assert_equal [1, 2, 3], a.take(3)
end

assert("Enumerable#take_while") do
  a = [1, 2, 3, 4, 5, 0]
  assert_equal [1, 2], a.take_while {|i| i < 3}
end

assert("Enumerable#each_cons") do
  a = []
  (1..5).each_cons(3){|e| a << e}
  assert_equal [[1, 2, 3], [2, 3, 4], [3, 4, 5]], a
end

assert("Enumerable#each_slice") do
  a = []
  (1..10).each_slice(3){|e| a << e}
  assert_equal [[1, 2, 3], [4, 5, 6], [7, 8, 9], [10]], a
end

assert("Enumerable#group_by") do
  r = (1..6).group_by {|i| i % 3 }
  assert_equal [3, 6], r[0]
  assert_equal [1, 4], r[1]
  assert_equal [2, 5], r[2]
end
