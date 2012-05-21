##
# Array ISO Test

assert('Array', '15.2.12') do
  Array.class == Class
end

assert('Array.[]', '15.2.12.4.1') do
  Array.[](1,2,3) == [1, 2, 3]
end

assert('Array#*', '15.2.12.5.1') do
  [1].*(3) == [1, 1, 1]
end

assert('Array#+', '15.2.12.5.2') do
  [1].+([1]) == [1, 1]
end

assert('Array#<<', '15.2.12.5.3') do
  [1].<<(1) == [1, 1]
end

assert('Array#[]', '15.2.12.5.4') do
  [1,2,3].[](1) == 2
end

assert('Array#[]=', '15.2.12.5.5') do
  [1,2,3].[]=(1,4) == [1, 4, 3]
end

assert('Array#clear', '15.2.12.5.6') do
  a = [1]
  a.clear
  a == []
end

# Not ISO specified


