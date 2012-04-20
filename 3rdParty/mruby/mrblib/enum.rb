#
#  Enumerable
#
module Enumerable
  # 15.3.2.2.1
  def all?(&block)
    st = true
    if block
      self.each{|val|
        unless block.call(val)
          st = false
          break
        end
      }
    else
      self.each{|val|
        unless val
          st = false
          break
        end
      }
    end
    st
  end

  # 15.3.2.2.2
  def any?(&block)
    st = false
    if block
      self.each{|val|
        if block.call(val)
          st = true
          break
        end
      }
    else
      self.each{|val|
        if val
          st = true
          break
        end
      }
    end
    st
  end

  # 15.3.2.2.3
  def collect(&block)
    ary = []
    self.each{|val|
      ary.push(block.call(val))
    }
    ary
  end

  # 15.3.2.2.4
  def detect(ifnone=nil, &block)
    ret = ifnone
    self.each{|val|
      if block.call(val)
        ret = val
        break
      end
    }
    ret
  end

  # 15.3.2.2.5
  def each_with_index(&block)
    i = 0
    self.each{|val|
      block.call(val, i)
      i += 1
    }
    self
  end

  # 15.3.2.2.6
  def entries
    ary = []
    self.each{|val|
      ary.push val
    }
    ary
  end

  # 15.3.2.2.7
  # find(ifnone=nil, &block)
  alias find detect

  # 15.3.2.2.8
  def find_all(&block)
    ary = []
    self.each{|val|
      ary.push(val) if block.call(val)
    }
    ary
  end

  # 15.3.2.2.9
  def grep(pattern, &block)
    ary = []
    self.each{|val|
      if pattern === val
        ary.push((block)? block.call(val): val)
      end
    }
    ary
  end

  # 15.3.2.2.10
  def include?(obj)
    st = false
    self.each{|val|
      if val == obj
        st = true
        break
      end
    }
    st
  end

  # 15.3.2.2.11
  def inject(*args, &block)
    raise ArgumentError, "too many arguments" if args.size > 2
    flag = true  # 1st element?
    result = nil
    self.each{|val|
      if flag
        # 1st element
        result = (args.empty?)? val: block.call(args[0], val)
        flag = false
      else
        result = block.call(result, val)
      end
    }
    result
  end

  # 15.3.2.2.12
  # map(&block)
  alias map collect

  # 15.3.2.2.13
  def max(&block)
    flag = true  # 1st element?
    result = nil
    self.each{|val|
      if flag
        # 1st element
        result = val
        flag = false
      else
        if block
          result = val if block.call(val, result) > 0
        else
          result = val if (val <=> result) > 0
        end
      end
    }
    result
  end

  # 15.3.2.2.14
  def min(&block)
    flag = true  # 1st element?
    result = nil
    self.each{|val|
      if flag
        # 1st element
        result = val
        flag = false
      else
        if block
          result = val if block.call(val, result) < 0
        else
          result = val if (val <=> result) < 0
        end
      end
    }
    result
  end

  # 15.3.2.2.15
  # member?(obj)
  alias member? include?

  # 15.3.2.2.16
  def partition(&block)
    ary_T = []
    ary_F = []
    self.each{|val|
      if block.call(val)
        ary_T.push(val)
      else
        ary_F.push(val)
      end
    }
    [ary_T, ary_F]
  end

  # 15.3.2.2.17
  def reject(&block)
    ary = []
    self.each{|val|
      ary.push(val) unless block.call(val)
    }
    ary
  end

  # 15.3.2.2.18
  # select(&block)
  alias select find_all


  # Does this OK? Please test it.
  def __sort_sub__(sorted, work, src_ary, head, tail, &block)
    if head == tail
      sorted[head] = work[head] if src_ary == 1
      return
    end

    # on current step, which is a src ary?
    if src_ary == 0
      src, dst = sorted, work
    else
      src, dst = work, sorted
    end

    key = src[head]    # key value for dividing values
    i, j = head, tail  # position to store on the dst ary

    (head + 1).upto(tail){|idx|
      if ((block)? block.call(src[idx], key): (src[idx] <=> key)) > 0
        # larger than key
        dst[j] = src[idx]
        j -= 1
      else
        dst[i] = src[idx]
        i += 1
      end
    }

    sorted[i] = key

    # sort each sub-array
    src_ary = (src_ary + 1) % 2  # exchange a src ary
    __sort_sub__(sorted, work, src_ary, head, i - 1, &block) if i > head
    __sort_sub__(sorted, work, src_ary, i + 1, tail, &block) if i < tail
  end
#  private :__sort_sub__

  # 15.3.2.2.19
  def sort(&block)
    ary = []
    self.each{|val| ary.push(val)}
    unless ary.empty?
      __sort_sub__(ary, ::Array.new(ary.size), 0, 0, ary.size - 1, &block)
    end
    ary
  end

  # 15.3.2.2.20
  # to_a
  alias to_a entries
end
