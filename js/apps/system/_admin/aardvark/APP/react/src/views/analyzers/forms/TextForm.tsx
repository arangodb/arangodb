import React, { ChangeEvent } from "react";
import { FormProps, TextState } from "../constants";
import CaseInput from "./inputs/CaseInput";
import { filter, isEmpty, negate } from 'lodash';
import LocaleInput from "./inputs/LocaleInput";
import { Cell, Grid } from "../../../components/pure-css/grid";
import Fieldset from "../../../components/pure-css/form/Fieldset";
import Textbox from "../../../components/pure-css/form/Textbox";
import Textarea from "../../../components/pure-css/form/Textarea";
import Checkbox from "../../../components/pure-css/form/Checkbox";
import NGramInput from "./inputs/NGramInput";
import AccentInput from "./inputs/AccentInput";

const TextForm = ({ formState, dispatch, disabled }: FormProps) => {
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

  const textFormState = formState as TextState;

  const getStopwords = () => {
    return (textFormState.properties.stopwords || []).join('\n');
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

  const updateStemming = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.stemming',
        value: event.target.checked
      }
    });
  };

  return <Grid>
    <Cell size={'1-2'}>
      <Grid>
        <Cell size={'1'}>
          <LocaleInput formState={formState} dispatch={dispatch} disabled={disabled}/>
        </Cell>

        <Cell size={'1-2'}>
          <Checkbox onChange={updateStemming} label={'Stemming'} inline={true} disabled={disabled}
                    checked={textFormState.properties.stemming || false}/>
        </Cell>

        <Cell size={'1-2'}>
          <AccentInput formState={formState} dispatch={dispatch} disabled={disabled}/>
        </Cell>

        <Cell size={'1'}>
          <CaseInput formState={formState} dispatch={dispatch} disabled={disabled}/>
        </Cell>
      </Grid>
    </Cell>

    <Cell size={'1-2'}>
      <Grid>
        <Cell size={'1'}>
          <Textbox label={'Stopwords Path'} type={'text'} value={textFormState.properties.stopwordsPath || ''}
                   onChange={updateStopwordsPath} disabled={disabled}/>
        </Cell>

        <Cell size={'1'}>
          <Textarea label={'Stopwords (One per line)'} value={getStopwords()} onChange={updateStopwords}
                    disabled={disabled}/>
        </Cell>
      </Grid>
    </Cell>

    <Cell size={'1'}>
      <hr/>
    </Cell>

    <Cell size={'1'}>
      <Fieldset legend={'Edge N-Gram'}>
        <NGramInput formState={formState} dispatch={dispatch} disabled={disabled}
                    basePath={'properties.edgeNgram'}/>
      </Fieldset>
    </Cell>
  </Grid>;
};

export default TextForm;
