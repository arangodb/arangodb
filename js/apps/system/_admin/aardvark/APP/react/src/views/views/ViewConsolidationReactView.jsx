import { cloneDeep } from 'lodash';
import React, { useEffect, useReducer, useRef, useState } from 'react';
import { getReducer, isAdminUser as userIsAdmin, usePermissions } from '../../utils/helpers';
import { SaveButton } from './Actions';
import ConsolidationPolicyForm from './forms/ConsolidationPolicyForm';
import { postProcessor, useNavbar, useView } from './helpers';

const ViewConsolidationReactView = ({ name }) => {
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

  useNavbar(name, isAdminUser, changed, 'Consolidation Policy');

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
          <div className="tab-pane tab-pane-modal active" id="Consolidation">
            <ConsolidationPolicyForm formState={formState} dispatch={dispatch}
                                     disabled={!isAdminUser}/>
          </div>
          {
            isAdminUser && changed
              ? <div className="tab-pane tab-pane-modal active" id="Save">
                <SaveButton view={formState} oldName={name} setChanged={setChanged}/>
              </div>
              : null
          }
        </div>
      </div>
    </div>
  </div>;
};

window.ViewConsolidationReactView = ViewConsolidationReactView;
