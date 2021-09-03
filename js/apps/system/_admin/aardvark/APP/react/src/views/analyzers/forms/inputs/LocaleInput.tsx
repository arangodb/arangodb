import React, { ChangeEvent } from "react";
import { FormProps } from "../../constants";
import Textbox from "../../../../components/pure-css/form/Textbox";
import { get } from "lodash";

const LocaleInput = ({ formState, dispatch, disabled }: FormProps) => {
  const updateLocale = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.locale',
        value: event.target.value
      }
    });
  };

  return <Textbox label={'Locale'} type={'text'} placeholder="language[_COUNTRY][.encoding][@variant]"
                  required={true} disabled={disabled} value={get(formState, 'properties.locale', '')}
                  onChange={updateLocale}/>;
};

export default LocaleInput;
