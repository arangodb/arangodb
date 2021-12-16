import React from 'react';

const AqlEditor = ({
  queryString, onQueryChange, onNewSearch, onOnclickHandler
}) => {
  let textInput = React.createRef();

  return (
    <>
      <div className='graph-list'>
        <input ref={textInput} list="graphlist" id="graphcollections" name="graphcollections" placeholder="Choose graph..." />
        <datalist id="graphlist">
          <option value="routeplanner" />
          <option value="social" />
        </datalist>
      </div>
      <button
        className="btn mx-1 btn-sm btn-primary bi bi-search"
        onClick={()=> onOnclickHandler(textInput.current.value)}>
          Load data
      </button>
    </>
  );
  /*
  return (
          <div className="d-flex align-items-center bg-light px-3 py-2 small rounded-3">
              <div className="d-flex align-items-center flex-grow-1">
                <label htmlFor="queryString" className="me-2 fw-bold text-secondary">
                  Select graph data:
                </label>
                <input
                  ref={textInput}
                  id="queryString"
                  className="form-control form-control-sm me-2"
                  type="text"
                />
              </div>
              <div className="d-flex align-items-center">
                <button
                  className="btn mx-1 btn-sm btn-primary bi bi-search"
                  onClick={()=> onOnclickHandler(textInput.current.value)}>
                    Receive data
                </button>
              </div>
          </div>
      );
      */
  };

export default AqlEditor;
