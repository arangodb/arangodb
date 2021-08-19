import React, { ChangeEvent } from "react";
import { FormProps } from "../../constants";
import { getPath } from "../../helpers";
import { get } from "lodash";
import Checkbox from "../../../../components/pure-css/form/Checkbox";

const AccentInput = ({ formState, dispatch, disabled, basePath }: FormProps) => {
  const updateAccent = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'accent',
        value: event.target.checked
      },
      basePath
    });
  };

  const accentProperty = get(formState, getPath(basePath, 'accent'));

  return <Checkbox onChange={updateAccent} label={'Accent'} inline={true} disabled={disabled}
                   checked={accentProperty}/>;
};

export default AccentInput;
