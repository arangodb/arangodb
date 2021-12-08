import { cloneDeep, omit } from 'lodash';
import React, { useEffect, useReducer, useRef, useState } from 'react';
import useSWR from 'swr';
import { getApiRouteForCurrentDB } from '../../utils/arangoClient';
import { getReducer, isAdminUser as userIsAdmin, usePermissions } from '../../utils/helpers';
import { DeleteButton, SaveButton } from './Actions';
import ConsolidationPolicyForm from './forms/ConsolidationPolicyForm';
import { buildSubNav, postProcessor } from './helpers';

const ViewConsolidationReactView = ({ name }) => {
  const initialState = useRef({
    formState: { name },
    formCache: { name }
  });
  const [state, dispatch] = useReducer(getReducer(initialState.current, postProcessor),
    initialState.current);
  const { data } = useSWR(`/view/${name}/properties`,
    path => getApiRouteForCurrentDB().get(path));
  const { data: permData } = usePermissions();
  const [isAdminUser, setIsAdminUser] = useState(false);

  useEffect(() => {
    const view = data
      ? omit(data.body, 'error', 'code')
      : { name };

    initialState.current.formCache = cloneDeep(view);

    dispatch({
      type: 'setFormState',
      formState: view
    });
  }, [data, name]);

  useEffect(() => {
    buildSubNav(isAdminUser, name, 'Consolidation');
  }, [isAdminUser, name]);

  if (data && permData) {
    const tempIsAdminUser = userIsAdmin(permData);
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
          </div>
        </div>
        <div className="modal-footer">
          <DeleteButton view={formState} modalCid={`modal-content-delete-${formState.globallyUniqueId}`}/>
          <SaveButton view={formState} oldName={name}/>
        </div>
      </div>
    </div>;
  }

  return <h1>View: {name}</h1>;
};

window.ViewConsolidationReactView = ViewConsolidationReactView;
