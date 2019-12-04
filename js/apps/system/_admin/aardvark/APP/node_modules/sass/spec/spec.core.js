
describe 'Sass'
  before
    render = function(path, options) {
      return sass.render(fixture(path + '.sass'), options)
    }
    
    expected = function(path) {
      return fixture(path + '.css')
    }
    
    assert = function(path, options) {
      render(path, options).should.eql expected(path)
    }
  end
  
  describe '.version'
    it 'should be a triplet'
      sass.version.should.match(/^\d+\.\d+\.\d+$/)
    end
  end
  
  describe '.render()'
    describe 'with "cache" enabled'
      describe 'without "filename"'
        it 'should throw an error'
          -{ assert('selectors', { cache: true }) }.should.throw_error 'filename option must be passed when cache is enabled'
        end
      end
      
      describe 'with "filename"'
        it 'should still work'
          assert('selectors', { cache: true, filename: 'style.sass' })
        end
      end
    end
  
    it 'should support complex selectors'
      assert('selectors')
    end
    
    describe '// ...'
      it 'should be a sass-specific comment'
        assert('comment')
      end
    end
    
    describe '& ...'
      it 'should continue a selector'
        assert('continuation')
      end
    end
  
    describe '{...}'
      it 'should have access to variables'
        assert('literal')
      end
    end
    
    describe ':key val'
      it 'should define a property'
        assert('properties')
      end
      
      describe 'when nested'
        it 'should traverse correctly'
          assert('properties.nested')
        end
        
        describe 'incorrectly'
          it 'should throw an error'
            try { assert('properties.nested.invalid') }
            catch (e) {
              e.message.should.eql 'ParseError: on line 3; invalid indentation, to much nesting'
            }
          end
        end
      end
      
      describe 'when at the top level'
        it 'should throw an error'
          try { assert('properties.invalid') }
          catch (e) {
            e.message.should.eql 'ParseError: on line 1; properties must be nested within a selector'
          }
        end
      end
    end
    
    describe '=:key val'
      it 'should expand to -{moz, webkit}-border-radius'
        assert('properties.expand')
      end
    end
    
    describe '!key = val'
      it 'should define a variable'
        assert('variables.regular')
      end
    end
    
    describe '!key1 = !key2'
      it 'should reference a previously-defined variable'
        assert('variables.dependent')
      end
    end
    
    describe 'key: val'
      it 'should define a variable'
        assert('variables.alternate')
      end
    end
  end
  
  describe '+mixin'
    it 'should create a mixin'
      assert('mixin')
    end
    
    it 'should create multiple appropriate mixins'
      assert('mixin.multiple')
    end
    
    describe 'when the mixin does not exist'
      try { assert('mixin.undefined') }
      catch (e) {
        e.message.should.eql 'ParseError: on line 2; mixin `large\' does not exist'
      }
    end
  end
  
  describe '.collect()'
    it 'should return variables defined'
      var collected = sass.collect(fixture('collect.sass'))
      collected.variables.should.eql { red: '#ff0000', black: '#000' }
    end
    
    it 'should return mixins defined'
      var collected = sass.collect(fixture('mixin.sass'))
      collected.mixins.should.have_property 'large'
      collected.mixins.should.have_property 'striped'
    end
  end
end
