import React, { useReducer } from 'react';
import Modal, { ModalBody, ModalFooter, ModalHeader } from "../../components/modal/Modal";
import { cloneDeep, merge, set, uniqueId, unset } from 'lodash';
import JsonForm from "./forms/JsonForm";
import FeatureForm from "./forms/FeatureForm";
import BaseForm from "./forms/BaseForm";
import { mutate } from "swr";
import { getApiRouteForCurrentDB } from '../../utils/arangoClient';
import { DispatchArgs, FormState, State } from "./constants";
import CopyFromInput from "./forms/inputs/CopyFromInput";
import { Cell, Grid } from "../../components/pure-css/grid";
import { getForm, getPath, validateAndFix } from "./helpers";

declare var arangoHelper: { [key: string]: any };

const initialFormState: FormState = {
  name: '',
  type: 'delimiter',
  features: [],
  properties: {
    delimiter: ''
  }
};

const initialState: State = {
  formState: cloneDeep(initialFormState),
  formCache: cloneDeep(initialFormState),
  show: false,
  showJsonForm: false,
  lockJsonForm: false,
  renderKey: uniqueId('force_re-render_')
};

const reducer = (state: State, action: DispatchArgs): State => {
  const newState = cloneDeep(state);

  switch (action.type) {
    case 'lockJsonForm':
      newState.lockJsonForm = true;
      break;

    case 'unlockJsonForm':
      newState.lockJsonForm = false;
      break;

    case 'show':
      newState.show = true;
      break;

    case 'showJsonForm':
      newState.showJsonForm = true;
      break;

    case 'hideJsonForm':
      newState.showJsonForm = false;
      break;

    case 'regenRenderKey':
      newState.renderKey = uniqueId('force_re-render_');
      break;

    case 'setField':
      if (action.field && action.field.value !== undefined) {
        const path = getPath(action.basePath, action.field.path);

        set(newState.formCache, path, action.field.value);

        if (action.field.path === 'type') {
          const tempFormState = cloneDeep(newState.formCache);
          validateAndFix(tempFormState);
          newState.formState = tempFormState as FormState;

          merge(newState.formCache, newState.formState);
        } else {
          set(newState.formState, path, action.field.value);
        }
      }
      break;

    case 'unsetField':
      if (action.field) {
        const path = getPath(action.basePath, action.field.path);

        unset(newState.formState, path);
        unset(newState.formCache, path);
      }
      break;

    case 'setFormState':
      if (action.formState) {
        newState.formState = action.formState;
        merge(newState.formCache, newState.formState);
      }
      break;

    case 'reset':
      return initialState;
  }

  return newState;
};

interface AddAnalyzerProps {
  analyzers: FormState[];
}

const AddAnalyzer = ({ analyzers }: AddAnalyzerProps) => {
  const [state, dispatch] = useReducer(reducer, initialState);

  const formState = state.formState;

  const handleAdd = async () => {
    try {
      const result = await getApiRouteForCurrentDB().post('/analyzer/', formState);

      if (result.body.error) {
        arangoHelper.arangoError('Failure', `Got unexpected server response: ${result.body.errorMessage}`);
      } else {
        arangoHelper.arangoNotification('Success', `Created Analyzer: ${result.body.name}`);
        await mutate('/analyzer');

        dispatch({ type: 'reset' });
      }
    } catch (e) {
      arangoHelper.arangoError('Failure', `Got unexpected server response: ${e.message}`);
    }
  };

  const toggleJsonForm = () => {
    dispatch({ type: state.showJsonForm ? 'hideJsonForm' : 'showJsonForm' });
  };

  return <>
    <button className={'pure-button'} onClick={() => dispatch({ type: 'show' })} style={{
      background: 'transparent',
      color: 'white',
      paddingLeft: 0,
      paddingTop: 0
    }}>
      <i className="fa fa-plus-circle"/> Add Analyzer
    </button>
    <Modal show={state.show} setShow={(show) => dispatch({ type: show ? 'show' : 'reset' })}
           key={`${analyzers.length}-${state.show}`}>
      <ModalHeader title={'Create Analyzer'}>
        <Grid>
          <Cell size={'2-3'}>
            <CopyFromInput analyzers={analyzers} dispatch={dispatch}/>
          </Cell>
          <Cell size={'1-3'}>
            <button className={'button-info'} onClick={toggleJsonForm} disabled={state.lockJsonForm}
                    style={{
                      float: 'right'
                    }}>
              {state.showJsonForm ? 'Switch to form view' : 'Switch to code view'}
            </button>
          </Cell>
        </Grid>
      </ModalHeader>
      <ModalBody maximize={true} show={state.show}>
        <Grid>
          {
            state.showJsonForm
              ? <Cell size={'1'}>
                <JsonForm formState={formState} dispatch={dispatch} renderKey={state.renderKey}/>
              </Cell>
              : <>
                <Cell size={'11-24'}>
                  <fieldset>
                    <legend style={{ fontSize: '12pt' }}>Basic</legend>
                    <BaseForm formState={formState} dispatch={dispatch}/>
                  </fieldset>
                </Cell>
                <Cell size={'1-12'}/>
                <Cell size={'11-24'}>
                  <fieldset>
                    <legend style={{ fontSize: '12pt' }}>Features</legend>
                    <FeatureForm formState={formState} dispatch={dispatch}/>
                  </fieldset>
                </Cell>

                {
                  formState.type === 'identity' ? null
                    : <Cell size={'1'}>
                      <fieldset>
                        <legend style={{ fontSize: '12pt' }}>Configuration</legend>
                        {
                          getForm({
                            formState,
                            dispatch,
                            disabled: false
                          })
                        }
                      </fieldset>
                    </Cell>
                }
              </>
          }
        </Grid>
      </ModalBody>
      <ModalFooter>
        <button className="button-close" onClick={() => dispatch({ type: 'reset' })}>Close</button>
        <button className="button-success" style={{ float: 'right' }} onClick={handleAdd}
                disabled={state.lockJsonForm}>
          Create
        </button>
      </ModalFooter>
    </Modal>
  </>
    ;
};

export default AddAnalyzer;
