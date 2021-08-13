import React, { ChangeEvent } from "react";
import { FormProps, StemState } from "../constants";

const StemForm = ({ state, dispatch }: FormProps) => {
  const updateLocale = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.locale',
        value: event.target.value
      }
    });
  };

  const formState = state.formState as StemState;

  return <div className={'pure-g'}>
    <div className={'pure-u-12-24 pure-u-md-12-24 pure-u-lg-12-24 pure-u-xl-12-24'}>
      <label htmlFor={'locale'}>Locale</label>
      <input id="locale" type="text" placeholder="language[_COUNTRY][.encoding][@variant]"
             value={formState.properties.locale} onChange={updateLocale} required={true}
             style={{
               height: 'auto',
               width: '90%'
             }}/>
    </div>
  </div>;
};

export default StemForm;
