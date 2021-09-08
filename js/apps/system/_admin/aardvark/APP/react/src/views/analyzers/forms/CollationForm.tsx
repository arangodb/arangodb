import React from "react";
import { FormProps } from "../constants";
import LocaleInput from "./inputs/LocaleInput";
import { Cell, Grid } from "../../../components/pure-css/grid";

const CollationForm = ({ formState, dispatch, disabled }: FormProps) =>
  <Grid>
    <Cell size={'1-2'}>
      <LocaleInput formState={formState} dispatch={dispatch} disabled={disabled}/>
    </Cell>
  </Grid>;

export default CollationForm;
