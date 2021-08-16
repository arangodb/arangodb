import React, { ChangeEvent } from "react";
import { map } from "lodash";
import { FormProps, typeNameMap } from "../constants";

const BaseForm = ({ state, dispatch }: FormProps) => {
  const updateName = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'name',
        value: event.target.value
      }
    });
  };

  const updateType = (event: ChangeEvent<HTMLSelectElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'type',
        value: event.target.value
      }
    });
  };

  return <div className={'pure-g'}>
    <div className={'pure-u-12-24 pure-u-md-12-24 pure-u-lg-12-24 pure-u-xl-12-24'}>
      <label htmlFor={'analyzer-name'} style={{ cursor: 'default' }}>Analyzer Name:</label>
      <input id="analyzer-name" type="text" placeholder="Analyzer Name" value={state.formState.name}
             onChange={updateName} required={true} style={{
        height: 'auto',
        width: 'auto'
      }}/>
    </div>

    <div className={'pure-u-12-24 pure-u-md-12-24 pure-u-lg-12-24 pure-u-xl-12-24'}>
      <label htmlFor={'analyzer-type'} style={{ cursor: 'default' }}>Analyzer Type:</label>
      <select id="analyzer-type" value={state.formState.type} style={{ width: 'auto' }}
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
