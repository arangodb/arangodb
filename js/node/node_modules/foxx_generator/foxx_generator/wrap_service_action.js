(function () {
  'use strict';
  var wrapEntity,
    wrapRepository,
    selectByStateType,
    wrapServiceAction;

  wrapEntity = function (state) {
    return function (req) {
      var id = req.params('id');
      return {
        superstate: {
          repository: state.repository,
          entity: state.repository.byId(id)
        }
      };
    };
  };

  wrapRepository = function (state) {
    return function () {
      return {
        superstate: {
          repository: state.repository
        }
      };
    };
  };

  selectByStateType = function (state) {
    var wrapper;

    if (!state) {
      return function () { return {}; };
    }

    if (state.type === 'entity') {
      wrapper = wrapEntity;
    } else if (state.type === 'repository') {
      wrapper = wrapRepository;
    }

    return wrapper(state);
  };

  wrapServiceAction = function (serviceState) {
    return function (req, res) {
      serviceState.action(req, res, selectByStateType(serviceState.superstate)(req));
    };
  };

  exports.wrapServiceAction = wrapServiceAction;
}());
