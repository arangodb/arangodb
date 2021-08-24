import React, { ChangeEvent } from "react";
import { clone, pull } from "lodash";
import { BaseFormState, Feature, FormProps } from "../constants";
import { Cell, Grid } from "../../../components/pure-css/grid";
import Checkbox from "../../../components/pure-css/form/Checkbox";

const FeatureForm = ({ formState, dispatch, disabled }: FormProps) => {
  const features = (formState as BaseFormState).features;
  const frequency = features.includes('frequency');
  const norm = features.includes('norm');
  const position = features.includes('position');

  const getFeatureToggler = (feature: Feature) => {
    return (event: ChangeEvent<HTMLInputElement>) => {
      const formFeatures = clone(features);
      if (event.target.checked) {
        formFeatures.push(feature);
      } else {
        pull(formFeatures, feature);
      }

      dispatch({
        type: 'setField',
        field: {
          path: 'features',
          value: formFeatures
        }
      });
    };
  };

  return <Grid>
    <Cell size={'1-3'}>
      <Checkbox label={'Frequency'} onChange={getFeatureToggler('frequency')} checked={frequency}
                disabled={disabled}/>
    </Cell>

    <Cell size={'1-3'}>
      <Checkbox label={'Norm'} onChange={getFeatureToggler('norm')} checked={norm}
                disabled={disabled}/>
    </Cell>

    <Cell size={'1-3'}>
      <Checkbox label={'Position'} onChange={getFeatureToggler('position')} checked={position}
                disabled={disabled}/>
    </Cell>
  </Grid>;
};

export default FeatureForm;
