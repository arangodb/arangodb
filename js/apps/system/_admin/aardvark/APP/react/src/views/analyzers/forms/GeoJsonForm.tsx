import React, { ChangeEvent } from "react";
import { FormProps, GeoJsonState } from "../constants";
import GeoOptionsInput from "./inputs/GeoOptionsInput";
import { Cell, Grid } from "../../../components/pure-css/grid";
import RadioGroup from "../../../components/pure-css/form/RadioGroup";

const GeoJsonForm = ({ formState, dispatch, disabled, basePath }: FormProps) => {
  const updateType = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.type',
        value: event.target.value
      },
      basePath
    });
  };

  const typeProperty = (formState as GeoJsonState).properties.type;

  return <Grid>
    <Cell size={'1-2'}>
      <RadioGroup legend={'Type'} onChange={updateType} name={'type'} items={[
        {
          label: 'Shape',
          value: 'shape'
        },
        {
          label: 'Centroid',
          value: 'centroid'
        },
        {
          label: 'Point',
          value: 'point'
        }
      ]} checked={typeProperty || 'shape'} disabled={disabled}/>
    </Cell>
    <Cell size={'1'}>
      <GeoOptionsInput formState={formState} dispatch={dispatch} disabled={disabled} basePath={basePath}/>
    </Cell>
  </Grid>;
};

export default GeoJsonForm;
