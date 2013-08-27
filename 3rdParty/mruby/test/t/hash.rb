##
# Hash ISO Test

assert('Hash', '15.2.13') do
  assert_equal Class, Hash.class
end

assert('Hash superclass', '15.2.13.2') do
  assert_equal Object, Hash.superclass
end

assert('Hash#==', '15.2.13.4.1') do
  assert_true({ 'abc' => 'abc' } == { 'abc' => 'abc' })
  assert_false({ 'abc' => 'abc' } ==  { 'cba' => 'cba' })
end

assert('Hash#[]', '15.2.13.4.2') do
  a = { 'abc' => 'abc' }

  assert_equal 'abc', a['abc']
end

assert('Hash#[]=', '15.2.13.4.3') do
  a = Hash.new
  a['abc'] = 'abc'

  assert_equal 'abc', a['abc']
end

assert('Hash#clear', '15.2.13.4.4') do
  a = { 'abc' => 'abc' }
  a.clear

  assert_equal({ }, a)
end

assert('Hash#default', '15.2.13.4.5') do
  a = Hash.new
  b = Hash.new('abc')
  c = Hash.new {|s,k| s[k] = k}

  assert_nil a.default
  assert_equal 'abc', b.default
  assert_nil c.default
  assert_equal 'abc', c.default('abc')
end

assert('Hash#default=', '15.2.13.4.6') do
  a = { 'abc' => 'abc' }
  a.default = 'cba'

  assert_equal 'abc', a['abc']
  assert_equal 'cba', a['notexist']
end

assert('Hash#default_proc', '15.2.13.4.7') do
  a = Hash.new
  b = Hash.new {|s,k| s[k] = k + k}
  c = b[2]
  d = b['cat']

  assert_nil a.default_proc
  assert_equal Proc, b.default_proc.class
  assert_equal 4, c
  assert_equal 'catcat', d
end

assert('Hash#delete', '15.2.13.4.8') do
  a = { 'abc' => 'abc' }
  b = { 'abc' => 'abc' }
  b_tmp_1 = false
  b_tmp_2 = false

  a.delete('abc')
  b.delete('abc') do |k|
    b_tmp_1 = true
  end
  b.delete('abc') do |k|
    b_tmp_2 = true
  end

  assert_nil a.delete('cba')
  assert_false a.has_key?('abc')
  assert_false b_tmp_1
  assert_true b_tmp_2
end

assert('Hash#each', '15.2.13.4.9') do
  a = { 'abc_key' => 'abc_value' }
  key = nil
  value = nil

  a.each  do |k,v|
    key = k
    value = v
  end

  assert_equal 'abc_key', key
  assert_equal 'abc_value', value
end

assert('Hash#each_key', '15.2.13.4.10') do
  a = { 'abc_key' => 'abc_value' }
  key = nil

  a.each_key  do |k|
    key = k
  end

  assert_equal 'abc_key', key
end

assert('Hash#each_value', '15.2.13.4.11') do
  a = { 'abc_key' => 'abc_value' }
  value = nil

  a.each_value  do |v|
    value = v
  end

  assert_equal 'abc_value', value
end

assert('Hash#empty?', '15.2.13.4.12') do
  a = { 'abc_key' => 'abc_value' }
  b = Hash.new

  assert_false a.empty?
  assert_true b.empty?
end

assert('Hash#has_key?', '15.2.13.4.13') do
  a = { 'abc_key' => 'abc_value' }
  b = Hash.new

  assert_true a.has_key?('abc_key')
  assert_false b.has_key?('cba')
end

assert('Hash#has_value?', '15.2.13.4.14') do
  a = { 'abc_key' => 'abc_value' }
  b = Hash.new

  assert_true a.has_value?('abc_value')
  assert_false b.has_value?('cba')
end

assert('Hash#include?', '15.2.13.4.15') do
  a = { 'abc_key' => 'abc_value' }
  b = Hash.new

  assert_true a.include?('abc_key')
  assert_false b.include?('cba')
end

assert('Hash#initialize', '15.2.13.4.16') do
  # Testing initialize by new.
  h = Hash.new
  h2 = Hash.new(:not_found)

  assert_true h.is_a? Hash
  assert_equal({ }, h)
  assert_nil h["hello"]
  assert_equal :not_found, h2["hello"]
end

assert('Hash#initialize_copy', '15.2.13.4.17') do
  a = { 'abc_key' => 'abc_value' }
  b = Hash.new.initialize_copy(a)

  assert_equal({ 'abc_key' => 'abc_value' }, b)
end

assert('Hash#key?', '15.2.13.4.18') do
  a = { 'abc_key' => 'abc_value' }
  b = Hash.new

  assert_true a.key?('abc_key')
  assert_false b.key?('cba')
end

assert('Hash#keys', '15.2.13.4.19') do
  a = { 'abc_key' => 'abc_value' }

  assert_equal ['abc_key'], a.keys
end

assert('Hash#length', '15.2.13.4.20') do
  a = { 'abc_key' => 'abc_value' }
  b = Hash.new

  assert_equal 1, a.length
  assert_equal 0, b.length
end

assert('Hash#member?', '15.2.13.4.21') do
  a = { 'abc_key' => 'abc_value' }
  b = Hash.new

  assert_true a.member?('abc_key')
  assert_false b.member?('cba')
end

assert('Hash#merge', '15.2.13.4.22') do
  a = { 'abc_key' => 'abc_value', 'cba_key' => 'cba_value' }
  b = { 'cba_key' => 'XXX',  'xyz_key' => 'xyz_value' }

  result_1 = a.merge b
  result_2 = a.merge(b) do |key, original, new|
    original
  end

  assert_equal({'abc_key' => 'abc_value', 'cba_key' => 'XXX',
                'xyz_key' => 'xyz_value' }, result_1)
  assert_equal({'abc_key' => 'abc_value', 'cba_key' => 'cba_value',
                'xyz_key' => 'xyz_value' }, result_2)
end

assert('Hash#replace', '15.2.13.4.23') do
  a = { 'abc_key' => 'abc_value' }
  b = Hash.new.replace(a)

  assert_equal({ 'abc_key' => 'abc_value' }, b)
end

assert('Hash#shift', '15.2.13.4.24') do
  a = { 'abc_key' => 'abc_value', 'cba_key' => 'cba_value' }
  b = a.shift

  assert_equal({ 'abc_key' => 'abc_value' }, a)
  assert_equal [ 'cba_key', 'cba_value' ], b
end

assert('Hash#size', '15.2.13.4.25') do
  a = { 'abc_key' => 'abc_value' }
  b = Hash.new

  assert_equal 1, a.size
  assert_equal 0, b.size
end

assert('Hash#store', '15.2.13.4.26') do
  a = Hash.new
  a.store('abc', 'abc')

  assert_equal 'abc', a['abc']
end

assert('Hash#value?', '15.2.13.4.27') do
  a = { 'abc_key' => 'abc_value' }
  b = Hash.new

  assert_true a.value?('abc_value')
  assert_false b.value?('cba')
end

assert('Hash#values', '15.2.13.4.28') do
  a = { 'abc_key' => 'abc_value' }

  assert_equal ['abc_value'], a.values
end

# Not ISO specified

assert('Hash#reject') do
  h = {:one => 1, :two => 2, :three => 3, :four => 4}
  ret = h.reject do |k,v|
    v % 2 == 0
  end
  assert_equal({:one => 1, :three => 3}, ret)
  assert_equal({:one => 1, :two => 2, :three => 3, :four => 4}, h)
end

assert('Hash#reject!') do
  h = {:one => 1, :two => 2, :three => 3, :four => 4}
  ret = h.reject! do |k,v|
    v % 2 == 0
  end
  assert_equal({:one => 1, :three => 3}, ret)
  assert_equal({:one => 1, :three => 3}, h)
end

assert('Hash#select') do
  h = {:one => 1, :two => 2, :three => 3, :four => 4}
  ret = h.select do |k,v|
    v % 2 == 0
  end
  assert_equal({:two => 2, :four => 4}, ret)
  assert_equal({:one => 1, :two => 2, :three => 3, :four => 4}, h)
end

assert('Hash#select!') do
  h = {:one => 1, :two => 2, :three => 3, :four => 4}
  ret = h.select! do |k,v|
    v % 2 == 0
  end
  assert_equal({:two => 2, :four => 4}, ret)
  assert_equal({:two => 2, :four => 4}, h)
end

# Not ISO specified

assert('Hash#inspect') do
  h = { "c" => 300, "a" => 100, "d" => 400, "c" => 300  }
  ret = h.to_s

  assert_include ret, '"c"=>300'
  assert_include ret, '"a"=>100'
  assert_include ret, '"d"=>400'
end
