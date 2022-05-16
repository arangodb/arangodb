import React, { ChangeEvent, Dispatch } from "react";
import { NGramBaseProperty, NGramState } from "../constants";
import { DispatchArgs, FormProps } from "../../../utils/constants";
import { Cell, Grid } from "../../../components/pure-css/grid";
import Textbox from "../../../components/pure-css/form/Textbox";
import NGramInput from "./inputs/NGramInput";
import Select from "../../../components/pure-css/form/Select";

const NGramForm = ({ formState, dispatch, disabled }: FormProps<NGramState>) => {
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

  const updateStreamType = (event: ChangeEvent<HTMLSelectElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.streamType',
        value: event.target.value
      }
    });
  };

  return <Grid>
    <Cell size={'1'}>
      <NGramInput formState={formState.properties as NGramBaseProperty}
                  dispatch={dispatch as Dispatch<DispatchArgs<NGramBaseProperty>>} disabled={disabled}
                  basePath={'properties'}/>
    </Cell>

    <Cell size={'1-3'}>
      <Textbox label={'Start Marker'} type={'text'} disabled={disabled}
               onChange={updateStartMarker} value={formState.properties.startMarker || ''}/>
    </Cell>

    <Cell size={'1-3'}>
      <Textbox label={'End Marker'} type={'text'} onChange={updateEndMarker}
               value={formState.properties.endMarker || ''} disabled={disabled}/>
    </Cell>

    <Cell size={'1-3'}>
      <Select label={'Stream Type'} value={formState.properties.streamType || 'binary'}
              onChange={updateStreamType} disabled={disabled}>
        <option value={'binary'}>Binary</option>
        <option value={'utf8'}>UTF8</option>
      </Select>
    </Cell>
  </Grid>;
};

export default NGramForm;
