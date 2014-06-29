##### Signed by https://keybase.io/max
```
-----BEGIN PGP SIGNATURE-----
Version: GnuPG v2

iQEcBAABAgAGBQJTqiJUAAoJEJgKPw0B/gTfs18IAMi+imf5rm3VFeY+XaA4EoON
3R1Lu45XJT0zzeP+O5MJD2AWgS98ySUwxgFeLrL8wUeg0nPUVD9o3PSxywQB+Gff
N+0ALBkbemfQ5n19e58V24Dh2aO1tHwVEhBZ3k7kfItgeOVbYDmWMP7dVLj0TFTx
mbv/c86hGUeNiLkr6WrKNHZdBzly1BGCdUys5VAzFJdKsuxPqiqIF2mEnIO5ZUFo
EUQnUvEy+TNJqa5tXopcLLE4k5bA04IsuxWCMBubhIh94sIthI28YLwp8yomFP4h
w5hWGcuY7OH3URE4MD0qrYOxAWkC5uC3I6K5F9B3KqVFHffY8LpiaOa+REUr3fA=
=zeyJ
-----END PGP SIGNATURE-----

```

<!-- END SIGNATURES -->

### Begin signed statement 

#### Expect

```
size  exec  file              contents                                                        
            ./                                                                                
547           .gitignore      a3260451040bdf523be635eac16d28044d0064c4e8c4c444b0a49b9258851bec
358           CHANGELOG.md    ebedbb377b5c4e12809e9cad7c7b92e0dab64d5a586b17cc95a088b98b1abbcd
1062          LICENSE         f5f4ff3d4bd2239d1cc4c9457ccee03a7e1fb1a6298a0b13ff002bee369bf043
407           Makefile        32e05964b75b43fd5492411f781568a8e8ea25f1ec9c12ddc795868549d231f6
56            README.md       0bd4207a9490a68de9a1b58feac0cdeff349eaa778c8a0eaf859d07873026b07
              lib/                                                                            
916             const.js      bee1762e6130dd8167212f07dda3c6416116d2e87fc364e8fce4ef80214b2da6
8917            library.js    37ab82fb064111b42497ad8c93d3aef595fc9883cb61a4a1d424ff7ae5cb5447
348             main.js       f29a1669ebc50cbda9dec9685acfb4af2248f48b997d823b43bb28671ba8b6f4
5454            runtime.js    5d521040b0f9acb61a99842f8ad75973ba7c1799adbe9da85df3a8adf8d60982
621           package.json    1c5474a99936057b185680c151c1ffe880d1fe68856e3c15d2e3d2655004a4f4
              src/                                                                            
785             const.iced    cce728a752c7b9bdaa0cee2466a4e996d525dc27804a1de68ed588be4f82c205
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