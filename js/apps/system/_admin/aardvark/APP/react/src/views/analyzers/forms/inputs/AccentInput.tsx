import React, { ChangeEvent } from "react";
import { FormProps } from "../../constants";
import { get } from "lodash";
import Checkbox from "../../../../components/pure-css/form/Checkbox";

type AccentInputProps = FormProps & {
  inline?: boolean;
};

const AccentInput = ({ formState, dispatch, disabled, inline = true }: AccentInputProps) => {
  const updateAccent = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.accent',
        value: event.target.checked
      }
    });
  };

  return <Checkbox onChange={updateAccent} label={'Accent'} inline={inline} disabled={disabled}
                   checked={get(formState, 'properties.accent', false)}/>;
};

export default AccentInput;
