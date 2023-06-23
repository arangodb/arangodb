import React, { ChangeEvent } from "react";
import { FormProps } from "../../../../utils/constants";
import Textbox from "../../../../components/pure-css/form/Textbox";
import { LocaleProperty } from "../../constants";

type LocaleFormProps = FormProps<LocaleProperty>;

type LocaleInputProps = LocaleFormProps & {
  placeholder?: string;
};

const LocaleInput = ({ formState, dispatch, disabled, placeholder }: LocaleInputProps) => {
  const updateLocale = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.locale',
        value: event.target.value
      }
    });
  };

  return <Textbox label={'Locale'} type={'text'} placeholder={placeholder}
                  required={true} disabled={disabled} value={formState.properties.locale || ''}
                  onChange={updateLocale}/>;
};

export default LocaleInput;
