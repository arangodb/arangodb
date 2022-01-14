import React, { ChangeEvent } from "react";
import { FormProps } from '../../../../utils/constants';
import Textbox from "../../../../components/pure-css/form/Textbox";
import { Cell, Grid } from "../../../../components/pure-css/grid";
import { getNumericFieldSetter, getNumericFieldValue } from "../../../../utils/helpers";
import { ModelProperty } from "../../constants";

const ModelInput = ({ formState, dispatch, disabled }: FormProps<ModelProperty>) => {
  const updateModelLocation = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.model_location',
        value: event.target.value
      }
    });
  };

  return <Grid>
    <Cell size={'2-3'}>
      <Textbox label={'Model Location'} type={'text'} value={formState.properties.model_location || ''}
               disabled={disabled} onChange={updateModelLocation} required={true}/>
    </Cell>

    <Cell size={'1-3'}>
      <Textbox label={'Top K'} type={'number'} min={0} max={2147483647} disabled={disabled}
               value={getNumericFieldValue(formState.properties.top_k)}
               onChange={getNumericFieldSetter('properties.top_k', dispatch)}/>
    </Cell>
  </Grid>;
};

export default ModelInput;
