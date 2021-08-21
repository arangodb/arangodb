import React, { ChangeEvent } from "react";
import { FormProps, NGramState } from "../constants";
import { Cell, Grid } from "../../../components/pure-css/grid";
import Textbox from "../../../components/pure-css/form/Textbox";
import RadioGroup from "../../../components/pure-css/form/RadioGroup";
import NGramInput from "./inputs/NGramInput";

const NGramForm = ({ formState, dispatch, disabled }: FormProps) => {
  const updateStartMarker = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.startMarker',
        value: event.target.value
      }
    });
  };

  const updateEndMarker = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.endMarker',
        value: event.target.value
      }
    });
  };

  const updateStreamType = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.streamType',
        value: event.target.value
      }
    });
  };

  const ngramFormState = formState as NGramState;

  return <Grid>
    <Cell size={'1'}>
      <NGramInput formState={formState} dispatch={dispatch} disabled={disabled} basePath={'properties'}/>
    </Cell>

    <Cell size={'1-3'}>
      <Textbox label={'Start Marker'} type={'text'} placeholder={disabled ? '' : '^'} disabled={disabled}
               onChange={updateStartMarker} value={ngramFormState.properties.startMarker || ''}/>
    </Cell>

    <Cell size={'1-3'}>
      <Textbox label={'End Marker'} type={'text'} placeholder={disabled ? '' : '$'} onChange={updateEndMarker}
               value={ngramFormState.properties.endMarker || ''} disabled={disabled}/>
    </Cell>

    <Cell size={'1-3'}>
      <RadioGroup legend={'Stream Type'} onChange={updateStreamType} items={[
        {
          label: 'Binary',
          value: 'binary'
        },
        {
          label: 'UTF8',
          value: 'utf8'
        }
      ]} checked={ngramFormState.properties.streamType || 'binary'} disabled={disabled}/>
    </Cell>
  </Grid>;
};

export default NGramForm;
