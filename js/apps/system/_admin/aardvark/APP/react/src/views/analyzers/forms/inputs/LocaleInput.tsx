import React, { ChangeEvent } from "react";
import { FormProps } from "../../../../utils/constants";
import Textbox from "../../../../components/pure-css/form/Textbox";
import { LocaleProperty } from "../../constants";

const LocaleInput = ({ formState, dispatch, disabled }: FormProps<LocaleProperty>) => {
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
                  required={true} disabled={disabled} value={formState.properties.locale || ''}
                  onChange={updateLocale}/>;
};

export default LocaleInput;
