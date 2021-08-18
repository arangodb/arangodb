import React from "react";
import { FormProps, StemState } from "../constants";
import LocaleInput from "./inputs/LocaleInput";
import { Cell, Grid } from "../../../components/pure-css/grid";

const StemForm = ({ state, dispatch, disabled }: FormProps) => {
  const formState = state.formState as StemState;

  return <Grid>
    <Cell size={'1-2'}>
      <LocaleInput formState={formState} dispatch={dispatch} disabled={disabled}/>
    </Cell>
  </Grid>;
};

export default StemForm;
