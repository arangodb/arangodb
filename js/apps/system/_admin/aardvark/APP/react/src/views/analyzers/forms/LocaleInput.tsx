import React, { ChangeEvent } from "react";
import { FormProps } from "../constants";

type LocaleInputProps = {
  formState: {
    properties: {
      locale: string;
    }
  };
} & Pick<FormProps, 'dispatch'>;

const LocaleInput = ({ formState, dispatch }: LocaleInputProps) => {
  const updateLocale = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.locale',
        value: event.target.value
      }
    });
  };

  return <>
    <label htmlFor={'locale'} style={{ cursor: 'default' }}>
      Locale&nbsp;
      <a target={'_blank'} href={'https://www.arangodb.com/docs/stable/analyzers.html#stem'} rel="noreferrer">
        <i className={'fa fa-question-circle'}/>
      </a>
    </label>
    <input id="locale" type="text" placeholder="language[_COUNTRY][.encoding][@variant]"
           value={formState.properties.locale} onChange={updateLocale} required={true}
           style={{
             height: 'auto',
             width: '90%'
           }}/>
  </>;
};

export default LocaleInput;
