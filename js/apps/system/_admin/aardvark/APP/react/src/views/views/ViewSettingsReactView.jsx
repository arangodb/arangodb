/* global frontendConfig */

import { cloneDeep } from 'lodash';
import React, { useEffect, useReducer, useRef, useState } from 'react';
import Textbox from '../../components/pure-css/form/Textbox';
import {
  getNumericFieldSetter, getNumericFieldValue, getReducer, isAdminUser as userIsAdmin,
  usePermissions
} from '../../utils/helpers';
import { DeleteButton, SaveButton } from './Actions';
import { buildSubNav, postProcessor, useView } from './helpers';

const ViewSettingsReactView = ({ name }) => {
  const initialState = useRef({
    formState: { name },
    formCache: { name }
  });
  const [state, dispatch] = useReducer(getReducer(initialState.current, postProcessor),
    initialState.current);
  const view = useView(name);
  const permissions = usePermissions();
  const [isAdminUser, setIsAdminUser] = useState(false);

  useEffect(() => {
    initialState.current.formCache = cloneDeep(view);

    dispatch({
      type: 'setFormState',
      formState: view
    });
  }, [view, name]);

  useEffect(() => {
    buildSubNav(isAdminUser, name, 'Settings');
  }, [isAdminUser, name]);

  const updateName = (event) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'name',
        value: event.target.value
      }
    });
  };

  const tempIsAdminUser = userIsAdmin(permissions);
  if (tempIsAdminUser !== isAdminUser) { // Prevents an infinite render loop.
    setIsAdminUser(tempIsAdminUser);
  }

  const formState = state.formState;

  return <div className={'centralContent'} id={'content'}>
    <div id={'modal-dialog'} className={'createModalDialog'} tabIndex={-1} role={'dialog'}
         aria-labelledby={'myModalLabel'} aria-hidden={'true'}>
      <div className="modal-body">
        <div className={'tab-content'}>
          <div className="tab-pane tab-pane-modal active" id="General">
            <table>
              <tbody>
              <tr className="tableRow" id="row_change-view-name">
                <th className="collectionTh">
                  Name*:
                </th>
                <th className="collectionTh">
                  <Textbox type={'text'} value={formState.name} onChange={updateName}
                           required={true} disabled={frontendConfig.isCluster || !isAdminUser}/>
                </th>
              </tr>

              <tr className="tableRow" id="row_change-view-cleanupIntervalStep">
                <th className="collectionTh">
                  Cleanup Interval Step:
                </th>
                <th className="collectionTh">
                  <Textbox type={'number'} disabled={!isAdminUser}
                           value={getNumericFieldValue(formState.cleanupIntervalStep)}
                           onChange={getNumericFieldSetter('cleanupIntervalStep', dispatch)}/>
                </th>
              </tr>

              <tr className="tableRow" id="row_change-view-commitIntervalMsec">
                <th className="collectionTh">
                  Commit Interval (msec):
                </th>
                <th className="collectionTh">
                  <Textbox type={'number'} disabled={!isAdminUser}
                           value={getNumericFieldValue(formState.commitIntervalMsec)}
                           onChange={getNumericFieldSetter('commitIntervalMsec', dispatch)}/>
                </th>
              </tr>

              <tr className="tableRow" id="row_change-view-id">
                <th className="collectionTh">
                  ID:
                </th>
                <th className="collectionTh">
                  <div className="modal-text" id="change-view-id">
                    {formState.id}
                  </div>
                </th>
              </tr>

              <tr className="tableRow" id="row_change-view-globallyUniqueId">
                <th className="collectionTh">
                  Globally Unique ID:
                </th>
                <th className="collectionTh">
                  <div className="modal-text" id="change-view-globallyUniqueId">
                    {formState.globallyUniqueId}
                  </div>
                </th>
              </tr>

              <tr className="tableRow" id="row_change-view-type">
                <th className="collectionTh">
                  Type:
                </th>
                <th className="collectionTh">
                  <div className="modal-text" id="change-view-type">
                    {formState.type}
                  </div>
                </th>
              </tr>
              </tbody>
            </table>
          </div>
        </div>
      </div>
      <div className="modal-footer">
        <DeleteButton view={formState}
                      modalCid={`modal-content-delete-${formState.globallyUniqueId}`}/>
        <SaveButton view={formState} oldName={name}/>
      </div>
    </div>
  </div>;
};

window.ViewSettingsReactView = ViewSettingsReactView;
