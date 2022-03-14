import React, { useState, useContext } from "react";

const LinkContext = React.createContext();
const ShowContext = React.createContext();

export const useShow = () => {
  return useContext(LinkContext);
}

export const useShowUpdate = () => {
  return useContext(ShowContext);
}

export const LinkProvider = ({ children }) => {
  const [show, setShow] = useState("LinkList");

  const handleShowState = (str) => {
    setShow(str);
  }

  return (
    <LinkContext.Provider value={show}>
      <ShowContext.Provider value={handleShowState}>
        {children}
      </ShowContext.Provider>
    </LinkContext.Provider>
  )
}

export default LinkProvider;
