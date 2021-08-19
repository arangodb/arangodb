import React, { ChangeEvent } from "react";
import { FormProps, LocaleState } from "../../constants";
import Textbox from "../../../../components/pure-css/form/Textbox";

const LocaleInput = ({ formState, dispatch, disabled, basePath }: FormProps) => {
  const updateLocale = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.locale',
        value: event.target.value
      },
      basePath
    });
  };

  const localeProperty = (formState as LocaleState).properties.locale;

  return <Textbox label={
    <>
      Locale&nbsp;
      <a target={'_blank'} href={'https://www.arangodb.com/docs/stable/analyzers.html#stem'} rel="noreferrer">
        <i className={'fa fa-question-circle'}/>
      </a>
    </>
  } type={'text'} placeholder="language[_COUNTRY][.encoding][@variant]" value={localeProperty}
                  onChange={updateLocale} required={true} disabled={disabled}/>;
};

export default LocaleInput;
