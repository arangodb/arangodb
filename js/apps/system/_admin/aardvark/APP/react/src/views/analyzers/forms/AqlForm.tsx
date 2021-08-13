import React, { ChangeEvent } from "react";
import { AqlState, FormProps } from "../constants";

const AqlForm = ({ state, dispatch }: FormProps) => {
  const updateBatchSize = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.batchSize',
        value: parseInt(event.target.value)
      }
    });
  };

  const updateMemoryLimit = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.memoryLimit',
        value: parseInt(event.target.value)
      }
    });
  };

  const updateQueryString = (event: ChangeEvent<HTMLTextAreaElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.queryString',
        value: event.target.value
      }
    });
  };

  const updateReturnType = (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.returnType',
        value: event.target.value
      }
    });
  };

  const formState = state.formState as AqlState;

  const toggleCollapsePositions = () => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.collapsePositions',
        value: !formState.properties.collapsePositions
      }
    });
  };

  const toggleKeepNull = () => {
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.keepNull',
        value: !formState.properties.keepNull
      }
    });
  };

  const returnTypeProperty = formState.properties.returnType;

  return <div className={'pure-g'}>
    <div className={'pure-u-8-24 pure-u-md-8-24 pure-u-lg-8-24 pure-u-xl-8-24'}>
      <label htmlFor={'queryString'}>Query String</label>
      <textarea id="queryString" value={formState.properties.queryString}
                style={{ width: '90%' }} onChange={updateQueryString} rows={4} required={true}/>
    </div>

    <div className={'pure-u-8-24 pure-u-md-8-24 pure-u-lg-8-24 pure-u-xl-8-24'}>
      <div className={'pure-g'}>
        <div className={'pure-u-1 pure-u-md-1 pure-u-lg-1 pure-u-xl-1'}>
          <label htmlFor={'batchSize'}>Batch Size</label>
          <input id="batchSize" type="number" min={1} placeholder="10" required={true}
                 value={formState.properties.batchSize} onChange={updateBatchSize}
                 style={{
                   height: 'auto',
                   width: '90%'
                 }}/>
        </div>
        <div className={'pure-u-1 pure-u-md-1 pure-u-lg-1 pure-u-xl-1'}>
          <label htmlFor={'memoryLimit'}>Memory Limit</label>
          <input id="memoryLimit" type="number" min={1} max={33554432} placeholder="1048576" required={true}
                 value={formState.properties.memoryLimit} onChange={updateMemoryLimit}
                 style={{
                   height: 'auto',
                   width: '90%'
                 }}/>
        </div>
      </div>
    </div>

    <div className={'pure-u-8-24 pure-u-md-8-24 pure-u-lg-8-24 pure-u-xl-8-24'}>
      <div className={'pure-g'}>
        <div className={'pure-u-12-24 pure-u-md-12-24 pure-u-lg-12-24 pure-u-xl-12-24'}>
          <label htmlFor={'collapsePositions'} className="pure-checkbox">Collapse Positions</label>
          <input id={'collapsePositions'} type={'checkbox'}
                 checked={formState.properties.collapsePositions}
                 onChange={toggleCollapsePositions} style={{ width: 'auto' }}/>
        </div>
        <div className={'pure-u-12-24 pure-u-md-12-24 pure-u-lg-12-24 pure-u-xl-12-24'}>
          <label htmlFor={'keepNull'} className="pure-checkbox">Keep Null</label>
          <input id={'keepNull'} type={'checkbox'}
                 checked={formState.properties.keepNull}
                 onChange={toggleKeepNull} style={{ width: 'auto' }}/>
        </div>
        <div className={'pure-u-1 pure-u-md-1 pure-u-lg-1 pure-u-xl-1'}>
          <fieldset>
            <legend style={{
              fontSize: '14px',
              marginBottom: 12,
              marginTop: 14,
              borderBottom: 'none',
              lineHeight: 'normal',
              color: 'inherit'
            }}>
              Return Type
            </legend>
            <div className={'pure-g'}>
              <div className={'pure-u-8-24 pure-u-md-8-24 pure-u-lg-8-24 pure-u-xl-8-24'}>
                <label htmlFor="string" className="pure-radio">
                  <input type="radio" id="string" name="returnType" value="string" onChange={updateReturnType}
                         style={{
                           width: 'auto',
                           marginBottom: 10
                         }}
                         checked={!returnTypeProperty || returnTypeProperty === 'string'}/> String
                </label>
              </div>
              <div className={'pure-u-8-24 pure-u-md-8-24 pure-u-lg-8-24 pure-u-xl-8-24'}>
                <label htmlFor="number" className="pure-radio">
                  <input type="radio" id="number" name="returnType" value="number" onChange={updateReturnType}
                         style={{
                           width: 'auto',
                           marginBottom: 10
                         }}
                         checked={returnTypeProperty === 'number'}/> Number
                </label>
              </div>
              <div className={'pure-u-8-24 pure-u-md-8-24 pure-u-lg-8-24 pure-u-xl-8-24'}>
                <label htmlFor="bool" className="pure-radio">
                  <input type="radio" id="bool" name="returnType" value="bool" onChange={updateReturnType}
                         style={{
                           width: 'auto',
                           marginBottom: 10
                         }}
                         checked={returnTypeProperty === 'bool'}/> Boolean
                </label>
              </div>
            </div>
          </fieldset>
        </div>
      </div>
    </div>
  </div>;
};

export default AqlForm;
