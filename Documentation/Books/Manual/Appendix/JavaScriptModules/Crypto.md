!CHAPTER Crypto Module

`const crypto = require('@arangodb/crypto')`

The crypto module provides implementations of various hashing algorithms as well as cryptography related functions.

!SECTION Nonces

These functions deal with cryptographic nonces.

!SUBSECTION createNonce

`crypto.createNonce(): string`

Creates a cryptographic nonce.

Returns the created nonce.

!SUBSECTION checkAndMarkNonce

`crypto.checkAndMarkNonce(nonce): void`

Checks and marks a nonce.

**Arguments**

* **nonce**: `string`

  The nonce to check and mark.

Returns nothing.

!SECTION Random values

The following functions deal with generating random values.

!SUBSECTION rand

`crypto.rand(): number`

Generates a random integer that may be positive, negative or even zero.

Returns the generated number.

!SUBSECTION genRandomAlphaNumbers

`crypto.genRandomAlphaNumbers(length): string`

Generates a string of random alpabetical characters and digits.

**Arguments**

* **length**: `number`

  The length of the string to generate.

Returns the generated string.

!SUBSECTION genRandomNumbers

`crypto.genRandomNumbers(length): string`

Generates a string of random digits.

**Arguments**

* **length**: `number`

  The length of the string to generate.

Returns the generated string.

!SUBSECTION genRandomSalt

`crypto.genRandomSalt(length): string`

Generates a string of random (printable) ASCII characters.

**Arguments**

* **length**: `number`

  The length of the string to generate.

Returns the generated string.

!SECTION JSON Web Tokens (JWT)

These methods implement the JSON Web Token standard.

!SUBSECTION jwtEncode

`crypto.jwtEncode(key, message, algorithm): string`

Generates a JSON Web Token for the given message.

**Arguments**

* **key**: `string | null`

  The secret cryptographic key to be used to sign the message using the given algorithm.
  Note that this function will raise an error if the key is omitted but the algorithm expects a key,
  and also if the algorithm does not expect a key but a key is provided (e.g. when using `"none"`).

* **message**: `string`

  Message to be encoded as JWT. Note that the message will only be base64-encoded and signed, not encrypted.
  Do not store sensitive information in tokens unless they will only be handled by trusted parties.

* **algorithm**: `string`

  Name of the algorithm to use for signing the message, e.g. `"HS512"`.

Returns the JSON Web Token.

!SUBSECTION jwtDecode

`crypto.jwtDecode(key, token, noVerify): string | null`

**Arguments**

* **key**: `string | null`

  The secret cryptographic key that was used to sign the message using the algorithm indicated by the token.
  Note that this function will raise an error if the key is omitted but the algorithm expects a key.

  If the algorithm does not expect a key but a key is provided, the token will fail to verify.

* **token**: `string`

  The token to decode.

  Note that the function will raise an error if the token is malformed (e.g. does not have exactly three segments).

* **noVerify**: `boolean` (Default: `false`)

  Whether verification should be skipped. If this is set to `true` the signature of the token will not be verified.
  Otherwise the function will raise an error if the signature can not be verified using the given key.

Returns the decoded JSON message or `null` if no token is provided.

!SUBSECTION jwtAlgorithms

A helper object containing the supported JWT algorithms. Each attribute name corresponds to a JWT `alg` and the value is an object with `sign` and `verify` methods.

!SUBSECTION jwtCanonicalAlgorithmName

`crypto.jwtCanonicalAlgorithmName(name): string`

A helper function that translates a JWT `alg` value found in a JWT header into the canonical name of the algorithm in `jwtAlgorithms`. Raises an error if no algorithm with a matching name is found.

**Arguments**

* **name**: `string`

  Algorithm name to look up.

Returns the canonical name for the algorithm.

!SECTION Hashing algorithms

!SUBSECTION md5

`crypto.md5(message): string`

Hashes the given message using the MD5 algorithm.

**Arguments**

* **message**: `string`

  The message to hash.

Returns the cryptographic hash.

!SUBSECTION sha1

`crypto.sha1(message): string`

Hashes the given message using the SHA-1 algorithm.

**Arguments**

* **message**: `string`

  The message to hash.

Returns the cryptographic hash.

!SUBSECTION sha224

`crypto.sha224(message): string`

Hashes the given message using the SHA-224 algorithm.

**Arguments**

* **message**: `string`

  The message to hash.

Returns the cryptographic hash.

!SUBSECTION sha256

`crypto.sha256(message): string`

Hashes the given message using the SHA-256 algorithm.

**Arguments**

* **message**: `string`

  The message to hash.

Returns the cryptographic hash.

!SUBSECTION sha384

`crypto.sha384(message): string`

Hashes the given message using the SHA-384 algorithm.

**Arguments**

* **message**: `string`

  The message to hash.

Returns the cryptographic hash.

!SUBSECTION sha512

`crypto.sha512(message): string`

Hashes the given message using the SHA-512 algorithm.

**Arguments**

* **message**: `string`

  The message to hash.

Returns the cryptographic hash.

!SECTION Miscellaneous

!SUBSECTION constantEquals

`crypto.constantEquals(str1, str2): boolean`

Compares two strings.
This function iterates over the entire length of both strings
and can help making certain timing attacks harder.

**Arguments**

* **str1**: `string`

  The first string to compare.

* **str2**: `string`

  The second string to compare.

Returns `true` if the strings are equal, `false` otherwise.

!SUBSECTION pbkdf2

`crypto.pbkdf2(salt, password, iterations, keyLength): string`

Generates a PBKDF2-HMAC-SHA1 hash of the given password.

**Arguments**

* **salt**: `string`

  The cryptographic salt to hash the password with.

* **password**: `string`

  The message or password to hash.

* **iterations**: `number`

  The number of iterations.
  This should be a very high number.
  OWASP recommended 64000 iterations in 2012 and recommends doubling that number every two years.

  When using PBKDF2 for password hashes it is also recommended to add a random value
  (typically between 0 and 32000) to that number that is different for each user.

* **keyLength**: `number`

  The key length.

Returns the cryptographic hash.

!SUBSECTION hmac

`crypto.hmac(key, message, algorithm): string`

Generates an HMAC hash of the given message.

**Arguments**

* **key**: `string`

  The cryptographic key to use to hash the message.

* **message**: `string`

  The message to hash.

* **algorithm**: `string`

  The name of the algorithm to use.

Returns the cryptographic hash.
