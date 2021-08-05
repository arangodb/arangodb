import React, { ChangeEvent, useEffect } from "react";
import { FormProps } from "../constants";
import { get, has } from 'lodash';

const NGramForm = ({ formState, updateFormField }: FormProps) => {
  const updateMinLength = (event: ChangeEvent<HTMLInputElement>) => {
    updateFormField('properties.min', event.target.value);
  };

  const updateMaxLength = (event: ChangeEvent<HTMLInputElement>) => {
    updateFormField('properties.max', event.target.value);
  };

  const togglePreserve = () => {
    updateFormField('properties.preserveOriginal', !get(formState, ['properties', 'preserveOriginal']));
  };

  useEffect(() => {
    if (!has(formState, ['properties', 'preserveOriginal'])) {
      updateFormField('properties.preserveOriginal', false);
    }
  }, [formState, updateFormField]);

  return <div className={'pure-g'}>
    <div className={'pure-u-12-24 pure-u-md-12-24 pure-u-lg-12-24 pure-u-xl-12-24'}>
      <label htmlFor={'min'}>Minimum N-Gram Length</label>
      <input id="min" type="number" min={1} placeholder="3" required={true}
             value={get(formState, ['properties', 'min'], '')} onChange={updateMinLength}
             style={{
               height: 'auto',
               width: '90%'
             }}/>
    </div>

    <div className={'pure-u-12-24 pure-u-md-12-24 pure-u-lg-12-24 pure-u-xl-12-24'}>
      <label htmlFor={'max'}>Maximum N-Gram Length</label>
      <input id="max" type="number" min={1} placeholder="3" required={true}
             value={get(formState, ['properties', 'max'], '')} onChange={updateMaxLength}
             style={{
               height: 'auto',
               width: '90%'
             }}/>
    </div>

    <div className={'pure-u-8-24 pure-u-md-8-24 pure-u-lg-8-24 pure-u-xl-8-24'}>
      <label htmlFor={'preserve'} className="pure-checkbox">
        <input id={'preserve'} type={'checkbox'}
               checked={get(formState, ['properties', 'preserveOriginal'], false)}
               onChange={togglePreserve} style={{ width: 'auto' }}/> Preserve Original
      </label>
    </div>
  </div>;
};

export default NGramForm;
