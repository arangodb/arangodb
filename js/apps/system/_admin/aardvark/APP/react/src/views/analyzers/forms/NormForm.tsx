import React from "react";
import { FormProps, NormState } from "../constants";
import CaseInput from "./inputs/CaseInput";
import LocaleInput from "./inputs/LocaleInput";
import { Cell, Grid } from "../../../components/pure-css/grid";
import Checkbox from "../../../components/pure-css/form/Checkbox";

const NormForm = ({ state, dispatch, disabled }: FormProps) => {
  const formState = state.formState as NormState;

  const toggleAccent = () => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.accent',
        value: !formState.properties.accent
      }
    });
  };

  return <Grid>
    <Cell size={'1-2'}>
      <LocaleInput formState={formState} dispatch={dispatch} disabled={disabled}/>
    </Cell>

    <Cell size={'1-2'}>
      <CaseInput state={state} dispatch={dispatch} disabled={disabled}/>
    </Cell>

    <Cell size={'1-3'}>
      <Checkbox onChange={toggleAccent} label={'Accent'} inline={true} checked={formState.properties.accent}
                disabled={disabled}/>
    </Cell>
  </Grid>;
};

export default NormForm;
