import { FormProps } from "../constants";
import { Cell, Grid } from "../../../components/pure-css/grid";
import PrimarySortInput from "./inputs/PrimarySortInput";
import React from "react";

const ViewPropertiesForm = ({ formState, dispatch, disabled }: FormProps) => {
  return <Grid>
    <Cell size={'1'}>
      <PrimarySortInput formState={formState} dispatch={dispatch} disabled={disabled}/>
    </Cell>
  </Grid>;
};

export default ViewPropertiesForm;
