import React, { ChangeEvent, useEffect } from "react";
import { FormProps } from "../constants";
import { get, has } from 'lodash';

const NGramForm = ({ formState, updateFormField }: FormProps) => {
  const updateMinLength = (event: ChangeEvent<HTMLInputElement>) => {
    updateFormField('properties.min', parseInt(event.target.value));
  };

  const updateMaxLength = (event: ChangeEvent<HTMLInputElement>) => {
    updateFormField('properties.max', parseInt(event.target.value));
  };

  const updateStartMarker = (event: ChangeEvent<HTMLInputElement>) => {
    updateFormField('properties.startMarker', event.target.value);
  };

  const updateEndMarker = (event: ChangeEvent<HTMLInputElement>) => {
    updateFormField('properties.endMarker', event.target.value);
  };

  const updateStreamType = (event: ChangeEvent<HTMLInputElement>) => {
    updateFormField('properties.streamType', event.target.value);
  };

  const togglePreserve = () => {
    updateFormField('properties.preserveOriginal', !get(formState, ['properties', 'preserveOriginal']));
  };

  const streamTypeProperty = get(formState, ['properties', 'streamType'], 'binary');

  useEffect(() => {
    if (!has(formState, ['properties', 'preserveOriginal'])) {
      updateFormField('properties.preserveOriginal', false);
    }
  }, [formState, updateFormField]);

  return <div className={'pure-g'}>
    <div className={'pure-u-8-24 pure-u-md-8-24 pure-u-lg-8-24 pure-u-xl-8-24'}>
      <label htmlFor={'min'}>Minimum N-Gram Length</label>
      <input id="min" type="number" min={1} placeholder="2" required={true}
             value={get(formState, ['properties', 'min'], '')} onChange={updateMinLength}
             style={{
               height: 'auto',
               width: '90%'
             }}/>
    </div>

    <div className={'pure-u-8-24 pure-u-md-8-24 pure-u-lg-8-24 pure-u-xl-8-24'}>
      <label htmlFor={'max'}>Maximum N-Gram Length</label>
      <input id="max" type="number" min={1} placeholder="3" required={true}
             value={get(formState, ['properties', 'max'], '')} onChange={updateMaxLength}
             style={{
               height: 'auto',
               width: '90%'
             }}/>
    </div>

    <div className={'pure-u-8-24 pure-u-md-8-24 pure-u-lg-8-24 pure-u-xl-8-24'}>
      <label htmlFor={'preserve'} className="pure-checkbox">Preserve Original</label>
      <input id={'preserve'} type={'checkbox'}
             checked={get(formState, ['properties', 'preserveOriginal'], false)}
             onChange={togglePreserve} style={{ width: 'auto' }}/>
    </div>

    <div className={'pure-u-8-24 pure-u-md-8-24 pure-u-lg-8-24 pure-u-xl-8-24'}>
      <label htmlFor={'startMarker'}>Start Marker</label>
      <input id="startMarker" type="text" placeholder={'^'}
             value={get(formState, ['properties', 'startMarker'], '')}
             onChange={updateStartMarker}
             style={{
               height: 'auto',
               width: '90%'
             }}/>
    </div>

    <div className={'pure-u-8-24 pure-u-md-8-24 pure-u-lg-8-24 pure-u-xl-8-24'}>
      <label htmlFor={'endMarker'}>End Marker</label>
      <input id="endMarker" type="text" placeholder={'$'}
             value={get(formState, ['properties', 'endMarker'], '')}
             onChange={updateEndMarker}
             style={{
               height: 'auto',
               width: '90%'
             }}/>
    </div>

    <div className={'pure-u-8-24 pure-u-md-8-24 pure-u-lg-8-24 pure-u-xl-8-24'}>
      <fieldset>
        <legend style={{
          fontSize: '14px',
          marginBottom: 12,
          borderBottom: 'none',
          lineHeight: 'normal',
          color: 'inherit'
        }}>
          Stream Type
        </legend>
        <div className={'pure-g'}>
          <div className={'pure-u-12-24 pure-u-md-12-24 pure-u-lg-12-24 pure-u-xl-12-24'}>
            <label htmlFor="binary" className="pure-radio">
              <input type="radio" id="binary" name="streamType" value="binary" onChange={updateStreamType}
                     style={{
                       width: 'auto',
                       marginBottom: 10
                     }}
                     checked={streamTypeProperty === 'binary'}/> Binary
            </label>
          </div>
          <div className={'pure-u-12-24 pure-u-md-12-24 pure-u-lg-12-24 pure-u-xl-12-24'}>
            <label htmlFor="utf8" className="pure-radio">
              <input type="radio" id="utf8" name="streamType" value="utf8" onChange={updateStreamType}
                     style={{
                       width: 'auto',
                       marginBottom: 10
                     }}
                     checked={streamTypeProperty === 'utf8'}/> UTF8
            </label>
          </div>
        </div>
      </fieldset>
    </div>
  </div>;
};

export default NGramForm;
