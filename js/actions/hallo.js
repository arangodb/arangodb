defineAction("hallo",
  function (req, res) {
    res.responseCode = 200;
    res.body = "Hallo World\n";
  }
);
