import { JsonEditor as Editor } from 'jsoneditor-react';
import React, { useState } from 'react';
import Ajv from 'ajv';
import { FormProps, formSchema, FormState, State } from "../constants";
import { Cell, Grid } from "../../../components/pure-css/grid";

const ajv = new Ajv({
  allErrors: true,
  verbose: true,
  discriminator: true,
  $data: true
});
const validate = ajv.compile(formSchema);

type JsonFormProps = Pick<FormProps, 'formState' | 'dispatch'> & Pick<State, 'renderKey'>;

const JsonForm = ({ formState, dispatch, renderKey }: JsonFormProps) => {
  const [formErrors, setFormErrors] = useState<string[]>([]);

  const changeHandler = (json: FormState) => {
    if (validate(json)) {
      dispatch({
        type: 'setFormState',
        formState: json
      });
      dispatch({ type: 'unlockJsonForm' });
      setFormErrors([]);
    } else if (Array.isArray(validate.errors)) {
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
