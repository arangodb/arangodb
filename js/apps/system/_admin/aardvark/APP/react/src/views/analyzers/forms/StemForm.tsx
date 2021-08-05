import React, { ChangeEvent } from "react";
import { FormProps } from "../constants";
import { get } from "lodash";

const StemForm = ({ formState, updateFormField }: FormProps) => {
  const updateLocale = (event: ChangeEvent<HTMLInputElement>) => {
    updateFormField('properties.locale', event.target.value);
  };

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
  </div>;
};

export default StemForm;
