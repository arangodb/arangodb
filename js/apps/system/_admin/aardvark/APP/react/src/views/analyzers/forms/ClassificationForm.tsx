import React, { Dispatch } from "react";
import { ClassificationState, ModelProperty } from "../constants";
import { DispatchArgs, FormProps } from '../../../utils/constants';
import ModelInput from "./inputs/ModelInput";
import { Cell, Grid } from "../../../components/pure-css/grid";
import { getNumericFieldSetter } from "../../../utils/helpers";
import Textbox from "../../../components/pure-css/form/Textbox";

const ClassificationForm = ({ formState, dispatch, disabled }: FormProps<ClassificationState>) =>
  <Grid>
    <Cell size={'1'}>
      <ModelInput formState={formState} dispatch={dispatch as Dispatch<DispatchArgs<ModelProperty>>}
                  disabled={disabled}/>
    </Cell>
    <Cell size={'1-3'}>
      <Textbox label={'Threshold'} type={'number'} value={formState.properties.threshold} disabled={disabled}
               onChange={getNumericFieldSetter('properties.threshold', dispatch)}/>
    </Cell>
  </Grid>;

export default ClassificationForm;
