import React from "react";
import { DispatchArgs, FormProps, PipelineStates, typeNameMap } from "../constants";
import { Cell, Grid } from "../../../components/pure-css/grid";
import { getForm } from "../helpers";
import { omit } from "lodash";
import TypeInput from "./inputs/TypeInput";
import styled from 'styled-components';
import { Int } from "../../../utils/constants";
import { getPath } from "../../../utils/helpers";

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

  const removeItem = (index: Int) => {
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

  const getRemover = (index: Int) => () => {
    removeItem(index);
  };

  const moveUp = (index: Int) => {
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

  const moveDown = (index: Int) => {
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

  const getShifter = (direction: 'up' | 'down', index: Int) => {
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

  const getWrappedDispatch = (index: Int) => (action: DispatchArgs) => {
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
      <table className={'arango-table'} style={{
        marginTop: 5,
        marginBottom: 5
      }}>
        <tbody>
        {
          items.map((item, idx) => {
            const isLast = idx === items.length - 1;
            const index = idx as Int;
            const itemDispatch = getWrappedDispatch(index);

            return <tr key={idx} style={{ borderBottom: '1px  solid #DDD' }}>
              <td className={'arango-table-td table-cell0'} valign={'middle'}>{idx + 1}.</td>
              <td className={`arango-table-td table-cell1`}>
                <Grid>
                  <Cell size={'3-4'}>
                    <TypeInput formState={item} dispatch={itemDispatch} inline={true} key={idx}
                               typeNameMap={restrictedTypeNameMap} disabled={disabled}/>
                  </Cell>
                  {
                    disabled
                      ? null
                      : <Cell size={'1-4'}>
                        <StyledButton className={'button-danger'} onClick={getRemover(index)}>
                          <StyledIcon className={'fa fa-trash-o'}/>
                        </StyledButton>&nbsp;
                        <StyledButton className={'button-warning'} onClick={getShifter('up', index)}
                                      disabled={index === 0}>
                          <StyledIcon className={'fa fa-arrow-up'}/>
                        </StyledButton>&nbsp;
                        <StyledButton className={'button-warning'} onClick={getShifter('down', index)}
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
              </td>
            </tr>;
          })
        }
        </tbody>
      </table>
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
