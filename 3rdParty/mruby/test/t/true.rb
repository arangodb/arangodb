##
# TrueClass ISO Test

assert('TrueClass', '15.2.5') do
  TrueClass.class == Class
end

assert('TrueClass superclass', '15.2.5.2') do
  TrueClass.superclass == Object
end

assert('TrueClass true', '15.2.5.1') do
  true
end

assert('TrueClass#&', '15.2.5.3.1') do
  TrueClass.new.&(true) and not TrueClass.new.&(false)
end

assert('TrueClass#^', '15.2.5.3.2') do
  not TrueClass.new.^(true) and TrueClass.new.^(false)
end

assert('TrueClass#to_s', '15.2.5.3.3') do
  TrueClass.new.to_s == 'true'
end

assert('TrueClass#|', '15.2.5.3.4') do
  TrueClass.new.|(true) and TrueClass.new.|(false)
end
