import React, { ChangeEvent } from "react";
import { FormProps } from "../constants";
import { get } from 'lodash';

const CaseInput = ({ formState, updateFormField }: FormProps) => {
  const updateCase = (event: ChangeEvent<HTMLInputElement>) => {
    updateFormField('properties.case', event.target.value);
  };

  const caseProperty = get(formState, ['properties', 'case'], 'none');

  return <div className={'pure-u-1 pure-u-md-1 pure-u-lg-1 pure-u-xl-1'}>
    <div className={'pure-g'}>
      <div className={'pure-u-8-24 pure-u-md-8-24 pure-u-lg-8-24 pure-u-xl-8-24'}>
        <label htmlFor="case-lower" className="pure-radio">
          <input type="radio" id="case-lower" name="case" value="lower" onChange={updateCase}
                 style={{
                   width: 'auto',
                   marginBottom: 10
                 }}
                 checked={caseProperty === 'lower'}/> Lower
        </label>
      </div>
      <div className={'pure-u-8-24 pure-u-md-8-24 pure-u-lg-8-24 pure-u-xl-8-24'}>
        <label htmlFor="case-uppper" className="pure-radio">
          <input type="radio" id="case-uppper" name="case" value="upper" onChange={updateCase}
                 style={{
                   width: 'auto',
                   marginBottom: 10
                 }}
                 checked={caseProperty === 'upper'}/> Upper
        </label>
      </div>
      <div className={'pure-u-8-24 pure-u-md-8-24 pure-u-lg-8-24 pure-u-xl-8-24'}>
        <label htmlFor="case-none" className="pure-radio">
          <input type="radio" id="case-none" name="case" value="none" onChange={updateCase}
                 style={{
                   width: 'auto',
                   marginBottom: 10
                 }}
                 checked={caseProperty === 'none'}/> None
        </label>
      </div>
    </div>
  </div>;
};

export default CaseInput;
