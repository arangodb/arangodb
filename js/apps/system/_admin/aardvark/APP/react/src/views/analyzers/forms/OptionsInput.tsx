import { FormProps, GeoOptionsState } from "../constants";
import React, { ChangeEvent } from "react";
import { get } from "lodash";

const OptionsInput = ({ state, dispatch }: FormProps) => {
  const updateMaxCells = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.options.maxCells',
        value: parseInt(event.target.value)
      }
    });
  };

  const updateMinLevel = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.options.minLevel',
        value: parseInt(event.target.value)
      }
    });
  };

  const updateMaxLevel = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.options.maxLevel',
        value: parseInt(event.target.value)
      }
    });
  };

  const options = (state.formState as GeoOptionsState).properties.options;

  return <fieldset>
    <legend style={{
      fontSize: '14px',
      marginBottom: 12,
      borderBottom: 'none',
      lineHeight: 'normal',
      color: 'inherit'
    }}>
      Options
    </legend>
    <div className={'pure-g'}>
      <div className={'pure-u-8-24 pure-u-md-8-24 pure-u-lg-8-24 pure-u-xl-8-24'}>
        <label htmlFor={'maxCells'} style={{ cursor: 'default' }}>Max S2 Cells</label>
        <input id="maxCells" type="number" placeholder="20"
               value={get(options, 'maxCells', '')} onChange={updateMaxCells}
               style={{
                 height: 'auto',
                 width: '90%'
               }}/>
      </div>

      <div className={'pure-u-8-24 pure-u-md-8-24 pure-u-lg-8-24 pure-u-xl-8-24'}>
        <label htmlFor={'minLevel'} style={{ cursor: 'default' }}>Least Precise S2 Level</label>
        <input id="minLevel" type="number" placeholder="4"
               value={get(options, 'minLevel', '')} onChange={updateMinLevel}
               style={{
                 height: 'auto',
                 width: '90%'
               }}/>
      </div>

      <div className={'pure-u-8-24 pure-u-md-8-24 pure-u-lg-8-24 pure-u-xl-8-24'}>
        <label htmlFor={'maxLevel'} style={{ cursor: 'default' }}>Most Precise S2 Level</label>
        <input id="maxLevel" type="number" placeholder="23"
               value={get(options, 'maxLevel', '')} onChange={updateMaxLevel}
               style={{
                 height: 'auto',
                 width: '90%'
               }}/>
      </div>
    </div>
  </fieldset>;
};

export default OptionsInput;
