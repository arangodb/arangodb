##
# Bootstrap tests for blocks

assert('BS Block 1') do
  assert_equal(1) do
    1.times{
      begin
        a = 1
      ensure
        foo = nil
      end
    }
  end
end

assert('BS Block 2') do
  assert_equal 2, [1,2,3].find{|x| x == 2}
end

assert('BS Block 3') do
  class E
    include Enumerable
    def each(&block)
      [1, 2, 3].each(&block)
    end
  end
  assert_equal 2, E.new.find {|x| x == 2 }
end

assert('BS Block 3') do
  sum = 0
  for x in [1, 2, 3]
    sum += x
  end
  assert_equal 6, sum
end

assert('BS Block 4') do
  sum = 0
  for x in (1..5)
    sum += x
  end
  assert_equal 15, sum
end

assert('BS Block 5') do
  sum = 0
  for x in []
    sum += x
  end
  assert_equal 0, sum
end

assert('BS Block 6') do
  ans = []
  assert_equal(1) do
    1.times{
      for n in 1..3
        a = n
        ans << a
      end
    }
  end
end

assert('BS Block 7') do
  ans = []
  assert_equal((1..3)) do
    for m in 1..3
      for n in 2..4
        a = [m, n]
        ans << a
      end
    end
  end
end

assert('BS Block 8') do
  assert_equal [1, 2, 3], (1..3).to_a
end

assert('BS Block 9') do
  assert_equal([4, 8, 12]) do
    (1..3).map{|e|
      e * 4
    }
  end
end

assert('BS Block 10') do
  def m
    yield
  end
  def n
    yield
  end

  assert_equal(100) do
    m{
      n{
        100
      }
    }
  end
end

assert('BS Block 11') do
  def m
    yield 1
  end

  assert_equal(20) do
    m{|ib|
      m{|jb|
        i = 20
      }
    }
  end
end

assert('BS Block 12') do
  def m
    yield 1
  end

  assert_equal(2) do
    m{|ib|
      m{|jb|
        ib = 20
        kb = 2
      }
    }
  end
end

assert('BS Block 13') do
  def iter1
    iter2{
      yield
    }
  end

  def iter2
    yield
  end

  assert_equal(3) do
    iter1{
      jb = 2
      iter1{
        jb = 3
      }
      jb
    }
  end
end

assert('BS Block 14') do
  def iter1
    iter2{
      yield
    }
  end

  def iter2
    yield
  end

  assert_equal(2) do
    iter1{
      jb = 2
      iter1{
        jb
      }
      jb
    }
  end
end

assert('BS Block 15') do
  def m
    yield 1
  end
  assert_equal(2) do
    m{|ib|
      ib*2
    }
  end
end

assert('BS Block 16') do
  def m
    yield 12345, 67890
  end
  assert_equal(92580) do
    m{|ib,jb|
      ib*2+jb
    }
  end
end

assert('BS Block 17') do
  def iter
    yield 10
  end

  a = nil
  assert_equal [10, nil] do
    [iter{|a|
      a
    }, a]
  end
end

assert('BS Block 18') do
  def iter
    yield 10
  end

  assert_equal(21) do
    iter{|a|
      iter{|a|
        a + 1
      } + a
    }
  end
end

assert('BS Block 19') do
  def iter
    yield 10, 20, 30, 40
  end

  a = b = c = d = nil
  assert_equal([10, 20, 30, 40, nil, nil, nil, nil]) do
    iter{|a, b, c, d|
      [a, b, c, d]
    } + [a, b, c, d]
  end
end

assert('BS Block 20') do
  def iter
    yield 10, 20, 30, 40
  end

  a = b = nil
  assert_equal([10, 20, 30, 40, nil, nil]) do
    iter{|a, b, c, d|
      [a, b, c, d]
    } + [a, b]
  end
end

assert('BS Block 21') do
  def iter
    yield 1, 2
  end

  assert_equal([1, [2]]) do
    iter{|a, *b|
      [a, b]
    }
  end
end

assert('BS Block 22') do
  def iter
    yield 1, 2
  end

  assert_equal([[1, 2]]) do
    iter{|*a|
      [a]
    }
  end
end

assert('BS Block 23') do
  def iter
    yield 1, 2
  end

  assert_equal([1, 2, []]) do
    iter{|a, b, *c|
      [a, b, c]
    }
  end
end

assert('BS Block 24') do
  def m
    yield
  end
  assert_equal(1) do
    m{
      1
    }
  end
end

assert('BS Block 25') do
  def m
    yield 123
  end
  assert_equal(15129) do
    m{|ib|
      m{|jb|
        ib*jb
      }
    }
  end
end

assert('BS Block 26') do
  def m a
    yield a
  end
  assert_equal(2) do
    m(1){|ib|
      m(2){|jb|
        ib*jb
      }
    }
  end
end

assert('BS Block 27') do
  sum = 0
  3.times{|ib|
    2.times{|jb|
      sum += ib + jb
    }}
  assert_equal sum, 9
end

assert('BS Block 28') do
  assert_equal(10) do
    3.times{|bl|
      break 10
    }
  end
end

assert('BS Block 29') do
  def iter
    yield 1,2,3
  end

  assert_equal([1, 2]) do
    iter{|i, j|
      [i, j]
    }
  end
end

assert('BS Block 30') do
  def iter
    yield 1
  end

  assert_equal([1, nil]) do
    iter{|i, j|
      [i, j]
    }
  end
end

assert('BS Block [ruby-dev:31147]') do
  def m
    yield
  end
  assert_nil m{|&b| b}
end

assert('BS Block [ruby-dev:31160]') do
  def m()
    yield
  end
  assert_nil m {|(v,(*))|}
end

assert('BS Block [issue #750]') do
  def m(a, *b)
    yield
  end
  args = [1, 2, 3]
  assert_equal m(*args){ 1 }, 1
end

assert('BS Block 31') do
  def m()
    yield
  end
  assert_nil m {|((*))|}
end

assert('BS Block [ruby-dev:31440]') do
  def m
    yield [0]
  end
  assert_equal m{|v, &b| v}, [0]
end

assert('BS Block 32') do
  r = false; 1.times{|&b| r = b}
  assert_equal NilClass, r.class
end

assert('BS Block [ruby-core:14395]') do
  class Controller
    def respond_to(&block)
      responder = Responder.new
      block.call(responder)
      responder.respond
    end
    def test_for_bug
      respond_to{|format|
        format.js{
          "in test"
          render{|obj|
            obj
          }
        }
      }
    end
    def render(&block)
      "in render"
    end
  end

  class Responder
    def method_missing(symbol, &block)
      "enter method_missing"
      @response = Proc.new{
        'in method missing'
        block.call
      }
      "leave method_missing"
    end
    def respond
      @response.call
    end
  end
  t = Controller.new
  assert_true t.test_for_bug
end

assert("BS Block 33") do
  module TestReturnFromNestedBlock
    def self.test
      1.times do
        1.times do
          return :ok
        end
      end
      :bad
    end
  end
  assert_equal :ok, TestReturnFromNestedBlock.test
end

assert("BS Block 34") do
  module TestReturnFromNestedBlock_BSBlock34
    def self.test
      1.times do
        while true
          return :ok
        end
      end
      :bad
    end
  end
  assert_equal :ok, TestReturnFromNestedBlock_BSBlock34.test
end

assert("BS Block 35") do
  module TestReturnFromNestedBlock_BSBlock35
    def self.test
      1.times do
        until false
          return :ok
        end
      end
      :bad
    end
  end
  assert_equal :ok, TestReturnFromNestedBlock_BSBlock35.test
end
