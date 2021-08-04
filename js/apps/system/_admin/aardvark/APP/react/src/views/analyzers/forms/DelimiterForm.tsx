import React, { ChangeEvent } from "react";
import { FormProps } from "../constants";

const DelimiterForm = ({ formState, updateFormField }: FormProps) => {
  const updateDelimiter = (event: ChangeEvent<HTMLInputElement>) => {
    updateFormField('properties.delimiter', event.target.value);
  };

  return <div className={'pure-u-8-24 pure-u-md-8-24 pure-u-lg-8-24 pure-u-xl-8-24'}>
    <label htmlFor={'delimiter'}>Delimiter:</label>
    <input id="delimiter" type="text" placeholder="Delimiting Characters"
           value={formState.properties.delimiter || ''} onChange={updateDelimiter}
           style={{
             height: 'auto',
             width: 'auto'
           }}/>
  </div>;
};

export default DelimiterForm;
