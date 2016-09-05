/* jshint strict: false */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

const arangodb = require('@arangodb');
const actions = require('@arangodb/actions');
const users = require('@arangodb/users');
const db = require('@arangodb').db;

// check if user is an administrator (aka member of _system)
function needSystemUser (req, res) {
  const user = req.user;

  // authentication disabled
  if (user === null) {
    return true;
  }

  const allowed = users.permission(user, '_system') === 'rw';

  if (!allowed) {
    actions.resultError(req, res, actions.HTTP_FORBIDDEN,
      String('you need administrator privileges'));
  }

  return allowed;
}

// check if user is asking infos for itself (or is an administrator)
function needMyself (req, res, username) {
  const user = req.user;

  // authentication disabled
  if (user === null) {
    return true;
  }

  let allowed = (user === username);

  if (!allowed) {
    allowed = (users.permission(user, '_system') === 'rw');
  }

  if (!allowed) {
    actions.resultError(req, res, actions.HTTP_FORBIDDEN,
      String('you cannot access other users'));
  }

  return allowed;
}

// handle exception
function handleException (req, res, err) {
  if (err.errorNum === arangodb.errors.ERROR_USER_NOT_FOUND.code) {
    actions.resultNotFound(req, res, arangodb.errors.ERROR_USER_NOT_FOUND.code);
  } else {
    throw err;
  }
}

// GET /_api/user | GET /_api/user/<username>
function get_api_user (req, res) {
  if (req.suffix.length === 0) {
    if (needSystemUser(req, res)) {
      actions.resultOk(req, res, actions.HTTP_OK, {
        result: users.all()
      });
    }

    return;
  }

  const user = decodeURIComponent(req.suffix[0]);

  try {
    if (needMyself(req, res, user)) {
      actions.resultOk(req, res, actions.HTTP_OK, users.document(user));
    }
  } catch (err) {
    handleException(req, res, err);
  }
}

// GET /_api/user/<username>/database | GET /_api/user/<username>/database/<dbname>
function get_api_database (req, res, key) {
  const user = decodeURIComponent(req.suffix[0]);

  try {
    if (needMyself(req, res, user)) {
      actions.resultOk(req, res, actions.HTTP_OK, {
        result: users.permission(user, key)
      });
    }
  } catch (err) {
    handleException(req, res, err);
  }
}

// GET /_api/user/<username>/config | GET /_api/user/<username>/config/<key>
function get_api_config (req, res, key) {
  const user = decodeURIComponent(req.suffix[0]);

  try {
    if (needMyself(req, res, user)) {
      actions.resultOk(req, res, actions.HTTP_OK, {
        result: users.configData(user, key)
      });
    }
  } catch (err) {
    handleException(req, res, err);
  }
}

// GET /_api/user/...
function get_api_user_request (req, res) {
  const oldDbname = db._name();
  db._useDatabase('_system');

  try {
    if (req.suffix.length === 0) {
      get_api_user(req, res);
    } else if (req.suffix.length === 1) {
      get_api_user(req, res);
    } else if (req.suffix.length === 2) {
      if (req.suffix[1] === 'config') {
        get_api_config(req, res, null);
      } else if (req.suffix[1] === 'database') {
        get_api_database(req, res, null);
      } else {
        actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
      }
    } else if (req.suffix.length === 3) {
      if (req.suffix[1] === 'config') {
        get_api_config(req, res, req.suffix[2]);
      } else {
        actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
      }
    } else {
      actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    }
  } finally {
    db._useDatabase(oldDbname);
  }
}

// POST /_api/user | POST /_api/user/<username>
function post_api_user (req, res) {
  const json = actions.getJsonBody(req, res, actions.HTTP_BAD);

  if (json === undefined) {
    return;
  }

  // validate if a combination or username / password is valid
  if (req.suffix.length === 1) {
    const user = decodeURIComponent(req.suffix[0]);
    const result = users.isValid(user, json.passwd);

    if (result) {
      actions.resultOk(req, res, actions.HTTP_OK, {
        result: true
      });
    } else {
      actions.resultNotFound(req, res, arangodb.errors.ERROR_USER_NOT_FOUND.code);
    }
    return;
  }

  if (needSystemUser(req, res)) {
    const user = json.user;
    const doc = users.save(user, json.passwd, json.active, json.extra,
      json.changePassword);

    if (json.passwordToken) {
      users.setPasswordToken(user, json.passwordToken);
    }

    users.reload();

    actions.resultOk(req, res, actions.HTTP_CREATED, doc);
  }
}

// POST /_api/user/...
function post_api_user_request (req, res) {
  const oldDbname = db._name();
  db._useDatabase('_system');

  try {
    if (req.suffix.length < 2) {
      post_api_user(req, res);
    } else {
      actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    }
  } finally {
    db._useDatabase(oldDbname);
  }
}

// PUT /_api/user/<username>
function put_api_user (req, res) {
  const user = decodeURIComponent(req.suffix[0]);
  const json = actions.getJsonBody(req, res, actions.HTTP_BAD);

  if (json === undefined) {
    return;
  }

  try {
    const isActive = users.document(user).active;

    if (isActive) {
      if (needMyself(req, res, user)) {
        actions.resultOk(req, res, actions.HTTP_OK,
          users.replace(user, json.passwd, json.active, json.extra));
      }
    } else {
      if (needSystemUser(req, res, user)) {
        actions.resultOk(req, res, actions.HTTP_OK,
          users.replace(user, json.passwd, json.active, json.extra));
      }
    }

    users.reload();
  } catch (err) {
    handleException(req, res, err);
  }
}

// PUT /_api/user/<username>/database/<dbname>
function put_api_permission (req, res) {
  const user = decodeURIComponent(req.suffix[0]);
  const dbname = decodeURIComponent(req.suffix[2]);
  const json = actions.getJsonBody(req, res, actions.HTTP_BAD);

  if (json === undefined) {
    return;
  }

  try {
    let doc;

    if (json.grant === 'rw' || json.grant === 'ro') {
      doc = users.grantDatabase(user, dbname, json.grant);
    } else {
      doc = users.revokeDatabase(user, dbname, json.grant);
    }

    users.reload();

    actions.resultOk(req, res, actions.HTTP_OK, doc);
  } catch (err) {
    handleException(req, res, err);
  }
}

// PUT /_api/user/<username>/config/<key>
function put_api_config (req, res) {
  const user = decodeURIComponent(req.suffix[0]);
  const key = decodeURIComponent(req.suffix[2]);
  const json = actions.getJsonBody(req, res, actions.HTTP_BAD);

  if (json === undefined) {
    return;
  }

  try {
    if (needMyself(req, res, user)) {
      users.updateConfigData(user, key, json.value);

      actions.resultOk(req, res, actions.HTTP_OK, {});
    }
  } catch (err) {
    handleException(req, res, err);
  }
}

// PUT /_api/user/...
function put_api_user_request (req, res) {
  const oldDbname = db._name();
  db._useDatabase('_system');

  try {
    if (req.suffix.length === 1) {
      put_api_user(req, res);
    } else if (req.suffix.length === 3) {
      if (req.suffix[1] === 'database') {
        put_api_permission(req, res);
      } else if (req.suffix[1] === 'config') {
        put_api_config(req, res);
      } else {
        actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
      }
    } else {
      actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    }
  } finally {
    db._useDatabase(oldDbname);
  }
}

// PATCH /_api/user/<username>
function patch_api_user (req, res) {
  const user = decodeURIComponent(req.suffix[0]);
  const json = actions.getJsonBody(req, res, actions.HTTP_BAD);

  if (json === undefined) {
    return;
  }

  try {
    const isActive = users.document(user).active;

    if (isActive) {
      if (needMyself(req, res, user)) {
        actions.resultOk(req, res, actions.HTTP_OK,
          users.update(user, json.passwd, json.active, json.extra));
      }
    } else {
      if (needSystemUser(req, res, user)) {
        actions.resultOk(req, res, actions.HTTP_OK,
          users.update(user, json.passwd, json.active, json.extra));
      }
    }

    users.reload();
  } catch (err) {
    handleException(req, res, err);
  }
}

// PATCH /_api/user/...
function patch_api_user_request (req, res) {
  const oldDbname = db._name();
  db._useDatabase('_system');

  try {
    if (req.suffix.length === 1) {
      patch_api_user(req, res);
    } else {
      actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    }
  } finally {
    db._useDatabase(oldDbname);
  }
}

// DELETE /_api/user/<username>
function delete_api_user (req, res) {
  const user = decodeURIComponent(req.suffix[0]);

  try {
    if (needSystemUser(req, res)) {
      users.remove(user);
      users.reload();

      actions.resultOk(req, res, actions.HTTP_ACCEPTED, {});
    }
  } catch (err) {
    handleException(req, res, err);
  }
}

// DELETE /_api/user/<username>/database/<dbname>
function delete_api_permission (req, res) {
  const user = decodeURIComponent(req.suffix[0]);
  const dbname = decodeURIComponent(req.suffix[2]);

  try {
    users.revokeDatabase(user, dbname);
    users.reload();

    actions.resultOk(req, res, actions.HTTP_ACCEPTED, {});
  } catch (err) {
    handleException(req, res, err);
  }
}

// DELETE /_api/user/<username>/config/<key>
function delete_api_config (req, res, key) {
  const user = decodeURIComponent(req.suffix[0]);

  try {
    if (needMyself(req, res, user)) {
      users.updateConfigData(user, key);

      actions.resultOk(req, res, actions.HTTP_ACCEPTED, {});
    }
  } catch (err) {
    handleException(req, res, err);
  }
}

// DELETE /_api/user/...
function delete_api_user_request (req, res) {
  const oldDbname = db._name();
  db._useDatabase('_system');

  try {
    if (req.suffix.length >= 1) {
      if (req.suffix.length === 1) {
        delete_api_user(req, res);
      } else if (req.suffix.length === 2) {
        if (req.suffix[1] === 'config') {
          delete_api_config(req, res, null);
        } else {
          actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
        }
      } else if (req.suffix.length === 3) {
        if (req.suffix[1] === 'database') {
          delete_api_permission(req, res);
        } else if (req.suffix[1] === 'config') {
          delete_api_config(req, res, req.suffix[2]);
        } else {
          actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
        }
      } else {
        actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
      }
    } else {
      actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    }
  } finally {
    db._useDatabase(oldDbname);
  }
}

// all api calls
actions.defineHttp({
  url: '_api/user',
  allowUseDatabase: true,

  callback: function (req, res) {
    try {
      switch (req.requestType) {
        case actions.GET:
          get_api_user_request(req, res);
          break;

        case actions.POST:
          post_api_user_request(req, res);
          break;

        case actions.PUT:
          put_api_user_request(req, res);
          break;

        case actions.PATCH:
          patch_api_user_request(req, res);
          break;

        case actions.DELETE:
          delete_api_user_request(req, res);
          break;

        default:
          actions.resultUnsupported(req, res);
      }
    } catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});
