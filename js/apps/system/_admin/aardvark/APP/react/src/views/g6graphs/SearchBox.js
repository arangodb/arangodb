const SearchBox = ({
  login, onLoginChange
}) => {
  return (
          <div className="d-flex align-items-center bg-light px-3 py-2 small rounded-3">
              <div className="d-flex align-items-center flex-grow-1">
                  <label htmlFor="queryString" className="me-2 fw-bold text-secondary">
                  Login-Name
                  </label>
                  <input
                  id="login"
                  className="form-control form-control-sm me-2"
                  type="text"
                  value={login}
                  onChange = {(event) => {onLoginChange(event.target.value)}}
                  />
              </div>
          </div>
      );
  };

export default SearchBox;
