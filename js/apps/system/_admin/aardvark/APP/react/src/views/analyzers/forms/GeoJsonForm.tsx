import React, { ChangeEvent } from "react";
import { FormProps, GeoJsonState } from "../constants";
import GeoOptionsInput from "./inputs/GeoOptionsInput";
import { Cell, Grid } from "../../../components/pure-css/grid";
import Select from "../../../components/pure-css/form/Select";

const GeoJsonForm = ({ formState, dispatch, disabled }: FormProps) => {
  const updateType = (event: ChangeEvent<HTMLSelectElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.type',
        value: event.target.value
      }
    });
  };

  const typeProperty = (formState as GeoJsonState).properties.type;

  return <Grid>
    <Cell size={'1-2'}>
      <Select label={'Type'} value={typeProperty || 'shape'} onChange={updateType} disabled={disabled}>
        <option value={'shape'}>Shape</option>
        <option value={'centroid'}>Centroid</option>
        <option value={'point'}>Point</option>
      </Select>
    </Cell>
    <Cell size={'1'}>
      <GeoOptionsInput formState={formState} dispatch={dispatch} disabled={disabled}/>
    </Cell>
  </Grid>;
};

export default GeoJsonForm;
