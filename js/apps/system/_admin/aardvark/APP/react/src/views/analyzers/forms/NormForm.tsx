import React, { ChangeEvent } from "react";
import { FormProps, NormState } from "../constants";
import CaseInput from "./CaseInput";

const NormForm = ({ state, dispatch }: FormProps) => {

  const updateLocale = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.locale',
        value: event.target.value
      }
    });
  };

  const formState = state.formState as NormState;

  const toggleAccent = () => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.accent',
        value: !formState.properties.accent
      }
    });
  };

  return <div className={'pure-g'}>
      <div className={'pure-u-12-24 pure-u-md-12-24 pure-u-lg-12-24 pure-u-xl-12-24'}>
        <label htmlFor={'locale'} style={{ cursor: 'default' }}>Locale</label>
        <input id="locale" type="text" placeholder="language[_COUNTRY][.encoding][@variant]"
               value={formState.properties.locale} onChange={updateLocale} required={true}
               style={{
                 height: 'auto',
                 width: '90%'
               }}/>
      </div>

      <div className={'pure-u-12-24 pure-u-md-12-24 pure-u-lg-12-24 pure-u-xl-12-24'}>
        <CaseInput state={state} dispatch={dispatch}/>
      </div>

      <div className={'pure-u-8-24 pure-u-md-8-24 pure-u-lg-8-24 pure-u-xl-8-24'}>
        <label htmlFor={'accent'} className="pure-checkbox">
          <input id={'accent'} type={'checkbox'} checked={formState.properties.accent}
                 onChange={toggleAccent} style={{ width: 'auto' }}/> Accent
        </label>
      </div>

    </div>;
};

export default NormForm;
