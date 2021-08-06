import React, { ChangeEvent } from "react";
import { map } from "lodash";
import { FormProps, typeNameMap } from "../constants";

const BaseForm = ({ formState, updateFormField }: FormProps) => {
  const updateName = (event: ChangeEvent<HTMLInputElement>) => {
    updateFormField('name', event.target.value);
  };

  const updateType = (event: ChangeEvent<HTMLSelectElement>) => {
    updateFormField('type', event.target.value);
  };

  return <div className={'pure-g'}>
    <div className={'pure-u-12-24 pure-u-md-12-24 pure-u-lg-12-24 pure-u-xl-12-24'}>
      <label htmlFor={'analyzer-name'}>Analyzer Name:</label>
      <input id="analyzer-name" type="text" placeholder="Analyzer Name" value={formState.name}
             onChange={updateName} required={true} style={{
        height: 'auto',
        width: 'auto'
      }}/>
    </div>

    <div className={'pure-u-12-24 pure-u-md-12-24 pure-u-lg-12-24 pure-u-xl-12-24'}>
      <label htmlFor={'analyzer-type'}>Analyzer Type:</label>
      <select id="analyzer-type" value={formState.type} style={{ width: 'auto' }}
              onChange={updateType} required={true}>
        {
          map(typeNameMap, (value, key) => <option key={key}
                                                             value={key}>{value}</option>)
        }
      </select>
    </div>
  </div>;
};

export default BaseForm;
