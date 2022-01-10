import React, { Dispatch } from "react";
import { ModelProperty, NearestNeighborsState } from "../constants";
import { DispatchArgs, FormProps } from '../../../utils/constants';
import ModelInput from "./inputs/ModelInput";

const NearestNeighborsForm = ({ formState, dispatch, disabled }: FormProps<NearestNeighborsState>) =>
  <ModelInput formState={formState} dispatch={dispatch as Dispatch<DispatchArgs<ModelProperty>>}
              disabled={disabled}/>;

export default NearestNeighborsForm;
