# Ada Target for Snowball

The Ada Snowball generator generates an Ada child package for each Snowball algorithm.
The parent package is named `Stemmer` and it provides various operations used by the generated code.
The `Stemmer` package contains the Ada Snowball runtime available either in `ada/src` directory
or from https://github.com/stcarrez/ada-stemmer.

The generated child package declares the `Context_Type` tagged type and the `Stem` procedure:

```Ada
package Stemmer.<Algorithm-name> is
   type Context_Type is new Stemmer.Context_Type with private;
   procedure Stem (Z : in out Context_Type; Result : out Boolean);
private
   type Context_Type is new Stemmer.Context_Type with record
      ...
   end record;
end Stemmer.<Algorithm-name>;
```

It is possible to use directly the generated operation or use it through the `Stemmer.Factory` package.

## Usage

To generate Ada source for a Snowball algorithm:
```
$ snowball path/to/algorithm.sbl -ada -P <algorithm-name> -o src/stemmer-<algorithm>
```

### Ada specific options

`-P <Algorithm-name>` the child package name used in the generated Ada file (defaults to `snowball`).
It must be a valid Ada identifier.

## Code Organization

`compiler/generator_ada.c` has the Ada code generation logic

`ada/src` contains the default Ada Snowball runtime support which is also
available at https://github.com/stcarrez/ada-stemmer

`ada/algorithms` location where the makefile generated code will end up

## Using the Generated Stemmers

To use the generated stemmer, import the Ada generated package, declare an instance
of the generated `Context_Type` and call the `Stem_Word` procedure.

```
with Stemmer.English;

  Ctx : Stemmer.English.Context_Type;
  Result : Boolean;

  Ctx.Stem_Word ("zealously", Result);
  if Result then
     Ada.Text_IO.Put_Line (Ctx.Get_Result);
  end if;
```

You can use the context as many times as you want.

## Testing

To run the tests, you will need an Ada compiler such as GNAT as well as the `gprbuild` build tool.

Only the existing Snowball algorithms have been used for testing.  This does not exercise all features of the language.

Run:

```
$ make check_ada
```

