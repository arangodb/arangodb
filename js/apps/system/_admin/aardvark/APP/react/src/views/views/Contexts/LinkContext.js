import React, { useState, useContext } from "react";

const LinkContext = React.createContext();
const ShowContext = React.createContext();

export function useShow() {
  return useContext(LinkContext);
}

export function useShowUpdate() {
  return useContext(ShowContext);
}

export function LinkProvider({ children }) {
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
