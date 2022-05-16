import { NGramBaseProperty } from "../../constants";
import { FormProps } from "../../../../utils/constants";
import React from "react";
import { Cell, Grid } from "../../../../components/pure-css/grid";
import Textbox from "../../../../components/pure-css/form/Textbox";
import Checkbox from "../../../../components/pure-css/form/Checkbox";
import {
  getBooleanFieldSetter,
  getNumericFieldSetter,
  getNumericFieldValue
} from "../../../../utils/helpers";

type NGramFormProps = FormProps<NGramBaseProperty>;

type NGramInputProps = NGramFormProps & {
  basePath: string;
  required?: boolean
};

const NGramInput = ({ formState, dispatch, disabled, basePath, required = true }: NGramInputProps) =>
  <Grid>
    <Cell size={'1-3'}>
      <Textbox label={'Minimum N-Gram Length'} type={'number'} min={1} disabled={disabled}
               required={required} value={getNumericFieldValue(formState.min)}
               onChange={getNumericFieldSetter('min', dispatch, basePath)}/>
    </Cell>

    <Cell size={'1-3'}>
      <Textbox label={'Maximum N-Gram Length'} type={'number'} min={1} disabled={disabled}
               required={required} value={getNumericFieldValue(formState.max)}
               onChange={getNumericFieldSetter('max', dispatch, basePath)}/>
    </Cell>

    <Cell size={'1-3'}>
      <Checkbox onChange={getBooleanFieldSetter('preserveOriginal', dispatch, basePath)} label={'Preserve' +
      ' Original'} disabled={disabled}
                checked={formState.preserveOriginal || false}/>
    </Cell>
  </Grid>;

export default NGramInput;
