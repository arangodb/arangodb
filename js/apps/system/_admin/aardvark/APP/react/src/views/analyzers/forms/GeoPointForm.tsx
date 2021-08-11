import React, { ChangeEvent } from "react";
import { FormProps } from "../constants";
import { filter, get, isEmpty, negate } from 'lodash';
import OptionsInput from "./OptionsInput";

interface GeoPointFormProps extends FormProps {
  unsetFormField: (field: string) => void;
}

const GeoPointForm = ({ formState, updateFormField, unsetFormField }: GeoPointFormProps) => {
  const updateArray = (event: ChangeEvent<HTMLTextAreaElement>, field: string) => {
    const items = event.target.value.split('\n');

    if (filter(items, negate(isEmpty)).length) {
      updateFormField(field, items);
    } else {
      unsetFormField(field);
    }
  };

  const updateLatitude = (event: ChangeEvent<HTMLTextAreaElement>) => {
    updateArray(event, 'properties.latitude');
  };

  const updateLongitude = (event: ChangeEvent<HTMLTextAreaElement>) => {
    updateArray(event, 'properties.longitude');
  };

  const getArray = (field: string[]) => {
    return get(formState, field, []).join('\n');
  };

  const getLatitude = () => getArray(['properties', 'latitude']);
  const getLongitude = () => getArray(['properties', 'longitude']);

  return <div className={'pure-g'}>
    <div className={'pure-u-12-24 pure-u-md-12-24 pure-u-lg-12-24 pure-u-xl-12-24'}>
      <label htmlFor={'latitude'}>Latitude Path (One segment per line)</label>
      <textarea id="latitude" value={getLatitude()} style={{ width: '90%' }}
                onChange={updateLatitude}/>
    </div>

    <div className={'pure-u-12-24 pure-u-md-12-24 pure-u-lg-12-24 pure-u-xl-12-24'}>
      <label htmlFor={'longitude'}>Longitude Path (One segment per line)</label>
      <textarea id="longitude" value={getLongitude()} style={{ width: '90%' }}
                onChange={updateLongitude}/>
    </div>

    <div className={'pure-u-1 pure-u-md-1 pure-u-lg-1 pure-u-xl-1'}>
      <OptionsInput formState={formState} updateFormField={updateFormField}/>
    </div>
  </div>;
};

export default GeoPointForm;
