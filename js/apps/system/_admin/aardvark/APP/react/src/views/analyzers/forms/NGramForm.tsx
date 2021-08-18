import React, { ChangeEvent } from "react";
import { FormProps, NGramState } from "../constants";
import { Cell, Grid } from "../../../components/pure-css/grid";
import Textbox from "../../../components/pure-css/form/Textbox";
import Checkbox from "../../../components/pure-css/form/Checkbox";
import RadioGroup from "../../../components/pure-css/form/RadioGroup";

const NGramForm = ({ state, dispatch, disabled }: FormProps) => {
  const updateMinLength = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.min',
        value: parseInt(event.target.value)
      }
    });
  };

  const updateMaxLength = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.max',
        value: parseInt(event.target.value)
      }
    });
  };

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

  const formState = state.formState as NGramState;

  const togglePreserve = () => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.preserveOriginal',
        value: !formState.properties.preserveOriginal
      }
    });
  };

  return <Grid>
    <Cell size={'1-3'}>
      <Textbox label={'Minimum N-Gram Length'} type={'number'} min={1} placeholder="2" required={true}
               value={formState.properties.min} onChange={updateMinLength} disabled={disabled}/>
    </Cell>

    <Cell size={'1-3'}>
      <Textbox label={'Maximum N-Gram Length'} type={'number'} min={1} placeholder="3" required={true}
               value={formState.properties.max} onChange={updateMaxLength} disabled={disabled}/>
    </Cell>

    <Cell size={'1-3'}>
      <Checkbox onChange={togglePreserve} label={'Preserve Original'} disabled={disabled}
                checked={formState.properties.preserveOriginal}/>
    </Cell>

    <Cell size={'1-3'}>
      <Textbox label={'Start Marker'} type={'text'} placeholder={'^'} value={formState.properties.startMarker}
               onChange={updateStartMarker} disabled={disabled}/>
    </Cell>

    <Cell size={'1-3'}>
      <Textbox label={'End Marker'} type={'text'} placeholder={'$'} value={formState.properties.endMarker}
               onChange={updateEndMarker} disabled={disabled}/>
    </Cell>

    <Cell size={'1-3'}>
      <RadioGroup legend={'Stream Type'} onChange={updateStreamType} name={'streamType'} items={[
        {
          label: 'Binary',
          value: 'binary'
        },
        {
          label: 'UTF8',
          value: 'utf8'
        }
      ]} checked={formState.properties.streamType || 'binary'} disabled={disabled}/>
    </Cell>
  </Grid>;
};

export default NGramForm;
