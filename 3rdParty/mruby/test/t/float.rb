##
# Float ISO Test

assert('Float', '15.2.9') do
  assert_equal Class, Float.class
end

assert('Float superclass', '15.2.9.2') do
  assert_equal Numeric, Float.superclass
end

assert('Float#+', '15.2.9.3.1') do
  a = 3.123456788 + 0.000000001
  b = 3.123456789 + 1

  assert_float(3.123456789, a)
  assert_float(4.123456789, b)
end

assert('Float#-', '15.2.9.3.2') do
  a = 3.123456790 - 0.000000001
  b = 5.123456789 - 1

  assert_float(3.123456789, a)
  assert_float(4.123456789, b)
end

assert('Float#*', '15.2.9.3.3') do
  a = 3.125 * 3.125
  b = 3.125 * 1

  assert_float(9.765625, a)
  assert_float(3.125   , b)
end

assert('Float#/', '15.2.9.3.4') do
  a = 3.123456789 / 3.123456789
  b = 3.123456789 / 1

  assert_float(1.0        , a)
  assert_float(3.123456789, b)
end

assert('Float#%', '15.2.9.3.5') do
  a = 3.125 % 3.125
  b = 3.125 % 1

  assert_float(0.0  , a)
  assert_float(0.125, b)
end

assert('Float#<=>', '15.2.9.3.6') do
  a = 3.125 <=> 3.123
  b = 3.125 <=> 3.125
  c = 3.125 <=> 3.126
  a2 = 3.125 <=> 3
  c2 = 3.125 <=> 4

  assert_equal( 1, a)
  assert_equal( 0, b)
  assert_equal(-1, c)
  assert_equal( 1, a2)
  assert_equal(-1, c2)
end

assert('Float#==', '15.2.9.3.7') do
  assert_true 3.1 == 3.1
  assert_false 3.1 == 3.2
end

assert('Float#ceil', '15.2.9.3.8') do
  a = 3.123456789.ceil
  b = 3.0.ceil
  c = -3.123456789.ceil
  d = -3.0.ceil

  assert_equal( 4, a)
  assert_equal( 3, b)
  assert_equal(-3, c)
  assert_equal(-3, d)
end

assert('Float#finite?', '15.2.9.3.9') do
  assert_true 3.123456789.finite?
  assert_false (1.0 / 0.0).finite?
end

assert('Float#floor', '15.2.9.3.10') do
  a = 3.123456789.floor
  b = 3.0.floor
  c = -3.123456789.floor
  d = -3.0.floor

  assert_equal( 3, a)
  assert_equal( 3, b)
  assert_equal(-4, c)
  assert_equal(-3, d)
end

assert('Float#infinite?', '15.2.9.3.11') do
  a = 3.123456789.infinite?
  b = (1.0 / 0.0).infinite?
  c = (-1.0 / 0.0).infinite?

  assert_nil a
  assert_equal( 1, b)
  assert_equal(-1, c)
end

assert('Float#round', '15.2.9.3.12') do
  a = 3.123456789.round
  b = 3.5.round
  c = 3.4999.round
  d = (-3.123456789).round
  e = (-3.5).round
  f = 12345.67.round(-1)
  g = 3.423456789.round(0)
  h = 3.423456789.round(1)
  i = 3.423456789.round(3)

  assert_equal(    3, a)
  assert_equal(    4, b)
  assert_equal(    3, c)
  assert_equal(   -3, d)
  assert_equal(   -4, e)
  assert_equal(12350, f)
  assert_equal(    3, g)
  assert_float(  3.4, h)
  assert_float(3.423, i)
end

assert('Float#to_f', '15.2.9.3.13') do
  a = 3.123456789

  assert_float(a, a.to_f)
end

assert('Float#to_i', '15.2.9.3.14') do
  assert_equal(3, 3.123456789.to_i)
end

assert('Float#truncate', '15.2.9.3.15') do
  assert_equal( 3,  3.123456789.truncate)
  assert_equal(-3, -3.1.truncate)
end
