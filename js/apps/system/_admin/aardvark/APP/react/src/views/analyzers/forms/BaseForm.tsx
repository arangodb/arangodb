import React, { ChangeEvent } from "react";
import { BaseFormState, FormProps, typeNameMap } from "../constants";
import { Cell, Grid } from "../../../components/pure-css/grid";
import Textbox from "../../../components/pure-css/form/Textbox";
import TypeInput from "./inputs/TypeInput";

const BaseForm = ({ formState, dispatch, disabled }: FormProps) => {
  const updateName = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'name',
        value: event.target.value
      }
    });
  };

  const name = (formState as BaseFormState).name;

  return <Grid>
    <Cell size={'1-2'}>
      <Textbox label={'Analyzer Name'} type={'text'} value={name || ''}
               onChange={updateName} required={true} disabled={disabled}/>
    </Cell>

    <Cell size={'1-2'}>
      <TypeInput formState={formState} dispatch={dispatch} typeNameMap={typeNameMap} disabled={disabled}/>
    </Cell>
  </Grid>;
};

export default BaseForm;
