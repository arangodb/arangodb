import { JsonEditor as Editor } from 'jsoneditor-react';
import React from 'react';

declare var ace: any;

interface JsonFromProps {
  formState: { [key: string]: any };
  setFormState: Function;
}

const JsonForm = ({ formState, setFormState }: JsonFromProps) =>
  <div style={{ overflow: 'auto' }}>
    <Editor value={formState} onChange={setFormState} ace={ace} mode={'code'} history={true}
            allowedModes={['code', 'tree']} navigationBar={false}/>
  </div>;

export default JsonForm;
