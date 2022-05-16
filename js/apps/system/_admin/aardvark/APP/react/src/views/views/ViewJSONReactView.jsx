import { cloneDeep, isEqual, omit, uniqueId } from 'lodash';
import React, { useEffect, useReducer, useRef, useState } from 'react';
import useSWR from 'swr';
import { Cell, Grid } from '../../components/pure-css/grid';
import { getApiRouteForCurrentDB } from '../../utils/arangoClient';
import { getReducer, isAdminUser as userIsAdmin, usePermissions } from '../../utils/helpers';
import { SaveButton } from './Actions';
import CopyFromInput from './forms/inputs/CopyFromInput';
import JsonForm from './forms/JsonForm';
import { buildSubNav, postProcessor, useView } from './helpers';

const ViewJSONReactView = ({ name }) => {
  const { data } = useSWR('/view', (path) => getApiRouteForCurrentDB().get(path));
  const initialState = useRef({
    formState: { name },
    formCache: { name },
    renderKey: uniqueId('force_re-render_')
  });
  const [state, dispatch] = useReducer(getReducer(initialState.current, postProcessor),
    initialState.current);
  const view = useView(name);
  const permissions = usePermissions();
  const [isAdminUser, setIsAdminUser] = useState(false);
  const [views, setViews] = useState([]);

  useEffect(() => {
    initialState.current.formCache = cloneDeep(view);

    dispatch({
      type: 'setFormState',
      formState: omit(view, 'id', 'globallyUniqueId')
    });
    dispatch({
      type: 'regenRenderKey'
    });
  }, [view, name]);

  useEffect(() => {
    const observer = buildSubNav(isAdminUser, name, 'JSON');

    return () => observer.disconnect();
  }, [isAdminUser, name]);

  const tempIsAdminUser = userIsAdmin(permissions);
  if (tempIsAdminUser !== isAdminUser) { // Prevents an infinite render loop.
    setIsAdminUser(tempIsAdminUser);
  }

  const formState = state.formState;

  if (data) {
    if (!isEqual(data.body.result, views)) {
      setViews(data.body.result);
    }

    return <div className={'centralContent'} id={'content'}>
      <div id={'modal-dialog'} className={'createModalDialog'} tabIndex={-1} role={'dialog'}
           aria-labelledby={'myModalLabel'} aria-hidden={'true'}>
        <div className="modal-body" style={{ display: 'unset' }}>
          <div className={'tab-content'} style={{ display: 'unset' }}>
            <div className="tab-pane tab-pane-modal active" id="JSON">
              <Grid>
                <Cell size={'1'} style={{ paddingLeft: 10 }}>
                  {views.length ? <CopyFromInput views={views} dispatch={dispatch}
                                                 formState={formState}/> : null}
                </Cell>
                <Cell size={'1'}>
                  <JsonForm formState={formState} dispatch={dispatch} renderKey={state.renderKey}/>
                </Cell>
              </Grid>
            </div>
          </div>
        </div>
        <div className="modal-footer">
          <SaveButton view={formState} oldName={name} menu={'json'}/>
        </div>
      </div>
    </div>;
  }

  return <h1>View: {name}</h1>;
};

window.ViewJSONReactView = ViewJSONReactView;
