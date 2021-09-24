const githubQuery = (login) => {

  return (
    `https://api.github.com/users/${login}`
  )
};

export default githubQuery;
