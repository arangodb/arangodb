import { cloneDeep, merge, set, uniqueId } from 'lodash';
import React, { useReducer } from 'react';
import { mutate } from "swr";
import { IconButton } from "../../components/arango/buttons";
import { getApiRouteForCurrentDB } from '../../utils/arangoClient';
import { DispatchArgs, State } from '../../utils/constants';
import { getPath, getReducer } from "../../utils/helpers";
import { AddAnalyzerModal } from './AddAnalyzerModal';
import { FormState } from "./constants";
import { validateAndFix } from "./helpers";

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
    } catch (e: any) {
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
    <AddAnalyzerModal
      isOpen={state.show}
      onClose={() => dispatch({ type: "reset" })}
      state={state}
      dispatch={dispatch}
      analyzers={analyzers}
      toggleJsonForm={toggleJsonForm}
      formState={formState}
      handleAdd={handleAdd}
    />
  </>
    ;
};

export default AddAnalyzer;
