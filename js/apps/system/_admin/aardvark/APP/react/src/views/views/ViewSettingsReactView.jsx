import { cloneDeep, omit } from 'lodash';
import React, { useEffect, useReducer, useRef, useState } from 'react';
import useSWR from 'swr';
import { ArangoTable } from '../../components/arango/table';
import { getApiRouteForCurrentDB } from '../../utils/arangoClient';
import { getReducer, isAdminUser as userIsAdmin, usePermissions } from '../../utils/helpers';
import ConsolidationPolicyForm from './forms/ConsolidationPolicyForm';
import { buildSubNav, postProcessor } from './helpers';

const ViewSettingsReactView = ({ name }) => {
  const view = useRef({ name });
  const fullView = useRef(null);
  const initialState = useRef({
    formState: cloneDeep(view),
    formCache: cloneDeep(view)
  });
  const [state, dispatch] = useReducer(getReducer(initialState.current, postProcessor),
    initialState.current);
  const { data } = useSWR(`/view/${name}/properties`,
    path => getApiRouteForCurrentDB().get(path));
  const { data: permData } = usePermissions();
  const [isAdminUser, setIsAdminUser] = useState(false);

  useEffect(() => {
    fullView.current = data
      ? omit(data.body, 'error', 'code')
      : view.current;

    initialState.current.formCache = cloneDeep(fullView.current);

    dispatch({
      type: 'setFormState',
      formState: cloneDeep(fullView.current)
    });
  }, [data, view]);

  useEffect(() => {
    buildSubNav(isAdminUser, name, 'Settings');
  }, [isAdminUser, name]);

  if (fullView.current && permData) {
    console.log(fullView.current);

    const tempIsAdminUser = userIsAdmin(permData);
    if (tempIsAdminUser !== isAdminUser) { // Prevents an infinite render loop.
      setIsAdminUser(tempIsAdminUser);
    }

    const formState = state.formState;
    const currentView = fullView.current;

    return <div className={'centralContent'} id={'content'}>
      <div id={'modal-dialog'} className={'createModalDialog'} tabIndex={-1} role={'dialog'}
           aria-labelledby={'myModalLabel'} aria-hidden={'true'}>
        <div className="modal-body">
          <div>
            <div style={{
              color: '#717d90',
              fontWeight: 600,
              fontSize: '12.5pt',
              padding: 10,
              borderBottom: '1px solid rgba(0, 0, 0, .3)'
            }}>
              General
            </div>
            <ArangoTable id={'viewSettingsTable'}>
              <tbody>
              <tr>
                <th className="collectionInfoTh2">Cleanup Interval Step:</th>
                <th className="collectionInfoTh">
                  <div className="modal-text">
                    {currentView.cleanupIntervalStep}
                  </div>
                </th>
                <th className="tooltipInfoTh"/>
              </tr>

              <tr>
                <th className="collectionInfoTh2">Commit Interval (msec):</th>
                <th className="collectionInfoTh">
                  <div className="modal-text">
                    {currentView.commitIntervalMsec}
                  </div>
                </th>
                <th className="tooltipInfoTh"/>
              </tr>

              <tr>
                <th className="collectionInfoTh2">ID:</th>
                <th className="collectionInfoTh">
                  <div className="modal-text">
                    {currentView.id}
                  </div>
                </th>
                <th className="tooltipInfoTh"/>
              </tr>

              <tr>
                <th className="collectionInfoTh2">Globally Unique ID:</th>
                <th className="collectionInfoTh">
                  <div className="modal-text">
                    {currentView.globallyUniqueId}
                  </div>
                </th>
                <th className="tooltipInfoTh"/>
              </tr>

              <tr>
                <th className="collectionInfoTh2">Type:</th>
                <th className="collectionInfoTh">
                  <div className="modal-text">
                    {currentView.type}
                  </div>
                </th>
                <th className="tooltipInfoTh"/>
              </tr>
              </tbody>
            </ArangoTable>

            <div style={{
              color: '#717d90',
              fontWeight: 600,
              fontSize: '12.5pt',
              padding: 10,
              borderBottom: '1px solid rgba(0, 0, 0, .3)',
              borderTop: '1px solid rgba(0, 0, 0, .3)'
            }}>
              Consolidation
            </div>
            <ArangoTable id={'viewSettingsTable'}>
              <tbody>
              <tr>
                <th className="collectionInfoTh2" colSpan={3}>
                  <ConsolidationPolicyForm formState={formState} dispatch={dispatch}
                                           disabled={!isAdminUser}/>
                </th>
              </tr>
              </tbody>
            </ArangoTable>
          </div>
        </div>
      </div>
    </div>;
  }

  return <h1>View: {name}</h1>;
};

window.ViewSettingsReactView = ViewSettingsReactView;
