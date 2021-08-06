import { JsonEditor as Editor } from 'jsoneditor-react';
import React, { useState } from 'react';
import Ajv from 'ajv';
import { formSchema, FormState } from "../constants";
import { isArray } from "lodash";

const ajv = new Ajv({
  allErrors: true,
  verbose: true
});
const validate = ajv.compile(formSchema);

interface JsonFromProps {
  formState: { [key: string]: any };
  setFormState: (state: FormState) => void;
  setLockJsonForm: (lock: boolean) => void;
  renderKey: string;
}

const JsonForm = ({ formState, setFormState, setLockJsonForm, renderKey }: JsonFromProps) => {
  const [formErrors, setFormErrors] = useState<string[]>([]);

  const changeHandler = (json: FormState) => {
    if (validate(json)) {
      setFormState(json);
      setLockJsonForm(false);
      setFormErrors([]);
    } else if (isArray(validate.errors)) {
      setLockJsonForm(true);

      setFormErrors(validate.errors.map(error => {
        console.log(error);

        return `
          ${error.keyword} error: ${error.instancePath} ${error.message}.
          Schema: ${JSON.stringify(error.params)}
        `;
      }));
    }
  };

  return <div className={'pure-g'}>
    <div className={'pure-u-1 pure-u-md-1 pure-u-lg-1 pure-u-xl-1'}>
      <Editor value={formState} onChange={changeHandler} mode={'code'} history={true} key={renderKey}/>
    </div>
    {
      formErrors.length
        ? <div className={'pure-u-1 pure-u-md-1 pure-u-lg-1 pure-u-xl-1'}>
          <ul style={{ color: 'red' }}>
            {formErrors.map((error, idx) => <li key={idx} style={{ marginBottom: 5 }}>{error}</li>)}
          </ul>
        </div>
        : null
    }
  </div>;
};

export default JsonForm;
