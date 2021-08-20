import React, { ChangeEvent } from "react";
import { FormProps } from "../../constants";
import RadioGroup from "../../../../components/pure-css/form/RadioGroup";
import { get } from "lodash";

const CaseInput = ({ formState, dispatch, disabled }: FormProps) => {
  const updateCase = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.case',
        value: event.target.value
      }
    });
  };

  return <RadioGroup legend={'Case'} onChange={updateCase} name={'case'} items={[
    {
      label: 'Lower',
      value: 'lower'
    },
    {
      label: 'Upper',
      value: 'upper'
    },
    {
      label: 'None',
      value: 'none'
    }
  ]} checked={get(formState, 'properties.case', 'none')} disabled={disabled}/>;
};

export default CaseInput;
