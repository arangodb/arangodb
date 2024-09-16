import { Database } from "arangojs";

async function globalSetup() {
  const db = new Database({
    url: "http://localhost:3001"
  });
  db.createDatabase("test");
  // purge the database
}

export default globalSetup;
