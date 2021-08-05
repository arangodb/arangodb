import React, { ChangeEvent, useEffect } from "react";
import { FormProps } from "../constants";
import CaseInput from "./CaseInput";
import { get, has } from 'lodash';

const NormForm = ({ formState, updateFormField }: FormProps) => {
  const updateLocale = (event: ChangeEvent<HTMLInputElement>) => {
    updateFormField('properties.locale', event.target.value);
  };

  const toggleAccent = () => {
    updateFormField('properties.accent', !get(formState, ['properties', 'accent']));
  };

  useEffect(() => {
    if (!has(formState, ['properties', 'accent'])) {
      updateFormField('properties.accent', false);
    }
  }, [formState, updateFormField]);

  return <div className={'pure-g'}>
    <div className={'pure-u-12-24 pure-u-md-12-24 pure-u-lg-12-24 pure-u-xl-12-24'}>
      <label htmlFor={'locale'}>Locale</label>
      <input id="locale" type="text" placeholder="language[_COUNTRY][.encoding][@variant]"
             value={get(formState, ['properties', 'locale'], '')} onChange={updateLocale} required={true}
             style={{
               height: 'auto',
               width: '90%'
             }}/>
    </div>

    <div className={'pure-u-12-24 pure-u-md-12-24 pure-u-lg-12-24 pure-u-xl-12-24'}>
      <div className={'pure-g'}>
        <div className={'pure-u-1 pure-u-md-1 pure-u-lg-1 pure-u-xl-1'}>
          <label htmlFor={'accent'} className="pure-checkbox">
            <input id={'accent'} type={'checkbox'} checked={get(formState, ['properties', 'accent'], false)}
                   onChange={toggleAccent} style={{ width: 'auto' }}/> Accent
          </label>
        </div>
        <CaseInput formState={formState} updateFormField={updateFormField}/>
      </div>
    </div>
  </div>;
};

export default NormForm;
