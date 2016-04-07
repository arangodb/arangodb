start:
  // ...........................................................................
  // init nonces
  // ...........................................................................

  uint32_t optionNonceHashSize = 0;

  if (optionNonceHashSize > 0) {
    LOG(DEBUG) << "setting nonce hash size to " << optionNonceHashSize;
    Nonce::create(optionNonceHashSize);
  }



stop:

  Nonce::destroy();
