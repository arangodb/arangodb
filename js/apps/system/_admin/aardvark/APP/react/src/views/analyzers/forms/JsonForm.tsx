import { JsonEditor as Editor } from 'jsoneditor-react';
import React, { useState } from 'react';
import Ajv from 'ajv';
import { FormProps, formSchema, FormState } from "../constants";
import { isArray } from "lodash";
import { Cell, Grid } from "../../../components/pure-css/grid";

const ajv = new Ajv({
  allErrors: true,
  verbose: true,
  discriminator: true,
  $data: true
});
const validate = ajv.compile(formSchema);

const JsonForm = ({ state, dispatch }: FormProps) => {
  const [formErrors, setFormErrors] = useState<string[]>([]);

  const changeHandler = (json: FormState) => {
    if (validate(json)) {
      dispatch({
        type: 'setFormState',
        formState: json
      });
      dispatch({ type: 'unlockJsonForm' });
      setFormErrors([]);
    } else if (isArray(validate.errors)) {
      dispatch({ type: 'lockJsonForm' });

      setFormErrors(validate.errors.map(error =>
        `
          ${error.keyword} error: ${error.instancePath} ${error.message}.
          Schema: ${JSON.stringify(error.params)}
        `
      ));
    }
  };

  return <Grid>
    <Cell size={'1'}>
      <Editor value={state.formState} onChange={changeHandler} mode={'code'} history={true}
              key={state.renderKey}/>
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
