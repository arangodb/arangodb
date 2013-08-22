##
# Enumerable
#
module Enumerable
  ##
  # call-seq:
  #    enum.drop(n)               -> array
  #
  # Drops first n elements from <i>enum</i>, and returns rest elements
  # in an array.
  #
  #    a = [1, 2, 3, 4, 5, 0]
  #    a.drop(3)             #=> [4, 5, 0]

  def drop(n)
    raise TypeError, "expected Integer for 1st argument" unless n.kind_of? Integer
    raise ArgumentError, "attempt to drop negative size" if n < 0

    ary = []
    self.each {|e| n == 0 ? ary << e : n -= 1 }
    ary
  end

  ##
  # call-seq:
  #    enum.drop_while {|arr| block }   -> array
  #
  # Drops elements up to, but not including, the first element for
  # which the block returns +nil+ or +false+ and returns an array
  # containing the remaining elements.
  #
  #    a = [1, 2, 3, 4, 5, 0]
  #    a.drop_while {|i| i < 3 }   #=> [3, 4, 5, 0]

  def drop_while(&block)
    ary, state = [], false
    self.each do |e|
      state = true if !state and !block.call(e)
      ary << e if state
    end
    ary
  end

  ##
  # call-seq:
  #    enum.take(n)               -> array
  #
  # Returns first n elements from <i>enum</i>.
  #
  #    a = [1, 2, 3, 4, 5, 0]
  #    a.take(3)             #=> [1, 2, 3]

  def take(n)
    raise TypeError, "expected Integer for 1st argument" unless n.kind_of? Integer
    raise ArgumentError, "attempt to take negative size" if n < 0

    ary = []
    self.each do |e|
      break if ary.size >= n
      ary << e
    end
    ary
  end

  ##
  # call-seq:
  #    enum.take_while {|arr| block }   -> array
  #
  # Passes elements to the block until the block returns +nil+ or +false+,
  # then stops iterating and returns an array of all prior elements.
  #
  #
  #    a = [1, 2, 3, 4, 5, 0]
  #    a.take_while {|i| i < 3 }   #=> [1, 2]

  def take_while(&block)
    ary = []
    self.each do |e|
      return ary unless block.call(e)
      ary << e
    end
    ary
  end
  
  ##
  # call-seq:
  #   enum.each_cons(n) {...}   ->  nil
  #
  # Iterates the given block for each array of consecutive <n>
  # elements.
  #
  # e.g.:
  #     (1..10).each_cons(3) {|a| p a}
  #     # outputs below
  #     [1, 2, 3]
  #     [2, 3, 4]
  #     [3, 4, 5]
  #     [4, 5, 6]
  #     [5, 6, 7]
  #     [6, 7, 8]
  #     [7, 8, 9]
  #     [8, 9, 10]

  def each_cons(n, &block)
    raise TypeError, "expected Integer for 1st argument" unless n.kind_of? Integer
    raise ArgumentError, "invalid size" if n <= 0

    ary = []
    self.each do |e|
      ary.shift if ary.size == n
      ary << e
      block.call(ary.dup) if ary.size == n
    end
  end

  ##
  # call-seq:
  #   enum.each_slice(n) {...}  ->  nil
  #
  # Iterates the given block for each slice of <n> elements.
  #
  # e.g.:
  #     (1..10).each_slice(3) {|a| p a}
  #     # outputs below
  #     [1, 2, 3]
  #     [4, 5, 6]
  #     [7, 8, 9]
  #     [10]

  def each_slice(n, &block)
    raise TypeError, "expected Integer for 1st argument" unless n.kind_of? Integer
    raise ArgumentError, "invalid slice size" if n <= 0

    ary = []
    self.each do |e|
      ary << e
      if ary.size == n
        block.call(ary)
        ary = []
      end
    end
    block.call(ary) unless ary.empty?
  end

  ##
  # call-seq:
  #    enum.group_by {| obj | block }  -> a_hash
  #
  # Returns a hash, which keys are evaluated result from the
  # block, and values are arrays of elements in <i>enum</i>
  # corresponding to the key.
  #
  #    (1..6).group_by {|i| i%3}   #=> {0=>[3, 6], 1=>[1, 4], 2=>[2, 5]}

  def group_by(&block)
    h = {}
    self.each do |e|
      key = block.call(e)
      h.key?(key) ? (h[key] << e) : (h[key] = [e])
    end
    h
  end

end
