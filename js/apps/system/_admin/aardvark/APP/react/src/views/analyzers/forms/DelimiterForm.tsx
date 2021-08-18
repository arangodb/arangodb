import React, { ChangeEvent } from "react";
import { DelimiterState, FormProps } from "../constants";
import { Cell, Grid } from "../../../components/pure-css/grid";
import Textbox from "../../../components/pure-css/form/Textbox";

const DelimiterForm = ({ state, dispatch, disabled }: FormProps) => {
  const updateDelimiter = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.delimiter',
        value: event.target.value
      }
    });
  };

  const formState = state.formState as DelimiterState;

  return <Grid>
    <Cell size={'1-3'}>
      <Textbox label={
        <>
          Delimiter (characters to split on)&nbsp;
          <a target={'_blank'} href={'https://www.arangodb.com/docs/stable/analyzers.html#delimiter'}
             rel="noreferrer">
            <i className={'fa fa-question-circle'}/>
          </a>
        </>
      } type={'text'} placeholder="Delimiting Characters" required={true} onChange={updateDelimiter}
               value={formState.properties.delimiter} disabled={disabled}/>
    </Cell>
  </Grid>;
};

export default DelimiterForm;
