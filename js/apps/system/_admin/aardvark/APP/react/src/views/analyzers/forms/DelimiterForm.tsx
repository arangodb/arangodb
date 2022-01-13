import React, { ChangeEvent } from "react";
import { DelimiterState } from "../constants";
import { FormProps } from "../../../utils/constants";
import { Cell, Grid } from "../../../components/pure-css/grid";
import Textbox from "../../../components/pure-css/form/Textbox";

const DelimiterForm = ({ formState, dispatch, disabled }: FormProps<DelimiterState>) => {
  const updateDelimiter = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.delimiter',
        value: event.target.value
      }
    });
  };

  const delimiterProperty = formState.properties.delimiter;

  return <Grid>
    <Cell size={'1-3'}>
      <Textbox label={'Delimiter (characters to split on)'} type={'text'} value={delimiterProperty || ''}
               required={true} onChange={updateDelimiter} disabled={disabled}/>
    </Cell>
  </Grid>;
};

export default DelimiterForm;
