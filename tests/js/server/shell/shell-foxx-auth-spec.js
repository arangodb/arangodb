/*global describe, it */
const { expect } = require("chai");
const Auth = require("@arangodb/foxx/auth");
const crypto = require("@arangodb/crypto");

require("@arangodb/test-helper").waitForFoxxInitialized();

describe("@arangodb/foxx/auth", () => {
  it("uses sha256 by default", () => {
    const auth = Auth();
    const authData = auth.create("hunter2");
    expect(authData.hash).to.equal(crypto.sha256(authData.salt + "hunter2"));
  });
  it("prefixes the password with a random salt", () => {
    const auth = Auth({ method: "sha1" });
    const authData = auth.create("hunter2");
    expect(authData).to.have.property("salt");
    expect(authData.salt).not.to.be.empty;
    expect(authData.hash).not.to.equal(crypto.sha1("hunter2"));
    expect(authData.hash).to.equal(crypto.sha1(authData.salt + "hunter2"));
  });
  it("respects saltLength", () => {
    for (const len of [16, 42, 69]) {
      const auth = Auth({ saltLength: len });
      const authData = auth.create("hunter2");
      expect(authData.salt.length).to.equal(len);
    }
  });
  it("supports pbkdf2", () => {
    const auth = Auth({ method: "pbkdf2" });
    const authData = auth.create("hunter2");
    expect(authData).to.have.property("iter");
    expect(authData.iter).to.be.a("number");
    expect(authData.hash).to.equal(
      crypto.pbkdf2(
        authData.salt,
        "hunter2",
        authData.iter,
        authData.salt.length
      )
    );
  });
});
