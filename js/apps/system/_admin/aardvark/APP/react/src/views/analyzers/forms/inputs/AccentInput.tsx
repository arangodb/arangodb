import React, { ChangeEvent } from "react";
import { FormProps } from "../../constants";
import { get } from "lodash";
import Checkbox from "../../../../components/pure-css/form/Checkbox";

const AccentInput = ({ formState, dispatch, disabled }: FormProps) => {
  const updateAccent = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.accent',
        value: event.target.checked
      }
    });
  };

  return <Checkbox onChange={updateAccent} label={'Accent'} inline={true} disabled={disabled}
                   checked={get(formState, 'properties.accent', false)}/>;
};

export default AccentInput;
