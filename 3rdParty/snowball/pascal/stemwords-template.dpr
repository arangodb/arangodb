program stemwords;

{$ifdef windows}
{$APPTYPE CONSOLE}
{$endif}

uses
  SnowballProgram,
  { BEGIN TEMPLATE }
  %STEMMER%Stemmer in '%STEMMER%Stemmer.pas',
  { END TEMPLATE }
  SysUtils;

Var
    Stemmer : TSnowballProgram;
    CurWord : AnsiString;
    i : Integer;
    language : AnsiString;

Const
    Delimiters : Set Of Char = [#10, #13];

Function NextWord : Boolean;
Var C : Char;
Begin
    CurWord := '';

    Result := Not Eof;

    While Not Eof Do
    Begin
        Read(C);
        If IOResult <> 0 Then Break;
        If C In Delimiters Then Break;
        CurWord := CurWord + C;
    End;
End;

begin
    language := 'english';
    i := 0;
    while i < ParamCount do
    begin
        i := i + 1;
        if ParamStr(i) = '-l' then
        begin
            i := i + 1;
            language := ParamStr(i);
            continue;
        end;
        WriteLn('option '+ParamStr(i)+' unknown');
        Exit;
    end;
    if False then
    { BEGIN TEMPLATE }
    else if language = '%STEMMER%' then
        Stemmer := T%STEMMER%Stemmer.Create
    { END TEMPLATE }
    else
    begin
        WriteLn('Stemming language '+language+' unknown');
        Exit;
    end;

    Try
        While Not Eof Do
        Begin
            While NextWord Do
            Begin
                Stemmer.Current := CurWord;
                Stemmer.Stem;
                WriteLn(Stemmer.Current);
            End;
        End;
    Finally
        Stemmer.Free;
    End;
end.
