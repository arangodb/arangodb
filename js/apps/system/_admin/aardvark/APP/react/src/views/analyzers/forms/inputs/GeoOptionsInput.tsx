import { FormProps, GeoOptions } from "../../constants";
import React, { ChangeEvent } from "react";
import { Cell, Grid } from "../../../../components/pure-css/grid";
import Fieldset from "../../../../components/pure-css/form/Fieldset";
import Textbox from "../../../../components/pure-css/form/Textbox";
import { get } from "lodash";
import { setIntegerField } from "../../helpers";

const GeoOptionsInput = ({ formState, dispatch, disabled }: FormProps) => {
  const getNumericFieldSetter = (field: string) => (event: ChangeEvent<HTMLInputElement>) => {
    setIntegerField(field, event.target.value, dispatch);
  };

  const geoOptions = get(formState, 'properties.options', {}) as GeoOptions;

  return <Fieldset legend={'Options'}>
    <Grid>
      <Cell size={'1-3'}>
        <Textbox label={'Max S2 Cells'} type={'number'} placeholder={disabled ? '' : '20'} disabled={disabled}
                 onChange={getNumericFieldSetter('properties.options.maxCells')}
                 value={geoOptions.maxCells || ''}/>
      </Cell>

      <Cell size={'1-3'}>
        <Textbox label={'Least Precise S2 Level'} type={'number'} placeholder={disabled ? '' : '4'}
                 onChange={getNumericFieldSetter('properties.options.minLevel')}
                 value={geoOptions.minLevel || ''} disabled={disabled}/>
      </Cell>

      <Cell size={'1-3'}>
        <Textbox label={'Most Precise S2 Level'} type={'number'} placeholder={disabled ? '' : '23'}
                 onChange={getNumericFieldSetter('properties.options.maxLevel')}
                 value={geoOptions.maxLevel || ''} disabled={disabled}/>
      </Cell>
    </Grid>
  </Fieldset>;
};

export default GeoOptionsInput;
