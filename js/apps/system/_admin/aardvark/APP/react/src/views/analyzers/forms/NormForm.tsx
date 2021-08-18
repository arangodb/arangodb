import React from "react";
import { FormProps, NormState } from "../constants";
import CaseInput from "./CaseInput";
import LocaleInput from "./LocaleInput";
import { Cell, Grid } from "../../../components/pure-css/grid";
import Checkbox from "../../../components/pure-css/form/Checkbox";

const NormForm = ({ state, dispatch }: FormProps) => {
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
      <LocaleInput formState={formState} dispatch={dispatch}/>
    </Cell>

    <Cell size={'1-2'}>
      <CaseInput state={state} dispatch={dispatch}/>
    </Cell>

    <Cell size={'1-3'}>
      <Checkbox onChange={toggleAccent} label={'Accent'} inline={true} checked={formState.properties.accent}/>
    </Cell>
  </Grid>;
};

export default NormForm;
