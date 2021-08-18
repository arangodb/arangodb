import React, { ChangeEvent } from "react";
import { FormProps, TextState } from "../constants";
import CaseInput from "./CaseInput";
import { filter, get, isEmpty, negate } from 'lodash';
import LocaleInput from "./LocaleInput";
import { Cell, Grid } from "../../../components/pure-css/grid";
import Fieldset from "../../../components/pure-css/form/Fieldset";
import Textbox from "../../../components/pure-css/form/Textbox";
import Textarea from "../../../components/pure-css/form/Textarea";
import Checkbox from "../../../components/pure-css/form/Checkbox";

const EdgeNGramInput = ({ state, dispatch }: FormProps) => {
  const updateMinLength = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.edgeNgram.min',
        value: parseInt(event.target.value)
      }
    });
  };

  const updateMaxLength = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.edgeNgram.max',
        value: parseInt(event.target.value)
      }
    });
  };

  const formState = state.formState as TextState;

  const togglePreserve = () => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.edgeNgram.preserveOriginal',
        value: !get(formState, ['properties', 'edgeNgram', 'preserveOriginal'])
      }
    });
  };

  return <Fieldset legend={'Edge N-Gram'}>
    <Grid>
      <Cell size={'1-3'}>
        <Textbox label={'Minimum N-Gram Length'} type={'number'} min={1} placeholder="2" required={true}
                 value={get(formState, ['properties', 'edgeNgram', 'min'], '')}
                 onChange={updateMinLength}/>
      </Cell>

      <Cell size={'1-3'}>
        <Textbox label={'Maximum N-Gram Length'} type={'number'} min={1} placeholder="3" required={true}
                 value={get(formState, ['properties', 'edgeNgram', 'max'], '')}
                 onChange={updateMaxLength}/>
      </Cell>

      <Cell size={'1-3'}>
        <Checkbox onChange={togglePreserve} label={'Preserve Original'}
                  checked={get(formState, ['properties', 'edgeNgram', 'preserveOriginal'], false)}/>
      </Cell>
    </Grid>
  </Fieldset>;
};

const TextForm = ({ state, dispatch }: FormProps) => {
  const updateStopwords = (event: ChangeEvent<HTMLTextAreaElement>) => {
    const stopwords = event.target.value.split('\n');

    if (filter(stopwords, negate(isEmpty)).length) {
      dispatch({
        type: 'setField',
        field: {
          path: 'properties.stopwords',
          value: stopwords
        }
      });
    } else {
      dispatch({
        type: 'unsetField',
        field: {
          path: 'properties.stopwords'
        }
      });
    }
  };

  const formState = state.formState as TextState;

  const getStopwords = () => {
    return get(formState, ['properties', 'stopwords'], []).join('\n');
  };

  const updateStopwordsPath = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.stopwordsPath',
        value: event.target.value
      }
    });
  };

  const toggleAccent = () => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.accent',
        value: !formState.properties.accent
      }
    });
  };

  const toggleStemming = () => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.stemming',
        value: !formState.properties.stemming
      }
    });
  };

  return <Grid>
    <Cell size={'1-2'}>
      <Grid>
        <Cell size={'1'}>
          <LocaleInput formState={formState} dispatch={dispatch}/>
        </Cell>

        <Cell size={'1-2'}>
          <Checkbox onChange={toggleStemming} label={'Stemming'} inline={true} checked={formState.properties.stemming}/>
        </Cell>

        <Cell size={'1-2'}>
          <Checkbox onChange={toggleAccent} label={'Accent'} inline={true} checked={formState.properties.accent}/>
        </Cell>

        <Cell size={'1'}>
          <CaseInput state={state} dispatch={dispatch}/>
        </Cell>
      </Grid>
    </Cell>

    <Cell size={'1-2'}>
      <Grid>
        <Cell size={'1'}>
          <Textbox label={'Stopwords Path'} type={'text'} value={formState.properties.stopwordsPath}
                   onChange={updateStopwordsPath}/>
        </Cell>

        <Cell size={'1'}>
          <Textarea label={'Stopwords (One per line)'} value={getStopwords()} onChange={updateStopwords}/>
        </Cell>
      </Grid>
    </Cell>

    <Cell size={'1'}>
      <hr/>
    </Cell>

    <Cell size={'1'}>
      <EdgeNGramInput state={state} dispatch={dispatch}/>
    </Cell>
  </Grid>;
};

export default TextForm;
