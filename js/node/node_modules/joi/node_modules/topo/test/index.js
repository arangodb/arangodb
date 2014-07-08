// Load modules

var Lab = require('lab');
var Topo = require('..');


// Declare internals

var internals = {};


// Test shortcuts

var expect = Lab.expect;
var before = Lab.before;
var after = Lab.after;
var describe = Lab.experiment;
var it = Lab.test;


describe('Topo', function () {

    var testDeps = function (scenario) {

        var topo = new Topo();
        scenario.forEach(function (record, i) {

            var options = record.before || record.after || record.group ? { before: record.before, after: record.after, group: record.group } : null;
            topo.add(record.id, options);
        });

        return topo.nodes.join('');
    };

    it('sorts dependencies', function (done) {

        var scenario = [
            { id: '0', before: 'a' },
            { id: '1', after: 'f', group: 'a' },
            { id: '2', before: 'a' },
            { id: '3', before: ['b', 'c'], group: 'a' },
            { id: '4', after: 'c', group: 'b' },
            { id: '5', group: 'c' },
            { id: '6', group: 'd' },
            { id: '7', group: 'e' },
            { id: '8', before: 'd' },
            { id: '9', after: 'c', group: 'a' }
        ];

        expect(testDeps(scenario)).to.equal('0213547869');
        done();
    });

    it('sorts dependencies (seq)', function (done) {

        var scenario = [
            { id: '0' },
            { id: '1' },
            { id: '2' },
            { id: '3' }
        ];

        expect(testDeps(scenario)).to.equal('0123');
        done();
    });

    it('sorts dependencies (explicit)', function (done) {

        var set = '0123456789abcdefghijklmnopqrstuvwxyz';
        var array = set.split('');

        var scenario = [];
        for (var i = 0, il = array.length; i < il; ++i) {
            var item = {
                id: array[i],
                group: array[i],
                after: i ? array.slice(0, i) : [],
                before: array.slice(i + 1)
            };
            scenario.push(item);
        }

        var fisherYates = function (array) {

            var i = array.length;
            while (--i) {
                var j = Math.floor(Math.random() * (i + 1));
                var tempi = array[i];
                var tempj = array[j];
                array[i] = tempj;
                array[j] = tempi;
            }
        };

        fisherYates(scenario);
        expect(testDeps(scenario)).to.equal(set);
        done();
    });

    it('throws on circular dependency', function (done) {

        var scenario = [
            { id: '0', before: 'a', group: 'b' },
            { id: '1', before: 'c', group: 'a' },
            { id: '2', before: 'b', group: 'c' }
        ];

        expect(function () {

            testDeps(scenario);
        }).to.throw('item added into group c created a dependencies error');

        done();
    });
});
