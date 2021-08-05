import React, { ChangeEvent, useEffect } from "react";
import { clone, get, has, pull } from "lodash";
import { FormProps } from "../constants";

const FeatureForm = ({ formState, updateFormField }: FormProps) => {
  const features = get(formState, 'features', []);
  const frequency = features.includes('frequency');
  const norm = features.includes('norm');
  const position = features.includes('position');

  const getFeatureToggler = (feature: string) => {
    return (event: ChangeEvent<HTMLInputElement>) => {
      const formFeatures = clone(features);
      if (event.target.checked) {
        formFeatures.push(feature);
      } else {
        pull(formFeatures, feature);
      }

      updateFormField('features', formFeatures);
    };
  };

  useEffect(() => {
    if (!has(formState, 'features')) {
      updateFormField('features', []);
    }
  }, [formState, updateFormField]);

  return <div className={'pure-g'}>
    <div className={'pure-u-8-24 pure-u-md-8-24 pure-u-lg-8-24 pure-u-xl-8-24'}>
      <label htmlFor={'frequency'} className="pure-checkbox">
        <input id={'frequency'} type={'checkbox'} checked={frequency}
               onChange={getFeatureToggler('frequency')} style={{ width: 'auto' }}/> Frequency
      </label>
    </div>

    <div className={'pure-u-8-24 pure-u-md-8-24 pure-u-lg-8-24 pure-u-xl-8-24'}>
      <label htmlFor={'norm'} className="pure-checkbox">
        <input id={'norm'} type={'checkbox'} checked={norm} onChange={getFeatureToggler('norm')}
               style={{ width: 'auto' }}/> Norm
      </label>
    </div>

    <div className={'pure-u-8-24 pure-u-md-8-24 pure-u-lg-8-24 pure-u-xl-8-24'}>
      <label htmlFor={'position'} className="pure-checkbox">
        <input id={'position'} type={'checkbox'} checked={position} onChange={getFeatureToggler('position')}
               style={{ width: 'auto' }}/> Position
      </label>
    </div>
  </div>;
};

export default FeatureForm;
