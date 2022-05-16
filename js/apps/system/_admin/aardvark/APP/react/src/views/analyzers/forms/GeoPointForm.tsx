import React, { ChangeEvent, Dispatch } from "react";
import { GeoOptionsProperty, GeoPointState } from "../constants";
import { DispatchArgs, FormProps } from "../../../utils/constants";
import { filter, isEmpty, negate } from 'lodash';
import GeoOptionsInput from "./inputs/GeoOptionsInput";
import { Cell, Grid } from "../../../components/pure-css/grid";
import Textbox from "../../../components/pure-css/form/Textbox";

const GeoPointForm = ({ formState, dispatch, disabled }: FormProps<GeoPointState>) => {
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

  const getLatitude = () => (formState.properties.latitude || []).join('.');
  const getLongitude = () => (formState.properties.longitude || []).join('.');

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
      <GeoOptionsInput formState={formState} dispatch={dispatch as Dispatch<DispatchArgs<GeoOptionsProperty>>}
                       disabled={disabled}/>
    </Cell>
  </Grid>;
};

export default GeoPointForm;
