import React, { ChangeEvent } from "react";
import { clone, pull } from "lodash";
import { FormProps } from "../constants";

const FeatureForm = ({ formState, updateFormField }: FormProps) => {
  const frequency = formState.features.includes('frequency');
  const norm = formState.features.includes('norm');
  const position = formState.features.includes('position');

  const getFeatureToggler = (feature: string) => {
    return (event: ChangeEvent<HTMLInputElement>) => {
      const formFeatures = clone(formState.features);
      if (event.target.checked) {
        formFeatures.push(feature);
      } else {
        pull(formFeatures, feature);
      }

      updateFormField('features', formFeatures);
    };
  };

  return <>
    <div className={'pure-u-8-24 pure-u-md-8-24 pure-u-lg-8-24 pure-u-xl-8-24'}>
      <label htmlFor={'frequency'}>
        Frequency: <input id={'frequency'} type={'checkbox'} checked={frequency}
                          onChange={getFeatureToggler('frequency')} style={{ width: 'auto' }}/>
      </label>
    </div>

    <div className={'pure-u-8-24 pure-u-md-8-24 pure-u-lg-8-24 pure-u-xl-8-24'}>
      <label htmlFor={'norm'}>
        Norm: <input id={'norm'} type={'checkbox'} checked={norm}
                     onChange={getFeatureToggler('norm')} style={{ width: 'auto' }}/>
      </label>
    </div>

    <div className={'pure-u-8-24 pure-u-md-8-24 pure-u-lg-8-24 pure-u-xl-8-24'}>
      <label htmlFor={'position'}>
        Position: <input id={'position'} type={'checkbox'} checked={position}
                         onChange={getFeatureToggler('position')} style={{ width: 'auto' }}/>
      </label>
    </div>
  </>;
};

export default FeatureForm;
