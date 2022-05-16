import React, { ChangeEvent } from "react";
import { AnalyzerTypeState } from "../../constants";
import { FormProps } from "../../../../utils/constants";
import { map } from "lodash";
import Select from "../../../../components/pure-css/form/Select";

declare var versionHelper: { [key: string]: any };
declare var frontendConfig: { [key: string]: any };

type TypeInputProps = FormProps<AnalyzerTypeState> & {
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

  const docuVersion = versionHelper.toDocuVersion(frontendConfig.version.version);

  return <>
    <Select label={'Analyzer Type'} value={formState.type} onChange={updateType}
            required={true} disabled={disabled} inline={inline}>
      {
        map(typeNameMap, (value, key) => <option key={key} value={key}>{value}</option>)
      }
    </Select>
    &nbsp;&nbsp;
    <a target={'_blank'}
       href={`https://www.arangodb.com/docs/${docuVersion}/analyzers.html#${formState.type}`}
       rel="noreferrer">
      <i className={'fa fa-question-circle'}/>
    </a>
  </>;
};

export default TypeInput;
