import React, { ChangeEvent } from "react";
import { AnalyzerTypeState, FormProps } from "../../constants";
import { map } from "lodash";
import Select from "../../../../components/pure-css/form/Select";

type TypeInputProps = FormProps & {
  typeNameMap: {
    [key: string]: string;
  },
  inline?: boolean;
};

const TypeInput = ({ formState, dispatch, disabled, typeNameMap, inline }: TypeInputProps) => {
  const updateType = (event: ChangeEvent<HTMLSelectElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'type',
        value: event.target.value
      }
    });
  };

  return <Select label={'Analyzer Type'} value={(formState as AnalyzerTypeState).type} onChange={updateType}
                 required={true} disabled={disabled} inline={inline}>
    {
      map(typeNameMap, (value, key) => <option key={key} value={key}>{value}</option>)
    }
  </Select>;
};

export default TypeInput;
