export type CreateDatabaseUserValues = {
  extra: {
    img: string
  };
  active: boolean;
  username: string;
  name: string;
  gravatar: string;
  passwd: string;
};

export type DatabaseUserValues = {
  extra: {
    img: string;
    name: string;
  };
  user: string;
  active: boolean;
};
