import React, { Dispatch, useReducer } from 'react';
import Modal, { ModalBody, ModalFooter, ModalHeader } from "../../components/modal/Modal";
import { cloneDeep, merge, set, uniqueId } from 'lodash';
import JsonForm from "./forms/JsonForm";
import FeatureForm from "./forms/FeatureForm";
import BaseForm from "./forms/BaseForm";
import { mutate } from "swr";
import { getApiRouteForCurrentDB } from '../../utils/arangoClient';
import { AnalyzerTypeState, FormState } from "./constants";
import { DispatchArgs, State } from '../../utils/constants';
import CopyFromInput from "./forms/inputs/CopyFromInput";
import { Cell, Grid } from "../../components/pure-css/grid";
import { getForm, validateAndFix } from "./helpers";
import { getPath, getReducer } from "../../utils/helpers";
import { IconButton } from "../../components/arango/buttons";

declare var arangoHelper: { [key: string]: any };

const initialFormState: FormState = {
  name: '',
  type: 'delimiter',
  features: [],
  properties: {
    delimiter: ''
  }
};

const initialState: State<FormState> = {
  formState: cloneDeep(initialFormState),
  formCache: cloneDeep(initialFormState),
  show: false,
  showJsonForm: false,
  lockJsonForm: false,
  renderKey: uniqueId('force_re-render_')
};

const postProcessor = (state: State<FormState>, action: DispatchArgs<FormState>) => {
  if (action.type === 'setField' && action.field) {
    const path = getPath(action.basePath, action.field.path);

    if (action.field.path === 'type') {
      const tempFormState = cloneDeep(state.formCache);

      validateAndFix(tempFormState);
      state.formState = tempFormState as unknown as FormState;

      merge(state.formCache, state.formState);
    } else if (action.field.value !== undefined) {
      set(state.formState, path, action.field.value);
    }
  }
};

interface AddAnalyzerProps {
  analyzers: FormState[];
}

const AddAnalyzer = ({ analyzers }: AddAnalyzerProps) => {
  const [state, dispatch] = useReducer(getReducer<FormState>(initialState, postProcessor), initialState);

  const formState = state.formState;

  const handleAdd = async () => {
    try {
      const result = await getApiRouteForCurrentDB().post('/analyzer', formState);

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
    <IconButton icon={'plus-circle'} onClick={() => dispatch({ type: 'show' })} style={{
      background: 'transparent',
      color: 'white',
      paddingLeft: 0,
      paddingTop: 0
    }}>
      Add Analyzer
    </IconButton>
    <Modal show={state.show} setShow={(show) => dispatch({ type: show ? 'show' : 'reset' })}
           key={`${analyzers.length}-${state.show}`} cid={'modal-content-add-analyzer'}>
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
                            dispatch: dispatch as Dispatch<DispatchArgs<AnalyzerTypeState>>,
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
