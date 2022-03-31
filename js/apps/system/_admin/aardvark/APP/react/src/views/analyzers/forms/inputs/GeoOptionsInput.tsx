import { GeoOptionsProperty } from "../../constants";
import { FormProps } from "../../../../utils/constants";
import React from "react";
import { Cell, Grid } from "../../../../components/pure-css/grid";
import Fieldset from "../../../../components/pure-css/form/Fieldset";
import Textbox from "../../../../components/pure-css/form/Textbox";
import { getNumericFieldSetter, getNumericFieldValue } from "../../../../utils/helpers";

const GeoOptionsInput = ({ formState, dispatch, disabled }: FormProps<GeoOptionsProperty>) => {
  const geoOptions = formState.properties.options || {};

  return <Fieldset legend={'Options'}>
    <Grid>
      <Cell size={'1-3'}>
        <Textbox label={'Max S2 Cells'} type={'number'} disabled={disabled}
                 value={getNumericFieldValue(geoOptions.maxCells)}
                 onChange={getNumericFieldSetter('properties.options.maxCells', dispatch)}/>
      </Cell>

      <Cell size={'1-3'}>
        <Textbox label={'Least Precise S2 Level'} type={'number'} disabled={disabled}
                 value={getNumericFieldValue(geoOptions.minLevel)}
                 onChange={getNumericFieldSetter('properties.options.minLevel', dispatch)}/>
      </Cell>

      <Cell size={'1-3'}>
        <Textbox label={'Most Precise S2 Level'} type={'number'} disabled={disabled}
                 value={getNumericFieldValue(geoOptions.maxLevel)}
                 onChange={getNumericFieldSetter('properties.options.maxLevel', dispatch)}/>
      </Cell>
    </Grid>
  </Fieldset>;
};

export default GeoOptionsInput;
