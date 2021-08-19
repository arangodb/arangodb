import React, { ChangeEvent } from "react";
import { CaseState, FormProps } from "../../constants";
import RadioGroup from "../../../../components/pure-css/form/RadioGroup";

const CaseInput = ({ formState, dispatch, disabled, basePath }: FormProps) => {
  const updateCase = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.case',
        value: event.target.value
      },
      basePath
    });
  };

  const caseProperty = (formState as CaseState).properties.case;

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
  ]} checked={caseProperty || 'none'} disabled={disabled}/>;
};

export default CaseInput;
