import React, { ChangeEvent } from "react";
import { FormProps, StopwordsState } from "../constants";
import { Cell, Grid } from "../../../components/pure-css/grid";
import StopwordsInput from "./inputs/StopwordsInput";
import Checkbox from "../../../components/pure-css/form/Checkbox";

const StopwordsForm = ({ formState, dispatch, disabled }: FormProps) => {
  const updateHex = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.hex',
        value: event.target.checked
      }
    });
  };

  const hexProperty = (formState as StopwordsState).properties.hex;

  return <Grid>
    <Cell size={'1-2'}>
      <StopwordsInput formState={formState} dispatch={dispatch} disabled={disabled} required={true}/>
    </Cell>

    <Cell size={'1-2'}>
      <Checkbox onChange={updateHex} label={'Hex'} disabled={disabled} checked={hexProperty}/>
    </Cell>
  </Grid>;
};

export default StopwordsForm;
