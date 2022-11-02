import { cloneDeep } from 'lodash';
import React, { useEffect, useReducer, useRef, useState } from 'react';
import { HashRouter, Route, Switch } from 'react-router-dom';
import { getReducer, isAdminUser as userIsAdmin, usePermissions } from '../../utils/helpers';
import LinkList from './Components/LinkList';
import { ViewContext } from './constants';
import LinkPropertiesForm from './forms/LinkPropertiesForm';
import { postProcessor, useNavbar, useView } from './helpers';

const ViewLinksReactView = ({ name }) => {
  const initialState = useRef({
    formState: { name },
    formCache: { name }
  });
  const view = useView(name);
  const [changed, setChanged] = useState(!!window.sessionStorage.getItem(`${name}-changed`));
  const [state, dispatch] = useReducer(
    getReducer(initialState.current, postProcessor, setChanged, name),
    initialState.current
  );
  const permissions = usePermissions();
  const [isAdminUser, setIsAdminUser] = useState(false);

  useEffect(() => {
    initialState.current.formCache = cloneDeep(view);

    dispatch({
      type: 'initFormState',
      formState: view
    });
  }, [view]);

  useNavbar(name, isAdminUser, changed, 'Links');

  const tempIsAdminUser = userIsAdmin(permissions);
  if (tempIsAdminUser !== isAdminUser) {
    // Prevents an infinite render loop.
    setIsAdminUser(tempIsAdminUser);
  }

  const formState = state.formState;

  return <ViewContext.Provider
    value={{
      formState,
      dispatch,
      isAdminUser,
      changed,
      setChanged
    }}
  >
    <HashRouter basename={`view/${name}/links`} hashType={'noslash'}>
      <Switch>
        <Route path={'/:link'}>
          <LinkPropertiesForm name={name}/>
        </Route>
        <Route exact path={'/'}>
          <LinkList name={name}/>
        </Route>
      </Switch>
    </HashRouter>
  </ViewContext.Provider>;
};

window.ViewLinksReactView = ViewLinksReactView;
