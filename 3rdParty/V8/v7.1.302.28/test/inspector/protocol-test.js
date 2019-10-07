// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

InspectorTest = {};
InspectorTest._dumpInspectorProtocolMessages = false;
InspectorTest._commandsForLogging = new Set();
InspectorTest._sessions = new Set();

InspectorTest.log = utils.print.bind(utils);
InspectorTest.quitImmediately = utils.quit.bind(utils);

InspectorTest.logProtocolCommandCalls = function(command) {
  InspectorTest._commandsForLogging.add(command);
}

InspectorTest.completeTest = function() {
  var promises = [];
  for (var session of InspectorTest._sessions)
    promises.push(session.Protocol.Debugger.disable());
  Promise.all(promises).then(() => utils.quit());
}

InspectorTest.waitForPendingTasks = function() {
  var promises = [];
  for (var session of InspectorTest._sessions)
    promises.push(session.Protocol.Runtime.evaluate({ expression: "new Promise(r => setTimeout(r, 0))//# sourceURL=wait-for-pending-tasks.js", awaitPromise: true }));
  return Promise.all(promises);
}

InspectorTest.startDumpingProtocolMessages = function() {
  InspectorTest._dumpInspectorProtocolMessages = true;
}

InspectorTest.logMessage = function(originalMessage) {
  var message = JSON.parse(JSON.stringify(originalMessage));
  if (message.id)
    message.id = "<messageId>";

  const nonStableFields = new Set([
    'objectId', 'scriptId', 'exceptionId', 'timestamp', 'executionContextId',
    'callFrameId', 'breakpointId', 'bindRemoteObjectFunctionId',
    'formatterObjectId', 'debuggerId'
  ]);
  var objects = [ message ];
  while (objects.length) {
    var object = objects.shift();
    if (object && object.name === '[[StableObjectId]]')
      object.value = '<StablectObjectId>';
    for (var key in object) {
      if (nonStableFields.has(key))
        object[key] = `<${key}>`;
      else if (typeof object[key] === "string" && object[key].match(/\d+:\d+:\d+:\d+/))
        object[key] = object[key].substring(0, object[key].lastIndexOf(':')) + ":<scriptId>";
      else if (typeof object[key] === "object")
        objects.push(object[key]);
    }
  }

  InspectorTest.logObject(message);
  return originalMessage;
}

InspectorTest.logObject = function(object, title) {
  var lines = [];

  function dumpValue(value, prefix, prefixWithName) {
    if (typeof value === "object" && value !== null) {
      if (value instanceof Array)
        dumpItems(value, prefix, prefixWithName);
      else
        dumpProperties(value, prefix, prefixWithName);
    } else {
      lines.push(prefixWithName + String(value).replace(/\n/g, " "));
    }
  }

  function dumpProperties(object, prefix, firstLinePrefix) {
    prefix = prefix || "";
    firstLinePrefix = firstLinePrefix || prefix;
    lines.push(firstLinePrefix + "{");

    var propertyNames = Object.keys(object);
    propertyNames.sort();
    for (var i = 0; i < propertyNames.length; ++i) {
      var name = propertyNames[i];
      if (!object.hasOwnProperty(name))
        continue;
      var prefixWithName = "    " + prefix + name + " : ";
      dumpValue(object[name], "    " + prefix, prefixWithName);
    }
    lines.push(prefix + "}");
  }

  function dumpItems(object, prefix, firstLinePrefix) {
    prefix = prefix || "";
    firstLinePrefix = firstLinePrefix || prefix;
    lines.push(firstLinePrefix + "[");
    for (var i = 0; i < object.length; ++i)
      dumpValue(object[i], "    " + prefix, "    " + prefix + "[" + i + "] : ");
    lines.push(prefix + "]");
  }

  dumpValue(object, "", title || "");
  InspectorTest.log(lines.join("\n"));
}

InspectorTest.ContextGroup = class {
  constructor() {
    this.id = utils.createContextGroup();
  }

  schedulePauseOnNextStatement(reason, details) {
    utils.schedulePauseOnNextStatement(this.id, reason, details);
  }

  cancelPauseOnNextStatement() {
    utils.cancelPauseOnNextStatement(this.id);
  }

  addScript(string, lineOffset, columnOffset, url) {
    utils.compileAndRunWithOrigin(this.id, string, url || '', lineOffset || 0, columnOffset || 0, false);
  }

  addInlineScript(string, url) {
    const match = (new Error().stack).split('\n')[2].match(/([0-9]+):([0-9]+)/);
    this.addScript(
        string, match[1] * 1, match[1] * 1 + '.addInlineScript('.length, url);
  }

  addModule(string, url, lineOffset, columnOffset) {
    utils.compileAndRunWithOrigin(this.id, string, url, lineOffset || 0, columnOffset || 0, true);
  }

  loadScript(fileName) {
    this.addScript(utils.read(fileName));
  }

  connect() {
    return new InspectorTest.Session(this);
  }

  setupInjectedScriptEnvironment(session) {
    let scriptSource = '';
    // First define all getters on Object.prototype.
    let injectedScriptSource = utils.read('src/inspector/injected-script-source.js');
    let getterRegex = /\.[a-zA-Z0-9]+/g;
    let match;
    let getters = new Set();
    while (match = getterRegex.exec(injectedScriptSource)) {
      getters.add(match[0].substr(1));
    }
    scriptSource += `(function installSettersAndGetters() {
        let defineProperty = Object.defineProperty;
        let ObjectPrototype = Object.prototype;
        let ArrayPrototype = Array.prototype;
        defineProperty(ArrayPrototype, 0, {
          set() { debugger; throw 42; }, get() { debugger; throw 42; },
          __proto__: null
        });`,
        scriptSource += Array.from(getters).map(getter => `
        defineProperty(ObjectPrototype, '${getter}', {
          set() { debugger; throw 42; }, get() { debugger; throw 42; },
          __proto__: null
        });
        `).join('\n') + '})();';
    this.addScript(scriptSource);

    if (session) {
      InspectorTest.log('WARNING: setupInjectedScriptEnvironment with debug flag for debugging only and should not be landed.');
      InspectorTest.log('WARNING: run test with --expose-inspector-scripts flag to get more details.');
      InspectorTest.log('WARNING: you can additionally comment rjsmin in xxd.py to get unminified injected-script-source.js.');
      session.setupScriptMap();
      session.Protocol.Debugger.enable();
      session.Protocol.Debugger.onPaused(message => {
        let callFrames = message.params.callFrames;
        session.logSourceLocations(callFrames.map(frame => frame.location));
      })
    }
  }
};

InspectorTest.Session = class {
  constructor(contextGroup) {
    this.contextGroup = contextGroup;
    this._dispatchTable = new Map();
    this._eventHandlers = new Map();
    this._requestId = 0;
    this.Protocol = this._setupProtocol();
    InspectorTest._sessions.add(this);
    this.id = utils.connectSession(contextGroup.id, '', this._dispatchMessage.bind(this));
  }

  disconnect() {
    InspectorTest._sessions.delete(this);
    utils.disconnectSession(this.id);
  }

  reconnect() {
    var state = utils.disconnectSession(this.id);
    this.id = utils.connectSession(this.contextGroup.id, state, this._dispatchMessage.bind(this));
  }

  async addInspectedObject(serializable) {
    return this.Protocol.Runtime.evaluate({expression: `inspector.addInspectedObject(${this.id}, ${JSON.stringify(serializable)})`});
  }

  sendRawCommand(requestId, command, handler) {
    if (InspectorTest._dumpInspectorProtocolMessages)
      utils.print("frontend: " + command);
    this._dispatchTable.set(requestId, handler);
    utils.sendMessageToBackend(this.id, command);
  }

  setupScriptMap() {
    if (this._scriptMap)
      return;
    this._scriptMap = new Map();
  }

  logCallFrames(callFrames) {
    for (var frame of callFrames) {
      var functionName = frame.functionName || '(anonymous)';
      var scriptId = frame.location ? frame.location.scriptId : frame.scriptId;
      var url = frame.url ? frame.url : this._scriptMap.get(scriptId).url;
      var lineNumber = frame.location ? frame.location.lineNumber : frame.lineNumber;
      var columnNumber = frame.location ? frame.location.columnNumber : frame.columnNumber;
      InspectorTest.log(`${functionName} (${url}:${lineNumber}:${columnNumber})`);
    }
  }

  logSourceLocation(location, forceSourceRequest) {
    var scriptId = location.scriptId;
    if (!this._scriptMap || !this._scriptMap.has(scriptId)) {
      InspectorTest.log("setupScriptMap should be called before Protocol.Debugger.enable.");
      InspectorTest.completeTest();
    }
    var script = this._scriptMap.get(scriptId);
    if (!script.scriptSource || forceSourceRequest) {
      return this.Protocol.Debugger.getScriptSource({ scriptId })
          .then(message => script.scriptSource = message.result.scriptSource)
          .then(dumpSourceWithLocation);
    }
    return Promise.resolve().then(dumpSourceWithLocation);

    function dumpSourceWithLocation() {
      var lines = script.scriptSource.split('\n');
      var line = lines[location.lineNumber];
      line = line.slice(0, location.columnNumber) + '#' + (line.slice(location.columnNumber) || '');
      lines[location.lineNumber] = line;
      lines = lines.filter(line => line.indexOf('//# sourceURL=') === -1);
      InspectorTest.log(lines.slice(Math.max(location.lineNumber - 1, 0), location.lineNumber + 2).join('\n'));
      InspectorTest.log('');
    }
  }

  logSourceLocations(locations) {
    if (locations.length == 0) return Promise.resolve();
    return this.logSourceLocation(locations[0]).then(() => this.logSourceLocations(locations.splice(1)));
  }

  async logBreakLocations(inputLocations) {
    let locations = inputLocations.slice();
    let scriptId = locations[0].scriptId;
    let script = this._scriptMap.get(scriptId);
    if (!script.scriptSource) {
      let message = await this.Protocol.Debugger.getScriptSource({scriptId});
      script.scriptSource = message.result.scriptSource;
    }
    let lines = script.scriptSource.split('\n');
    locations = locations.sort((loc1, loc2) => {
      if (loc2.lineNumber !== loc1.lineNumber) return loc2.lineNumber - loc1.lineNumber;
      return loc2.columnNumber - loc1.columnNumber;
    });
    for (let location of locations) {
      let line = lines[location.lineNumber];
      line = line.slice(0, location.columnNumber) + locationMark(location.type) + line.slice(location.columnNumber);
      lines[location.lineNumber] = line;
    }
    lines = lines.filter(line => line.indexOf('//# sourceURL=') === -1);
    InspectorTest.log(lines.join('\n') + '\n');
    return inputLocations;

    function locationMark(type) {
      if (type === 'return') return '|R|';
      if (type === 'call') return '|C|';
      if (type === 'debuggerStatement') return '|D|';
      return '|_|';
    }
  }

  async logTypeProfile(typeProfile, source) {
    let entries = typeProfile.entries;

    // Sort in reverse order so we can replace entries without invalidating
    // the other offsets.
    entries = entries.sort((a, b) => b.offset - a.offset);

    for (let entry of entries) {
      source = source.slice(0, entry.offset) + typeAnnotation(entry.types) +
        source.slice(entry.offset);
    }
    InspectorTest.log(source);
    return typeProfile;

    function typeAnnotation(types) {
      return `/*${types.map(t => t.name).join(', ')}*/`;
    }
  }

  logAsyncStackTrace(asyncStackTrace) {
    while (asyncStackTrace) {
      InspectorTest.log(`-- ${asyncStackTrace.description || '<empty>'} --`);
      this.logCallFrames(asyncStackTrace.callFrames);
      if (asyncStackTrace.parentId) InspectorTest.log('  <external stack>');
      asyncStackTrace = asyncStackTrace.parent;
    }
  }

  _sendCommandPromise(method, params) {
    if (typeof params !== 'object')
      utils.print(`WARNING: non-object params passed to invocation of method ${method}`);
    if (InspectorTest._commandsForLogging.has(method))
      utils.print(method + ' called');
    var requestId = ++this._requestId;
    var messageObject = { "id": requestId, "method": method, "params": params };
    return new Promise(fulfill => this.sendRawCommand(requestId, JSON.stringify(messageObject), fulfill));
  }

  _setupProtocol() {
    return new Proxy({}, { get: (target, agentName, receiver) => new Proxy({}, {
      get: (target, methodName, receiver) => {
        const eventPattern = /^on(ce)?([A-Z][A-Za-z0-9]+)/;
        var match = eventPattern.exec(methodName);
        if (!match)
          return args => this._sendCommandPromise(`${agentName}.${methodName}`, args || {});
        var eventName = match[2];
        eventName = eventName.charAt(0).toLowerCase() + eventName.slice(1);
        if (match[1])
          return numOfEvents => this._waitForEventPromise(
                     `${agentName}.${eventName}`, numOfEvents || 1);
        return listener => this._eventHandlers.set(`${agentName}.${eventName}`, listener);
      }
    })});
  }

  _dispatchMessage(messageString) {
    var messageObject = JSON.parse(messageString);
    if (InspectorTest._dumpInspectorProtocolMessages)
      utils.print("backend: " + JSON.stringify(messageObject));
    try {
      var messageId = messageObject["id"];
      if (typeof messageId === "number") {
        var handler = this._dispatchTable.get(messageId);
        if (handler) {
          handler(messageObject);
          this._dispatchTable.delete(messageId);
        }
      } else {
        var eventName = messageObject["method"];
        var eventHandler = this._eventHandlers.get(eventName);
        if (this._scriptMap && eventName === "Debugger.scriptParsed")
          this._scriptMap.set(messageObject.params.scriptId, JSON.parse(JSON.stringify(messageObject.params)));
        if (eventName === "Debugger.scriptParsed" && messageObject.params.url === "wait-for-pending-tasks.js")
          return;
        if (eventHandler)
          eventHandler(messageObject);
      }
    } catch (e) {
      InspectorTest.log("Exception when dispatching message: " + e + "\n" + e.stack + "\n message = " + JSON.stringify(messageObject, null, 2));
      InspectorTest.completeTest();
    }
  };

  _waitForEventPromise(eventName, numOfEvents) {
    let events = [];
    return new Promise(fulfill => {
      this._eventHandlers.set(eventName, result => {
        --numOfEvents;
        events.push(result);
        if (numOfEvents === 0) {
          delete this._eventHandlers.delete(eventName);
          fulfill(events.length > 1 ? events : events[0]);
        }
      });
    });
  }
};

InspectorTest.runTestSuite = function(testSuite) {
  function nextTest() {
    if (!testSuite.length) {
      InspectorTest.completeTest();
      return;
    }
    var fun = testSuite.shift();
    InspectorTest.log("\nRunning test: " + fun.name);
    fun(nextTest);
  }
  nextTest();
}

InspectorTest.runAsyncTestSuite = async function(testSuite) {
  for (var test of testSuite) {
    InspectorTest.log("\nRunning test: " + test.name);
    try {
      await test();
    } catch (e) {
      utils.print(e.stack);
    }
  }
  InspectorTest.completeTest();
}

InspectorTest.start = function(description) {
  try {
    InspectorTest.log(description);
    var contextGroup = new InspectorTest.ContextGroup();
    var session = contextGroup.connect();
    return { session: session, contextGroup: contextGroup, Protocol: session.Protocol };
  } catch (e) {
    utils.print(e.stack);
  }
}
