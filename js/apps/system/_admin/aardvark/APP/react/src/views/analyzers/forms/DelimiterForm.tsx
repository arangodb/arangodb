import React, { ChangeEvent } from "react";
import { DelimiterState, FormProps } from "../constants";

const DelimiterForm = ({ state, dispatch }: FormProps) => {
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

  return <div className={'pure-g'}>
    <div className={'pure-u-8-24 pure-u-md-8-24 pure-u-lg-8-24 pure-u-xl-8-24'}>
      <label htmlFor={'delimiter'} style={{ cursor: 'default' }}>
        Delimiter (characters to split on)&nbsp;
        <a target={'_blank'} href={'https://www.arangodb.com/docs/stable/analyzers.html#delimiter'}
           rel="noreferrer">
          <i className={'fa fa-question-circle'}/>
        </a>
      </label>
      <input id="delimiter" type="text" placeholder="Delimiting Characters" required={true}
             value={formState.properties.delimiter} onChange={updateDelimiter}
             style={{
               height: 'auto',
               width: 'auto'
             }}/>
    </div>
  </div>;
};

export default DelimiterForm;
