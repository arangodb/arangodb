var chai = require('chai');
var expect = chai.expect;
var sinon = require('sinon');
chai.use(require("sinon-chai"));
var EventEmitter = require('events').EventEmitter;

var activators = require('../lib/activators');
var utils = require('../lib/utils');

var data = require('./data');
var data2 = require('./data2');
var testData = data.join('\n');
var testData2 = data2.join('\n');

var makeLineHandler = function (stopHandler) {
    return function (line) {
        if (lineHandler && lineHandler(line) === false) {
            stopHandler();
        }
    };
};

function stubProc() {
    var p = new EventEmitter();
    p.stdout = new EventEmitter();
    p.killed = false;
    p.kill = function () {
        p.killed = true;
        p.emit('close');
    };

    return p;
}

function resetSpawnStub() {
    proc = stubProc();
    proc2 = stubProc();

    var stub = sinon.stub(activators, '_spawn');
    stub.onCall(0).returns(proc);
    stub.onCall(1).returns(proc2);
}

var lineHandler = null;
var proc = null, proc2 = null;

describe('Activators', function () {    
    describe('asynchronous', function () {
        beforeEach(function () {
            lineHandler = null;
            resetSpawnStub();
        });

        afterEach(function () {
            activators._spawn.restore();
        });

        it('should call the line handler for each line present in the child processes stdout', function (done) {
            lineHandler = sinon.spy();

            activators.async('', '', makeLineHandler, function () {
                expect(lineHandler).to.have.callCount(data.length);
                done();
            }, false);

            proc.stdout.emit('data', testData);
            proc.stdout.emit('end');
            proc.emit('close');
        });

        it('should call done when processing is complete', function (done) {
            lineHandler = sinon.spy();

            activators.async('', '', makeLineHandler, done, false);

            proc.stdout.emit('data', testData);
            proc.stdout.emit('end');
            proc.emit('close');
        });

        it('should return an error if the child process encounters one', function (done) {
            var error = new Error('test error');

            activators.async('', '', makeLineHandler, function (err) {
                expect(err).to.equal(error);
                done();
            }, false);

            proc.emit('error', error);
        });

        it('should call done when canceled', function (done) {
            var handler = activators.async('', '', makeLineHandler, done);
            handler.cancel();
        });

        it('should take no effect when handler.cancel is called twice', function () {
            var watchDone = sinon.spy();
            var handler = activators.async('', '', makeLineHandler, watchDone);
            handler.cancel();
            handler.cancel();
            expect(watchDone).to.have.callCount(1);
        });
    });

    describe('synchronous', function () {
        beforeEach(function () {
            lineHandler = null;
            proc = {
                error: null,
                stdout: testData
            };

            sinon.stub(activators, '_spawnSync').returns(proc);
        });

        afterEach(function () {
            activators._spawnSync.restore();
        });

        it('should call the line handler for each line present in the child processes stdout', function () {
            lineHandler = sinon.spy();

            activators.sync('', '', makeLineHandler, function () {});

            expect(lineHandler).to.have.callCount(data.length);
        });

        it('should call done when processing is complete', function () {
            lineHandler = sinon.spy();
            var done = sinon.spy();

            activators.sync('', '', makeLineHandler, done);

            expect(lineHandler).to.have.callCount(data.length);
            expect(done).to.have.been.called;
        });

        it('should return an error if the child process encounters one', function () {
            proc.error = new Error('test error');
            var done = sinon.spy();

            activators.sync('', '', makeLineHandler, done);

            expect(done).to.have.been.calledWith(proc.error);
        });
    });

    describe('continuous', function () {
        var activator = null;

        beforeEach(function () {
            lineHandler = null;
            activator = function (cmd, args, makeLineHandler, done) {
                var handler = makeLineHandler(function () {});
                handler();
                done();
            };
        });

        it('should make repeated async activator calls until processing is stopped', function (done) {
            lineHandler = sinon.stub();
            lineHandler.onCall(3).returns(false);

            activators.continuous(activator, { cmd: '', args: '', makeLineHandler, done: function () {
                // sinon's onCall() is 0-based index, so call 3 = 4
                expect(lineHandler).to.have.callCount(4);

                done();
            } }, { sync: false });
        });


        it('should make repeated sync activator calls until processing is stopped', function () {
            lineHandler = sinon.stub();
            lineHandler.onCall(3).returns(false);
            var doneSpy = sinon.spy();

            activators.continuous(activator, { cmd: '', args: '', makeLineHandler, done: doneSpy }, { sync: true });
            expect(doneSpy).to.have.callCount(1);
            expect(lineHandler).to.have.callCount(4);
        });
    });
});
