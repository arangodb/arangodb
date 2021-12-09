import { cloneDeep } from 'lodash';
import React, { useEffect, useReducer, useRef, useState } from 'react';
import { getReducer, isAdminUser as userIsAdmin, usePermissions } from '../../utils/helpers';
import { SaveButton } from './Actions';
import LinkPropertiesForm from './forms/LinkPropertiesForm';
import { buildSubNav, postProcessor, useView } from './helpers';

const ViewLinksReactView = ({ name }) => {
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
    buildSubNav(isAdminUser, name, 'Links');
  }, [isAdminUser, name]);

  const tempIsAdminUser = userIsAdmin(permissions);
  if (tempIsAdminUser !== isAdminUser) { // Prevents an infinite render loop.
    setIsAdminUser(tempIsAdminUser);
  }

  const formState = state.formState;

  return <div className={'centralContent'} id={'content'}>
    <div id={'modal-dialog'} className={'createModalDialog'} tabIndex={-1} role={'dialog'}
         aria-labelledby={'myModalLabel'} aria-hidden={'true'}>
      <div className="modal-body" style={{ overflowY: 'visible' }}>
        <div className={'tab-content'}>
          <div className="tab-pane tab-pane-modal active" id="Links">
            <LinkPropertiesForm formState={formState} dispatch={dispatch} disabled={!isAdminUser}/>
          </div>
        </div>
      </div>
      <div className="modal-footer">
        <SaveButton view={formState} oldName={name}/>
      </div>
    </div>
  </div>;
};

window.ViewLinksReactView = ViewLinksReactView;
