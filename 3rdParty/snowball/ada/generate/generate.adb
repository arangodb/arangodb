with Ada.Characters.Handling;
with Ada.Text_IO;
with Ada.Command_Line;
with Ada.Containers.Indefinite_Vectors;
procedure Generate is

   use Ada.Characters.Handling;
   use Ada.Text_IO;

   package String_Vectors is new Ada.Containers.Indefinite_Vectors
     (Element_Type => String,
      Index_Type   => Positive);

   Languages : String_Vectors.Vector;

   function Capitalize (S : in String) return String is
      (To_Upper (S (S'First)) & S (S'First + 1 .. S'Last));

   procedure Write_Spec is
      File : File_Type;
      I    : Natural := 0;
   begin
      Create (File, Out_File, "stemmer-factory.ads");
      Put_Line (File, "package Stemmer.Factory with SPARK_Mode is");
      New_Line (File);
      Put (File, "   type Language_Type is (");
      for Lang of Languages loop
         Put (File, "L_" & To_Upper (Lang));
         I := I + 1;
         if I < Natural (Languages.Length) then
            Put_Line (File, ",");
            Put (File, "                          ");
         end if;
      end loop;
      Put_Line (File, ");");
      New_Line (File);
      Put_Line (File, "   function Stem (Language : in Language_Type;");
      Put_Line (File, "                  Word     : in String) return String;");
      New_Line (File);
      Put_Line (File, "end Stemmer.Factory;");
      Close (File);
   end Write_Spec;

   procedure Write_Body is
      File : File_Type;
   begin
      Create (File, Out_File, "stemmer-factory.adb");
      for Lang of Languages loop
         Put_Line (File, "with Stemmer." & Capitalize (Lang) & ";");
      end loop;
      Put_Line (File, "package body Stemmer.Factory with SPARK_Mode is");
      New_Line (File);
      Put_Line (File, "   function Stem (Language : in Language_Type;");
      Put_Line (File, "                  Word     : in String) return String is");
      Put_Line (File, "      Result : Boolean := False;");
      Put_Line (File, "   begin");
      Put_Line (File, "      case Language is");
      for Lang of Languages loop
         Put_Line (File, "      when L_" & To_Upper (Lang) & " =>");
         Put_Line (File, "         declare");
         Put_Line (File, "            C : Stemmer." & Capitalize (Lang) & ".Context_Type;");
         Put_Line (File, "         begin");
         Put_Line (File, "            C.Stem_Word (Word, Result);");
         Put_Line (File, "            return Get_Result (C);");
         Put_Line (File, "         end;");
         New_Line (File);
      end loop;
      Put_Line (File, "      end case;");
      Put_Line (File, "   end Stem;");
      New_Line (File);
      Put_Line (File, "end Stemmer.Factory;");
      Close (File);
   end Write_Body;

   Count  : constant Natural := Ada.Command_Line.Argument_Count;

begin
   for I in 1 .. Count loop
      Languages.Append (To_Lower (Ada.Command_Line.Argument (I)));
   end loop;
   Write_Spec;
   Write_Body;
end Generate;
