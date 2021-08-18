import React, { ChangeEvent } from "react";
import { clone, pull } from "lodash";
import { Feature, FormProps } from "../constants";
import { Cell, Grid } from "../../../components/pure-css/grid";
import Checkbox from "../../../components/pure-css/form/Checkbox";

const FeatureForm = ({ state, dispatch }: FormProps) => {
  const features = state.formState.features;
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
      <Checkbox label={'Frequency'} onChange={getFeatureToggler('frequency')} inline={true}
                checked={frequency}/>
    </Cell>

    <Cell size={'1-3'}>
      <Checkbox label={'Norm'} onChange={getFeatureToggler('norm')} inline={true} checked={norm}/>
    </Cell>

    <Cell size={'1-3'}>
      <Checkbox label={'Position'} onChange={getFeatureToggler('position')} inline={true}
                checked={position}/>
    </Cell>
  </Grid>;
};

export default FeatureForm;
