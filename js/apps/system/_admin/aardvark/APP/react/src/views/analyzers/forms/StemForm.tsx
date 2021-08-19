import React from "react";
import { FormProps } from "../constants";
import LocaleInput from "./inputs/LocaleInput";
import { Cell, Grid } from "../../../components/pure-css/grid";

const StemForm = ({ formState, dispatch, disabled, basePath }: FormProps) =>
  <Grid>
    <Cell size={'1-2'}>
      <LocaleInput formState={formState} dispatch={dispatch} disabled={disabled} basePath={basePath}/>
    </Cell>
  </Grid>;

export default StemForm;
