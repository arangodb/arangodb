const AqlEditor = ({
  queryString, onQueryChange, onNewSearch
}) => {
  return (
          <div className="d-flex align-items-center bg-light px-3 py-2 small rounded-3">
              <div className="d-flex align-items-center flex-grow-1">
                  <label htmlFor="queryString" className="me-2 fw-bold text-secondary">
                  Search
                  </label>
                  <input
                  id="queryString"
                  className="form-control form-control-sm me-2"
                  type="text"
                  value={queryString}
                  onChange = {(event) => {onQueryChange(event.target.value)}}
                  />
              </div>
              <div className="d-flex align-items-center">
                  <button className="btn mx-1 btn-sm btn-primary bi bi-search"
                  onClick={(event) => {onNewSearch(event.target.value)}}></button>
              </div>
          </div>
      );
  };

export default AqlEditor;
