import { JsonEditor as Editor } from 'jsoneditor-react';
import React, { useState } from 'react';
import Ajv from 'ajv';
import { formSchema, FormState } from "../constants";
import { FormProps, State } from "../../../utils/constants";
import { Cell, Grid } from "../../../components/pure-css/grid";
import { useJsonFormErrorHandler, useJsonFormUpdateEffect } from "../../../utils/helpers";
import ajvErrors from 'ajv-errors';

const ajv = new Ajv({
  allErrors: true,
  verbose: true,
  discriminator: true,
  $data: true
});
ajvErrors(ajv);
const validate = ajv.compile(formSchema);

type JsonFormProps =
  Pick<FormProps<FormState>, 'formState' | 'dispatch'>
  & Pick<State<FormState>, 'renderKey'>;

const JsonForm = ({ formState, dispatch, renderKey }: JsonFormProps) => {
  const [formErrors, setFormErrors] = useState<string[]>([]);
  const raiseError = useJsonFormErrorHandler(dispatch, setFormErrors);

  useJsonFormUpdateEffect(validate, formState, raiseError);

  const changeHandler = (json: FormState) => {
    if (validate(json)) {
      dispatch({
        type: 'setFormState',
        formState: json
      });
      dispatch({ type: 'unlockJsonForm' });
      setFormErrors([]);
    } else {
      raiseError(validate.errors);
    }
  };

  return <Grid>
    <Cell size={'1'}>
      <Editor value={formState} onChange={changeHandler} mode={'code'} history={true} key={renderKey}/>
    </Cell>
    {
      formErrors.length
        ? <Cell size={'1'} style={{ marginTop: 35 }}>
          <ul style={{ color: 'red' }}>
            {formErrors.map((error, idx) => <li key={idx} style={{ marginBottom: 5 }}>{error}</li>)}
          </ul>
        </Cell>
        : null
    }
  </Grid>;
};

export default JsonForm;
