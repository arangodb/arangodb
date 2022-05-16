-----------------------------------------------------------------------
--  stemmer -- Multi-language stemmer with Snowball generator
--  Written by Stephane Carrez (Stephane.Carrez@gmail.com)
--  All rights reserved.
--
--  Redistribution and use in source and binary forms, with or without
--  modification, are permitted provided that the following conditions
--  are met:
--
--    1. Redistributions of source code must retain the above copyright notice,
--       this list of conditions and the following disclaimer.
--    2. Redistributions in binary form must reproduce the above copyright notice,
--       this list of conditions and the following disclaimer in the documentation
--       and/or other materials provided with the distribution.
--    3. Neither the name of the Snowball project nor the names of its contributors
--       may be used to endorse or promote products derived from this software
--       without specific prior written permission.
--
--  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
--  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
--  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
--  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
--  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
--  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
--  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
--  ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
--  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
--  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
-----------------------------------------------------------------------
with Interfaces;
package body Stemmer with SPARK_Mode is

   subtype Byte is Interfaces.Unsigned_8;
   use type Interfaces.Unsigned_8;

   procedure Stem_Word (Context  : in out Context_Type'Class;
                        Word     : in String;
                        Result   : out Boolean) is
   begin
      Context.P (1 .. Word'Length) := Word;
      Context.C := 0;
      Context.L := Word'Length;
      Context.Lb := 0;
      Stemmer.Stem (Context, Result);
   end Stem_Word;

   function Get_Result (Context : in Context_Type'Class) return String is
   begin
      return Context.P (1 .. Context.L);
   end Get_Result;

   function Eq_S (Context : in Context_Type'Class;
                  S       : in String) return Char_Index is
   begin
      if Context.L - Context.C < S'Length then
         return 0;
      end if;
      if Context.P (Context.C + 1 .. Context.C + S'Length) /= S then
         return 0;
      end if;
      return S'Length;
   end Eq_S;

   function Eq_S_Backward (Context : in Context_Type'Class;
                           S       : in String) return Char_Index is
   begin
      if Context.C - Context.Lb < S'Length then
         return 0;
      end if;
      if Context.P (Context.C + 1 - S'Length .. Context.C) /= S then
         return 0;
      end if;
      return S'Length;
   end Eq_S_Backward;

   function Length (Context : in Context_Type'Class) return Natural is
   begin
      return Context.L - Context.Lb;
   end Length;

   function Length_Utf8 (Context : in Context_Type'Class) return Natural is
      Count : Natural := 0;
      Pos   : Positive := 1;
      Val   : Byte;
   begin
      while Pos <= Context.L loop
         Val := Character'Pos (Context.P (Pos));
         Pos := Pos + 1;
         if Val >= 16#C0# or Val < 16#80# then
            Count := Count + 1;
         end if;
      end loop;
      return Count;
   end Length_Utf8;

   function Check_Among (Context : in Context_Type'Class;
                         Pos     : in Char_Index;
                         Shift   : in Natural;
                         Mask    : in Mask_Type) return Boolean is
      use Interfaces;
      Val : constant Byte := Character'Pos (Context.P (Pos + 1));
   begin
      if Natural (Shift_Right (Val, 5)) /= Shift then
         return True;
      end if;
      return (Shift_Right (Unsigned_64 (Mask), Natural (Val and 16#1f#)) and 1) = 0;
   end Check_Among;

   procedure Find_Among (Context : in out Context_Type'Class;
                         Amongs  : in Among_Array_Type;
                         Pattern : in String;
                         Execute : access procedure
                           (Ctx       : in out Context_Type'Class;
                            Operation : in Operation_Index;
                            Status    : out Boolean);
                         Result  : out Integer) is
      I   : Natural := Amongs'First;
      J   : Natural := Amongs'Last + 1;
      Common_I : Natural := 0;
      Common_J : Natural := 0;
      First_Key_Inspected : Boolean := False;
      C   : constant Natural := Context.C;
      L   : constant Integer := Context.L;
   begin
      loop
         declare
            K      : constant Natural := I + (J - I) / 2;
            W      : constant Among_Type := Amongs (K);
            Common : Natural := (if Common_I < Common_J then Common_I else Common_J);
            Diff   : Integer := 0;
         begin
            for I2 in W.First + Common .. W.Last loop
               if C + Common = L then
                  Diff := -1;
                  exit;
               end if;
               Diff := Character'Pos (Context.P (C + Common + 1))
                 - Character'Pos (Pattern (I2));
               exit when Diff /= 0;
               Common := Common + 1;
            end loop;
            if Diff < 0 then
               J := K;
               Common_J := Common;
            else
               I := K;
               Common_I := Common;
            end if;
         end;
         if J - I <= 1 then
            exit when I > 0 or J = I or First_Key_Inspected;
            First_Key_Inspected := True;
         end if;
      end loop;

      loop
         declare
            W   : constant Among_Type := Amongs (I);
            Len : constant Natural := W.Last - W.First + 1;
            Status : Boolean;
         begin
            if Common_I >= Len then
               Context.C := C + Len;
               if W.Operation = 0 then
                  Result := W.Result;
                  return;
               end if;
               Execute (Context, W.Operation, Status);
               Context.C := C + Len;
               if Status then
                  Result := W.Result;
                  return;
               end if;
            end if;
            exit when W.Substring_I < 0;
            I := W.Substring_I;
         end;
      end loop;
      Result := 0;
   end Find_Among;

   procedure Find_Among_Backward (Context : in out Context_Type'Class;
                                  Amongs  : in Among_Array_Type;
                                  Pattern : in String;
                                  Execute : access procedure
                                    (Ctx       : in out Context_Type'Class;
                                     Operation : in Operation_Index;
                                     Status    : out Boolean);
                                  Result  : out Integer) is
      I   : Natural := Amongs'First;
      J   : Natural := Amongs'Last + 1;
      Common_I : Natural := 0;
      Common_J : Natural := 0;
      First_Key_Inspected : Boolean := False;
      C   : constant Integer := Context.C;
      Lb  : constant Integer := Context.Lb;
   begin
      loop
         declare
            K      : constant Natural := I + (J - I) / 2;
            W      : constant Among_Type := Amongs (K);
            Common : Natural := (if Common_I < Common_J then Common_I else Common_J);
            Diff   : Integer := 0;
         begin
            for I2 in reverse W.First .. W.Last - Common loop
               if C - Common = Lb then
                  Diff := -1;
                  exit;
               end if;
               Diff := Character'Pos (Context.P (C - Common))
                 - Character'Pos (Pattern (I2));
               exit when Diff /= 0;
               Common := Common + 1;
            end loop;
            if Diff < 0 then
               J := K;
               Common_J := Common;
            else
               I := K;
               Common_I := Common;
            end if;
         end;
         if J - I <= 1 then
            exit when I > 0 or J = I or First_Key_Inspected;
            First_Key_Inspected := True;
         end if;
      end loop;

      loop
         declare
            W   : constant Among_Type := Amongs (I);
            Len : constant Natural := W.Last - W.First + 1;
            Status : Boolean;
         begin
            if Common_I >= Len then
               Context.C := C - Len;
               if W.Operation = 0 then
                  Result := W.Result;
                  return;
               end if;
               Execute (Context, W.Operation, Status);
               Context.C := C - Len;
               if Status then
                  Result := W.Result;
                  return;
               end if;
            end if;
            exit when W.Substring_I < 0;
            I := W.Substring_I;
         end;
      end loop;
      Result := 0;
   end Find_Among_Backward;

   function Skip_Utf8 (Context : in Context_Type'Class) return Result_Index is
      Pos : Char_Index := Context.C;
      Val : Byte;
   begin
      if Pos >= Context.L then
         return -1;
      end if;
      Pos := Pos + 1;
      Val := Character'Pos (Context.P (Pos));
      if Val >= 16#C0# then
         while Pos < Context.L loop
            Val := Character'Pos (Context.P (Pos + 1));
            exit when Val >= 16#C0# or Val < 16#80#;
            Pos := Pos + 1;
         end loop;
      end if;
      return Pos;
   end Skip_Utf8;

   function Skip_Utf8 (Context : in Context_Type'Class;
                       N       : in Integer) return Result_Index is
      Pos : Char_Index := Context.C;
      Val : Byte;
   begin
      if N < 0 then
          return -1;
      end if;
      for I in 1 .. N loop
         if Pos >= Context.L then
            return -1;
         end if;
         Pos := Pos + 1;
         Val := Character'Pos (Context.P (Pos));
         if Val >= 16#C0# then
            while Pos < Context.L loop
               Val := Character'Pos (Context.P (Pos + 1));
               exit when Val >= 16#C0# or Val < 16#80#;
               Pos := Pos + 1;
            end loop;
         end if;
      end loop;
      return Pos;
   end Skip_Utf8;

   function Skip_Utf8_Backward (Context : in Context_Type'Class) return Result_Index is
      Pos : Char_Index := Context.C;
      Val : Byte;
   begin
      if Pos <= Context.Lb then
         return -1;
      end if;
      Val := Character'Pos (Context.P (Pos));
      Pos := Pos - 1;
      if Val >= 16#80# then
         while Pos > Context.Lb loop
            Val := Character'Pos (Context.P (Pos + 1));
            exit when Val >= 16#C0#;
            Pos := Pos - 1;
         end loop;
      end if;
      return Pos;
   end Skip_Utf8_Backward;

   function Skip_Utf8_Backward (Context : in Context_Type'Class;
                                N       : in Integer) return Result_Index is
      Pos : Char_Index := Context.C;
      Val : Byte;
   begin
      if N < 0 then
          return -1;
      end if;
      for I in 1 .. N loop
         if Pos <= Context.Lb then
            return -1;
         end if;
         Val := Character'Pos (Context.P (Pos));
         Pos := Pos - 1;
         if Val >= 16#80# then
            while Pos > Context.Lb loop
               Val := Character'Pos (Context.P (Pos + 1));
               exit when Val >= 16#C0#;
               Pos := Pos - 1;
            end loop;
         end if;
      end loop;
      return Pos;
   end Skip_Utf8_Backward;

   function Shift_Left (Value : in Utf8_Type;
                        Shift : in Natural) return Utf8_Type
     is (Utf8_Type (Interfaces.Shift_Left (Interfaces.Unsigned_32 (Value), Shift)));

   procedure Get_Utf8 (Context : in Context_Type'Class;
                       Value   : out Utf8_Type;
                       Count   : out Natural) is
      B0, B1, B2, B3 : Byte;
   begin
      if Context.C >= Context.L then
         Value := 0;
         Count := 0;
         return;
      end if;
      B0 := Character'Pos (Context.P (Context.C + 1));
      if B0 < 16#C0# or Context.C + 1 >= Context.L then
         Value := Utf8_Type (B0);
         Count := 1;
         return;
      end if;
      B1 := Character'Pos (Context.P (Context.C + 2)) and 16#3F#;
      if B0 < 16#E0# or Context.C + 2 >= Context.L then
         Value := Shift_Left (Utf8_Type (B0 and 16#1F#), 6) or Utf8_Type (B1);
         Count := 2;
         return;
      end if;
      B2 := Character'Pos (Context.P (Context.C + 3)) and 16#3F#;
      if B0 < 16#F0# or Context.C + 3 >= Context.L then
         Value := Shift_Left (Utf8_Type (B0 and 16#0F#), 12)
           or Shift_Left (Utf8_Type (B1), 6) or Utf8_Type (B2);
         Count := 3;
         return;
      end if;
      B3 := Character'Pos (Context.P (Context.C + 4)) and 16#3F#;
      Value := Shift_Left (Utf8_Type (B0 and 16#07#), 18)
        or Shift_Left (Utf8_Type (B1), 12)
        or Shift_Left (Utf8_Type (B2), 6) or Utf8_Type (B3);
      Count := 4;
   end Get_Utf8;

   procedure Get_Utf8_Backward (Context : in Context_Type'Class;
                                Value   : out Utf8_Type;
                                Count   : out Natural) is
      B0, B1, B2, B3 : Byte;
   begin
      if Context.C <= Context.Lb then
         Value := 0;
         Count := 0;
         return;
      end if;
      B3 := Character'Pos (Context.P (Context.C));
      if B3 < 16#80# or Context.C - 1 <= Context.Lb then
         Value := Utf8_Type (B3);
         Count := 1;
         return;
      end if;
      B2 := Character'Pos (Context.P (Context.C - 1));
      if B2 >= 16#C0# or Context.C - 2 <= Context.Lb then
         B3 := B3 and 16#3F#;
         Value := Shift_Left (Utf8_Type (B2 and 16#1F#), 6) or Utf8_Type (B3);
         Count := 2;
         return;
      end if;
      B1 := Character'Pos (Context.P (Context.C - 2));
      if B1 >= 16#E0# or Context.C - 3 <= Context.Lb then
         B3 := B3 and 16#3F#;
         B2 := B2 and 16#3F#;
         Value := Shift_Left (Utf8_Type (B1 and 16#0F#), 12)
           or Shift_Left (Utf8_Type (B2), 6) or Utf8_Type (B3);
         Count := 3;
         return;
      end if;
      B0 := Character'Pos (Context.P (Context.C - 3));
      B1 := B1 and 16#1F#;
      B2 := B2 and 16#3F#;
      B3 := B3 and 16#3F#;
      Value := Shift_Left (Utf8_Type (B0 and 16#07#), 18)
        or Shift_Left (Utf8_Type (B1), 12)
        or Shift_Left (Utf8_Type (B2), 6) or Utf8_Type (B3);
      Count := 4;
   end Get_Utf8_Backward;

   procedure Out_Grouping (Context : in out Context_Type'Class;
                           S       : in Grouping_Array;
                           Min     : in Utf8_Type;
                           Max     : in Utf8_Type;
                           Repeat  : in Boolean;
                           Result  : out Result_Index) is
      Ch    : Utf8_Type;
      Count : Natural;
   begin
      if Context.C >= Context.L then
         Result := -1;
         return;
      end if;

      loop
         Get_Utf8 (Context, Ch, Count);
         if Count = 0 then
            Result := -1;
            return;
         end if;
         if Ch <= Max and Ch >= Min then
            Ch := Ch - Min;
            if S (Ch) then
               Result := Count;
               return;
            end if;
         end if;
         Context.C := Context.C + Count;
         exit when not Repeat;
      end loop;
      Result := 0;
   end Out_Grouping;

   procedure Out_Grouping_Backward (Context : in out Context_Type'Class;
                                    S       : in Grouping_Array;
                                    Min     : in Utf8_Type;
                                    Max     : in Utf8_Type;
                                    Repeat  : in Boolean;
                                    Result  : out Result_Index) is
      Ch    : Utf8_Type;
      Count : Natural;
   begin
      if Context.C = 0 then
         Result := -1;
         return;
      end if;

      loop
         Get_Utf8_Backward (Context, Ch, Count);
         if Count = 0 then
            Result := -1;
            return;
         end if;
         if Ch <= Max and Ch >= Min then
            Ch := Ch - Min;
            if S (Ch) then
               Result := Count;
               return;
            end if;
         end if;
         Context.C := Context.C - Count;
         exit when not Repeat;
      end loop;
      Result := 0;
   end Out_Grouping_Backward;

   procedure In_Grouping (Context : in out Context_Type'Class;
                          S       : in Grouping_Array;
                          Min     : in Utf8_Type;
                          Max     : in Utf8_Type;
                          Repeat  : in Boolean;
                          Result  : out Result_Index) is
      Ch    : Utf8_Type;
      Count : Natural;
   begin
      if Context.C >= Context.L then
         Result := -1;
         return;
      end if;

      loop
         Get_Utf8 (Context, Ch, Count);
         if Count = 0 then
            Result := -1;
            return;
         end if;
         if Ch > Max or Ch < Min then
            Result := Count;
            return;
         end if;
         Ch := Ch - Min;
         if not S (Ch) then
            Result := Count;
            return;
         end if;
         Context.C := Context.C + Count;
         exit when not Repeat;
      end loop;
      Result := 0;
   end In_Grouping;

   procedure In_Grouping_Backward (Context : in out Context_Type'Class;
                                   S       : in Grouping_Array;
                                   Min     : in Utf8_Type;
                                   Max     : in Utf8_Type;
                                   Repeat  : in Boolean;
                                   Result  : out Result_Index) is
      Ch    : Utf8_Type;
      Count : Natural;
   begin
      if Context.C = 0 then
         Result := -1;
         return;
      end if;

      loop
         Get_Utf8_Backward (Context, Ch, Count);
         if Count = 0 then
            Result := -1;
            return;
         end if;
         if Ch > Max or Ch < Min then
            Result := Count;
            return;
         end if;
         Ch := Ch - Min;
         if not S (Ch) then
            Result := Count;
            return;
         end if;
         Context.C := Context.C - Count;
         exit when not Repeat;
      end loop;
      Result := 0;
   end In_Grouping_Backward;

   procedure Replace (Context    : in out Context_Type'Class;
                      C_Bra      : in Char_Index;
                      C_Ket      : in Char_Index;
                      S          : in String;
                      Adjustment : out Integer) is
   begin
      Adjustment := S'Length - (C_Ket - C_Bra);
      if Adjustment > 0 then
         Context.P (C_Bra + S'Length + 1 .. Context.Lb + Adjustment + 1)
           := Context.P (C_Ket + 1 .. Context.Lb + 1);
      end if;
      if S'Length > 0 then
         Context.P (C_Bra + 1 .. C_Bra + S'Length) := S;
      end if;
      if Adjustment < 0 then
         Context.P (C_Bra + S'Length + 1 .. Context.L + Adjustment + 1)
           := Context.P (C_Ket + 1 .. Context.L + 1);
      end if;
      Context.L := Context.L + Adjustment;
      if Context.C >= C_Ket then
         Context.C := Context.C + Adjustment;
      elsif Context.C > C_Bra then
         Context.C := C_Bra;
      end if;
   end Replace;

   procedure Slice_Del (Context : in out Context_Type'Class) is
      Result : Integer;
   begin
      Replace (Context, Context.Bra, Context.Ket, "", Result);
   end Slice_Del;

   procedure Slice_From (Context : in out Context_Type'Class;
                         Text    : in String) is
      Result : Integer;
   begin
      Replace (Context, Context.Bra, Context.Ket, Text, Result);
   end Slice_From;

   function Slice_To (Context : in Context_Type'Class) return String is
   begin
      return Context.P (Context.Bra + 1 .. Context.Ket);
   end Slice_To;

   procedure Insert (Context : in out Context_Type'Class;
                     C_Bra   : in Char_Index;
                     C_Ket   : in Char_Index;
                     S       : in String) is
      Result : Integer;
   begin
      Replace (Context, C_Bra, C_Ket, S, Result);
      if C_Bra <= Context.Bra then
         Context.Bra := Context.Bra + Result;
      end if;
      if C_Bra <= Context.Ket then
         Context.Ket := Context.Ket + Result;
      end if;
   end Insert;

end Stemmer;
