import { FormProps, NGramBase } from "../../constants";
import React, { ChangeEvent } from "react";
import { Cell, Grid } from "../../../../components/pure-css/grid";
import Textbox from "../../../../components/pure-css/form/Textbox";
import Checkbox from "../../../../components/pure-css/form/Checkbox";
import { get } from "lodash";
import { setIntegerField } from "../../helpers";

type NGramInputProps = FormProps & {
  basePath: string;
  required?: boolean
};

const NGramInput = ({ formState, dispatch, disabled, basePath, required = true }: NGramInputProps) => {
  const getNumericFieldSetter = (field: string) => (event: ChangeEvent<HTMLInputElement>) => {
    setIntegerField(field, event.target.value, dispatch, basePath);
  };

  const updatePreserve = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'preserveOriginal',
        value: event.target.checked
      },
      basePath
    });
  };

  const ngramBase = get(formState, basePath, {}) as NGramBase;

  return <Grid>
    <Cell size={'1-3'}>
      <Textbox label={'Minimum N-Gram Length'} type={'number'} min={1} disabled={disabled}
               required={required} value={ngramBase.min || ''} onChange={getNumericFieldSetter('min')}/>
    </Cell>

    <Cell size={'1-3'}>
      <Textbox label={'Maximum N-Gram Length'} type={'number'} min={1} disabled={disabled}
               required={required} value={ngramBase.max || ''} onChange={getNumericFieldSetter('max')}/>
    </Cell>

    <Cell size={'1-3'}>
      <Checkbox onChange={updatePreserve} label={'Preserve Original'} disabled={disabled}
                checked={ngramBase.preserveOriginal || false}/>
    </Cell>
  </Grid>;
};

export default NGramInput;
