import { JsonEditor as Editor } from 'jsoneditor-react';
import React, { useState } from 'react';
import Ajv from 'ajv';
import { FormProps, formSchema, FormState } from "../constants";
import { isArray } from "lodash";

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

  return <div className={'pure-g'}>
    <div className={'pure-u-1 pure-u-md-1 pure-u-lg-1 pure-u-xl-1'}>
      <Editor value={state.formState} onChange={changeHandler} mode={'code'} history={true}
              key={state.renderKey}/>
    </div>
    {
      formErrors.length
        ? <div className={'pure-u-1 pure-u-md-1 pure-u-lg-1 pure-u-xl-1'} style={{ marginTop: 35 }}>
          <ul style={{ color: 'red' }}>
            {formErrors.map((error, idx) => <li key={idx} style={{ marginBottom: 5 }}>{error}</li>)}
          </ul>
        </div>
        : null
    }
  </div>;
};

export default JsonForm;
