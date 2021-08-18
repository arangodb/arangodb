import React, { ChangeEvent } from "react";
import { FormProps } from "../../constants";
import Textbox from "../../../../components/pure-css/form/Textbox";

type LocaleInputProps = {
  formState: {
    properties: {
      locale: string;
    }
  };
} & Pick<FormProps, 'dispatch' | 'disabled'>;

const LocaleInput = ({ formState, dispatch, disabled }: LocaleInputProps) => {
  const updateLocale = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.locale',
        value: event.target.value
      }
    });
  };

  return <Textbox label={
    <>
      Locale&nbsp;
      <a target={'_blank'} href={'https://www.arangodb.com/docs/stable/analyzers.html#stem'} rel="noreferrer">
        <i className={'fa fa-question-circle'}/>
      </a>
    </>
  } type={'text'} placeholder="language[_COUNTRY][.encoding][@variant]" value={formState.properties.locale}
                  onChange={updateLocale} required={true} disabled={disabled}/>;
};

export default LocaleInput;
