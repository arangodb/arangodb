# Creating a Foxx script

Based on our knowledge of [making requests](MakingRequests.html) we can integrate with third-party web applications by writing our own Foxx scripts.

## Problem

I want to send a message to my [Gitter chat](https://gitter.im) from the ArangoDB web interface, the ArangoDB HTTP API or the `foxx-manager` CLI.

## Solution

Go to [Gitter](https://gitter.im) and sign up for a free account there. Create a room and click on "Configure your integrations" in the side bar and select "Custom". Note down the URL (it should look something like this: `https://webhooks.gitter.im/e/b4dc0ff33b4b3`).

We may want to be able to send messages to Gitter in other apps too, so let's create a new Foxx app we call `notify-gitter`. We'll only add a single JavaScript file containing an export called `webhook` and we want to allow configuring our Foxx app with the webhook URL, so our manifest should look similar to this:

```json
{
  "name": "notify-gitter",
  "version": "1.0.0",
  "scripts": {
    "webhook": "webhook.js"
  },
  "configuration": {
    "url": {
      "description": "Gitter webhook URL.",
      "type": "string",
      "default": "https://webhooks.gitter.im/e/123"
    }
  }
}
```

Next we'll create the new script called `webhook`. Foxx scripts are regular JavaScript modules that have access to the `applicationContext` and receive their input from the `applicationContext.argv` array and return their output via `module.exports`.

```js
'use strict';
var request = require('org/arangodb/request');
var util = require('util');
var data = applicationContext.argv[0];

var response = request.post(applicationContext.configuration.url, {
  form: data
});

if (response.status >= 400) {
  throw new Error(util.format(
    'Server returned error code %s with message: %s',
    response.status,
    response.body
  ));
}

module.exports = response.body;
```

We can now install the new Foxx app and mount it alongside our existing app. Make sure to swap out the default webhook URL with the one you received from Gitter before you proceed by opening the app in the application list of the ArangoDB web interface and finding the configuration icon in the toolbar.

To run the script, pick the "webhook" script from the scripts icon in the toolbar and enter the payload for the webhook as JSON, e.g. `{"message": "Hello from Foxx!"}`. Once you submit the payload, the script will execute immediately and you will be shown the response body or error returned by the script.

**Author**: [Alan Plum](https://github.com/pluma)

**Tags**: #foxx
