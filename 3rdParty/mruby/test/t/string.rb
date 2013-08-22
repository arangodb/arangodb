##
# String ISO Test

assert('String', '15.2.10') do
  assert_equal Class, String.class
end

assert('String superclass', '15.2.10.2') do
  assert_equal Object, String.superclass
end

assert('String#<=>', '15.2.10.5.1') do
  a = '' <=> ''
  b = '' <=> 'not empty'
  c = 'not empty' <=> ''
  d = 'abc' <=> 'cba'
  e = 'cba' <=> 'abc'

  assert_equal  0, a
  assert_equal(-1, b)
  assert_equal  1, c
  assert_equal(-1, d)
  assert_equal  1, e
end

assert('String#==', '15.2.10.5.2') do
  assert_equal 'abc', 'abc'
  assert_not_equal 'abc', 'cba'
end

assert('String#+', '15.2.10.5.4') do
  assert_equal 'ab', 'a' + 'b'
end

assert('String#*', '15.2.10.5.5') do
  assert_equal 'aaaaa', 'a' * 5
end

# 'String#=~', '15.2.10.5.5' will be tested in mrbgems.

assert('String#[]', '15.2.10.5.6') do
  # length of args is 1
  a = 'abc'[0]
  b = 'abc'[-1]
  c = 'abc'[10]
  d = 'abc'[-10]

  # length of args is 2
  a1 = 'abc'[0, -1]
  b1 = 'abc'[10, 0]
  c1 = 'abc'[-10, 0]
  d1 = 'abc'[0, 0]
  e1 = 'abc'[1, 2]

  # args is RegExp
  # It will be tested in mrbgems.

  # args is String
  a3 = 'abc'['bc']
  b3 = 'abc'['XX']

  assert_equal 'a', a
  assert_equal 'c', b
  assert_nil c
  assert_nil d
  assert_nil a1
  assert_nil b1
  assert_nil c1
  assert_equal '', d1
  assert_equal 'bc', e1
  assert_equal 'bc', a3
  assert_nil b3
end

assert('String#[] with Range') do
  a1 = 'abc'[1..0]
  b1 = 'abc'[1..1]
  c1 = 'abc'[1..2]
  d1 = 'abc'[1..3]
  e1 = 'abc'[1..4]
  f1 = 'abc'[0..-2]
  g1 = 'abc'[-2..3]
  h1 = 'abc'[3..4]
  i1 = 'abc'[4..5]
  a2 = 'abc'[1...0]
  b2 = 'abc'[1...1]
  c2 = 'abc'[1...2]
  d2 = 'abc'[1...3]
  e2 = 'abc'[1...4]
  f2 = 'abc'[0...-2]
  g2 = 'abc'[-2...3]
  h2 = 'abc'[3...4]
  i2 = 'abc'[4...5]

  assert_equal '', a1
  assert_equal 'b', b1
  assert_equal 'bc', c1
  assert_equal 'bc', d1
  assert_equal 'bc', e1
  assert_equal 'ab', f1
  assert_equal 'bc', g1
  assert_equal '', h1
  assert_nil i2
  assert_equal '', a2
  assert_equal '', b2
  assert_equal 'b', c2
  assert_equal 'bc', d2
  assert_equal 'bc', e2
  assert_equal 'a', f2
  assert_equal 'bc', g2
  assert_equal '', h2
  assert_nil i2
end

assert('String#capitalize', '15.2.10.5.7') do
  a = 'abc'
  a.capitalize

  assert_equal 'abc', a
  assert_equal 'Abc', 'abc'.capitalize
end

assert('String#capitalize!', '15.2.10.5.8') do
  a = 'abc'
  a.capitalize!

  assert_equal 'Abc', a
end

assert('String#chomp', '15.2.10.5.9') do
  a = 'abc'.chomp
  b = ''.chomp
  c = "abc\n".chomp
  d = "abc\n\n".chomp
  e = "abc\t".chomp("\t")
  f = "abc\n"

  f.chomp

  assert_equal 'abc', a
  assert_equal '', b
  assert_equal 'abc', c
  assert_equal "abc\n", d
  assert_equal 'abc', e
  assert_equal "abc\n", f
end

assert('String#chomp!', '15.2.10.5.10') do
  a = 'abc'
  b = ''
  c = "abc\n"
  d = "abc\n\n"
  e = "abc\t"

  a.chomp!
  b.chomp!
  c.chomp!
  d.chomp!
  e.chomp!("\t")

  assert_equal 'abc', a
  assert_equal '', b
  assert_equal 'abc', c
  assert_equal "abc\n", d
  assert_equal 'abc', e
end

assert('String#chop', '15.2.10.5.11') do
  a = ''.chop
  b = 'abc'.chop
  c = 'abc'

  c.chop

  assert_equal '', a
  assert_equal 'ab', b
  assert_equal 'abc', c
end

assert('String#chop!', '15.2.10.5.12') do
  a = ''
  b = 'abc'

  a.chop!
  b.chop!

  assert_equal a, ''
  assert_equal b, 'ab'
end

assert('String#downcase', '15.2.10.5.13') do
  a = 'ABC'.downcase
  b = 'ABC'

  b.downcase

  assert_equal 'abc', a
  assert_equal 'ABC', b
end

assert('String#downcase!', '15.2.10.5.14') do
  a = 'ABC'

  a.downcase!

  assert_equal 'abc', a
end

assert('String#each_line', '15.2.10.5.15') do
  a = "first line\nsecond line\nthird line"
  list = ["first line\n", "second line\n", "third line"]
  n_list = []

  a.each_line do |line|
    n_list << line
  end

  assert_equal list, n_list
end

assert('String#empty?', '15.2.10.5.16') do
  a = ''
  b = 'not empty'

  assert_true a.empty?
  assert_false b.empty?
end

assert('String#eql?', '15.2.10.5.17') do
  assert_true 'abc'.eql?('abc')
  assert_false 'abc'.eql?('cba')
end

assert('String#gsub', '15.2.10.5.18') do
  assert_equal('aBcaBc', 'abcabc'.gsub('b', 'B'), 'gsub without block')
  assert_equal('aBcaBc', 'abcabc'.gsub('b'){|w| w.capitalize }, 'gsub with block')
  assert_equal('$a$a$',  '#a#a#'.gsub('#', '$'), 'mruby/mruby#847')
  assert_equal('$a$a$',  '#a#a#'.gsub('#'){|w| '$' }, 'mruby/mruby#847 with block')
  assert_equal('$$a$$',  '##a##'.gsub('##', '$$'), 'mruby/mruby#847 another case')
  assert_equal('$$a$$',  '##a##'.gsub('##'){|w| '$$' }, 'mruby/mruby#847 another case with block')
  assert_equal('A',      'a'.gsub('a', 'A'))
  assert_equal('A',      'a'.gsub('a'){|w| w.capitalize })
end

assert('String#gsub!', '15.2.10.5.19') do
  a = 'abcabc'
  a.gsub!('b', 'B')

  b = 'abcabc'
  b.gsub!('b') { |w| w.capitalize }

  assert_equal 'aBcaBc', a
  assert_equal 'aBcaBc', b
end

assert('String#hash', '15.2.10.5.20') do
  a = 'abc'

  assert_equal 'abc'.hash, a.hash
end

assert('String#include?', '15.2.10.5.21') do
  assert_true 'abc'.include?(97)
  assert_false 'abc'.include?(100)
  assert_true 'abc'.include?('a')
  assert_false 'abc'.include?('d')
end

assert('String#index', '15.2.10.5.22') do
  assert_equal 0, 'abc'.index('a')
  assert_nil 'abc'.index('d')
  assert_equal 3, 'abcabc'.index('a', 1)
end

assert('String#initialize', '15.2.10.5.23') do
  a = ''
  a.initialize('abc')

  assert_equal 'abc', a
end

assert('String#initialize_copy', '15.2.10.5.24') do
  a = ''
  a.initialize_copy('abc')

  assert_equal 'abc', a
end

assert('String#intern', '15.2.10.5.25') do
  assert_equal :abc, 'abc'.intern
end

assert('String#length', '15.2.10.5.26') do
  assert_equal 3, 'abc'.length
end

# 'String#match', '15.2.10.5.27' will be tested in mrbgems.

assert('String#replace', '15.2.10.5.28') do
  a = ''
  a.replace('abc')

  assert_equal 'abc', a
end

assert('String#reverse', '15.2.10.5.29') do
  a = 'abc'
  a.reverse

  assert_equal 'abc', a
  assert_equal 'cba', 'abc'.reverse
end

assert('String#reverse!', '15.2.10.5.30') do
  a = 'abc'
  a.reverse!

  assert_equal 'cba', a
  assert_equal 'cba', 'abc'.reverse!
end

assert('String#rindex', '15.2.10.5.31') do
  assert_equal 0, 'abc'.rindex('a')
  assert_nil 'abc'.rindex('d')
  assert_equal 0, 'abcabc'.rindex('a', 1)
  assert_equal 3, 'abcabc'.rindex('a', 4)
end

# 'String#scan', '15.2.10.5.32' will be tested in mrbgems.

assert('String#size', '15.2.10.5.33') do
  assert_equal 3, 'abc'.size
end

assert('String#slice', '15.2.10.5.34') do
  # length of args is 1
  a = 'abc'.slice(0)
  b = 'abc'.slice(-1)
  c = 'abc'.slice(10)
  d = 'abc'.slice(-10)

  # length of args is 2
  a1 = 'abc'.slice(0, -1)
  b1 = 'abc'.slice(10, 0)
  c1 = 'abc'.slice(-10, 0)
  d1 = 'abc'.slice(0, 0)
  e1 = 'abc'.slice(1, 2)

  # slice of shared string
  e11 = e1.slice(0)

  # args is RegExp
  # It will be tested in mrbgems.

  # args is String
  a3 = 'abc'.slice('bc')
  b3 = 'abc'.slice('XX')

  assert_equal 'a', a
  assert_equal 'c', b
  assert_nil c
  assert_nil d
  assert_nil a1
  assert_nil b1
  assert_nil c1
  assert_equal '', d1
  assert_equal 'bc', e1
  assert_equal 'b', e11
  assert_equal 'bc', a3
  assert_nil b3
end

# TODO Broken ATM
assert('String#split', '15.2.10.5.35') do
  # without RegExp behavior is actually unspecified
  assert_equal ['abc', 'abc', 'abc'], 'abc abc abc'.split
  assert_equal ["a", "b", "c", "", "d"], 'a,b,c,,d'.split(',')
  assert_equal ['abc', 'abc', 'abc'], 'abc abc abc'.split(nil)
  assert_equal ['a', 'b', 'c'], 'abc'.split("")
end

assert('String#sub', '15.2.10.5.36') do
  assert_equal 'aBcabc', 'abcabc'.sub('b', 'B')
  assert_equal 'aBcabc', 'abcabc'.sub('b') { |w| w.capitalize }
  assert_equal 'aa$', 'aa#'.sub('#', '$')
end

assert('String#sub!', '15.2.10.5.37') do
  a = 'abcabc'
  a.sub!('b', 'B')

  b = 'abcabc'
  b.sub!('b') { |w| w.capitalize }

  assert_equal 'aBcabc', a
  assert_equal 'aBcabc', b
end

assert('String#to_f', '15.2.10.5.38') do
  a = ''.to_f
  b = '123456789'.to_f
  c = '12345.6789'.to_f

  assert_float(0.0, a)
  assert_float(123456789.0, b)
  assert_float(12345.6789, c)
end

assert('String#to_i', '15.2.10.5.39') do
  a = ''.to_i
  b = '32143'.to_i
  c = 'a'.to_i(16)
  d = '100'.to_i(2)

  assert_equal 0, a
  assert_equal 32143, b
  assert_equal 10, c
  assert_equal 4, d
end

assert('String#to_s', '15.2.10.5.40') do
  assert_equal 'abc', 'abc'.to_s
end

assert('String#to_sym', '15.2.10.5.41') do
  assert_equal :abc, 'abc'.to_sym
end

assert('String#upcase', '15.2.10.5.42') do
  a = 'abc'.upcase
  b = 'abc'

  b.upcase

  assert_equal 'ABC', a
  assert_equal 'abc', b
end

assert('String#upcase!', '15.2.10.5.43') do
  a = 'abc'

  a.upcase!

  assert_equal 'ABC', a
end

# Not ISO specified

assert('String interpolation (mrb_str_concat for shared strings)') do
  a = "A" * 32
  assert_equal "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA:", "#{a}:"
end

assert('Check the usage of a NUL character') do
  "qqq\0ppp"
end

assert('String#bytes') do
  str1 = "hello"
  bytes1 = [104, 101, 108, 108, 111]

  str2 = "\xFF"
  bytes2 = [0xFF]

  assert_equal bytes1, str1.bytes
  assert_equal bytes2, str2.bytes
end

assert('String#each_byte') do
  str1 = "hello"
  bytes1 = [104, 101, 108, 108, 111]
  bytes2 = []

  str1.each_byte {|b| bytes2 << b }

  assert_equal bytes1, bytes2
end

assert('String#inspect') do
  ("\1" * 100).inspect  # should not raise an exception - regress #1210
  assert_equal "\"\\000\"", "\0".inspect
end
