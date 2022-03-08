unit SnowballProgram;

interface

Type
    TAmongHandler = Function : Boolean of Object;

Type
    TAmong = record
        Str    : AnsiString;    // search string
        Index  : Integer;       // index to longest matching substring
        Result : Integer;       // result of the lookup
        Method : TAmongHandler; // method to use if substring matches
    End;

Type
    {$M+}
    TSnowballProgram = Class
    Protected
        FCurrent : AnsiString;
        FCursor  : Integer;
        FLimit   : Integer;
        FBkLimit : Integer;
        FBra     : Integer;
        FKet     : Integer;

        Procedure SetCurrent(Current: AnsiString);

    Protected
        Function InGrouping(s : array of char; min, max : Integer) : Boolean;
        Function InGroupingBk(s : array of char; min, max : Integer) : Boolean;
        Function OutGrouping(s : array of char; min, max : Integer) : Boolean;
        Function OutGroupingBk(s : array of char; min, max : Integer) : Boolean;

        Function EqS(s_size : Integer; s : AnsiString) : Boolean;
        Function EqSBk(s_size : Integer; s : AnsiString) : Boolean;

        Function EqV(s : AnsiString) : Boolean;
        Function EqVBk(s : AnsiString) : Boolean;

        Function FindAmong(v : array of TAmong; v_size : Integer) : Integer;
        Function FindAmongBk(v : array of TAmong; v_size : Integer) : Integer;

        Procedure SliceDel;
        Procedure SliceCheck;
        Procedure SliceFrom(s : AnsiString);

        Function  ReplaceS(bra, ket : Integer; s : AnsiString) : Integer;        
        Procedure Insert(bra, ket : Integer; s : AnsiString);

        Function SliceTo : AnsiString;
        Function AssignTo : AnsiString;

    Public
        { Set & Retrieve current string }
        Property Current: AnsiString Read FCurrent Write SetCurrent;

        { Method subclasses need to implement }
        Function stem : Boolean; Virtual; Abstract;
    End;

Implementation

Uses Math;

Procedure TSnowballProgram.SetCurrent(Current: AnsiString);
Begin
    FCurrent := Current;
    FCursor  := 0;
    FLimit   := Length(Current);
    FBkLimit := 0;
    FBra     := FCursor;
    FKet     := FLimit;
End;

Function TSnowballProgram.InGrouping(s : array of char; min, max : Integer) : Boolean;
Var ch : Integer;
Begin
    Result := False;
    If (FCursor >= FLimit) Then Exit;

    ch := Ord(FCurrent[FCursor + 1]);
    If (ch > max) Or (ch < min) Then Exit;

    ch := ch - min;
    If (Ord(s[ch Shr 3]) And Ord(1 Shl (ch And $7))) = 0 Then Exit;

    Inc(FCursor);
    Result := True;
End;

Function TSnowballProgram.InGroupingBk(s : array of char; min, max : Integer) : Boolean;
Var ch : Integer;
Begin
    Result := False;
    If (FCursor <= FBkLimit) Then Exit;

    ch := Ord(FCurrent[FCursor]);
    If (ch > max) Or (ch < min) Then Exit;

    ch := ch - min;
    If (Ord(s[ch Shr 3]) And Ord(1 Shl (ch And $7))) = 0 Then Exit;

    Dec(FCursor);
    Result := True;
End;

Function TSnowballProgram.OutGrouping(s : array of char; min, max : Integer) : Boolean;
Var ch : Integer;
Begin
    Result := False;
    If (FCursor >= FLimit) Then Exit;

    ch := Ord(FCurrent[FCursor + 1]);

    If (ch > max) Or (ch < min) Then
    Begin
        Inc(FCursor);
        Result := True;
        Exit;
    End;

    ch := ch - min;
    If (Ord(s[ch Shr 3]) And Ord(1 Shl (ch And $7))) = 0 Then
    Begin
        Inc(FCursor);
        Result := True;
    End;
End;

Function TSnowballProgram.OutGroupingBk(s : array of char; min, max : Integer) : Boolean;
Var ch : Integer;
Begin
    Result := False;

    If (FCursor <= FBkLimit) Then Exit;

    ch := Ord(FCurrent[FCursor]);
    If (ch > max) Or (ch < min) Then
    Begin
        Dec(FCursor);
        Result := True;
        Exit;
    End;

    ch := ch - min;
    If (Ord(s[ch Shr 3]) And Ord(1 Shl (ch And $7))) = 0 Then
    Begin
        Dec(FCursor);
        Result := True;
    End;
End;

Function TSnowballProgram.EqS(s_size : Integer; s : AnsiString) : Boolean;
Var I : Integer;
Begin
    Result := False;

    If (FLimit - FCursor) < s_size Then Exit;

    For I := 1 To s_size Do
        If FCurrent[FCursor + I] <> s[I] Then Exit;

    FCursor := FCursor + s_size;

    Result := True;
End;

Function TSnowballProgram.EqSBk(s_size : Integer; s : AnsiString) : Boolean;
Var I : Integer;
Begin
    Result := False;

    if (FCursor - FBkLimit) < s_size Then Exit;

    For I := 1 To s_size Do
        If FCurrent[FCursor - s_size + I] <> s[i] Then Exit;

    FCursor := FCursor - s_size;

    Result := True;
End;

Function TSnowballProgram.EqV(s : AnsiString) : Boolean;
Begin
    Result := EqS(Length(s), s);
End;

Function TSnowballProgram.EqVBk(s : AnsiString) : Boolean;
Begin
    Result := EqSBk(Length(s), s);
End;

Function TSnowballProgram.FindAmong(v : array of TAmong; v_size : Integer) : Integer;
Var i, i2, j, c, l, common_i, common_j, k, diff, common : Integer;
    first_key_inspected, res : Boolean;
    w : TAmong;
Begin
    i := 0;
    j := v_size;

    c := FCursor;
    l := FLimit;

    common_i := 0;
    common_j := 0;

    first_key_inspected := false;

    While True Do
    Begin
        k := i + ((j - i) Shr 1);
        diff := 0;
        common := Min(common_i, common_j); // smaller
        w := v[k];

        For i2 := common To Length(w.Str) - 1 Do
        Begin
            if (c + common) = l Then
            Begin
                diff := -1;
                Break;
            End;

            diff := Ord(FCurrent[c + common + 1]) - Ord(w.Str[i2 + 1]);
            if diff <> 0 Then Break;

            Inc(common);
        End;

        if diff < 0 Then
        Begin
            j := k;
            common_j := common;
        End
        Else
        Begin
            i := k;
            common_i := common;
        End;

        If (j - i) <= 1 Then
        Begin
            If (i > 0) Then Break; // v->s has been inspected
            if (j = i) Then Break; // only one item in v

            // - but now we need to go round once more to get
            // v->s inspected. This looks messy, but is actually
            // the optimal approach.

            if (first_key_inspected) Then Break;
            first_key_inspected := True;
        End;
    End;

    While True Do
    Begin
        w := v[i];
        If (common_i >= Length(w.Str)) Then
        Begin
            FCursor := c + Length(w.Str);
            If Not Assigned(w.Method) Then
            Begin
                Result := w.Result;
                Exit;   
            End;

            res := w.Method;

            FCursor := c + Length(w.Str);
            if (res) Then Begin
                Result := w.Result;
                Exit;
            End;
        End;

        i := w.Index;
        if i < 0 Then
        Begin
            Result := 0;
            Exit;
        End;
    End;
End;

Function TSnowballProgram.FindAmongBk(v : array of TAmong; v_size : Integer) : Integer;
Var i, j, c, lb, common_i, common_j, k, diff, common, i2 : Integer;
    first_key_inspected, res : Boolean;
    w : TAmong;
Begin
    i := 0;
    j := v_size;

    c := FCursor;
    lb := FBkLimit;

    common_i := 0;
    common_j := 0;

    first_key_inspected := false;

    While True Do
    Begin
        k := i + ((j - i) Shr 1);
        diff := 0;
        common := Min(common_i, common_j);
        w := v[k];

        For i2 := Length(w.Str) - 1 - common DownTo 0 Do
        Begin
            If (c - common) = lb Then
            Begin
                diff := -1;
                Break;
            End;

            diff := Ord(FCurrent[c - common]) - Ord(w.Str[i2 + 1]);
            if diff <> 0 Then Break;
            Inc(common);
        End;

        If diff < 0 Then
        Begin
            j := k;
            common_j := common;
        End
        Else
        Begin
            i := k;
            common_i := common;
        End;

        If (j - i) <= 1 Then
        Begin
            if i > 0 Then Break;
            if j = i Then Break;
            if first_key_inspected Then Break;
            first_key_inspected := True;
        End;
    End;

    While True Do
    Begin
        w := v[i];
        if common_i >= Length(w.Str) Then
        Begin
            FCursor := c - Length(w.Str);
            If Not Assigned(w.Method) Then
            Begin
                Result := w.Result;
                Exit;
            End;

            res := w.Method;

            FCursor := c - Length(w.Str);
            If Res Then
            Begin
                Result := w.Result;
                Exit;
            End;
        End;

        i := w.Index;
        If i < 0 Then
        Begin
                Result := 0;
                Exit;
        End;
    End;
End;

Procedure TSnowballProgram.SliceCheck;
Begin
    if (FBra < 0) Or (FBra > FKet) Or (FKet > FLimit) Or (FLimit > Length(FCurrent)) Then
    Begin
        WriteLn('Faulty slice operation.');
        Halt;
    End;
End;

Procedure TSnowballProgram.SliceDel;
Begin
    SliceFrom('');
End;

Function TSnowballProgram.ReplaceS(bra, ket : Integer; s : AnsiString) : Integer;
Var adjustment : Integer;
Begin
    adjustment := Length(s) - (ket - bra);

    Delete(FCurrent, bra + 1, ket - bra);
    System.Insert(s, FCurrent, bra + 1);

    FLimit := FLimit + adjustment;

    if (FCursor >= ket) Then
        FCursor := FCursor + adjustment
    Else If (FCursor > bra) Then
        FCursor := bra;

    Result := adjustment;
End;

Procedure TSnowballProgram.Insert(bra, ket : Integer; s : AnsiString);
Var adjustment : Integer;
Begin
    adjustment := ReplaceS(bra, ket, s);
    If (bra <= FBra) Then FBra := FBra + adjustment;
    If (bra <= FKet) Then FKet := FKet + adjustment;
End;

Function TSnowballProgram.SliceTo() : AnsiString;
Begin
    SliceCheck();
    Result := Copy(FCurrent, FBra + 1, FKet - FBra);
End;

Procedure TSnowballProgram.SliceFrom(s : AnsiString);
Begin
    SliceCheck();
    ReplaceS(FBra, FKet, s);
End;

Function TSnowballProgram.AssignTo() : AnsiString;
Begin
    Result := Copy(FCurrent, 1, FLimit);
End;

End.
