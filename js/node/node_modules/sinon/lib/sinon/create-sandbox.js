"use strict";

const arrayProto = require("@sinonjs/commons").prototypes.array;
const Sandbox = require("./sandbox");

const forEach = arrayProto.forEach;
const push = arrayProto.push;

function prepareSandboxFromConfig(config) {
    const sandbox = new Sandbox({ assertOptions: config.assertOptions });

    if (config.useFakeTimers) {
        if (typeof config.useFakeTimers === "object") {
            sandbox.useFakeTimers(config.useFakeTimers);
        } else {
            sandbox.useFakeTimers();
        }
    }

    return sandbox;
}

function exposeValue(sandbox, config, key, value) {
    if (!value) {
        return;
    }

    if (config.injectInto && !(key in config.injectInto)) {
        config.injectInto[key] = value;
        push(sandbox.injectedKeys, key);
    } else {
        push(sandbox.args, value);
    }
}

/**
 * Options to customize a sandbox
 *
 * The sandbox's methods can be injected into another object for
 * convenience. The `injectInto` configuration option can name an
 * object to add properties to.
 *
 * @typedef {object} SandboxConfig
 * @property {string[]} properties The properties of the API to expose on the sandbox. Examples: ['spy', 'fake', 'restore']
 * @property {object} injectInto an object in which to inject properties from the sandbox (a facade). This is mostly an integration feature (sinon-test being one).
 * @property {boolean} useFakeTimers  whether timers are faked by default
 * @property {object} [assertOptions] see CreateAssertOptions in ./assert
 *
 * This type def is really suffering from JSDoc not having standardized
 * how to reference types defined in other modules :(
 */

/**
 * A configured sinon sandbox (private type)
 *
 * @typedef {object} ConfiguredSinonSandboxType
 * @private
 * @augments Sandbox
 * @property {string[]} injectedKeys the keys that have been injected (from config.injectInto)
 * @property {*[]} args the arguments for the sandbox
 */

/**
 * Create a sandbox
 *
 * As of Sinon 5 the `sinon` instance itself is a Sandbox, so you
 * hardly ever need to create additional instances for the sake of testing
 *
 * @param config {SandboxConfig}
 * @returns {Sandbox}
 */
function createSandbox(config) {
    if (!config) {
        return new Sandbox();
    }

    const configuredSandbox = prepareSandboxFromConfig(config);
    configuredSandbox.args = configuredSandbox.args || [];
    configuredSandbox.injectedKeys = [];
    configuredSandbox.injectInto = config.injectInto;
    const exposed = configuredSandbox.inject({});

    if (config.properties) {
        forEach(config.properties, function (prop) {
            const value =
                exposed[prop] || (prop === "sandbox" && configuredSandbox);
            exposeValue(configuredSandbox, config, prop, value);
        });
    } else {
        exposeValue(configuredSandbox, config, "sandbox");
    }

    return configuredSandbox;
}

module.exports = createSandbox;
