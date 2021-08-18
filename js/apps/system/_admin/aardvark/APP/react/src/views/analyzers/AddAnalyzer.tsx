import React, { useReducer } from 'react';
import Modal, { ModalBody, ModalFooter, ModalHeader } from "../../components/modal/Modal";
import { cloneDeep, merge, set, uniqueId, unset } from 'lodash';
import JsonForm from "./forms/JsonForm";
import FeatureForm from "./forms/FeatureForm";
import DelimiterForm from "./forms/DelimiterForm";
import StemForm from "./forms/StemForm";
import NormForm from "./forms/NormForm";
import NGramForm from "./forms/NGramForm";
import BaseForm from "./forms/BaseForm";
import TextForm from "./forms/TextForm";
import { mutate } from "swr";
import { getApiRouteForCurrentDB } from '../../utils/arangoClient';
import { DispatchArgs, FormState, State, validateAndFix } from "./constants";
import CopyFromInput from "./forms/inputs/CopyFromInput";
import AqlForm from "./forms/AqlForm";
import GeoJsonForm from "./forms/GeoJsonForm";
import GeoPointForm from "./forms/GeoPointForm";
import { Cell, Grid } from "../../components/pure-css/grid";

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

    case 'hide':
      newState.show = false;
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
        set(newState.formCache, action.field.path, action.field.value);

        if (action.field.path === 'type') {
          const tempFormState = cloneDeep(newState.formCache);
          validateAndFix(tempFormState);
          newState.formState = tempFormState as FormState;

          merge(newState.formCache, newState.formState);
        } else {
          set(newState.formState, action.field.path, action.field.value);
        }
      }
      break;

    case 'unsetField':
      if (action.field) {
        unset(newState.formState, action.field.path);
        unset(newState.formCache, action.field.path);
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

  const handleAdd = async () => {
    try {
      const result = await getApiRouteForCurrentDB().post('/analyzer/', state.formState);

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

  const forms = {
    identity: null,
    delimiter: <DelimiterForm state={state} dispatch={dispatch}/>,
    stem: <StemForm state={state} dispatch={dispatch}/>,
    norm: <NormForm state={state} dispatch={dispatch}/>,
    ngram: <NGramForm state={state} dispatch={dispatch}/>,
    text: <TextForm state={state} dispatch={dispatch}/>,
    aql: <AqlForm state={state} dispatch={dispatch}/>,
    geojson: <GeoJsonForm state={state} dispatch={dispatch}/>,
    geopoint: <GeoPointForm state={state} dispatch={dispatch}/>,
    pipeline: 'Pipeline'
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
    <Modal show={state.show} setShow={(show) => dispatch({ type: show ? 'show' : 'hide' })}>
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
      <ModalBody>
        <Grid>
          {
            state.showJsonForm
              ? <Cell size={'1'}>
                <JsonForm state={state} dispatch={dispatch}/>
              </Cell>
              : <>
                <Cell size={'1'}>
                  <BaseForm state={state} dispatch={dispatch}/>
                </Cell>
                <Cell size={'1'}>
                  <fieldset>
                    <legend style={{ fontSize: '12pt' }}>Features</legend>
                    <FeatureForm state={state} dispatch={dispatch}/>
                  </fieldset>
                </Cell>

                {
                  state.formState.type === 'identity' ? null
                    : <Cell size={'1'}>
                      <fieldset>
                        <legend style={{ fontSize: '12pt' }}>Configuration</legend>
                        {forms[state.formState.type]}
                      </fieldset>
                    </Cell>
                }
              </>
          }
        </Grid>
      </ModalBody>
      <ModalFooter>
        <button className="button-close" onClick={() => dispatch({ type: 'hide' })}>Close</button>
        <button className="button-success" style={{ float: 'right' }} onClick={handleAdd}>Create</button>
      </ModalFooter>
    </Modal>
  </>
    ;
};

export default AddAnalyzer;
