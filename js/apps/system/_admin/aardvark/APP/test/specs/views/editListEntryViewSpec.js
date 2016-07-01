/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, spyOn, expect*/
/* global $, _*/

(function () {
  'use strict';

  describe('The view to edit one entry', function () {
    var ViewClass, tr, key, val;

    beforeEach(function () {
      ViewClass = window.EditListEntryView;
      tr = document.createElement('tr');
      document.body.appendChild(tr);
    });

    afterEach(function () {
      document.body.removeChild(tr);
    });

    describe('user input', function () {
      it('should be able to remove the row', function () {
        key = 'my_key';
        val = 'val';
        var view = new ViewClass({
          key: key,
          value: val,
          el: tr
        });
        $('.deleteAttribute').click();
        expect(tr.parentElement).toEqual(null);
        // Re add it for cleanup function
        document.body.appendChild(tr);
      });
    });

    describe('for system attributes', function () {
      it('should not offer edit options', function () {
        key = '_id';
        val = 'v/123';
        var view = new ViewClass({
          key: key,
          value: val,
          el: tr
        });
        expect($('.key', tr).text()).toEqual(key);
        expect($('.key', tr).is('input')).toBeFalsy();
        expect($('.key', tr).is('textarea')).toBeFalsy();
        expect($('.val', tr).text()).toEqual(JSON.stringify(val));
        expect($('.val', tr).is('input')).toBeFalsy();
        expect($('.val', tr).is('textarea')).toBeFalsy();
      });
    });

    describe('for booleans', function () {
      it('should be able to display true', function () {
        key = 'myKey';
        val = true;
        var view = new ViewClass({
          key: key,
          value: val,
          el: tr
        });
        expect($('.key', tr).val()).toEqual(key);
        expect($('.val', tr).val()).toEqual(JSON.stringify(val));
      });

      it('should be able to display false', function () {
        key = 'myKey';
        val = false;
        var view = new ViewClass({
          key: key,
          value: val,
          el: tr
        });
        expect($('.key', tr).val()).toEqual(key);
        expect($('.val', tr).val()).toEqual(JSON.stringify(val));
      });

      it('should be able to store true', function () {
        key = 'myKey';
        val = JSON.stringify(true);
        var view = new ViewClass({
          key: key,
          value: '',
          el: tr
        });
        $('.val', tr).val(val);
        expect(view.getKey()).toEqual(key);
        expect(view.getValue()).toEqual(true);
      });

      it('should be able to store false', function () {
        key = 'myKey';
        val = JSON.stringify(false);
        var view = new ViewClass({
          key: key,
          value: '',
          el: tr
        });
        $('.val', tr).val(val);
        expect(view.getKey()).toEqual(key);
        expect(view.getValue()).toEqual(false);
      });
    });

    describe('for numbers', function () {
      it('should be able to display an integer', function () {
        key = 'myKey';
        val = 123;
        var view = new ViewClass({
          key: key,
          value: val,
          el: tr
        });
        expect($('.key', tr).val()).toEqual(key);
        expect($('.val', tr).val()).toEqual(JSON.stringify(val));
      });

      it('should be able to display a float', function () {
        key = 'myKey';
        val = 12.345;
        var view = new ViewClass({
          key: key,
          value: val,
          el: tr
        });
        expect($('.key', tr).val()).toEqual(key);
        expect($('.val', tr).val()).toEqual(JSON.stringify(val));
      });

      it('should be able to store an integer', function () {
        key = 'myKey';
        val = JSON.stringify(123);
        var view = new ViewClass({
          key: key,
          value: '',
          el: tr
        });
        $('.val', tr).val(val);
        expect(view.getKey()).toEqual(key);
        expect(view.getValue()).toEqual(123);
      });

      it('should be able to store a float', function () {
        key = 'myKey';
        val = JSON.stringify(10.234);
        var view = new ViewClass({
          key: key,
          value: '',
          el: tr
        });
        $('.val', tr).val(val);
        expect(view.getKey()).toEqual(key);
        expect(view.getValue()).toEqual(10.234);
      });
    });

    describe('for strings', function () {
      it('should be able to display a simple string', function () {
        key = 'myKey';
        val = 'string';
        var view = new ViewClass({
          key: key,
          value: val,
          el: tr
        });
        expect($('.key', tr).val()).toEqual(key);
        expect($('.val', tr).val()).toEqual(JSON.stringify(val));
      });

      it('should be able to display a string with quotation marks', function () {
        key = 'myKey';
        val = 'my "internal" string';
        var view = new ViewClass({
          key: key,
          value: val,
          el: tr
        });
        expect($('.key', tr).val()).toEqual(key);
        expect($('.val', tr).val()).toEqual(JSON.stringify(val));
      });

      it('should be able to store a simple string', function () {
        key = 'myKey';
        val = 'string';
        var view = new ViewClass({
          key: key,
          value: '',
          el: tr
        });
        $('.val', tr).val(val);
        expect(view.getKey()).toEqual(key);
        expect(view.getValue()).toEqual(val);
      });

      it('should be able to store a float', function () {
        key = 'myKey';
        val = 'my "internal" string';
        var view = new ViewClass({
          key: key,
          value: '',
          el: tr
        });
        $('.val', tr).val(val);
        expect(view.getKey()).toEqual(key);
        expect(view.getValue()).toEqual(val);
      });
    });

    describe('for array', function () {
      it('should be able to display an array', function () {
        key = 'myKey';
        val = [1, 'string', true];
        var view = new ViewClass({
          key: key,
          value: val,
          el: tr
        });
        expect($('.key', tr).val()).toEqual(key);
        expect($('.val', tr).val()).toEqual(JSON.stringify(val));
      });

      it('should be able to display an array of arrays', function () {
        key = 'myKey';
        val = [
          [1, 2, 3],
          ['string']
        ];
        var view = new ViewClass({
          key: key,
          value: val,
          el: tr
        });
        expect($('.key', tr).val()).toEqual(key);
        expect($('.val', tr).val()).toEqual(JSON.stringify(val));
      });

      it('should be able to store an array', function () {
        key = 'myKey';
        val = [1, 'string', true];
        var view = new ViewClass({
            key: key,
            value: '',
            el: tr
          }),
          valString = '[1, "string", true]';
        $('.val', tr).val(valString);
        expect(view.getKey()).toEqual(key);
        expect(view.getValue()).toEqual(val);
      });

      it('should be able to store an array of arrays', function () {
        key = 'myKey';
        val = [
          [1, 2, 3],
          ['string']
        ];
        var view = new ViewClass({
            key: key,
            value: '',
            el: tr
          }),
          valString = '[[1, 2, 3], ["string"]]';
        $('.val', tr).val(valString);
        expect(view.getKey()).toEqual(key);
        expect(view.getValue()).toEqual(val);
      });
    });

    describe('for array', function () {
      it('should be able to display an object', function () {
        key = 'myKey';
        val = {
          a: 1,
          b: 'string',
          c: true
        };
        var view = new ViewClass({
          key: key,
          value: val,
          el: tr
        });
        expect($('.key', tr).val()).toEqual(key);
        expect($('.val', tr).val()).toEqual(JSON.stringify(val));
      });

      it('should be able to display nested objects', function () {
        key = 'myKey';
        val = {
          a: {
            c: 'my',
            d: 'way'
          },
          b: true
        };
        var view = new ViewClass({
          key: key,
          value: val,
          el: tr
        });
        expect($('.key', tr).val()).toEqual(key);
        expect($('.val', tr).val()).toEqual(JSON.stringify(val));
      });

      it('should be able to store an object', function () {
        key = 'myKey';
        val = {
          a: 1,
          b: 'string',
          c: true
        };
        var view = new ViewClass({
            key: key,
            value: '',
            el: tr
          }),
          valString = '{a: 1, b: "string", c: true}',
          valStr = JSON.stringify(val),
          resStr;
        $('.val', tr).val(valString);
        resStr = JSON.stringify(view.getValue());
        expect(view.getKey()).toEqual(key);
        expect(valStr).toEqual(resStr);
      });

      it('should be able to store nested objects', function () {
        key = 'myKey';
        val = {
          a: {
            c: 'my',
            d: 'way'
          },
          b: true
        };
        var view = new ViewClass({
            key: key,
            value: '',
            el: tr
          }),
          valString = '{a: {c: "my", d: "way"}, b: true}';
        $('.val', tr).val(valString);
        expect(view.getKey()).toEqual(key);
        expect(_.isEqual(view.getValue(), val)).toBeTruthy();
      });
    });
  });
}());
