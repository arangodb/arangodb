/* global frontendConfig */

import { cloneDeep, isEqual, uniqueId } from 'lodash';
import React, { useEffect, useReducer, useRef, useState } from 'react';
import "./split-pane-styles.css";
import "./viewsheader.css";
import useSWR from 'swr';
import { getApiRouteForCurrentDB } from '../../utils/arangoClient';
import { HashRouter } from 'react-router-dom';
import { ViewContext } from './constants';
import {
  getReducer, isAdminUser as userIsAdmin,
  usePermissions
} from '../../utils/helpers';
import { postProcessor, useView, useDisableNavBar } from './helpers';
import { ViewSection } from './ViewSection.tsx';
import { ViewHeader } from './ViewHeader.tsx';
import { ChakraProvider } from '@chakra-ui/react';

const ViewSettingsReactView = ({ name }) => {
  useDisableNavBar();
  const [editName, setEditName] = useState(false);

  const handleEditName = () => {
    setEditName(true);
  };

  const closeEditName = () => {
    setEditName(false);
  };


  const initialState = useRef({
    formState: { name },
    formCache: { name },
    renderKey: uniqueId('force_re-render_')
  });
  const view = useView(name);
  const [changed, setChanged] = useState(!!window.sessionStorage.getItem(`${name}-changed`));
  const [state, dispatch] = useReducer(
    getReducer(initialState.current, postProcessor, setChanged, name),
    initialState.current);
  const permissions = usePermissions();
  const [isAdminUser, setIsAdminUser] = useState(false);
  const { data } = useSWR(isAdminUser ? '/view' : null,
    (path) => getApiRouteForCurrentDB().get(path));
  const [views, setViews] = useState([]);

  useEffect(() => {
    initialState.current.formCache = cloneDeep(view);

    dispatch({
      type: 'initFormState',
      formState: view
    });
    dispatch({
      type: 'regenRenderKey'
    });
  }, [view, name]);

  const updateName = (event) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'name',
        value: event.target.value
      }
    });
  };

  
  useEffect(()=> {
    const tempIsAdminUser = userIsAdmin(permissions);
    if (tempIsAdminUser !== isAdminUser) {
      setIsAdminUser(tempIsAdminUser);
      dispatch({
        type: 'regenRenderKey'
      });
    }
  }, [isAdminUser, permissions]);


  const formState = state.formState;
  const nameEditDisabled = frontendConfig.isCluster || !isAdminUser;
  if (data) {
    if (!isEqual(data.body.result, views)) {
      setViews(data.body.result);
    }
  }
  return <ViewContext.Provider
                  value={{
                    formState,
                    dispatch,
                    isAdminUser,
                    changed,
                    setChanged
                  }}
                >
      <HashRouter basename={`view/${name}`} hashType={'noslash'}>
        <ChakraProvider>
          <ViewHeader
            editName={editName}
            formState={formState}
            handleEditName={handleEditName}
            updateName={updateName}
            nameEditDisabled={nameEditDisabled}
            closeEditName={closeEditName}
            isAdminUser={isAdminUser}
            views={views}
            dispatch={dispatch}
            changed={changed}
            name={name}
            setChanged={setChanged}
          />
          <ViewSection
            name={name}
            formState={formState}
            dispatch={dispatch}
            isAdminUser={isAdminUser}
            state={state}
          />
        </ChakraProvider>
      </HashRouter>
      </ViewContext.Provider>;
};
window.ViewSettingsReactView = ViewSettingsReactView;

