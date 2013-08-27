##
# Proc(Ext) Test

assert('Proc#lambda?') do
  assert_true lambda{}.lambda?
  assert_true !Proc.new{}.lambda?
end

assert('Proc#===') do
  proc = Proc.new {|a| a * 2}
  assert_equal 20, (proc === 10)
end

assert('Proc#yield') do
  proc = Proc.new {|a| a * 2}
  assert_equal 20, proc.yield(10)
end

assert('Proc#curry') do
  b = proc {|x, y, z| (x||0) + (y||0) + (z||0) }
  assert_equal 6, b.curry[1][2][3]
  assert_equal 6, b.curry[1, 2][3, 4]
  assert_equal 6, b.curry(5)[1][2][3][4][5]
  assert_equal 6, b.curry(5)[1, 2][3, 4][5]
  assert_equal 1, b.curry(1)[1]

  b = lambda {|x, y, z| (x||0) + (y||0) + (z||0) }
  assert_equal 6, b.curry[1][2][3]
  assert_raise(ArgumentError) { b.curry[1, 2][3, 4] }
  assert_raise(ArgumentError) { b.curry(5) }
  assert_raise(ArgumentError) { b.curry(1) }
end

assert('Proc#to_proc') do
  proc = Proc.new {}
  assert_equal proc, proc.to_proc
end

assert('Kernel#proc') do
  assert_true !proc{|a|}.lambda?
end
