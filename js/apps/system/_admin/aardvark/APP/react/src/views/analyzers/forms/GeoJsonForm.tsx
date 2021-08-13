import React, { ChangeEvent } from "react";
import { FormProps, GeoJsonState } from "../constants";
import OptionsInput from "./OptionsInput";

const GeoJsonForm = ({ state, dispatch }: FormProps) => {
  const updateType = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.type',
        value: event.target.value
      }
    });
  };

  const formState = state.formState as GeoJsonState;
  const typeProperty = formState.properties.type;

  return <div className={'pure-g'}>
    <div className={'pure-u-12-24 pure-u-md-12-24 pure-u-lg-12-24 pure-u-xl-12-24'}>
      <fieldset>
        <legend style={{
          fontSize: '14px',
          marginBottom: 12,
          marginTop: 14,
          borderBottom: 'none',
          lineHeight: 'normal',
          color: 'inherit'
        }}>
          Type
        </legend>
        <div className={'pure-g'}>
          <div className={'pure-u-8-24 pure-u-md-8-24 pure-u-lg-8-24 pure-u-xl-8-24'}>
            <label htmlFor="shape" className="pure-radio">
              <input type="radio" id="shape" name="type" value="shape" onChange={updateType}
                     style={{
                       width: 'auto',
                       marginBottom: 10
                     }}
                     checked={!typeProperty || typeProperty === 'shape'}/> Shape
            </label>
          </div>
          <div className={'pure-u-8-24 pure-u-md-8-24 pure-u-lg-8-24 pure-u-xl-8-24'}>
            <label htmlFor="centroid" className="pure-radio">
              <input type="radio" id="centroid" name="type" value="centroid" onChange={updateType}
                     style={{
                       width: 'auto',
                       marginBottom: 10
                     }}
                     checked={typeProperty === 'centroid'}/> Centroid
            </label>
          </div>
          <div className={'pure-u-8-24 pure-u-md-8-24 pure-u-lg-8-24 pure-u-xl-8-24'}>
            <label htmlFor="point" className="pure-radio">
              <input type="radio" id="point" name="type" value="point" onChange={updateType}
                     style={{
                       width: 'auto',
                       marginBottom: 10
                     }}
                     checked={typeProperty === 'point'}/> Point
            </label>
          </div>
        </div>
      </fieldset>
    </div>
    <div className={'pure-u-1 pure-u-md-1 pure-u-lg-1 pure-u-xl-1'}>
      <OptionsInput state={state} dispatch={dispatch}/>
    </div>
  </div>;
};

export default GeoJsonForm;
