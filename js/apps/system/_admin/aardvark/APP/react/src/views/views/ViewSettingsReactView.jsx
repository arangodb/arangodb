/* global frontendConfig */

import { cloneDeep } from 'lodash';
import React, { useEffect, useReducer, useRef, useState } from 'react';
import ToolTip from '../../components/arango/tootip';
import Textbox from '../../components/pure-css/form/Textbox';
import {
  getNumericFieldSetter, getNumericFieldValue, getReducer, isAdminUser as userIsAdmin,
  usePermissions
} from '../../utils/helpers';
import { DeleteButton, SaveButton } from './Actions';
import { postProcessor, useNavbar, useView } from './helpers';

const ViewSettingsReactView = ({ name }) => {
  const initialState = useRef({
    formState: { name },
    formCache: { name }
  });
  const view = useView(name);
  const [changed, setChanged] = useState(!!window.sessionStorage.getItem(`${name}-changed`));
  const [state, dispatch] = useReducer(
    getReducer(initialState.current, postProcessor, setChanged, name),
    initialState.current);
  const permissions = usePermissions();
  const [isAdminUser, setIsAdminUser] = useState(false);

  useEffect(() => {
    initialState.current.formCache = cloneDeep(view);

    dispatch({
      type: 'initFormState',
      formState: view
    });
  }, [view, name]);

  useNavbar(name, isAdminUser, changed, 'Settings');

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
  const nameEditDisabled = frontendConfig.isCluster || !isAdminUser;

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
                           required={true} disabled={nameEditDisabled}/>
                </th>
                <th className="collectionTh">
                  <ToolTip
                    title={`The View name (string${nameEditDisabled ? ', immutable' : ''}).`}
                    setArrow={true}
                  >
                    <span className="arangoicon icon_arangodb_info"></span>
                  </ToolTip>
                </th>
              </tr>

              <tr className="tableRow" id="row_change-view-cleanupIntervalStep">
                <th className="collectionTh">
                  Cleanup Interval Step:
                </th>
                <th className="collectionTh">
                  <Textbox type={'number'} disabled={!isAdminUser} min={0} step={1}
                           value={getNumericFieldValue(formState.cleanupIntervalStep)}
                           onChange={getNumericFieldSetter('cleanupIntervalStep', dispatch)}/>
                </th>
                <th className="collectionTh">
                  <ToolTip
                    title={`ArangoSearch waits at least this many commits between removing unused files in its data directory.`}
                    setArrow={true}
                  >
                    <span className="arangoicon icon_arangodb_info"></span>
                  </ToolTip>
                </th>
              </tr>

              <tr className="tableRow" id="row_change-view-commitIntervalMsec">
                <th className="collectionTh">
                  Commit Interval (msec):
                </th>
                <th className="collectionTh" style={{ width: '100%' }}>
                  <Textbox type={'number'} disabled={!isAdminUser} min={0} step={1}
                           value={getNumericFieldValue(formState.commitIntervalMsec)}
                           onChange={getNumericFieldSetter('commitIntervalMsec', dispatch)}/>
                </th>
                <th className="collectionTh">
                  <ToolTip
                    title="Wait at least this many milliseconds between committing View data store changes and making documents visible to queries."
                    setArrow={true}
                  >
                    <span className="arangoicon icon_arangodb_info"></span>
                  </ToolTip>
                </th>
              </tr>

              <tr className="tableRow" id="row_change-view-consolidationIntervalMsec">
                <th className="collectionTh">
                  Consolidation Interval (msec):
                </th>
                <th className="collectionTh">
                  <Textbox type={'number'} disabled={!isAdminUser} min={0} step={1}
                           value={getNumericFieldValue(formState.consolidationIntervalMsec)}
                           onChange={getNumericFieldSetter('consolidationIntervalMsec', dispatch)}/>
                </th>
                <th className="collectionTh">
                  <ToolTip
                    title="Wait at least this many milliseconds between index segments consolidations."
                    setArrow={true}
                  >
                    <span className="arangoicon icon_arangodb_info"></span>
                  </ToolTip>
                </th>
              </tr>
              </tbody>
            </table>
          </div>
          {
            isAdminUser
              ? <div className="tab-pane tab-pane-modal active" id="Actions">
                {
                  changed
                    ? <SaveButton view={formState} oldName={name} setChanged={setChanged}/>
                    : null
                }
                <DeleteButton view={formState}
                              modalCid={`modal-content-delete-${formState.globallyUniqueId}`}/>
              </div>
              : null
          }
        </div>
      </div>
    </div>
  </div>;
};
window.ViewSettingsReactView = ViewSettingsReactView;
