import React, { ChangeEvent } from "react";
import { FormProps } from "../../../../utils/constants";
import { AccentProperty } from "../../constants";
import Checkbox from "../../../../components/pure-css/form/Checkbox";

type AccentInputProps = FormProps<AccentProperty> & {
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
                   checked={formState.properties.accent}/>;
};

export default AccentInput;
