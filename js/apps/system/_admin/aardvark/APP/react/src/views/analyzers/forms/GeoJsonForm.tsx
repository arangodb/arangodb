import React, { ChangeEvent } from "react";
import { FormProps, GeoJsonState } from "../constants";
import OptionsInput from "./OptionsInput";
import { Cell, Grid } from "../../../components/pure-css/grid";
import RadioGroup from "../../../components/pure-css/form/RadioGroup";

const GeoJsonForm = ({ state, dispatch }: FormProps) => {
  const updateType = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.type',
        value: event.target.value
      }
    });
  };

  const formState = state.formState as GeoJsonState;

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
      ]} checked={formState.properties.type || 'shape'}/>
    </Cell>
    <Cell size={'1'}>
      <OptionsInput state={state} dispatch={dispatch}/>
    </Cell>
  </Grid>;
};

export default GeoJsonForm;
