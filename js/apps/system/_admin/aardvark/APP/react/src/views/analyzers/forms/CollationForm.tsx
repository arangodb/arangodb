import React, { Dispatch } from "react";
import { DispatchArgs, FormProps } from "../../../utils/constants";
import LocaleInput from "./inputs/LocaleInput";
import { Cell, Grid } from "../../../components/pure-css/grid";
import { CollationState, LocaleProperty } from "../constants";

const CollationForm = ({ formState, dispatch, disabled }: FormProps<CollationState>) =>
  <Grid>
    <Cell size={'1-2'}>
      <LocaleInput formState={formState} dispatch={dispatch as Dispatch<DispatchArgs<LocaleProperty>>}
                   disabled={disabled}/>
    </Cell>
  </Grid>;

export default CollationForm;
