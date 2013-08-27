class Array
  ##
  # call-seq:
  #    ary.uniq! -> ary or nil
  #
  # Removes duplicate elements from +self+.
  # Returns <code>nil</code> if no changes are made (that is, no
  # duplicates are found).
  #
  #    a = [ "a", "a", "b", "b", "c" ]
  #    a.uniq!   #=> ["a", "b", "c"]
  #    b = [ "a", "b", "c" ]
  #    b.uniq!   #=> nil
  #
  def uniq!
    ary = self.dup
    result = []
    while ary.size > 0
      result << ary.shift
      ary.delete(result.last)
    end
    if result.size == self.size
      nil
    else
      self.replace(result)
    end
  end

  ##
  # call-seq:
  #    ary.uniq   -> new_ary
  #
  # Returns a new array by removing duplicate values in +self+.
  #
  #    a = [ "a", "a", "b", "b", "c" ]
  #    a.uniq   #=> ["a", "b", "c"]
  #
  def uniq
    ary = self.dup
    ary.uniq!
    ary
  end

  ##
  # call-seq:
  #    ary - other_ary    -> new_ary
  #
  # Array Difference---Returns a new array that is a copy of
  # the original array, removing any items that also appear in
  # <i>other_ary</i>. (If you need set-like behavior, see the
  # library class Set.)
  #
  #    [ 1, 1, 2, 2, 3, 3, 4, 5 ] - [ 1, 2, 4 ]  #=>  [ 3, 3, 5 ]
  #
  def -(elem)
    raise TypeError, "can't convert #{elem.class} into Array" unless elem.class == Array

    hash = {}
    array = []
    elem.each { |x| hash[x] = true }
    self.each { |x| array << x unless hash[x] }
    array
  end

  ##
  # call-seq:
  #    ary | other_ary     -> new_ary
  #
  # Set Union---Returns a new array by joining this array with
  # <i>other_ary</i>, removing duplicates.
  #
  #    [ "a", "b", "c" ] | [ "c", "d", "a" ]
  #           #=> [ "a", "b", "c", "d" ]
  #
  def |(elem)
    raise TypeError, "can't convert #{elem.class} into Array" unless elem.class == Array

    ary = self + elem
    ary.uniq! or ary
  end

  ##
  # call-seq:
  #    ary & other_ary      -> new_ary
  #
  # Set Intersection---Returns a new array
  # containing elements common to the two arrays, with no duplicates.
  #
  #    [ 1, 1, 3, 5 ] & [ 1, 2, 3 ]   #=> [ 1, 3 ]
  #
  def &(elem)
    raise TypeError, "can't convert #{elem.class} into Array" unless elem.class == Array

    hash = {}
    array = []
    elem.each{|v| hash[v] = true }
    self.each do |v|
      if hash[v]
        array << v
        hash.delete v
      end
    end
    array
  end

  ##
  # call-seq:
  #    ary.flatten -> new_ary
  #    ary.flatten(level) -> new_ary
  #
  # Returns a new array that is a one-dimensional flattening of this
  # array (recursively). That is, for every element that is an array,
  # extract its elements into the new array.  If the optional
  # <i>level</i> argument determines the level of recursion to flatten.
  #
  #    s = [ 1, 2, 3 ]           #=> [1, 2, 3]
  #    t = [ 4, 5, 6, [7, 8] ]   #=> [4, 5, 6, [7, 8]]
  #    a = [ s, t, 9, 10 ]       #=> [[1, 2, 3], [4, 5, 6, [7, 8]], 9, 10]
  #    a.flatten                 #=> [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
  #    a = [ 1, 2, [3, [4, 5] ] ]
  #    a.flatten(1)              #=> [1, 2, 3, [4, 5]]
  #
  def flatten(depth=nil)
    ar = []
    self.each do |e|
      if e.is_a?(Array) && (depth.nil? || depth > 0)
        ar += e.flatten(depth.nil? ? nil : depth - 1)
      else
        ar << e
      end
    end
    ar
  end

  ##
  # call-seq:
  #    ary.flatten!        -> ary or nil
  #    ary.flatten!(level) -> array or nil
  #
  # Flattens +self+ in place.
  # Returns <code>nil</code> if no modifications were made (i.e.,
  # <i>ary</i> contains no subarrays.)  If the optional <i>level</i>
  # argument determines the level of recursion to flatten.
  #
  #    a = [ 1, 2, [3, [4, 5] ] ]
  #    a.flatten!   #=> [1, 2, 3, 4, 5]
  #    a.flatten!   #=> nil
  #    a            #=> [1, 2, 3, 4, 5]
  #    a = [ 1, 2, [3, [4, 5] ] ]
  #    a.flatten!(1) #=> [1, 2, 3, [4, 5]]
  #
  def flatten!(depth=nil)
    modified = false
    ar = []
    self.each do |e|
      if e.is_a?(Array) && (depth.nil? || depth > 0)
        ar += e.flatten(depth.nil? ? nil : depth - 1)
        modified = true
      else
        ar << e
      end
    end
    if modified
      self.replace(ar)
    else
      nil
    end
  end

  ##
  # call-seq:
  #    ary.compact     -> new_ary
  #
  # Returns a copy of +self+ with all +nil+ elements removed.
  #
  #    [ "a", nil, "b", nil, "c", nil ].compact
  #                      #=> [ "a", "b", "c" ]
  #
  def compact
    result = self.dup
    result.compact!
    result
  end

  ##
  # call-seq:
  #    ary.compact!    -> ary  or  nil
  #
  # Removes +nil+ elements from the array.
  # Returns +nil+ if no changes were made, otherwise returns
  # <i>ary</i>.
  #
  #    [ "a", nil, "b", nil, "c" ].compact! #=> [ "a", "b", "c" ]
  #    [ "a", "b", "c" ].compact!           #=> nil
  #
  def compact!
    result = self.select { |e| e != nil }
    if result.size == self.size
      nil
    else
      self.replace(result)
    end
  end
end
