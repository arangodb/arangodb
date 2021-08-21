import React, { ChangeEvent } from "react";
import { FormProps } from "../../constants";
import RadioGroup from "../../../../components/pure-css/form/RadioGroup";
import { get } from "lodash";

type CaseInputProps = FormProps & {
  defaultValue?: string;
};

const CaseInput = ({ formState, dispatch, disabled, defaultValue = 'none' }: CaseInputProps) => {
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
  ]} checked={get(formState, 'properties.case', defaultValue)} disabled={disabled}/>;
};

export default CaseInput;
