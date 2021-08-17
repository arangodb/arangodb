import React, { ChangeEvent } from "react";
import { FormProps, GeoPointState } from "../constants";
import { filter, get, isEmpty, negate } from 'lodash';
import OptionsInput from "./OptionsInput";

const GeoPointForm = ({ state, dispatch }: FormProps) => {
  const updateArray = (event: ChangeEvent<HTMLInputElement>, field: string) => {
    const items = event.target.value.split('.');

    if (filter(items, negate(isEmpty)).length) {
      dispatch({
        type: 'setField',
        field: {
          path: field,
          value: items
        }
      });
    } else {
      dispatch({
        type: 'unsetField',
        field: {
          path: field
        }
      });
    }
  };

  const updateLatitude = (event: ChangeEvent<HTMLInputElement>) => {
    updateArray(event, 'properties.latitude');
  };

  const updateLongitude = (event: ChangeEvent<HTMLInputElement>) => {
    updateArray(event, 'properties.longitude');
  };

  const formState = state.formState as GeoPointState;

  const getArray = (field: string[]) => {
    return get(formState, field, []).join('.');
  };

  const getLatitude = () => getArray(['properties', 'latitude']);
  const getLongitude = () => getArray(['properties', 'longitude']);

  return <div className={'pure-g'}>
    <div className={'pure-u-12-24 pure-u-md-12-24 pure-u-lg-12-24 pure-u-xl-12-24'}>
      <label htmlFor={'latitude'} style={{ cursor: 'default' }}>Latitude Path (One segment per line)</label>
      <input id="latitude" type="text" value={getLatitude()} onChange={updateLatitude}
             style={{
               height: 'auto',
               width: '90%'
             }}/>
    </div>

    <div className={'pure-u-12-24 pure-u-md-12-24 pure-u-lg-12-24 pure-u-xl-12-24'}>
      <label htmlFor={'longitude'} style={{ cursor: 'default' }}>Longitude Path (One segment per line)</label>
      <input id="longitude" type="text" value={getLongitude()} onChange={updateLongitude}
             style={{
               height: 'auto',
               width: '90%'
             }}/>
    </div>

    <div className={'pure-u-1 pure-u-md-1 pure-u-lg-1 pure-u-xl-1'}>
      <OptionsInput state={state} dispatch={dispatch}/>
    </div>
  </div>;
};

export default GeoPointForm;
