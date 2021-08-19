import React, { ChangeEvent } from "react";
import { FormProps, NormState } from "../constants";
import CaseInput from "./inputs/CaseInput";
import LocaleInput from "./inputs/LocaleInput";
import { Cell, Grid } from "../../../components/pure-css/grid";
import Checkbox from "../../../components/pure-css/form/Checkbox";

const NormForm = ({ formState, dispatch, disabled, basePath }: FormProps) => {
  const updateAccent = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.accent',
        value: event.target.checked
      },
      basePath
    });
  };

  const accentProperty = (formState as NormState).properties.accent;

  return <Grid>
    <Cell size={'1-2'}>
      <LocaleInput formState={formState} dispatch={dispatch} disabled={disabled} basePath={basePath}/>
    </Cell>

    <Cell size={'1-2'}>
      <CaseInput formState={formState} dispatch={dispatch} disabled={disabled} basePath={basePath}/>
    </Cell>

    <Cell size={'1-3'}>
      <Checkbox onChange={updateAccent} label={'Accent'} inline={true} disabled={disabled}
                checked={accentProperty}/>
    </Cell>
  </Grid>;
};

export default NormForm;
