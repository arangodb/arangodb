var chai = require('chai');
var expect = chai.expect;
var utils = require('../lib/utils');

describe('Utils', function () {
    describe('parseAddress', function () {
        it('should parse ip4 addresses with port', function () {
            expect(utils.parseAddress('192.168.1.1:1234')).to.deep.equal({
                address: '192.168.1.1',
                port: 1234
            });//[::]:0
        });

        it('should parse darwin-format ip4 addresses with port', function () {
            expect(utils.parseAddress('10.59.107.189.54728')).to.deep.equal({
                address: '10.59.107.189',
                port: 54728
            });//[::]:0
        });

        it('should parse ip6 addresses with port', function () {
            expect(utils.parseAddress('[2001:0db8:85a3:0000:0000:8a2e:0370:7334]:1900')).to.deep.equal({
                address: '2001:0db8:85a3:0000:0000:8a2e:0370:7334',
                port: 1900
            });//[::]:0
        });

        it('should return null for invalid ip4 addresses', function () {
            expect(utils.parseAddress('0.0.0.0:0')).to.deep.equal({
                address: null,
                port: 0
            });
        });

        it('should return null for invalid ip6 addresses', function () {
            expect(utils.parseAddress('[::]:0')).to.deep.equal({
                address: null,
                port: 0
            });
        });
    });
});