import React, { ChangeEvent } from "react";
import { FormProps } from "../../../utils/constants";
import { Cell, Grid } from "../../../components/pure-css/grid";
import Textbox from "../../../components/pure-css/form/Textbox";
import { FormState } from "../constants";

const BaseForm = ({ formState, dispatch, disabled }: FormProps<FormState>) => {
  const updateName = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'name',
        value: event.target.value
      }
    });
  };

  return <Grid>
    <Cell size={'1-2'}>
      <Textbox label={'View Name'} type={'text'} value={formState.name || ''}
               onChange={updateName} required={true} disabled={disabled}/>
    </Cell>
    <Cell size={'1-2'}>
      <Textbox label={'View Type'} type={'text'} value={formState.type} required={true} disabled={true}/>
    </Cell>
  </Grid>;
};

export default BaseForm;
