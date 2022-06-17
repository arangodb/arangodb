import { cloneDeep, isEqual, uniqueId } from 'lodash';
import React, { useEffect, useReducer, useRef, useState } from 'react';
import useSWR from 'swr';
import Textarea from '../../components/pure-css/form/Textarea';
import { Cell, Grid } from '../../components/pure-css/grid';
import { getApiRouteForCurrentDB } from '../../utils/arangoClient';
import { getReducer, isAdminUser as userIsAdmin, usePermissions } from '../../utils/helpers';
import { SaveButton } from './Actions';
import CopyFromInput from './forms/inputs/CopyFromInput';
import JsonForm from './forms/JsonForm';
import { postProcessor, useNavbar, useView } from './helpers';

const ViewJSONReactView = ({ name }) => {
  const [isAdminUser, setIsAdminUser] = useState(false);
  const { data } = useSWR(isAdminUser ? '/view' : null,
    (path) => getApiRouteForCurrentDB().get(path));
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
  }, [view]);

  useNavbar(name, isAdminUser, changed, 'JSON');

  const tempIsAdminUser = userIsAdmin(permissions);
  if (tempIsAdminUser !== isAdminUser) { // Prevents an infinite render loop.
    setIsAdminUser(tempIsAdminUser);
  }

  const formState = state.formState;

  if (data) {
    if (!isEqual(data.body.result, views)) {
      setViews(data.body.result);
    }
  }

  let jsonFormState = '';
  let jsonRows = 1;
  if (!isAdminUser) {
    jsonFormState = JSON.stringify(formState, null, 4);
    jsonRows = jsonFormState.split('\n').length;
  }

  return <div className={'centralContent'} id={'content'}>
    <div id={'modal-dialog'} className={'createModalDialog'} tabIndex={-1} role={'dialog'}
         aria-labelledby={'myModalLabel'} aria-hidden={'true'}>
      <div className="modal-body" style={{ display: 'unset' }}>
        <div className={'tab-content'} style={{ display: 'unset' }}>
          <div className="tab-pane tab-pane-modal active" id="JSON">
            <Grid>
              {
                isAdminUser && views.length
                  ? <Cell size={'1'} style={{ paddingLeft: 10 }}>
                    <CopyFromInput views={views} dispatch={dispatch} formState={formState}/>
                  </Cell>
                  : null
              }

              <Cell size={'1'}>
                {
                  isAdminUser
                    ? <JsonForm formState={formState} dispatch={dispatch}
                                renderKey={state.renderKey}/>
                    : <Textarea label={'JSON Dump'} disabled={true} value={jsonFormState}
                                rows={jsonRows}
                                style={{ cursor: 'text' }}/>
                }
              </Cell>
            </Grid>
          </div>
        </div>
        {
          isAdminUser && changed
            ? <div className="tab-content" id="Save">
              <SaveButton view={formState} oldName={name} menu={'json'} setChanged={setChanged}/>
            </div>
            : null
        }
      </div>
    </div>
  </div>;
};

window.ViewJSONReactView = ViewJSONReactView;
