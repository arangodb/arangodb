##
# Array(Ext) Test

assert("Array::try_convert") do
  Array.try_convert([1]) == [1] and
  Array.try_convert("1").nil?
end

assert("Array#assoc") do
  s1 = [ "colors", "red", "blue", "green" ]
  s2 = [ "letters", "a", "b", "c" ]
  s3 = "foo"
  a  = [ s1, s2, s3 ]

  a.assoc("letters") == [ "letters", "a", "b", "c" ] and
  a.assoc("foo").nil?
end

assert("Array#at") do
  a = [ "a", "b", "c", "d", "e" ]
  a.at(0)  == "a" and a.at(-1) == "e"
end

assert("Array#rassoc") do
  a = [ [ 1, "one"], [2, "two"], [3, "three"], ["ii", "two"] ]

  a.rassoc("two") == [2, "two"] and
  a.rassoc("four").nil?
end

assert("Array#uniq!") do
  a = [1, 2, 3, 1]
  a.uniq!
  a == [1, 2, 3]
end

assert("Array#uniq") do
  a = [1, 2, 3, 1]
  a.uniq == [1, 2, 3] && a == [1, 2, 3, 1]
end

assert("Array#-") do
  a = [1, 2, 3, 1]
  b = [1]
  c = 1
  e1 = nil

  begin
    a - c
  rescue => e1
  end

  (a - b) == [2, 3] and e1.class == TypeError and a == [1, 2, 3, 1]
end

assert("Array#|") do
  a = [1, 2, 3, 1]
  b = [1, 4]
  c = 1
  e1 = nil

  begin
    a | c
  rescue => e1
  end

  (a | b) == [1, 2, 3, 4] and e1.class == TypeError and a == [1, 2, 3, 1]
end

assert("Array#&") do
  a = [1, 2, 3, 1]
  b = [1, 4]
  c = 1
  e1 = nil

  begin
    a & c
  rescue => e1
  end

  (a & b) == [1] and e1.class == TypeError and a == [1, 2, 3, 1]
end

assert("Array#flatten") do
  [1, 2, "3", {4=>5}, :'6'] == [1, 2, "3", {4=>5}, :'6'].flatten and
  [1, 2, 3, 4, 5, 6] == [1, 2, [3, 4, 5], 6].flatten and
  [1, 2, 3, 4, 5, 6] == [1, 2, [3, [4, 5], 6]].flatten and
  [1, [2, [3, [4, [5, [6]]]]]] == [1, [2, [3, [4, [5, [6]]]]]].flatten(0) and
  [1, 2, [3, [4, [5, [6]]]]] == [1, [2, [3, [4, [5, [6]]]]]].flatten(1) and
  [1, 2, 3, [4, [5, [6]]]] == [1, [2, [3, [4, [5, [6]]]]]].flatten(2) and
  [1, 2, 3, 4, [5, [6]]] == [1, [2, [3, [4, [5, [6]]]]]].flatten(3) and
  [1, 2, 3, 4, 5, [6]] == [1, [2, [3, [4, [5, [6]]]]]].flatten(4) and
  [1, 2, 3, 4, 5, 6] == [1, [2, [3, [4, [5, [6]]]]]].flatten(5)
end

assert("Array#flatten!") do
  [1, 2, 3, 4, 5, 6] == [1, 2, [3, [4, 5], 6]].flatten!
end

assert("Array#compact") do
  a = [1, nil, "2", nil, :t, false, nil]
  a.compact == [1, "2", :t, false] && a == [1, nil, "2", nil, :t, false, nil]
end

assert("Array#compact!") do
  a = [1, nil, "2", nil, :t, false, nil]
  a.compact!
  a == [1, "2", :t, false]
end
