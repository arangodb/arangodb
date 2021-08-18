import React from "react";
import { FormProps, StemState } from "../constants";
import LocaleInput from "./LocaleInput";
import { Cell, Grid } from "../../../components/pure-css/grid";

const StemForm = ({ state, dispatch }: FormProps) => {
  const formState = state.formState as StemState;

  return <Grid>
    <Cell size={'1-2'}>
      <LocaleInput formState={formState} dispatch={dispatch}/>
    </Cell>
  </Grid>;
};

export default StemForm;
