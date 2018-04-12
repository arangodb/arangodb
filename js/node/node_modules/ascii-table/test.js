'use strict';

var assert = require('assert')
  , ase = assert.strictEqual
  , ade = assert.deepEqual
  , AsciiTable = require('./index')
  , info = require('./package.json')

describe('Ascii Table v' + info.version, function() {

  describe('Examples', function() {

    it('default', function() {
      var table = new AsciiTable('A Title')
      table
        .setHeading('', 'Name', 'Age')
        .addRow(1, 'Bob', 52)
        .addRow(2, 'John', 34)
        .addRow(3, 'Jim', 0)

      var output = ''
             + '.----------------.'
      + '\n' + '|    A Title     |'
      + '\n' + '|----------------|'
      + '\n' + '|   | Name | Age |'
      + '\n' + '|---|------|-----|'
      + '\n' + '| 1 | Bob  |  52 |'
      + '\n' + '| 2 | John |  34 |'
      + '\n' + '| 3 | Jim  |   0 |'
      + '\n' + "'----------------'"

      var table2 = new AsciiTable('A Title')
        , headings = ['', 'Name', 'Age']
        , row = [1, 'Bob', 52]

      var matrix = [
        [2, 'John', 34]
      , [3, 'Jim', 0]
      ]

      table2
        .setHeading(headings)
        .addRow(row)
        .addRowMatrix(matrix)

      ase(table.toString(), output)
      ase(table2.toString(), output)

    })

    it('dataObjects', function() {
      var defaultOutput = ''
                 + '.----------------.'
          + '\n' + '|    A Title     |'
          + '\n' + '|----------------|'
          + '\n' + '|   | Name | Age |'
          + '\n' + '|---|------|-----|'
          + '\n' + '| 1 | Bob  |  52 |'
          + '\n' + '| 2 | John |  34 |'
          + '\n' + '| 3 | Jim  |  83 |'
          + '\n' + "'----------------'"

      var matrixOutput = ''
                 + '.----------------.'
          + '\n' + '|    A Title     |'
          + '\n' + '|----------------|'
          + '\n' + '|   | Name | Age |'
          + '\n' + '|---|------|-----|'
          + '\n' + '| 1 | Bob  |  52 |'
          + '\n' + '| 2 | John |  34 |'
          + '\n' + '|   |      |  83 |'
          + '\n' + "'----------------'"

      var arrayData = [
        {index: 2, name: 'John', age: 34, secondAge: 83}
        , {index: 3, name: 'Jim', age: 83}
      ]

      var table = new AsciiTable('A Title')
          , headings = ['', 'Name', 'Age']
          , row = [1, 'Bob', 52]

      table
          .setHeading(headings)
          .addRow(row)
          .addData(arrayData, function(data) {return [data.index, data.name, data.age]})

      var table2 = new AsciiTable('A Title')

      table2
          .setHeading(headings)
          .addRow(row)
          .addData([{index: 2, name: 'John', age: 34, secondAge: 83}], function(data) {return [[data.index, data.name, data.age], ["", "", data.secondAge]]}, true)

      ase(table.toString(), defaultOutput)
      ase(table2.toString(), matrixOutput)
    })

    it('prefixed', function() {
      var table = new AsciiTable('A Title', {
        prefix: '    '
      })
      table
        .setHeading('', 'Name', 'Age')
        .addRow(1, 'Bob', 52)
        .addRow(2, 'John', 34)
        .addRow(3, 'Jim', 83)

      var output = ''
             + '    .----------------.'
      + '\n' + '    |    A Title     |'
      + '\n' + '    |----------------|'
      + '\n' + '    |   | Name | Age |'
      + '\n' + '    |---|------|-----|'
      + '\n' + '    | 1 | Bob  |  52 |'
      + '\n' + '    | 2 | John |  34 |'
      + '\n' + '    | 3 | Jim  |  83 |'
      + '\n' + "    '----------------'"
      
      ase(table.toString(), output)
    })

    it('all', function() {
      var table = new AsciiTable('Something')
      table
        .setBorder()
        .removeBorder()
        .setAlign(0, AsciiTable.CENTER)
        .setAlignLeft(1)
        .setAlignCenter(1)
        .setAlignRight(1)
        
        .setTitle('Hi')
        .setTitleAlign(AsciiTable.LEFT)
        .setTitleAlignLeft(1)
        .setTitleAlignCenter(1)
        .setTitleAlignRight(1)

        .setHeading('one', 'two', 'three')
        .setHeading(['one', 'two', 'three'])
        .setHeadingAlign(0, AsciiTable.CENTER)
        .setHeadingAlignLeft(1)
        .setHeadingAlignCenter(1)
        .setHeadingAlignRight(1)

        .addRow(1, 2, 3)
        .addRow([4, 5, 6])
        .addRowMatrix([
          [7, 8, 9]
        , [10, 11, 12]
        ])
        .setJustify()
        .setJustify(false)

        .sort(function(a, b) { return a })
        .sortColumn(1, function(a, b) { return a })

      table.toJSON()
      table.toString()
      table.valueOf()
      table.render()
    })

    it('alignment', function() {
      var table = new AsciiTable()
      table
        .setTitle('Something')
        .setTitleAlign(AsciiTable.LEFT)
        .setHeading('', 'Name', 'Age')
        .setHeadingAlign(AsciiTable.RIGHT)
        .setAlignCenter(0)
        .setAlign(2, AsciiTable.RIGHT)
        .addRow('a', 'apple', 'Some longer string')
        .addRow('b', 'banana', 'hi')
        .addRow('c', 'carrot', 'meow')
        .addRow('efg', 'elephants')

      var str = ""
             + ".--------------------------------------."
      + "\n" + "| Something                            |"
      + "\n" + "|--------------------------------------|"
      + "\n" + "|     |      Name |                Age |"
      + "\n" + "|-----|-----------|--------------------|"
      + "\n" + "|  a  | apple     | Some longer string |"
      + "\n" + "|  b  | banana    |                 hi |"
      + "\n" + "|  c  | carrot    |               meow |"
      + "\n" + "| efg | elephants |                    |"
      + "\n" + "'--------------------------------------'"
      ase(str, table.toString())
    })
  })

  describe('Static methods', function() {

    it('#version', function() {
      ase(info.version, AsciiTable.VERSION)
      ase(info.version, require('./ascii-table.min').VERSION)
      ase(info.version, require('./bower.json').version)
    })

    it('#align', function() {
      ase(AsciiTable.align(AsciiTable.LEFT, 'a', 10), AsciiTable.alignLeft('a', 10))
      ase(AsciiTable.align(AsciiTable.CENTER, 'a', 10), AsciiTable.alignCenter('a', 10))
      ase(AsciiTable.align(AsciiTable.RIGHT, 'a', 10), AsciiTable.alignRight('a', 10))
    })

    it('#alignLeft', function() {
      var str = AsciiTable.alignLeft('foo', 30)
      ase(str, 'foo                           ')

      var str = AsciiTable.alignLeft(null, 30)
      ase(str, '                              ')

      var str = AsciiTable.alignLeft('bar', 10, '-')
      ase(str, 'bar-------')

      var str = AsciiTable.alignLeft('meow', 1, '-')
      ase(str, 'meow')
    })

    it('#alignRight', function() {
      var str = AsciiTable.alignRight('foo', 30)
      ase(str, '                           foo')

      var str = AsciiTable.alignRight(null, 30)
      ase(str, '                              ')

      var str = AsciiTable.alignRight('bar', 10, '-')
      ase(str, '-------bar')

      var str = AsciiTable.alignRight('meow', 1, '-')
      ase(str, 'meow')
    })

    it('#alignCenter', function() {
      var str = AsciiTable.alignCenter('foo', 30)
      ase(str, '             foo              ')

      var str = AsciiTable.alignCenter(null, 30)
      ase(str, '                              ')


      var str = AsciiTable.alignCenter('bar', 10, '-')
      ase(str, '---bar----')

      var str = AsciiTable.alignCenter('bars', 10, '-')
      ase(str, '---bars---')

      var str = AsciiTable.alignCenter('bar', 11, '-')
      ase(str, '----bar----')
    })

    it('#alignAuto', function() {
      
    })

    it('#arrayFill', function() {
      var arr = AsciiTable.arrayFill(10, '-')
      ase(arr.length, 10)
      ase(arr[0], '-')
    })

    it('#factory', function() {
      var table = AsciiTable.factory('title')
      ase(table instanceof AsciiTable, true)
      ase(table.getTitle(), 'title')
    })

    it('#factory with object', function() {
      var obj = {
        title: 'foo',
        heading: ['id', 'name'],
        rows: [
          [1, 'bob'],
          [2, 'jim']
        ]
      }
      var table = AsciiTable.factory(obj)
      ase(table instanceof AsciiTable, true)
      ase(table.getTitle(), 'foo')
    })
  })

  describe('Instance methods', function() {

    it('#setBorder', function() {
      var table = new AsciiTable('a')
      table
        .setBorder('*')
        .setHeading('one', 'two')
        .addRow('abc', 'def')

      var str = ''
             + '*************'
      + '\n' + '*     a     *'
      + '\n' + '*************'
      + '\n' + '* one * two *'
      + '\n' + '*************'
      + '\n' + '* abc * def *'
      + '\n' + '*************'

      ase(str, table.toString())
    })

    it('#removeBorder', function() {
      
    })

    it('#setAlign', function() {
      
    })

    it('#setAlignLeft', function() {
      
    })

    it('#setAlignCenter', function() {
      
    })

    it('#setAlignRight', function() {
      
    })

    it('#setTitle', function() {
      var table = new AsciiTable('meow')
      ase(table.getTitle(), 'meow')
      table.setTitle('bark')
      ase(table.getTitle(), 'bark')
    })

    it('#sort', function() {
      
    })

    it('#sortColumn', function() {
      
    })

    it('#setHeading', function() {
    })

    it('#addRow', function() {
      
    })

    it('#setJustify', function() {
      
    })

    it('#toString', function() {
      
    })

    it('#toJSON', function() {
      var table = new AsciiTable('cat')
      table
        .setHeading('one', 'two', 'three')
        .addRow(1, 2, 3)
        .addRow(4, 5, 6)

      var output = {
        title: 'cat'
      , heading: ['one', 'two', 'three']
      , rows: [
          [1, 2, 3]
        , [4, 5, 6]
        ]
      }
      var js = table.toJSON()
      ade(js, output)

      js.heading[0] = 'test'
      ase(table.getHeading()[0], 'one')

      js.rows[0][0] = 'test'
      ase(table.getRows()[0][0], 1)
    })

    it('#clear', function() {
      
    })

    it('#clearRows', function() {
      
    })
  })
})
