import React, { ChangeEvent } from "react";
import { FormProps, TextState } from "../constants";
import CaseInput from "./inputs/CaseInput";
import LocaleInput from "./inputs/LocaleInput";
import { Cell, Grid } from "../../../components/pure-css/grid";
import Fieldset from "../../../components/pure-css/form/Fieldset";
import Textbox from "../../../components/pure-css/form/Textbox";
import Checkbox from "../../../components/pure-css/form/Checkbox";
import NGramInput from "./inputs/NGramInput";
import AccentInput from "./inputs/AccentInput";
import StopwordsInput from "./inputs/StopwordsInput";

const TextForm = ({ formState, dispatch, disabled }: FormProps) => {
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

  const textFormState = formState as TextState;

  return <Grid>
    <Cell size={'1-2'}>
      <Grid>
        <Cell size={'1'}>
          <LocaleInput formState={formState} dispatch={dispatch} disabled={disabled}/>
        </Cell>

        <Cell size={'1'}>
          <StopwordsInput formState={formState} dispatch={dispatch} disabled={disabled}/>
        </Cell>
      </Grid>
    </Cell>

    <Cell size={'1-2'}>
      <Grid>
        <Cell size={'1'}>
          <Textbox label={'Stopwords Path'} type={'text'} value={textFormState.properties.stopwordsPath || ''}
                   onChange={updateStopwordsPath} disabled={disabled}/>
        </Cell>

        <Cell size={'1-3'}>
          <CaseInput formState={formState} dispatch={dispatch} disabled={disabled}/>
        </Cell>

        <Cell size={'1-3'}>
          <Checkbox onChange={updateStemming} label={'Stemming'} disabled={disabled}
                    checked={textFormState.properties.stemming || false}/>
        </Cell>

        <Cell size={'1-3'}>
          <AccentInput formState={formState} dispatch={dispatch} disabled={disabled} inline={false}/>
        </Cell>
      </Grid>
    </Cell>

    <Cell size={'1'}>
      <hr/>
    </Cell>

    <Cell size={'1'}>
      <Fieldset legend={'Edge N-Gram'}>
        <NGramInput formState={formState} dispatch={dispatch} disabled={disabled} required={false}
                    basePath={'properties.edgeNgram'}/>
      </Fieldset>
    </Cell>
  </Grid>;
};

export default TextForm;
