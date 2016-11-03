'use strict';

// Load modules

const Code = require('code');
const Lab = require('lab');
const Hoek = require('hoek');
const Topo = require('..');


// Declare internals

const internals = {};


// Test shortcuts

const lab = exports.lab = Lab.script();
const describe = lab.describe;
const it = lab.it;
const expect = Code.expect;


describe('Topo', () => {

    const testDeps = function (scenario) {

        const topo = new Topo();
        scenario.forEach((record, i) => {

            const options = record.before || record.after || record.group ? { before: record.before, after: record.after, group: record.group } : null;
            topo.add(record.id, options);
        });

        return topo.nodes.join('');
    };

    it('sorts dependencies', (done) => {

        const scenario = [
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

    it('sorts dependencies (before as array)', (done) => {

        const scenario = [
            { id: '0', group: 'a' },
            { id: '1', group: 'b' },
            { id: '2', before: ['a', 'b'] }
        ];

        expect(testDeps(scenario)).to.equal('201');
        done();
    });

    it('sorts dependencies (after as array)', (done) => {

        const scenario = [
            { id: '0', after: ['a', 'b'] },
            { id: '1', group: 'a' },
            { id: '2', group: 'b' }
        ];

        expect(testDeps(scenario)).to.equal('120');
        done();
    });


    it('sorts dependencies (seq)', (done) => {

        const scenario = [
            { id: '0' },
            { id: '1' },
            { id: '2' },
            { id: '3' }
        ];

        expect(testDeps(scenario)).to.equal('0123');
        done();
    });

    it('sorts dependencies (explicitly using after or before)', (done) => {

        const set = '0123456789abcdefghijklmnopqrstuvwxyz';
        const groups = set.split('');

        // Use Fisher-Yates for shuffling

        const fisherYates = function (array) {

            let i = array.length;
            while (--i) {
                const j = Math.floor(Math.random() * (i + 1));
                const tempi = array[i];
                const tempj = array[j];
                array[i] = tempj;
                array[j] = tempi;
            }
        };

        const scenarioAfter = [];
        const scenarioBefore = [];
        for (let i = 0; i < groups.length; ++i) {
            const item = {
                id: groups[i],
                group: groups[i]
            };
            const afterMod = {
                after: i ? groups.slice(0, i) : []
            };
            const beforeMod = {
                before: groups.slice(i + 1)
            };

            scenarioAfter.push(Hoek.applyToDefaults(item, afterMod));
            scenarioBefore.push(Hoek.applyToDefaults(item, beforeMod));
        }

        fisherYates(scenarioAfter);
        expect(testDeps(scenarioAfter)).to.equal(set);

        fisherYates(scenarioBefore);
        expect(testDeps(scenarioBefore)).to.equal(set);
        done();
    });

    it('throws on circular dependency', (done) => {

        const scenario = [
            { id: '0', before: 'a', group: 'b' },
            { id: '1', before: 'c', group: 'a' },
            { id: '2', before: 'b', group: 'c' }
        ];

        expect(() => {

            testDeps(scenario);
        }).to.throw('item added into group c created a dependencies error');

        done();
    });

    describe('merge()', () => {

        it('merges objects', (done) => {

            const topo = new Topo();
            topo.add('0', { before: 'a' });
            topo.add('2', { before: 'a' });
            topo.add('4', { after: 'c', group: 'b' });
            topo.add('6', { group: 'd' });
            topo.add('8', { before: 'd' });
            expect(topo.nodes.join('')).to.equal('02486');

            const other = new Topo();
            other.add('1', { after: 'f', group: 'a' });
            other.add('3', { before: ['b', 'c'], group: 'a' });
            other.add('5', { group: 'c' });
            other.add('7', { group: 'e' });
            other.add('9', { after: 'c', group: 'a' });
            expect(other.nodes.join('')).to.equal('13579');

            topo.merge(other);
            expect(topo.nodes.join('')).to.equal('0286135479');
            done();
        });

        it('merges objects (explicit sort)', (done) => {

            const topo = new Topo();
            topo.add('0', { before: 'a', sort: 1 });
            topo.add('2', { before: 'a', sort: 2 });
            topo.add('4', { after: 'c', group: 'b', sort: 3 });
            topo.add('6', { group: 'd', sort: 4 });
            topo.add('8', { before: 'd', sort: 5 });
            expect(topo.nodes.join('')).to.equal('02486');

            const other = new Topo();
            other.add('1', { after: 'f', group: 'a', sort: 6 });
            other.add('3', { before: ['b', 'c'], group: 'a', sort: 7 });
            other.add('5', { group: 'c', sort: 8 });
            other.add('7', { group: 'e', sort: 9 });
            other.add('9', { after: 'c', group: 'a', sort: 10 });
            expect(other.nodes.join('')).to.equal('13579');

            topo.merge(other);
            expect(topo.nodes.join('')).to.equal('0286135479');
            done();
        });

        it('merges objects (mixed sort)', (done) => {

            const topo = new Topo();
            topo.add('0', { before: 'a', sort: 1 });
            topo.add('2', { before: 'a', sort: 3 });
            topo.add('4', { after: 'c', group: 'b', sort: 5 });
            topo.add('6', { group: 'd', sort: 7 });
            topo.add('8', { before: 'd', sort: 9 });
            expect(topo.nodes.join('')).to.equal('02486');

            const other = new Topo();
            other.add('1', { after: 'f', group: 'a', sort: 2 });
            other.add('3', { before: ['b', 'c'], group: 'a', sort: 4 });
            other.add('5', { group: 'c', sort: 6 });
            other.add('7', { group: 'e', sort: 8 });
            other.add('9', { after: 'c', group: 'a', sort: 10 });
            expect(other.nodes.join('')).to.equal('13579');

            topo.merge(other);
            expect(topo.nodes.join('')).to.equal('0213547869');
            done();
        });

        it('merges objects (multiple)', (done) => {

            const topo1 = new Topo();
            topo1.add('0', { before: 'a', sort: 1 });
            topo1.add('2', { before: 'a', sort: 3 });
            topo1.add('4', { after: 'c', group: 'b', sort: 5 });

            const topo2 = new Topo();
            topo2.add('6', { group: 'd', sort: 7 });
            topo2.add('8', { before: 'd', sort: 9 });

            const other = new Topo();
            other.add('1', { after: 'f', group: 'a', sort: 2 });
            other.add('3', { before: ['b', 'c'], group: 'a', sort: 4 });
            other.add('5', { group: 'c', sort: 6 });
            other.add('7', { group: 'e', sort: 8 });
            other.add('9', { after: 'c', group: 'a', sort: 10 });
            expect(other.nodes.join('')).to.equal('13579');

            topo1.merge([topo2, null, other]);
            expect(topo1.nodes.join('')).to.equal('0213547869');
            done();
        });

        it('throws on circular dependency', (done) => {

            const topo = new Topo();
            topo.add('0', { before: 'a', group: 'b' });
            topo.add('1', { before: 'c', group: 'a' });

            const other = new Topo();
            other.add('2', { before: 'b', group: 'c' });

            expect(() => {

                topo.merge(other);
            }).to.throw('merge created a dependencies error');

            done();
        });
    });
});
