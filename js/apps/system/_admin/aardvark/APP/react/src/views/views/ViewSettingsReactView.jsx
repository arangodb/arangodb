/* global frontendConfig */

import { cloneDeep, isEqual, uniqueId } from 'lodash';
import React, { useEffect, useReducer, useRef, useState } from 'react';
import "./split-pane-styles.css";
import "./viewsheader.css";
import SplitPane from "react-split-pane";
import useSWR from 'swr';
import { Cell } from '../../components/pure-css/grid';
import { getApiRouteForCurrentDB } from '../../utils/arangoClient';
import { HashRouter, Route, Switch } from 'react-router-dom';
import LinkList from './Components/LinkList';
import { ViewContext } from './constants';
import LinkPropertiesForm from './forms/LinkPropertiesForm';
import CopyFromInput from './forms/inputs/CopyFromInput';
import Textbox from '../../components/pure-css/form/Textbox';
import {
  getReducer, isAdminUser as userIsAdmin,
  usePermissions
} from '../../utils/helpers';
import { DeleteButton, SaveButton } from './Actions';
import ConsolidationPolicyForm from './forms/ConsolidationPolicyForm';
import { postProcessor, useView, useDisableNavBar } from './helpers';
import AccordionView from './Components/Accordion/Accordion';
import { ViewRightPane } from './ViewRightPane.tsx';
import { GeneralContent } from './GeneralContent.tsx';
import { StoredValuesContent } from './StoredValuesContent.tsx';
import { PrimarySortContent, PrimarySortTitle } from './PrimarySortContent.tsx';

const ViewSettingsReactView = ({ name }) => {
  useDisableNavBar();
  const [editName, setEditName] = useState(false);

  const handleEditName = () => {
    setEditName(true);
  };

  const closeEditName = () => {
    setEditName(false);
  };

  const EditableViewName = () => {
    return (<div>
      {!editName ? (
        <>
          <div style={{
            color: '#717d90',
            fontWeight: 600,
            fontSize: '12.5pt',
            padding: 10,
            float: 'left'
          }}>
            {formState.name}
          </div>
          {!frontendConfig.isCluster ?
            <i className="fa fa-edit" onClick={handleEditName} style={{paddingTop: '14px'}}></i> :
            null}
          </>
      ) : (
        <>
          <Textbox type={'text'} value={formState.name} onChange={updateName}
              required={true} disabled={nameEditDisabled}/> <i className="fa fa-check" onClick={closeEditName} style={{paddingTop: '14px'}}></i>
        </>
      )}
      </div>
    )
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
      <div className="viewsstickyheader">
        <EditableViewName />
        {
          isAdminUser && views.length
            ? <Cell size={'1'} style={{ paddingLeft: 10 }}>
                <div style={{display: 'flex'}}>
                  <div style={{flex: 1, padding: '10'}}>
                    <CopyFromInput views={views} dispatch={dispatch} formState={formState}/>
                  </div>
                  <div style={{flex: 1, padding: '10'}}>
                    {
                      isAdminUser && changed
                      ?
                        <SaveButton view={formState} oldName={name} menu={'json'} setChanged={setChanged}/>
                      : <SaveButton view={formState} oldName={name} menu={'json'} setChanged={setChanged} disabled/>
                    }
                    <DeleteButton view={formState}
                              modalCid={`modal-content-delete-${formState.globallyUniqueId}`}/>
                  </div>
                </div>
            </Cell>
            : null
        }
      </div>
      <section>
        <SplitPane
          paneStyle={{ overflow: 'scroll' }}
          maxSize={800}
          defaultSize={parseInt(localStorage.getItem('splitPos'), 10)}
          onChange={(size) => localStorage.setItem('splitPos', size)}
          style={{ paddingTop: '15px', marginTop: '10px', marginLeft: '15px', marginRight: '15px' }}>
          <div style={{ marginRight: '15px' }}>
            <AccordionView
              allowMultipleOpen
              accordionConfig={[
                {
                  index: 0,
                  content: <div>
                    <Switch>
                      <Route path={'/:link'}>
                        <LinkList name={name}/>
                        <LinkPropertiesForm name={name}/>
                      </Route>
                      <Route exact path={'/'}>
                        <LinkList name={name}/>
                      </Route>
                    </Switch>
                  </div>,
                  label: "Links",
                  testID: "accordionItem0",
                  defaultActive: true
                },
                {
                  index: 1,
                  content: (
                    <div>
                      <GeneralContent
                        formState={formState}
                        dispatch={dispatch}
                        isAdminUser={isAdminUser} 
                      />
                    </div>
                  ),
                  label: "General",
                  testID: "accordionItem1"
                },
                {
                  index: 2,
                  content: (
                    <div>
                      <ConsolidationPolicyForm formState={formState} dispatch={dispatch}
                                          disabled={!isAdminUser}/>
                    </div>
                  ),
                  label: "Consolidation Policy",
                  testID: "accordionItem2"
                },
                {
                  index: 3,
                  content: <div><PrimarySortContent  formState={formState} /></div>,
                  label: <PrimarySortTitle formState={formState} />,
                  testID: "accordionItem3"
                },
                {
                  index: 4,
                  content: <div><StoredValuesContent formState={formState} /></div>,
                  label: "Stored Values",
                  testID: "accordionItem4"
                }
              ]}
            />
          </div>
          <ViewRightPane isAdminUser={isAdminUser} formState={formState} dispatch={dispatch} state={state} />
        </SplitPane>
      </section>
      </HashRouter>
      </ViewContext.Provider>;
};
window.ViewSettingsReactView = ViewSettingsReactView;


