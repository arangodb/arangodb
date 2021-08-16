import React, { ChangeEvent } from "react";
import { FormProps, TextState } from "../constants";
import CaseInput from "./CaseInput";
import { filter, get, isEmpty, negate } from 'lodash';

const EdgeNGramInput = ({ state, dispatch }: FormProps) => {
  const updateMinLength = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.edgeNgram.min',
        value: parseInt(event.target.value)
      }
    });
  };

  const updateMaxLength = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.edgeNgram.max',
        value: parseInt(event.target.value)
      }
    });
  };

  const formState = state.formState as TextState;

  const togglePreserve = () => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.edgeNgram.preserveOriginal',
        value: !get(formState, ['properties', 'edgeNgram', 'preserveOriginal'])
      }
    });
  };

  return <fieldset>
    <legend style={{
      fontSize: '14px',
      marginBottom: 12,
      borderBottom: 'none',
      lineHeight: 'normal',
      color: 'inherit'
    }}>
      Edge N-Gram
    </legend>
    <div className={'pure-g'}>
      <div className={'pure-u-8-24 pure-u-md-8-24 pure-u-lg-8-24 pure-u-xl-8-24'}>
        <label htmlFor={'min'} style={{ cursor: 'default' }}>Minimum N-Gram Length</label>
        <input id="min" type="number" min={1} placeholder="2" required={true}
               value={get(formState, ['properties', 'edgeNgram', 'min'], '')} onChange={updateMinLength}
               style={{
                 height: 'auto',
                 width: '90%'
               }}/>
      </div>

      <div className={'pure-u-8-24 pure-u-md-8-24 pure-u-lg-8-24 pure-u-xl-8-24'}>
        <label htmlFor={'max'} style={{ cursor: 'default' }}>Maximum N-Gram Length</label>
        <input id="max" type="number" min={1} placeholder="3" required={true}
               value={get(formState, ['properties', 'edgeNgram', 'max'], '')} onChange={updateMaxLength}
               style={{
                 height: 'auto',
                 width: '90%'
               }}/>
      </div>

      <div className={'pure-u-8-24 pure-u-md-8-24 pure-u-lg-8-24 pure-u-xl-8-24'}>
        <label htmlFor={'preserve'} className="pure-checkbox">Preserve Original</label>
        <input id={'preserve'} type={'checkbox'}
               checked={get(formState, ['properties', 'edgeNgram', 'preserveOriginal'], false)}
               onChange={togglePreserve} style={{ width: 'auto' }}/>
      </div>
    </div>
  </fieldset>;
};

const TextForm = ({ state, dispatch }: FormProps) => {
  const updateLocale = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.locale',
        value: event.target.value
      }
    });
  };

  const updateStopwords = (event: ChangeEvent<HTMLTextAreaElement>) => {
    const stopwords = event.target.value.split('\n');

    if (filter(stopwords, negate(isEmpty)).length) {
      dispatch({
        type: 'setField',
        field: {
          path: 'properties.stopwords',
          value: stopwords
        }
      });
    } else {
      dispatch({
        type: 'unsetField',
        field: {
          path: 'properties.stopwords'
        }
      });
    }
  };

  const formState = state.formState as TextState;

  const getStopwords = () => {
    return get(formState, ['properties', 'stopwords'], []).join('\n');
  };

  const updateStopwordsPath = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.stopwordsPath',
        value: event.target.value
      }
    });
  };

  const toggleAccent = () => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.accent',
        value: !formState.properties.accent
      }
    });
  };

  const toggleStemming = () => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.stemming',
        value: !formState.properties.stemming
      }
    });
  };

  return <div className={'pure-g'}>
    <div className={'pure-u-12-24 pure-u-md-12-24 pure-u-lg-12-24 pure-u-xl-12-24'}>
      <div className={'pure-g'}>
        <div className={'pure-u-1 pure-u-md-1 pure-u-lg-1 pure-u-xl-1'}>
          <label htmlFor={'locale'} style={{ cursor: 'default' }}>Locale</label>
          <input id="locale" type="text" placeholder="language[_COUNTRY][.encoding][@variant]"
                 value={formState.properties.locale} onChange={updateLocale} required={true}
                 style={{
                   height: 'auto',
                   width: '90%'
                 }}/>
        </div>

        <div className={'pure-u-12-24 pure-u-md-12-24 pure-u-lg-12-24 pure-u-xl-12-24'}>
          <label htmlFor={'stemming'} className="pure-checkbox">
            <input id={'stemming'} type={'checkbox'}
                   checked={formState.properties.stemming}
                   onChange={toggleStemming} style={{ width: 'auto' }}/> Stemming
          </label>
        </div>

        <div className={'pure-u-12-24 pure-u-md-12-24 pure-u-lg-12-24 pure-u-xl-12-24'}>
          <label htmlFor={'accent'} className="pure-checkbox">
            <input id={'accent'} type={'checkbox'} checked={formState.properties.accent}
                   onChange={toggleAccent} style={{ width: 'auto' }}/> Accent
          </label>
        </div>

        <div className={'pure-u-1 pure-u-md-1 pure-u-lg-1 pure-u-xl-1'}>
          <CaseInput state={state} dispatch={dispatch}/>
        </div>
      </div>
    </div>

    <div className={'pure-u-12-24 pure-u-md-12-24 pure-u-lg-12-24 pure-u-xl-12-24'}>
      <div className={'pure-g'}>
        <div className={'pure-u-1 pure-u-md-1 pure-u-lg-1 pure-u-xl-1'}>
          <label htmlFor={'stopwordsPath'} style={{ cursor: 'default' }}>Stopwords Path</label>
          <input id="stopwordsPath" type="text"
                 value={formState.properties.stopwordsPath}
                 onChange={updateStopwordsPath}
                 style={{
                   height: 'auto',
                   width: '90%'
                 }}/>
        </div>

        <div className={'pure-u-1 pure-u-md-1 pure-u-lg-1 pure-u-xl-1'}>
          <label htmlFor={'stopwords'} style={{ cursor: 'default' }}>Stopwords (One per line)</label>
          <textarea id="stopwords" value={getStopwords()} style={{ width: '90%' }}
                    onChange={updateStopwords}/>
        </div>
      </div>
    </div>

    <div className={'pure-u-1 pure-u-md-1 pure-u-lg-1 pure-u-xl-1'}>
      <hr/>
    </div>

    <div className={'pure-u-1 pure-u-md-1 pure-u-lg-1 pure-u-xl-1'}>
      <EdgeNGramInput state={state} dispatch={dispatch}/>
    </div>
  </div>;
};

export default TextForm;
