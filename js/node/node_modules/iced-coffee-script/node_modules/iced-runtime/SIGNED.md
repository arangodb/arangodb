##### Signed by https://keybase.io/max
```
-----BEGIN PGP SIGNATURE-----
Version: GnuPG v2

iQEcBAABAgAGBQJUKUs5AAoJEJgKPw0B/gTfSdMH/RdyJdoqnVprr6MCBzgmJM8Y
rg0WyDtlBxrIHWUahwaJKETnpj6xFGFnblBgcuXT6Ssw0XNuLs6pUFE1l3NBsPO4
HmV94pDqXl3IpCKp1AvsPNheZ0EHgq71gNFV2YBxbuT83rsm1Ii22S19+fL2w0t+
kiHj7D1I7c5W0p1Nc0RPGb1c7XTYgbFRIADYi99yP1SZKJlcNrOMPGiMZ3tp/8tK
7qtsqlIWlkSvH21sxghJkMQ89J5U8DxIik7uTYKJ+uAIokz6bBn/hfAfem1pFBRL
bsNM9MtwobK9qVct3GPEzIhtSFBjLoM6Emk5Grd6sH0YOF8AWF/SKX/PU1Wor2E=
=kF1G
-----END PGP SIGNATURE-----

```

<!-- END SIGNATURES -->

### Begin signed statement 

#### Expect

```
size  exec  file              contents                                                        
            ./                                                                                
547           .gitignore      a3260451040bdf523be635eac16d28044d0064c4e8c4c444b0a49b9258851bec
469           CHANGELOG.md    9b0855fc2d72965a423495efd01bcce47954281f911f85ae723a60ab6dc6a7bf
1062          LICENSE         f5f4ff3d4bd2239d1cc4c9457ccee03a7e1fb1a6298a0b13ff002bee369bf043
407           Makefile        32e05964b75b43fd5492411f781568a8e8ea25f1ec9c12ddc795868549d231f6
56            README.md       0bd4207a9490a68de9a1b58feac0cdeff349eaa778c8a0eaf859d07873026b07
              lib/                                                                            
948             const.js      4f5863bd6285c4e14639131ee041466f6a6b9c778eb4674d0646486eff65788a
8917            library.js    37ab82fb064111b42497ad8c93d3aef595fc9883cb61a4a1d424ff7ae5cb5447
348             main.js       f29a1669ebc50cbda9dec9685acfb4af2248f48b997d823b43bb28671ba8b6f4
5454            runtime.js    5d521040b0f9acb61a99842f8ad75973ba7c1799adbe9da85df3a8adf8d60982
621           package.json    28b0d351e91b94035a5b63c2b051ec6541ef6b41d0302d8d5ef30cd629c3d2bd
              src/                                                                            
815             const.iced    e8b6ad6483b983c28e2ac82c8f4f5458a13ec4d38e2eec21d7bea6dbf1108fad
3881            library.iced  2acf49cd6eceae202a162af11b98b4faf5cd2bcee186cad669b59a2d22643dda
142             main.iced     bb74f97e5473e7f9b5f79264f3a3ba7126a4849d97fd37c49dfdbb711120e314
5273            runtime.iced  2c45cb02f7fc17352605103720b0cace4d0c42e9f81a9bb99112ff83fb663fe6
```

#### Ignore

```
/SIGNED.md
```

#### Presets

```
git      # ignore .git and anything as described by .gitignore files
dropbox  # ignore .dropbox-cache and other Dropbox-related files    
kb       # ignore anything as described by .kbignore files          
```

<!-- summarize version = 0.0.9 -->

### End signed statement

<hr>

#### Notes

With keybase you can sign any directory's contents, whether it's a git repo,
source code distribution, or a personal documents folder. It aims to replace the drudgery of:

  1. comparing a zipped file to a detached statement
  2. downloading a public key
  3. confirming it is in fact the author's by reviewing public statements they've made, using it

All in one simple command:

```bash
keybase dir verify
```

There are lots of options, including assertions for automating your checks.

For more info, check out https://keybase.io/docs/command_line/code_signing