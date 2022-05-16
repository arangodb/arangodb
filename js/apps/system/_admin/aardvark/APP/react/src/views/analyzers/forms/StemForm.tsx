import React, { Dispatch } from "react";
import { LocaleProperty, StemState } from "../constants";
import { DispatchArgs, FormProps } from "../../../utils/constants";
import LocaleInput from "./inputs/LocaleInput";
import { Cell, Grid } from "../../../components/pure-css/grid";

const StemForm = ({ formState, dispatch, disabled }: FormProps<StemState>) =>
  <Grid>
    <Cell size={'1-2'}>
      <LocaleInput formState={formState} dispatch={dispatch as Dispatch<DispatchArgs<LocaleProperty>>}
                   disabled={disabled}/>
    </Cell>
  </Grid>;

export default StemForm;
