
assert('Comparable#<', '15.3.3.2.1') do
  class Foo
    include Comparable
    def <=>(x)
      0
    end
  end

  assert_false(Foo.new < Foo.new)
end

assert('Comparable#<=', '15.3.3.2.2') do
  class Foo
    include Comparable
    def <=>(x)
      0
    end
  end

  assert_true(Foo.new <= Foo.new)
end

assert('Comparable#==', '15.3.3.2.3') do
  class Foo
    include Comparable
    def <=>(x)
      0
    end
  end

  assert_true(Foo.new == Foo.new)
end

assert('Comparable#>', '15.3.3.2.4') do
  class Foo
    include Comparable
    def <=>(x)
      0
    end
  end

  assert_false(Foo.new > Foo.new)
end

assert('Comparable#>=', '15.3.3.2.5') do
  class Foo
    include Comparable
    def <=>(x)
      0
    end
  end

  assert_true(Foo.new >= Foo.new)
end

assert('Comparable#between?', '15.3.3.2.6') do
  class Foo
    include Comparable
    def <=>(x)
      x
    end
  end

  c = Foo.new

  assert_false(c.between?(-1,  1))
  assert_false(c.between?(-1, -1))
  assert_false(c.between?( 1,  1))
  assert_true(c.between?( 1, -1))
  assert_true(c.between?(0, 0))
end
