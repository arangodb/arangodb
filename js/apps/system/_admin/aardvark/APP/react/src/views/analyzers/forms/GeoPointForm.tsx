import React, { ChangeEvent } from "react";
import { FormProps, GeoPointState } from "../constants";
import { filter, get, isEmpty, negate } from 'lodash';
import OptionsInput from "./inputs/OptionsInput";
import { Cell, Grid } from "../../../components/pure-css/grid";
import Textbox from "../../../components/pure-css/form/Textbox";

const GeoPointForm = ({ state, dispatch, disabled }: FormProps) => {
  const updateArray = (event: ChangeEvent<HTMLInputElement>, field: string) => {
    const items = event.target.value.split('.');

    if (filter(items, negate(isEmpty)).length) {
      dispatch({
        type: 'setField',
        field: {
          path: field,
          value: items
        }
      });
    } else {
      dispatch({
        type: 'unsetField',
        field: {
          path: field
        }
      });
    }
  };

  const updateLatitude = (event: ChangeEvent<HTMLInputElement>) => {
    updateArray(event, 'properties.latitude');
  };

  const updateLongitude = (event: ChangeEvent<HTMLInputElement>) => {
    updateArray(event, 'properties.longitude');
  };

  const formState = state.formState as GeoPointState;

  const getArray = (field: string[]) => {
    return get(formState, field, []).join('.');
  };

  const getLatitude = () => getArray(['properties', 'latitude']);
  const getLongitude = () => getArray(['properties', 'longitude']);

  return <Grid>
    <Cell size={'1-2'}>
      <Textbox label={'Latitude Path'} type={'text'} value={getLatitude()} onChange={updateLatitude}
               disabled={disabled}/>
    </Cell>

    <Cell size={'1-2'}>
      <Textbox label={'Longitude Path'} type={'text'} value={getLongitude()} onChange={updateLongitude}
               disabled={disabled}/>
    </Cell>

    <Cell size={'1'}>
      <OptionsInput state={state} dispatch={dispatch} disabled={disabled}/>
    </Cell>
  </Grid>;
};

export default GeoPointForm;
