##
# Integer ISO Test

assert('Integer', '15.2.8') do
  assert_equal Class, Integer.class
end

assert('Integer superclass', '15.2.8.2') do
  assert_equal Numeric, Integer.superclass
end

assert('Integer#+', '15.2.8.3.1') do
  a = 1+1
  b = 1+1.0

  assert_equal 2, a
  assert_equal 2.0, b
end

assert('Integer#-', '15.2.8.3.2') do
  a = 2-1
  b = 2-1.0

  assert_equal 1, a
  assert_equal 1.0, b
end

assert('Integer#*', '15.2.8.3.3') do
  a = 1*1
  b = 1*1.0

  assert_equal 1, a
  assert_equal 1.0, b
end

assert('Integer#/', '15.2.8.3.4') do
  a = 2/1
  b = 2/1.0

  assert_equal 2, a
  assert_equal 2.0, b
end

assert('Integer#%', '15.2.8.3.5') do
  a = 1%1
  b = 1%1.0
  c = 2%4

  assert_equal 0, a
  assert_equal 0.0, b
  assert_equal 2, c
end

assert('Integer#<=>', '15.2.8.3.6') do
  a = 1<=>0
  b = 1<=>1
  c = 1<=>2

  assert_equal  1, a
  assert_equal  0, b
  assert_equal(-1, c)
end

assert('Integer#==', '15.2.8.3.7') do
  a = 1==0
  b = 1==1

  assert_false a
  assert_true b
end

assert('Integer#~', '15.2.8.3.8') do
  # Complement
  assert_equal(-1, ~0)
  assert_equal(-3, ~2)
end

assert('Integer#&', '15.2.8.3.9') do
  # Bitwise AND
  #   0101 (5)
  # & 0011 (3)
  # = 0001 (1)
  assert_equal 1, 5 & 3
end

assert('Integer#|', '15.2.8.3.10') do
  # Bitwise OR
  #   0101 (5)
  # | 0011 (3)
  # = 0111 (7)
  assert_equal 7, 5 | 3
end

assert('Integer#^', '15.2.8.3.11') do
  # Bitwise XOR
  #   0101 (5)
  # ^ 0011 (3)
  # = 0110 (6)
  assert_equal 6, 5 ^ 3
end

assert('Integer#<<', '15.2.8.3.12') do
  # Left Shift by one
  #   00010111 (23)
  # = 00101110 (46)
  assert_equal 46, 23 << 1

  # Left Shift by a negative is Right Shift
  assert_equal 23, 46 << -1

  # Raise when shift is too large
  assert_raise(RangeError) do
    2 << 128
  end
end

assert('Integer#>>', '15.2.8.3.13') do
  # Right Shift by one
  #   00101110 (46)
  # = 00010111 (23)
  assert_equal 23, 46 >> 1

  # Right Shift by a negative is Left Shift
  assert_equal 46, 23 >> -1

  # Don't raise on large Right Shift
  assert_equal 0, 23 >> 128

  # Raise when shift is too large
  assert_raise(RangeError) do
    2 >> -128
  end
end

assert('Integer#ceil', '15.2.8.3.14') do
  assert_equal 10, 10.ceil
end

assert('Integer#downto', '15.2.8.3.15') do
  a = 0
  3.downto(1) do |i|
    a += i
  end
  assert_equal 6, a
end

assert('Integer#eql?', '15.2.8.3.16') do
  a = 1.eql?(1)
  b = 1.eql?(2)
  c = 1.eql?(nil)

  assert_true a
  assert_false b
  assert_false c
end

assert('Integer#floor', '15.2.8.3.17') do
  a = 1.floor

  assert_equal 1, a
end

assert('Integer#next', '15.2.8.3.19') do
  assert_equal 2, 1.next
end

assert('Integer#round', '15.2.8.3.20') do
  assert_equal 1, 1.round
end

assert('Integer#succ', '15.2.8.3.21') do
  assert_equal 2, 1.succ
end

assert('Integer#times', '15.2.8.3.22') do
  a = 0
  3.times do
    a += 1
  end
  assert_equal 3, a
end

assert('Integer#to_f', '15.2.8.3.23') do
  assert_equal 1.0, 1.to_f
end

assert('Integer#to_i', '15.2.8.3.24') do
  assert_equal 1, 1.to_i
end

assert('Integer#to_s', '15.2.8.3.25') do
  assert_equal '1', 1.to_s
  assert_equal("-1", -1.to_s)
end

assert('Integer#truncate', '15.2.8.3.26') do
  assert_equal 1, 1.truncate
end

assert('Integer#upto', '15.2.8.3.27') do
  a = 0
  1.upto(3) do |i|
    a += i
  end
  assert_equal 6, a
end

# Not ISO specified

assert('Integer#step') do
  a = []
  b = []
  1.step(3) do |i|
    a << i
  end
  1.step(6, 2) do |i|
    b << i
  end

  assert_equal [1, 2, 3], a
  assert_equal [1, 3, 5], b
end
