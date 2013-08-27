##
# Literals ISO Test

assert('Literals Numerical', '8.7.6.2') do
  # signed and unsigned integer
  assert_equal 1, 1
  assert_equal(-1, -1)
  assert_equal(+1, +1)
  # signed and unsigned float
  assert_equal 1.0, 1.0
  assert_equal(-1.0, -1.0)
  # binary
  assert_equal 128, 0b10000000
  assert_equal 128, 0B10000000
  # octal
  assert_equal 8, 0o10
  assert_equal 8, 0O10
  assert_equal 8, 0_10
  # hex
  assert_equal 255, 0xff
  assert_equal 255, 0Xff
  # decimal
  assert_equal 999, 0d999
  assert_equal 999, 0D999
  # decimal seperator
  assert_equal 10000000, 10_000_000
  assert_equal       10, 1_0
  # integer with exponent
  assert_equal 10.0, 1e1,
  assert_equal(0.1, 1e-1)
  assert_equal 10.0, 1e+1
  # float with exponent
  assert_equal 10.0, 1.0e1
  assert_equal(0.1, 1.0e-1)
  assert_equal 10.0, 1.0e+1
end

assert('Literals Strings Single Quoted', '8.7.6.3.2') do
  assert_equal 'abc', 'abc'
  assert_equal '\'', '\''
  assert_equal '\\', '\\'
end

assert('Literals Strings Double Quoted', '8.7.6.3.3') do
  a = "abc"

  assert_equal "abc", "abc"
  assert_equal "\"", "\""
  assert_equal "\\", "\\"
  assert_equal "abc", "#{a}"
end

assert('Literals Strings Quoted Non-Expanded', '8.7.6.3.4') do
  a = %q{abc}
  b = %q(abc)
  c = %q[abc]
  d = %q<abc>
  e = %q/abc/
  f = %q/ab\/c/
  g = %q{#{a}}

  assert_equal 'abc', a
  assert_equal 'abc', b
  assert_equal 'abc', c
  assert_equal 'abc', d
  assert_equal 'abc', e
  assert_equal 'ab/c', f
  assert_equal '#{a}', g
end

assert('Literals Strings Quoted Expanded', '8.7.6.3.5') do
  a = %Q{abc}
  b = %Q(abc)
  c = %Q[abc]
  d = %Q<abc>
  e = %Q/abc/
  f = %Q/ab\/c/
  g = %Q{#{a}}

  assert_equal 'abc', a
  assert_equal 'abc', b
  assert_equal 'abc', c
  assert_equal 'abc', d
  assert_equal 'abc', e
  assert_equal 'ab/c', f
  assert_equal 'abc', g
end

assert('Literals Strings Here documents', '8.7.6.3.6') do
  a = <<AAA
aaa
AAA
   b = <<b_b
bbb
b_b
    c = [<<CCC1, <<"CCC2", <<'CCC3']
c1
CCC1
c 2
CCC2
c  3
CCC3

      d = <<DDD
d#{1+2}DDD
d\t
DDD\n
DDD
  e = <<'EEE'
e#{1+2}EEE
e\t
EEE\n
EEE
  f = <<"FFF"
F
FF#{"f"}FFF
F
FFF

  g = <<-GGG
  ggg
  GGG
  h = <<-"HHH"
  hhh
  HHH
  i = <<-'III'
  iii
  III
  j = [<<-JJJ1   , <<-"JJJ2"   , <<-'JJJ3' ]
  j#{1}j
  JJJ1
  j#{2}j
  JJJ2
  j#{3}j
  JJJ3

  k = <<'KKK'.to_i
123
KKK

  z = <<'ZZZ'
ZZZ

  assert_equal "aaa\n", a
  assert_equal "bbb\n", b
  assert_equal ["c1\n", "c 2\n", "c  3\n"], c
  assert_equal "d3DDD\nd\t\nDDD\n\n", d
  assert_equal "e\#{1+2}EEE\ne\\t\nEEE\\n\n", e
  assert_equal "F\nFFfFFF\nF\n", f
  assert_equal "  ggg\n", g
  assert_equal "  hhh\n", h
  assert_equal "  iii\n", i
  assert_equal ["  j1j\n", "  j2j\n", "  j\#{3}j\n"], j
  assert_equal 123, k
  assert_equal "", z
end

assert('Literals Array', '8.7.6.4') do
  a = %W{abc#{1+2}def \}g}
  b = %W(abc #{2+3} def \(g)
  c = %W[#{3+4}]
  d = %W< #{4+5} >
  e = %W//
  f = %W[[ab cd][ef]]
  g = %W{
    ab
    #{-1}1
    2#{2}
  }
  h = %W(a\nb
         test\ abc
         c\
d
         x\y x\\y x\\\y)

  assert_equal ['abc3def', '}g'], a
  assert_equal ['abc', '5', 'def', '(g'], b
  assert_equal ['7'],c
  assert_equal ['9'], d
  assert_equal [], e
  assert_equal ['[ab', 'cd][ef]'], f
  assert_equal ['ab', '-11', '22'], g
  assert_equal ["a\nb", 'test abc', "c\nd", "xy", "x\\y", "x\\y"], h

  a = %w{abc#{1+2}def \}g}
  b = %w(abc #{2+3} def \(g)
  c = %w[#{3+4}]
  d = %w< #{4+5} >
  e = %w//
  f = %w[[ab cd][ef]]
  g = %w{
    ab
    #{-1}1
    2#{2}
  }
  h = %w(a\nb
         test\ abc
         c\
d
         x\y x\\y x\\\y)

  assert_equal ['abc#{1+2}def', '}g'], a
  assert_equal ['abc', '#{2+3}', 'def', '(g'], b
  assert_equal ['#{3+4}'], c
  assert_equal ['#{4+5}'], d
  assert_equal [], e
  assert_equal ['[ab', 'cd][ef]'], f
  assert_equal ['ab', '#{-1}1', '2#{2}'], g
  assert_equal ["a\\nb", "test abc", "c\nd", "x\\y", "x\\y", "x\\\\y"], h
end

assert('Literals Array of symbols') do
  a = %I{abc#{1+2}def \}g}
  b = %I(abc #{2+3} def \(g)
  c = %I[#{3+4}]
  d = %I< #{4+5} >
  e = %I//
  f = %I[[ab cd][ef]]
  g = %I{
    ab
    #{-1}1
    2#{2}
  }

  assert_equal [:'abc3def', :'}g'], a
  assert_equal [:'abc', :'5', :'def', :'(g'], b
  assert_equal [:'7'],c
  assert_equal [:'9'], d
  assert_equal [], e
  assert_equal [:'[ab', :'cd][ef]'], f
  assert_equal [:'ab', :'-11', :'22'], g

  a = %i{abc#{1+2}def \}g}
  b = %i(abc #{2+3} def \(g)
  c = %i[#{3+4}]
  d = %i< #{4+5} >
  e = %i//
  f = %i[[ab cd][ef]]
  g = %i{
    ab
    #{-1}1
    2#{2}
  }

  assert_equal [:'abc#{1+2}def', :'}g'], a
  assert_equal [:'abc', :'#{2+3}', :'def', :'(g'], b
  assert_equal [:'#{3+4}'], c
  assert_equal [:'#{4+5}'], d
  assert_equal [] ,e
  assert_equal [:'[ab', :'cd][ef]'], f
  assert_equal [:'ab', :'#{-1}1', :'2#{2}'], g
end

assert('Literals Symbol', '8.7.6.6') do
  # do not compile error
  :$asd
  :@asd
  :@@asd
  :asd=
  :asd!
  :asd?
  :+
  :+@
  :if
  :BEGIN

  a = :"asd qwe"
  b = :'foo bar'
  c = :"a#{1+2}b"
  d = %s(asd)
  e = %s( foo \))
  f = %s[asd \[
qwe]
  g = %s/foo#{1+2}bar/
  h = %s{{foo bar}}

  assert_equal :'asd qwe', a
  assert_equal :"foo bar", b
  assert_equal :a3b, c
  assert_equal :asd, d
  assert_equal :' foo )', e
  assert_equal :"asd [\nqwe", f
  assert_equal :'foo#{1+2}bar', g
  assert_equal :'{foo bar}', h
end

# Not Implemented ATM assert('Literals Regular expression', '8.7.6.5') do
