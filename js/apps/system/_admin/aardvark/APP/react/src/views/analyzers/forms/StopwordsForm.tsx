import React, { Dispatch } from "react";
import { StopwordsProperty, StopwordsState } from "../constants";
import { DispatchArgs, FormProps } from '../../../utils/constants';
import { Cell, Grid } from "../../../components/pure-css/grid";
import StopwordsInput from "./inputs/StopwordsInput";
import Checkbox from "../../../components/pure-css/form/Checkbox";
import { getBooleanFieldSetter } from "../../../utils/helpers";

const StopwordsForm = ({ formState, dispatch, disabled }: FormProps<StopwordsState>) =>
  <Grid>
    <Cell size={'1-2'}>
      <StopwordsInput formState={formState} dispatch={dispatch as Dispatch<DispatchArgs<StopwordsProperty>>}
                      disabled={disabled} required={true}/>
    </Cell>

    <Cell size={'1-2'}>
      <Checkbox onChange={getBooleanFieldSetter('properties.hex', dispatch)} label={'Hex'} disabled={disabled}
                checked={formState.properties.hex}/>
    </Cell>
  </Grid>;

export default StopwordsForm;
