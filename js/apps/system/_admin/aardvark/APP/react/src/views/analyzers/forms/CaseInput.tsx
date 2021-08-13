import React, { ChangeEvent } from "react";
import { CaseState, FormProps } from "../constants";

const CaseInput = ({ state, dispatch }: FormProps) => {
  const updateCase = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.case',
        value: event.target.value
      }
    });
  };

  const caseProperty = (state.formState as CaseState).properties.case;

  return <fieldset>
    <legend style={{
      fontSize: '14px',
      marginBottom: 12,
      borderBottom: 'none',
      lineHeight: 'normal',
      color: 'inherit'
    }}>
      Case
    </legend>
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
                 checked={!caseProperty || caseProperty === 'none'}/> None
        </label>
      </div>
    </div>
  </fieldset>;
};

export default CaseInput;
