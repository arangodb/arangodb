import React from "react";
import { DispatchArgs, FormProps, PipelineStates, typeNameMap } from "../constants";
import { Cell, Grid } from "../../../components/pure-css/grid";
import { getForm } from "../helpers";
import { omit } from "lodash";
import TypeInput from "./inputs/TypeInput";
import styled from 'styled-components';
import { getPath } from "../../../utils/helpers";
import { ArangoTable, ArangoTD } from "../../../components/arango/table";

const StyledButton = styled.button`
  &&& {
    margin-top: 10px;
  }
`;
const StyledIcon = styled.i`
  &&& {
    margin-left: auto;
  }
`;
const restrictedTypeNameMap = omit(typeNameMap, 'geojson', 'geopoint', 'pipeline', 'identity');

const PipelineForm = ({ formState, dispatch, disabled }: FormProps) => {
  const items = (formState as PipelineStates).properties.pipeline;

  const removeItem = (index: number) => {
    const tempItems = items.slice();

    tempItems.splice(index, 1);
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.pipeline',
        value: tempItems
      }
    });
  };

  const getRemover = (index: number) => () => {
    removeItem(index);
  };

  const moveUp = (index: number) => {
    const tempItems = items.slice();
    const item = tempItems[index];
    const prevItem = tempItems[index - 1];

    tempItems.splice(index - 1, 2, item, prevItem);
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.pipeline',
        value: tempItems
      }
    });
  };

  const moveDown = (index: number) => {
    const tempItems = items.slice();
    const item = tempItems[index];
    const nextItem = tempItems[index + 1];

    tempItems.splice(index, 2, nextItem, item);
    dispatch({
      type: 'setField',
      field: {
        path: 'properties.pipeline',
        value: tempItems
      }
    });
  };

  const getShifter = (direction: 'up' | 'down', index: number) => {
    switch (direction) {
      case 'up':
        return () => {
          moveUp(index);
        };
      case 'down':
        return () => {
          moveDown(index);
        };
    }
  };

  const addAnalyzer = () => {
    const analyzer = {
      type: 'delimiter',
      properties: {
        delimiter: ''
      }
    };

    dispatch({
      type: 'setField',
      field: {
        path: `properties.pipeline[${items.length}]`,
        value: analyzer
      }
    });
  };

  const getWrappedDispatch = (index: number) => (action: DispatchArgs) => {
    action.basePath = getPath(`properties.pipeline[${index}]`, action.basePath);
    dispatch(action);
  };

  return <Grid>
    {
      disabled
        ? null
        : <Cell size={'1'}>
          <button className={'button-warning'} onClick={addAnalyzer}>
            <i className={'fa fa-plus'}/>&nbsp;Add Analyzer
          </button>
        </Cell>
    }
    <Cell size={'1'}>
      <ArangoTable style={{
        marginTop: 5,
        marginBottom: 5
      }}>
        <tbody>
        {
          items.map((item, idx) => {
            const isLast = idx === items.length - 1;
            const itemDispatch = getWrappedDispatch(idx);

            return <tr key={idx} style={{ borderBottom: '1px  solid #DDD' }}>
              <ArangoTD seq={0} valign={'middle'}>{idx + 1}.</ArangoTD>
              <ArangoTD seq={1}>
                <Grid>
                  <Cell size={'3-4'}>
                    <TypeInput formState={item} dispatch={itemDispatch} inline={true} key={idx}
                               typeNameMap={restrictedTypeNameMap} disabled={disabled}/>
                  </Cell>
                  {
                    disabled
                      ? null
                      : <Cell size={'1-4'}>
                        <StyledButton className={'button-danger'} onClick={getRemover(idx)}>
                          <StyledIcon className={'fa fa-trash-o'}/>
                        </StyledButton>&nbsp;
                        <StyledButton className={'button-warning'} onClick={getShifter('up', idx)}
                                      disabled={idx === 0}>
                          <StyledIcon className={'fa fa-arrow-up'}/>
                        </StyledButton>&nbsp;
                        <StyledButton className={'button-warning'} onClick={getShifter('down', idx)}
                                      disabled={isLast}>
                          <StyledIcon className={'fa fa-arrow-down'}/>
                        </StyledButton>
                      </Cell>
                  }
                  <Cell size={'1'}>
                    {
                      getForm({
                        formState: item,
                        dispatch: itemDispatch,
                        disabled
                      })
                    }
                  </Cell>
                </Grid>
              </ArangoTD>
            </tr>;
          })
        }
        </tbody>
      </ArangoTable>
    </Cell>
    {
      disabled || items.length === 0
        ? null
        : <Cell size={'1'}>
          <button className={'button-warning'} onClick={addAnalyzer}>
            <i className={'fa fa-plus'}/>&nbsp;Add Analyzer
          </button>
        </Cell>
    }
  </Grid>;
};

export default PipelineForm;
