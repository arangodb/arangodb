##
# Kernel ISO Test

assert('Kernel', '15.3.1') do
  assert_equal Module, Kernel.class
end

assert('Kernel.block_given?', '15.3.1.2.2') do
  def bg_try(&b)
    if Kernel.block_given?
      yield
    else
      "no block"
    end
  end

  assert_false Kernel.block_given?
  # test without block
  assert_equal "no block", bg_try
  # test with block
  assert_equal "block" do
    bg_try { "block" }
  end
  # test with block
  assert_equal "block" do
    bg_try do
      "block"
    end
  end
end

# Kernel.eval is provided by the mruby-gem mrbgem. '15.3.1.2.3'

assert('Kernel.global_variables', '15.3.1.2.4') do
  assert_equal Array, Kernel.global_variables.class
end

assert('Kernel.iterator?', '15.3.1.2.5') do
  assert_false Kernel.iterator?
end

assert('Kernel.lambda', '15.3.1.2.6') do
  l = Kernel.lambda do
    true
  end

  m = Kernel.lambda(&l)

  assert_true l.call
  assert_equal Proc, l.class
  assert_true m.call
  assert_equal Proc, m.class
end

# Not implemented at the moment
#assert('Kernel.local_variables', '15.3.1.2.7') do
#  Kernel.local_variables.class == Array
#end

assert('Kernel.loop', '15.3.1.2.8') do
  i = 0

  Kernel.loop do
    i += 1
    break if i == 100
  end

  assert_equal 100, i
end

assert('Kernel.p', '15.3.1.2.9') do
  # TODO search for a way to test p to stdio
  assert_true true
end

assert('Kernel.print', '15.3.1.2.10') do
  # TODO search for a way to test print to stdio
  assert_true true
end

assert('Kernel.puts', '15.3.1.2.11') do
  # TODO search for a way to test puts to stdio
  assert_true true
end

assert('Kernel.raise', '15.3.1.2.12') do
  assert_raise RuntimeError do
    Kernel.raise
  end

  assert_raise RuntimeError do
    Kernel.raise RuntimeError.new
  end
end

assert('Kernel#__id__', '15.3.1.3.3') do
  assert_equal Fixnum, __id__.class
end

assert('Kernel#__send__', '15.3.1.3.4') do
  # test with block
  l = __send__(:lambda) do
    true
  end

  assert_true l.call
  assert_equal Proc, l.class
  # test with argument
  assert_true __send__(:respond_to?, :nil?)
  # test without argument and without block
  assert_equal  Array, __send__(:public_methods).class
end

assert('Kernel#block_given?', '15.3.1.3.6') do
  def bg_try(&b)
    if block_given?
      yield
    else
      "no block"
    end
  end

  assert_false block_given?
  assert_equal "no block", bg_try
  assert_equal "block" do
    bg_try { "block" }
  end
  assert_equal "block" do
    bg_try do
      "block"
    end
  end
end

assert('Kernel#class', '15.3.1.3.7') do
  assert_equal Module, Kernel.class
end

assert('Kernel#clone', '15.3.1.3.8') do
  class KernelCloneTest
    def initialize
      @v = 0
    end

    def get
      @v
    end

    def set(v)
      @v = v
    end
  end

  a = KernelCloneTest.new
  a.set(1)
  b = a.clone

  def a.test
  end
  a.set(2)
  c = a.clone

  immutables = [ 1, :foo, true, false, nil ]
  error_count = 0
  immutables.each do |i|
    begin
      i.clone
    rescue TypeError
      error_count += 1
    end
  end

  assert_equal 2, a.get
  assert_equal 1, b.get
  assert_equal 2, c.get
  assert_true a.respond_to?(:test)
  assert_false b.respond_to?(:test)
  assert_true c.respond_to?(:test)
end

assert('Kernel#dup', '15.3.1.3.9') do
  class KernelDupTest
    def initialize
      @v = 0
    end

    def get
      @v
    end

    def set(v)
      @v = v
    end
  end

  a = KernelDupTest.new
  a.set(1)
  b = a.dup

  def a.test
  end
  a.set(2)
  c = a.dup

  immutables = [ 1, :foo, true, false, nil ]
  error_count = 0
  immutables.each do |i|
    begin
      i.dup
    rescue TypeError
      error_count += 1
    end
  end

  assert_equal immutables.size, error_count
  assert_equal 2, a.get
  assert_equal 1, b.get
  assert_equal 2, c.get
  assert_true a.respond_to?(:test)
  assert_false b.respond_to?(:test)
  assert_false c.respond_to?(:test)
end

# Kernel#eval is provided by mruby-eval mrbgem '15.3.1.3.12'

assert('Kernel#extend', '15.3.1.3.13') do
  class Test4ExtendClass
  end

  module Test4ExtendModule
    def test_method; end
  end

  a = Test4ExtendClass.new
  a.extend(Test4ExtendModule)
  b = Test4ExtendClass.new

  assert_true a.respond_to?(:test_method)
  assert_false b.respond_to?(:test_method)
end

assert('Kernel#extend works on toplevel', '15.3.1.3.13') do
  module Test4ExtendModule
    def test_method; end
  end
  # This would crash...
  extend(Test4ExtendModule)

  assert_true respond_to?(:test_method)
end

assert('Kernel#global_variables', '15.3.1.3.14') do
  assert_equal Array, global_variables.class
end

assert('Kernel#hash', '15.3.1.3.15') do
  assert_equal hash, hash
end

assert('Kernel#inspect', '15.3.1.3.17') do
  s = inspect

  assert_equal String, s.class
  assert_equal "main", s
end

assert('Kernel#instance_variables', '15.3.1.3.23') do
  o = Object.new
  o.instance_eval do
    @a = 11
    @b = 12
  end
  ivars = o.instance_variables

  assert_equal Array, ivars.class,
  assert_equal(2, ivars.size)
  assert_true ivars.include?(:@a)
  assert_true ivars.include?(:@b)
end

assert('Kernel#is_a?', '15.3.1.3.24') do
  assert_true is_a?(Kernel)
  assert_false is_a?(Array)
end

assert('Kernel#iterator?', '15.3.1.3.25') do
  assert_false iterator?
end

assert('Kernel#kind_of?', '15.3.1.3.26') do
  assert_true kind_of?(Kernel)
  assert_false kind_of?(Array)
end

assert('Kernel#lambda', '15.3.1.3.27') do
  l = lambda do
    true
  end

  m = lambda(&l)

  assert_true l.call
  assert_equal Proc, l.class
  assert_true m.call
  assert_equal Proc, m.class
end

# Not implemented yet
#assert('Kernel#local_variables', '15.3.1.3.28') do
#  local_variables.class == Array
#end

assert('Kernel#loop', '15.3.1.3.29') do
  i = 0

  loop do
    i += 1
    break if i == 100
  end

  assert_equal i, 100
end

assert('Kernel#methods', '15.3.1.3.31') do
  assert_equal Array, methods.class
end

assert('Kernel#nil?', '15.3.1.3.32') do
  assert_false nil?
end

assert('Kernel#object_id', '15.3.1.3.33') do
  assert_equal Fixnum, object_id.class
end

# Kernel#p is defined in mruby-print mrbgem. '15.3.1.3.34'

# Kernel#print is defined in mruby-print mrbgem. '15.3.1.3.35'

assert('Kernel#private_methods', '15.3.1.3.36') do
  assert_equal Array, private_methods.class
end

assert('Kernel#protected_methods', '15.3.1.3.37') do
  assert_equal Array, protected_methods.class
end

assert('Kernel#public_methods', '15.3.1.3.38') do
  assert_equal Array, public_methods.class
end

# Kernel#puts is defined in mruby-print mrbgem. '15.3.1.3.39'

assert('Kernel#raise', '15.3.1.3.40') do
  assert_raise RuntimeError do
    raise
  end

  assert_raise RuntimeError do
    raise RuntimeError.new
  end
end

# Kernel#require is defined in mruby-require. '15.3.1.3.42'

assert('Kernel#respond_to?', '15.3.1.3.43') do
  class Test4RespondTo
    def valid_method; end

    def test_method; end
    undef test_method
  end

  assert_raise TypeError do
    Test4RespondTo.new.respond_to?(1)
  end

  assert_true respond_to?(:nil?)
  assert_true Test4RespondTo.new.respond_to?(:valid_method)
  assert_true Test4RespondTo.new.respond_to?('valid_method')
  assert_false Test4RespondTo.new.respond_to?(:test_method)
end

assert('Kernel#send', '15.3.1.3.44') do
  # test with block
  l = send(:lambda) do
    true
  end

  assert_true l.call
  assert_equal l.class, Proc
  # test with argument
  assert_true send(:respond_to?, :nil?)
  # test without argument and without block
  assert_equal send(:public_methods).class, Array
end

assert('Kernel#singleton_methods', '15.3.1.3.45') do
  assert_equal singleton_methods.class, Array
end

assert('Kernel#to_s', '15.3.1.3.46') do
  assert_equal to_s.class, String
end

assert('Kernel#!=') do
  str1 = "hello"
  str2 = str1
  str3 = "world"

  assert_false (str1[1] != 'e')
  assert_true (str1 != str3)
  assert_false (str2 != str1)
end

assert('Kernel#respond_to_missing?') do
  class Test4RespondToMissing
    def respond_to_missing?(method_name, include_private = false)
      method_name == :a_method
    end
  end

  assert_true Test4RespondToMissing.new.respond_to?(:a_method)
  assert_false Test4RespondToMissing.new.respond_to?(:no_method)
end
