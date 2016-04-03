/* -*- Mode: java; tab-width: 8 -*-
 * Copyright (C) 1997, 1998 Netscape Communications Corporation, All Rights Reserved.
 */


package com.netscape.javascript.qa.liveconnect;

/**
 *  Attempt to get and set the public fields.
 *
 *  XXX need to override toString, toNumber with values in
 *  static fields.
 *
 *  XXX override both instance and static versions of the method
 *  @author christine m begle
 */

public class DataTypeClass {
    /**
     * Constructor stuff
     *
     */

    public DataTypeClass() {
        PUB_INT_CONSTRUCTOR_ARG = CONSTRUCTOR_ARG_NONE;
    }

    // instance setters

    public DataTypeClass(boolean b) {
        PUB_INT_CONSTRUCTOR_ARG = CONSTRUCTOR_ARG_BOOLEAN;
    }
    public DataTypeClass(Boolean b) {
        PUB_INT_CONSTRUCTOR_ARG = CONSTRUCTOR_ARG_BOOLEAN_OBJECT;
    }
    public DataTypeClass(byte b) {
        PUB_INT_CONSTRUCTOR_ARG = CONSTRUCTOR_ARG_BYTE;
    }
    public DataTypeClass(Byte b) {
        PUB_INT_CONSTRUCTOR_ARG = CONSTRUCTOR_ARG_BYTE_OBJECT;
    }
    public DataTypeClass(Integer i) {
        PUB_INT_CONSTRUCTOR_ARG = CONSTRUCTOR_ARG_INTEGER_OBJECT;
    }
    public DataTypeClass(int i) {
        PUB_INT_CONSTRUCTOR_ARG = CONSTRUCTOR_ARG_INT;
    }
    public DataTypeClass(Double d ) {
        PUB_INT_CONSTRUCTOR_ARG = CONSTRUCTOR_ARG_DOUBLE_OBJECT;
    }
    public DataTypeClass(double d) {
        PUB_INT_CONSTRUCTOR_ARG = CONSTRUCTOR_ARG_DOUBLE;
    }
    public DataTypeClass(Float f) {
        PUB_INT_CONSTRUCTOR_ARG = CONSTRUCTOR_ARG_FLOAT_OBJECT;
    }
    public DataTypeClass(float f) {
        PUB_INT_CONSTRUCTOR_ARG = CONSTRUCTOR_ARG_FLOAT;
    }
    public DataTypeClass(Long l) {
        PUB_INT_CONSTRUCTOR_ARG = CONSTRUCTOR_ARG_LONG_OBJECT;
    }
    public DataTypeClass(long l) {
        PUB_INT_CONSTRUCTOR_ARG = CONSTRUCTOR_ARG_LONG;
    }
    public DataTypeClass(Short s) {
        PUB_INT_CONSTRUCTOR_ARG = CONSTRUCTOR_ARG_SHORT_OBJECT;
    }
    public DataTypeClass(short s) {
        PUB_INT_CONSTRUCTOR_ARG = CONSTRUCTOR_ARG_SHORT;
    }
    public DataTypeClass(String s) {
        PUB_INT_CONSTRUCTOR_ARG = CONSTRUCTOR_ARG_STRING;
    }
    public DataTypeClass(Object o ) {
        PUB_INT_CONSTRUCTOR_ARG = CONSTRUCTOR_ARG_OBJECT;
    }
    public DataTypeClass(char c) {
        PUB_INT_CONSTRUCTOR_ARG = CONSTRUCTOR_ARG_CHAR;
    }
    public DataTypeClass(Character c) {
        PUB_INT_CONSTRUCTOR_ARG = CONSTRUCTOR_ARG_CHAR_OBJECT;
    }
    public DataTypeClass( byte b[] ) {
        PUB_INT_CONSTRUCTOR_ARG = CONSTRUCTOR_ARG_BYTE_ARRAY;
    }
    public int PUB_INT_CONSTRUCTOR_ARG;
    public int CONSTRUCTOR_ARG_NONE = 0;
    public int CONSTRUCTOR_ARG_BOOLEAN = 1;
    public int CONSTRUCTOR_ARG_BOOLEAN_OBJECT=2;
    public int CONSTRUCTOR_ARG_BYTE=3;
    public int CONSTRUCTOR_ARG_BYTE_OBJECT=4;
    public int CONSTRUCTOR_ARG_INT=5;
    public int CONSTRUCTOR_ARG_INTEGER_OBJECT=6;
    public int CONSTRUCTOR_ARG_DOUBLE=7;
    public int CONSTRUCTOR_ARG_DOUBLE_OBJECT=8;
    public int CONSTRUCTOR_ARG_FLOAT=9;
    public int CONSTRUCTOR_ARG_FLOAT_OBJECT=10;
    public int CONSTRUCTOR_ARG_LONG=11;
    public int CONSTRUCTOR_ARG_LONG_OBJECT=12;
    public int CONSTRUCTOR_ARG_SHORT=13;
    public int CONSTRUCTOR_ARG_SHORT_OBJECT=14;
    public int CONSTRUCTOR_ARG_STRING=15;
    public int CONSTRUCTOR_ARG_OBJECT=16;
    public int CONSTRUCTOR_ARG_CHAR=17;
    public int CONSTRUCTOR_ARG_CHAR_OBJECT=18;
    public int CONSTRUCTOR_ARG_BYTE_ARRAY=19;
    /**
     *  Override toNumber
     *
     */
    public static double PUB_DOUBLE_REPRESENTATION = 0.2134;

    public double doubleValue() {
        return PUB_DOUBLE_REPRESENTATION;
    }

    public int toNumber() {
        return PUB_NUMBER_REPRESENTATION;
    }

    public boolean booleanValue() {
        return PUB_BOOLEAN_REPRESENTATION;
    }

    public boolean PUB_BOOLEAN_REPRESENTATION = true;

    public static int PUB_NUMBER_REPRESENTATION = 2134;

    /**
     * Override toString
     */

    public String toString() {
        return PUB_STRING_REPRESENTATION;
    }

    public static String PUB_STRING_REPRESENTATION =
        "DataTypeClass Instance";

    /**
     *  Have a method that has the same name as a field.
     */

    public String amIAFieldOrAMethod() {
        return "METHOD!";
    }

    public String amIAFieldOrAMethod = "FIELD!";

    public static boolean staticGetBoolean() {
        return PUB_STATIC_BOOLEAN;
    }
    public static Boolean staticGetBooleanObject() {
        return PUB_STATIC_BOOLEAN_OBJECT;
    }

    public static byte staticGetByte() {
        return PUB_STATIC_BYTE;
    }
    public static Byte staticGetByteObject() {
        return PUB_STATIC_BYTE_OBJECT;
    }

    public static Integer staticGetIntegerObject() {
        return PUB_STATIC_INTEGER_OBJECT;
    }
    public static int staticGetInteger() {
        return new Integer(PUB_STATIC_INT).intValue();
    }

    public static Double staticGetDoubleObject() {
        return PUB_STATIC_DOUBLE_OBJECT;
    }

    public static double staticGetDouble() {
        return new Double(PUB_STATIC_DOUBLE).doubleValue();
    }

    public static Float staticGetFloatObject() {
        return PUB_STATIC_FLOAT_OBJECT;
    }

    public static float staticGetFloat() {
        return new Float(PUB_STATIC_FLOAT).floatValue();
    }

    public static Long staticGetLongObject() {
        return PUB_STATIC_LONG_OBJECT;
    }

    public static long staticGetLong() {
        return new Long(PUB_STATIC_LONG).longValue();
    }

    public static Short staticGetShortObject() {
        return PUB_STATIC_SHORT_OBJECT;
    }

    public static short staticGetShort() {
        return new Short(PUB_STATIC_SHORT).shortValue();
    }

    public static String staticGetStringObject() {
        return PUB_STATIC_STRING;
    }

    public static char staticGetChar() {
        return PUB_STATIC_CHAR;
    }

    public static Character staticGetCharacter() {
        return PUB_STATIC_CHAR_OBJECT;
    }

    // instance getters

    public boolean getBoolean() {
        return PUB_BOOLEAN;
    }
    public Boolean getBooleanObject() {
        return PUB_BOOLEAN_OBJECT;
    }

    public byte getByte() {
        return PUB_BYTE;
    }
    public Byte getByteObject() {
        return PUB_BYTE_OBJECT;
    }

    public Integer getIntegerObject() {
        return PUB_INTEGER_OBJECT;
    }
    public int getInteger() {
        return new Integer(PUB_INT).intValue();
    }

    public Double getDoubleObject() {
        return PUB_DOUBLE_OBJECT;
    }

    public double getDouble() {
        return new Double(PUB_DOUBLE).doubleValue();
    }

    public Float getFloatObject() {
        return PUB_FLOAT_OBJECT;
    }

    public float getFloat() {
        return new Float(PUB_FLOAT).floatValue();
    }

    public Long getLongObject() {
        return PUB_LONG_OBJECT;
    }

    public long getLong() {
        return new Long(PUB_LONG).longValue();
    }

    public String getStringObject() {
        return PUB_STRING;
    }

    public Object getObject() {
        return PUB_OBJECT;
    }

    public Short getShortObject() {
        return PUB_SHORT_OBJECT;
    }

    public short getShort() {
        return new Short(PUB_SHORT).shortValue();
    }

    public char getChar() {
        return PUB_CHAR;
    }

    public Character getCharacter() {
        return PUB_CHAR_OBJECT;
    }

//  SETTERS
    public static void staticSetBoolean(boolean b) {
        PUB_STATIC_BOOLEAN = b;
    }
    public static void staticSetBooleanObject(Boolean b) {
        PUB_STATIC_BOOLEAN_OBJECT = b;
    }

    public static void staticSetByte(byte b) {
        PUB_STATIC_BYTE = b;
    }
    public static void staticSetByteObject(Byte b) {
        PUB_STATIC_BYTE_OBJECT = b;
    }

    public static void staticSetIntegerObject(Integer i) {
        PUB_STATIC_INTEGER_OBJECT = i;
    }
    public static void staticSetInteger(int i) {
        PUB_STATIC_INT = i;
    }

    public static void staticSetDoubleObject( Double d ) {
        PUB_STATIC_DOUBLE_OBJECT = d;
    }

    public static void staticSetDouble(double d) {
        PUB_STATIC_DOUBLE = d;
    }

    public static void staticSetFloatObject(Float f) {
        PUB_STATIC_FLOAT_OBJECT = f ;
    }

    public static void staticSetFloat(float f) {
        PUB_STATIC_FLOAT = f;
    }

    public static void staticSetLongObject(Long l) {
        PUB_STATIC_LONG_OBJECT = l ;
    }

    public static void staticSetLong(long l) {
        PUB_STATIC_LONG = l ;
    }

    public static void staticSetShortObject(Short s) {
        PUB_STATIC_SHORT_OBJECT = s;
    }

    public static void staticSetShort(short s) {
        PUB_STATIC_SHORT = s;
    }

    public static void staticSetStringObject(String s) {
        PUB_STATIC_STRING = s;
    }

    public static void staticSetChar(char c) {
        PUB_STATIC_CHAR = c;
    }

    public static void staticSetCharacter(Character c) {
        PUB_STATIC_CHAR_OBJECT = c ;
    }

    // instance setters

    public void setBoolean(boolean b) {
        PUB_BOOLEAN = b;
    }
    public void setBooleanObject(Boolean b) {
        PUB_BOOLEAN_OBJECT = b;
    }

    public void setByte(byte b) {
        PUB_BYTE = b;
    }
    public void setByteObject(Byte b) {
        PUB_BYTE_OBJECT = b;
    }

    public void setIntegerObject(Integer i) {
        PUB_INTEGER_OBJECT = i;
    }
    public void setInteger(int i) {
        PUB_INT = i;
    }

    public void setDoubleObject( Double d ) {
        PUB_DOUBLE_OBJECT = d;
    }

    public void setDouble(double d) {
        PUB_DOUBLE = d;
    }

    public void setFloatObject(Float f) {
        PUB_FLOAT_OBJECT = f ;
    }

    public void setFloat(float f) {
        PUB_FLOAT = f;
    }

    public void setLongObject(Long l) {
        PUB_LONG_OBJECT = l ;
    }

    public void setLong(long l) {
        PUB_LONG = l ;
    }

    public void setShortObject(Short s) {
        PUB_SHORT_OBJECT = s;
    }

    public void setShort(short s) {
        PUB_SHORT = s;
    }

    public void setStringObject(String s) {
        PUB_STRING = s;
    }

    public void setObject( Object o ) {
        PUB_OBJECT = o;
    }

    public void setChar(char c) {
        PUB_CHAR = c;
    }

    public void setCharacter(Character c) {
        PUB_CHAR_OBJECT = c ;
    }

//  STATIC FIELDS

    public static final Class PUB_STATIC_FINAL_CLASS = null;
    public static Class       PUB_STATIC_CLASS       = PUB_STATIC_FINAL_CLASS;
    public Class              PUB_CLASS              = PUB_STATIC_FINAL_CLASS;

    public Class instanceGetClass() {
       return PUB_CLASS;
    }
    public void setClass( Class c ) {
        PUB_CLASS = c;
    }
    public Class staticGetClass() {
        return PUB_STATIC_CLASS;
    }
    public void staticSetClass( Class c ) {
        PUB_STATIC_CLASS = c;
    }

    public static final boolean PUB_STATIC_FINAL_BOOLEAN    = true;
    public static final byte    PUB_STATIC_FINAL_BYTE       = Byte.MAX_VALUE;
    public static final short   PUB_STATIC_FINAL_SHORT      = Short.MAX_VALUE;
    public static final int     PUB_STATIC_FINAL_INT        = Integer.MAX_VALUE;
    public static final long    PUB_STATIC_FINAL_LONG       = Long.MAX_VALUE;
    public static final float   PUB_STATIC_FINAL_FLOAT      = Float.MAX_VALUE;
    public static final double  PUB_STATIC_FINAL_DOUBLE     = Double.MAX_VALUE;
    public static final char    PUB_STATIC_FINAL_CHAR       = Character.MAX_VALUE;


    public static final Object      PUB_STATIC_FINAL_OBJECT = new Object();
    public static final Boolean     PUB_STATIC_FINAL_BOOLEAN_OBJECT = new Boolean(PUB_STATIC_FINAL_BOOLEAN);
    public static final Byte        PUB_STATIC_FINAL_BYTE_OBJECT = new Byte(PUB_STATIC_FINAL_BYTE);
    public static final Short       PUB_STATIC_FINAL_SHORT_OBJECT = new Short(PUB_STATIC_FINAL_SHORT);
    public static final Integer     PUB_STATIC_FINAL_INTEGER_OBJECT = new Integer(PUB_STATIC_FINAL_INT);
    public static final Long        PUB_STATIC_FINAL_LONG_OBJECT = new Long(PUB_STATIC_FINAL_LONG);
    public static final Float       PUB_STATIC_FINAL_FLOAT_OBJECT = new Float(PUB_STATIC_FINAL_FLOAT);
    public static final Double      PUB_STATIC_FINAL_DOUBLE_OBJECT = new Double(PUB_STATIC_FINAL_DOUBLE);
    public static final Character   PUB_STATIC_FINAL_CHAR_OBJECT = new Character(PUB_STATIC_FINAL_CHAR);
    public static final String      PUB_STATIC_FINAL_STRING  = new String("JavaScript Test");


    public static boolean PUB_STATIC_BOOLEAN    = PUB_STATIC_FINAL_BOOLEAN;
    public static byte    PUB_STATIC_BYTE       = PUB_STATIC_FINAL_BYTE;
    public static short   PUB_STATIC_SHORT      = PUB_STATIC_FINAL_SHORT;
    public static int     PUB_STATIC_INT        = PUB_STATIC_FINAL_INT;
    public static long    PUB_STATIC_LONG       = PUB_STATIC_FINAL_LONG;
    public static float   PUB_STATIC_FLOAT      = PUB_STATIC_FINAL_FLOAT;
    public static double  PUB_STATIC_DOUBLE     = PUB_STATIC_FINAL_DOUBLE;
    public static char    PUB_STATIC_CHAR       = PUB_STATIC_FINAL_CHAR;

    public static Object       PUB_STATIC_OBJECT = PUB_STATIC_FINAL_OBJECT;
    public static Boolean      PUB_STATIC_BOOLEAN_OBJECT = new Boolean(PUB_STATIC_FINAL_BOOLEAN);
    public static Byte         PUB_STATIC_BYTE_OBJECT = new Byte(PUB_STATIC_FINAL_BYTE);
    public static Short        PUB_STATIC_SHORT_OBJECT = new Short(PUB_STATIC_FINAL_SHORT);
    public static Integer      PUB_STATIC_INTEGER_OBJECT = new Integer(PUB_STATIC_FINAL_INT);
    public static Long         PUB_STATIC_LONG_OBJECT = new Long(PUB_STATIC_FINAL_LONG);
    public static Float        PUB_STATIC_FLOAT_OBJECT = new Float(PUB_STATIC_FINAL_FLOAT);
    public static Double       PUB_STATIC_DOUBLE_OBJECT = new Double(PUB_STATIC_FINAL_DOUBLE);
    public static Character    PUB_STATIC_CHAR_OBJECT = new Character(PUB_STATIC_FINAL_CHAR);
    public static String       PUB_STATIC_STRING  = PUB_STATIC_FINAL_STRING;

    private static final boolean PRI_STATIC_FINAL_BOOLEAN   = PUB_STATIC_FINAL_BOOLEAN;
    private static final byte    PRI_STATIC_FINAL_BYTE      = PUB_STATIC_FINAL_BYTE;
    private static final short   PRI_STATIC_FINAL_SHORT     = PUB_STATIC_FINAL_SHORT;
    private static final int     PRI_STATIC_FINAL_INT       = PUB_STATIC_FINAL_INT;
    private static final long    PRI_STATIC_FINAL_LONG      = PUB_STATIC_FINAL_LONG;
    private static final float   PRI_STATIC_FINAL_FLOAT     = PUB_STATIC_FINAL_FLOAT;
    private static final double  PRI_STATIC_FINAL_DOUBLE    = PUB_STATIC_FINAL_DOUBLE;
    private static final char    PRI_STATIC_FINAL_CHAR      = PUB_STATIC_FINAL_CHAR;

    private static final Object      PRI_STATIC_FINAL_OBJECT = PUB_STATIC_FINAL_OBJECT;
    private static final Boolean     PRI_STATIC_FINAL_BOOLEAN_OBJECT = new Boolean(PUB_STATIC_FINAL_BOOLEAN);
    private static final Byte        PRI_STATIC_FINAL_BYTE_OBJECT = new Byte( PUB_STATIC_FINAL_BYTE );
    private static final Short       PRI_STATIC_FINAL_SHORT_OBJECT = new Short(PUB_STATIC_FINAL_SHORT);
    private static final Integer     PRI_STATIC_FINAL_INTEGER_OBJECT = new Integer(PUB_STATIC_FINAL_INT);
    private static final Long        PRI_STATIC_FINAL_LONG_OBJECT = new Long(PUB_STATIC_FINAL_LONG);
    private static final Float       PRI_STATIC_FINAL_FLOAT_OBJECT = new Float(PUB_STATIC_FINAL_FLOAT);
    private static final Double      PRI_STATIC_FINAL_DOUBLE_OBJECT = new Double(PUB_STATIC_FINAL_DOUBLE);
    private static final Character   PRI_STATIC_FINAL_CHAR_OBJECT = new Character(PUB_STATIC_FINAL_CHAR);
    private static final String      PRI_STATIC_FINAL_STRING  = PUB_STATIC_FINAL_STRING;

    private static boolean PRI_STATIC_BOOLEAN   = PUB_STATIC_FINAL_BOOLEAN;
    private static byte    PRI_STATIC_BYTE      = PUB_STATIC_FINAL_BYTE;
    private static short   PRI_STATIC_SHORT     = PUB_STATIC_FINAL_SHORT;
    private static int     PRI_STATIC_INT       = PUB_STATIC_FINAL_INT;
    private static long    PRI_STATIC_LONG      = PUB_STATIC_FINAL_LONG;
    private static float   PRI_STATIC_FLOAT     = PUB_STATIC_FINAL_FLOAT;
    private static double  PRI_STATIC_DOUBLE    = PUB_STATIC_FINAL_DOUBLE;
    private static char    PRI_STATIC_CHAR      = PUB_STATIC_FINAL_CHAR;

    private static Object       PRI_STATIC_OBJECT   = PUB_STATIC_FINAL_OBJECT;
    private static Boolean      PRI_STATIC_BOOLEAN_OBJECT = new Boolean(PUB_STATIC_FINAL_BOOLEAN);
    private static Byte         PRI_STATIC_BYTE_OBJECT = new Byte( PUB_STATIC_FINAL_BYTE );
    private static Short        PRI_STATIC_SHORT_OBJECT = new Short(PUB_STATIC_FINAL_SHORT);
    private static Integer      PRI_STATIC_INTEGER_OBJECT = new Integer(PUB_STATIC_FINAL_INT);
    private static Long         PRI_STATIC_LONG_OBJECT = new Long(PUB_STATIC_FINAL_LONG);
    private static Float        PRI_STATIC_FLOAT_OBJECT = new Float(PUB_STATIC_FINAL_FLOAT);
    private static Double       PRI_STATIC_DOUBLE_OBJECT = new Double(PUB_STATIC_FINAL_DOUBLE);
    private static Character    PRI_STATIC_CHAR_OBJECT = new Character(PUB_STATIC_FINAL_CHAR);
    private static String       PRI_STATIC_STRING  = PUB_STATIC_FINAL_STRING;

    protected static final boolean PRO_STATIC_FINAL_BOOLEAN = PUB_STATIC_FINAL_BOOLEAN;
    protected static final byte    PRO_STATIC_FINAL_BYTE   = PUB_STATIC_FINAL_BYTE;
    protected static final short   PRO_STATIC_FINAL_SHORT = PUB_STATIC_FINAL_SHORT;
    protected static final int     PRO_STATIC_FINAL_INT = PUB_STATIC_FINAL_INT;
    protected static final long    PRO_STATIC_FINAL_LONG = PUB_STATIC_FINAL_LONG;
    protected static final float   PRO_STATIC_FINAL_FLOAT = PUB_STATIC_FINAL_FLOAT;
    protected static final double  PRO_STATIC_FINAL_DOUBLE = PUB_STATIC_FINAL_DOUBLE;
    protected static final char    PRO_STATIC_FINAL_CHAR = PUB_STATIC_FINAL_CHAR;

    protected static final Object      PRO_STATIC_FINAL_OBJECT = PUB_STATIC_FINAL_OBJECT;
    protected static final Boolean     PRO_STATIC_FINAL_BOOLEAN_OBJECT = new Boolean(PUB_STATIC_FINAL_BOOLEAN);
    protected static final Byte        PRO_STATIC_FINAL_BYTE_OBJECT = new Byte(PUB_STATIC_FINAL_BYTE);
    protected static final Short       PRO_STATIC_FINAL_SHORT_OBJECT = new Short(PUB_STATIC_FINAL_SHORT);
    protected static final Integer     PRO_STATIC_FINAL_INTEGER_OBJECT = new Integer(PUB_STATIC_FINAL_INT);
    protected static final Long        PRO_STATIC_FINAL_LONG_OBJECT = new Long(PUB_STATIC_FINAL_LONG);
    protected static final Float       PRO_STATIC_FINAL_FLOAT_OBJECT = new Float(PUB_STATIC_FINAL_FLOAT);
    protected static final Double      PRO_STATIC_FINAL_DOUBLE_OBJECT = new Double(PUB_STATIC_FINAL_DOUBLE);
    protected static final Character   PRO_STATIC_FINAL_CHAR_OBJECT = new Character(PUB_STATIC_FINAL_CHAR);
    protected static final String PRO_STATIC_FINAL_STRING  = PUB_STATIC_FINAL_STRING;

    protected static boolean PRO_STATIC_BOOLEAN = PUB_STATIC_FINAL_BOOLEAN;
    protected static byte    PRO_STATIC_BYTE    = PUB_STATIC_FINAL_BYTE;
    protected static short   PRO_STATIC_SHORT   = PUB_STATIC_FINAL_SHORT;
    protected static int     PRO_STATIC_INT     = PUB_STATIC_FINAL_INT;
    protected static long    PRO_STATIC_LONG    = PUB_STATIC_FINAL_LONG;
    protected static float   PRO_STATIC_FLOAT   = PUB_STATIC_FINAL_FLOAT;
    protected static double  PRO_STATIC_DOUBLE  = PUB_STATIC_FINAL_DOUBLE;
    protected static char    PRO_STATIC_CHAR    = PUB_STATIC_FINAL_CHAR;

    protected static Object       PRO_STATIC_OBJECT;
    protected static Boolean      PRO_STATIC_BOOLEAN_OBJECT = new Boolean(PUB_STATIC_FINAL_BOOLEAN);
    protected static Byte         PRO_STATIC_BYTE_OBJECT = new Byte(PUB_STATIC_FINAL_BYTE);
    protected static Short        PRO_STATIC_SHORT_OBJECT = new Short(PUB_STATIC_FINAL_SHORT);
    protected static Integer      PRO_STATIC_INTEGER_OBJECT = new Integer(PUB_STATIC_FINAL_INT);
    protected static Long         PRO_STATIC_LONG_OBJECT = new Long(PUB_STATIC_FINAL_LONG);
    protected static Float        PRO_STATIC_FLOAT_OBJECT = new Float(PUB_STATIC_FINAL_FLOAT);
    protected static Double       PRO_STATIC_DOUBLE_OBJECT = new Double(PUB_STATIC_FINAL_DOUBLE);
    protected static Character    PRO_STATIC_CHAR_OBJECT = new Character(PUB_STATIC_FINAL_CHAR);
    protected static String       PRO_STATIC_STRING  = PUB_STATIC_FINAL_STRING;

    static final boolean STATIC_FINAL_BOOLEAN   = PUB_STATIC_FINAL_BOOLEAN;
    static final byte    STATIC_FINAL_BYTE      = PUB_STATIC_FINAL_BYTE;
    static final short   STATIC_FINAL_SHORT     = PUB_STATIC_FINAL_SHORT;
    static final int     STATIC_FINAL_INT       = PUB_STATIC_FINAL_INT;
    static final long    STATIC_FINAL_LONG      = PUB_STATIC_FINAL_LONG;
    static final float   STATIC_FINAL_FLOAT     = PUB_STATIC_FINAL_FLOAT;
    static final double  STATIC_FINAL_DOUBLE    = PUB_STATIC_FINAL_DOUBLE;
    static final char    STATIC_FINAL_CHAR      = PUB_STATIC_FINAL_CHAR;

    static final Object      STATIC_FINAL_OBJECT    = PUB_STATIC_FINAL_OBJECT;
    static final Boolean     STATIC_FINAL_BOOLEAN_OBJECT = new Boolean(PUB_STATIC_FINAL_BOOLEAN);
    static final Byte        STATIC_FINAL_BYTE_OBJECT  = new Byte(PUB_STATIC_FINAL_BYTE);
    static final Short       STATIC_FINAL_SHORT_OBJECT = new Short(PUB_STATIC_FINAL_SHORT);
    static final Integer     STATIC_FINAL_INTEGER_OBJECT = new Integer(PUB_STATIC_FINAL_INT);
    static final Long        STATIC_FINAL_LONG_OBJECT = new Long(PUB_STATIC_FINAL_LONG);
    static final Float       STATIC_FINAL_FLOAT_OBJECT = new Float(PUB_STATIC_FINAL_FLOAT);
    static final Double      STATIC_FINAL_DOUBLE_OBJECT = new Double(PUB_STATIC_FINAL_DOUBLE);
    static final Character   STATIC_FINAL_CHAR_OBJECT = new Character(PUB_STATIC_FINAL_CHAR);
    static final String STATIC_FINAL_STRING  = PUB_STATIC_FINAL_STRING;

    static boolean STATIC_BOOLEAN   = PUB_STATIC_FINAL_BOOLEAN;
    static byte    STATIC_BYTE      = PUB_STATIC_FINAL_BYTE;
    static short   STATIC_SHORT     = PUB_STATIC_FINAL_SHORT;
    static int     STATIC_INT       = PUB_STATIC_FINAL_INT;
    static long    STATIC_LONG      = PUB_STATIC_FINAL_LONG;
    static float   STATIC_FLOAT     = PUB_STATIC_FINAL_FLOAT;
    static double  STATIC_DOUBLE    = PUB_STATIC_FINAL_DOUBLE;
    static char    STATIC_CHAR      = PUB_STATIC_FINAL_CHAR;

    static Object       STATIC_OBJECT   = PUB_STATIC_FINAL_OBJECT;
    static Boolean      STATIC_BOOLEAN_OBJECT = new Boolean(PUB_STATIC_FINAL_BOOLEAN);
    static Byte         STATIC_BYTE_OBJECT  = new Byte(PUB_STATIC_FINAL_BYTE);
    static Short        STATIC_SHORT_OBJECT = new Short(PUB_STATIC_FINAL_SHORT);
    static Integer      STATIC_INTEGER_OBJECT = new Integer(PUB_STATIC_FINAL_INT);
    static Long         STATIC_LONG_OBJECT = new Long(PUB_STATIC_FINAL_LONG);
    static Float        STATIC_FLOAT_OBJECT = new Float(PUB_STATIC_FINAL_FLOAT);
    static Double       STATIC_DOUBLE_OBJECT = new Double(PUB_STATIC_FINAL_DOUBLE);
    static Character    STATIC_CHAR_OBJECT = new Character(PUB_STATIC_FINAL_CHAR);
    static String       STATIC_STRING  = PUB_STATIC_FINAL_STRING;

//  INSTANCE FIELDS
    boolean BOOLEAN = PUB_STATIC_FINAL_BOOLEAN;
    byte    BYTE = PUB_STATIC_FINAL_BYTE;
    short   SHORT = PUB_STATIC_FINAL_SHORT;
    int     INT = PUB_STATIC_FINAL_INT;
    long    LONG = PUB_STATIC_FINAL_LONG;
    float   FLOAT = PUB_STATIC_FINAL_FLOAT;
    double  DOUBLE = PUB_STATIC_FINAL_DOUBLE;
    char    CHAR = PUB_STATIC_FINAL_CHAR;

    Boolean      BOOLEAN_OBJECT = new Boolean( BOOLEAN );
    Byte         BYTE_OBJECT    = new Byte( BYTE );
    Integer      INTEGER_OBJECT = new Integer( INT );
    Long         LONG_OBJECT    = new Long( LONG );
    Float        FLOAT_OBJECT   = new Float( FLOAT );
    Double       DOUBLE_OBJECT  = new Double( DOUBLE );
    Character    CHARACTER      = new Character( CHAR );
    Object       OBJECT         = new Object();
    String       STRING         = new String("PASSED");
    Short        SHORT_OBJECT   = new Short( SHORT );

    public boolean PUB_BOOLEAN  = PUB_STATIC_FINAL_BOOLEAN;
    public byte    PUB_BYTE     = PUB_STATIC_FINAL_BYTE;
    public short   PUB_SHORT    = PUB_STATIC_FINAL_SHORT;
    public int     PUB_INT      = PUB_STATIC_FINAL_INT;
    public long    PUB_LONG     = PUB_STATIC_FINAL_LONG;
    public float   PUB_FLOAT    = PUB_STATIC_FINAL_FLOAT;
    public double  PUB_DOUBLE   = PUB_STATIC_FINAL_DOUBLE;
    public char    PUB_CHAR     = PUB_STATIC_FINAL_CHAR;

    public Object       PUB_OBJECT          = new Object();
    public Boolean      PUB_BOOLEAN_OBJECT  = new Boolean(PUB_STATIC_FINAL_BOOLEAN);
    public Byte         PUB_BYTE_OBJECT     = new Byte( BYTE );
    public Integer      PUB_INTEGER_OBJECT  = new Integer(PUB_STATIC_FINAL_INT);
    public Short        PUB_SHORT_OBJECT    = new Short( PUB_STATIC_FINAL_SHORT );
    public Long         PUB_LONG_OBJECT     = new Long(PUB_STATIC_FINAL_LONG);
    public Float        PUB_FLOAT_OBJECT    = new Float(PUB_STATIC_FINAL_FLOAT);
    public Double       PUB_DOUBLE_OBJECT   = new Double(PUB_STATIC_FINAL_DOUBLE);
    public Character    PUB_CHAR_OBJECT     = new Character(PUB_STATIC_FINAL_CHAR);
    public String       PUB_STRING          = PUB_STATIC_FINAL_STRING;

    private boolean PRI_BOOLEAN = PUB_STATIC_FINAL_BOOLEAN;
    private byte    PRI_BYTE    = PUB_STATIC_FINAL_BYTE;
    private short   PRI_SHORT   = PUB_STATIC_FINAL_SHORT;
    private int     PRI_INT     = PUB_STATIC_FINAL_INT;
    private long    PRI_LONG    = PUB_STATIC_FINAL_LONG;
    private float   PRI_FLOAT   = PUB_STATIC_FINAL_FLOAT;
    private double  PRI_DOUBLE  = PUB_STATIC_FINAL_DOUBLE;
    private char    PRI_CHAR    = PUB_STATIC_FINAL_CHAR;

    private Object       PRI_OBJECT = PUB_STATIC_FINAL_OBJECT;
    private Boolean      PRI_BOOLEAN_OBJECT = new Boolean(PUB_STATIC_FINAL_BOOLEAN);
    private Byte         PRI_BYTE_OBJECT = new Byte(PUB_STATIC_FINAL_BYTE);
    private Short        PRI_SHORT_OBJECT = new Short(PUB_STATIC_FINAL_SHORT);
    private Integer      PRI_INTEGER_OBJECT = new Integer(PUB_STATIC_FINAL_INT);
    private Long         PRI_LONG_OBJECT = new Long(PUB_STATIC_FINAL_LONG);
    private Float        PRI_FLOAT_OBJECT = new Float(PUB_STATIC_FINAL_FLOAT);
    private Double       PRI_DOUBLE_OBJECT = new Double(PUB_STATIC_FINAL_DOUBLE);
    private Character    PRI_CHAR_OBJECT = new Character(PUB_STATIC_FINAL_CHAR);
    private String       PRI_STRING  = PUB_STATIC_FINAL_STRING;

    protected boolean PRO_BOOLEAN   = PUB_STATIC_FINAL_BOOLEAN;
    protected byte    PRO_BYTE      = PUB_STATIC_FINAL_BYTE;
    protected short   PRO_SHORT     = PUB_STATIC_FINAL_SHORT;
    protected int     PRO_INT       = PUB_STATIC_FINAL_INT;
    protected long    PRO_LONG      = PUB_STATIC_FINAL_LONG;
    protected float   PRO_FLOAT     = PUB_STATIC_FINAL_FLOAT;
    protected double  PRO_DOUBLE    = PUB_STATIC_FINAL_DOUBLE;
    protected char    PRO_CHAR      = PUB_STATIC_FINAL_CHAR;

    protected Object       PRO_OBJECT   = PUB_STATIC_FINAL_OBJECT;
    protected Boolean      PRO_BOOLEAN_OBJECT = new Boolean(PUB_STATIC_FINAL_BOOLEAN);
    protected Byte         PRO_BYTE_OBJECT  = new Byte(PUB_STATIC_FINAL_BYTE);
    protected Short        PRO_SHORT_OBJECT = new Short(PUB_STATIC_FINAL_SHORT);
    protected Integer      PRO_INTEGER_OBJECT = new Integer(PUB_STATIC_FINAL_INT);
    protected Long         PRO_LONG_OBJECT = new Long(PUB_STATIC_FINAL_LONG);
    protected Float        PRO_FLOAT_OBJECT = new Float(PUB_STATIC_FINAL_FLOAT);
    protected Double       PRO_DOUBLE_OBJECT = new Double(PUB_STATIC_FINAL_DOUBLE);
    protected Character    PRO_CHAR_OBJECT = new Character(PUB_STATIC_FINAL_CHAR);
    protected String       PRO_STRING  = PUB_STATIC_FINAL_STRING;

//  STATIC ARRAYS
    public static byte[] staticGetByteArray() {
        return PUB_STATIC_ARRAY_BYTE;
    }
    public static char[] staticGetCharArray() {
        return PUB_STATIC_ARRAY_CHAR;
    }
    public static double[] staticGetDoubleArray() {
        return PUB_STATIC_ARRAY_DOUBLE;
    }
    public static short[] staticGetShortArray() {
        return PUB_STATIC_ARRAY_SHORT;
    }
    public static long[] staticGetLongArray() {
        return PUB_STATIC_ARRAY_LONG;
    }
    public static int[] staticGetIntArray() {
        return PUB_STATIC_ARRAY_INT;
    }
    public static float[] staticGetFloatArray() {
        return PUB_STATIC_ARRAY_FLOAT;
    }
    public static Object[] staticGetObjectArray() {
        return PUB_STATIC_ARRAY_OBJECT;
    }

//  INSTANCE ARRAYS
    public byte[] getByteArray() {
        return PUB_STATIC_ARRAY_BYTE;
    }
    public char[] getCharArray() {
        return PUB_STATIC_ARRAY_CHAR;
    }
    public double[] getDoubleArray() {
        return PUB_STATIC_ARRAY_DOUBLE;
    }
    public short[] getShortArray() {
        return PUB_STATIC_ARRAY_SHORT;
    }
    public long[] getLongArray() {
        return PUB_STATIC_ARRAY_LONG;
    }
    public int[] getIntArray() {
        return PUB_STATIC_ARRAY_INT;
    }
    public float[] getFloatArray() {
        return PUB_STATIC_ARRAY_FLOAT;
    }
    public Object[] getObjectArray() {
        return PUB_STATIC_ARRAY_OBJECT;
    }

//  INSTANCE ARRAY SETTERS
    public void setByteArray(byte[] b) {
        PUB_STATIC_ARRAY_BYTE = b;
    }
    public void setCharArray(char[] c) {
        PUB_STATIC_ARRAY_CHAR = c;
    }
    public void setDoubleArray(double[] d) {
        PUB_STATIC_ARRAY_DOUBLE = d;
    }
    public void setShortArray(short[] s) {
        PUB_STATIC_ARRAY_SHORT = s;
    }
    public void setLongArray(long[] l) {
        PUB_STATIC_ARRAY_LONG = l;
    }
    public void setIntArray(int[] i) {
        PUB_STATIC_ARRAY_INT= i;
    }
    public void setFloatArray(float[] f) {
        PUB_STATIC_ARRAY_FLOAT = f;
    }
    public void setObjectArray(Object[] o) {
        PUB_STATIC_ARRAY_OBJECT = o;
    }

//  STATIC ARRAY DEFINITIONS

    public static byte      PUB_STATIC_ARRAY_BYTE[]    =
        new String( PUB_STATIC_FINAL_STRING ).getBytes();

    public static char      PUB_STATIC_ARRAY_CHAR[]    =
        new String( PUB_STATIC_FINAL_STRING).toCharArray();

    public static double    PUB_STATIC_ARRAY_DOUBLE[]  =
        { Double.MIN_VALUE, Double.MAX_VALUE, Double.NaN,
        Double.NEGATIVE_INFINITY, Double.POSITIVE_INFINITY };

    public static short     PUB_STATIC_ARRAY_SHORT[]   =
        { Short.MIN_VALUE, Short.MAX_VALUE };

    public static int       PUB_STATIC_ARRAY_INT[]     =
        { Integer.MIN_VALUE, Integer.MAX_VALUE};

    public static long      PUB_STATIC_ARRAY_LONG[]    =
        { Long.MIN_VALUE, Long.MAX_VALUE};

    public static float     PUB_STATIC_ARRAY_FLOAT[]   =
        { Float.MIN_VALUE, Float.MAX_VALUE, Float.NaN,
        Float.NEGATIVE_INFINITY, Float.POSITIVE_INFINITY };

    public static Object    PUB_STATIC_ARRAY_OBJECT[]  =
        {   PUB_STATIC_ARRAY_BYTE, PUB_STATIC_ARRAY_CHAR,
            PUB_STATIC_ARRAY_DOUBLE, PUB_STATIC_ARRAY_SHORT,
            PUB_STATIC_ARRAY_INT, PUB_STATIC_ARRAY_LONG,
            PUB_STATIC_ARRAY_FLOAT };

    public static String    PUB_STATIC_ARRAY_STRING[] =
        { "JavaScript", "LiveConnect", "Java" };

//   public static JSObject  PUB_STATIC_ARRAY_JSOBJECT[]

//  INSTANCE ARRAY DEFINITIONS

    public byte      PUB_ARRAY_BYTE[]    =
        new String( PUB_STATIC_FINAL_STRING ).getBytes();

    public char      PUB_ARRAY_CHAR[]    =
        new String( PUB_STATIC_FINAL_STRING ).toCharArray();

    public double    PUB_ARRAY_DOUBLE[]  =
        { Double.MIN_VALUE, Double.MAX_VALUE, Double.NaN,
        Double.NEGATIVE_INFINITY, Double.POSITIVE_INFINITY };

    public short     PUB_ARRAY_SHORT[]   =
        { Short.MIN_VALUE, Short.MAX_VALUE };

    public int       PUB_ARRAY_INT[]     =
        { Integer.MIN_VALUE, Integer.MAX_VALUE};

    public long      PUB_ARRAY_LONG []     =
        { Long.MIN_VALUE, Long.MAX_VALUE};

    public float     PUB_ARRAY_FLOAT[]   =
        { Float.MIN_VALUE, Float.MAX_VALUE, Float.NaN,
        Float.NEGATIVE_INFINITY, Float.POSITIVE_INFINITY };

    public Object    PUB_ARRAY_OBJECT[]  =
        {   PUB_ARRAY_BYTE, PUB_ARRAY_CHAR,
            PUB_ARRAY_DOUBLE, PUB_ARRAY_SHORT,
            PUB_ARRAY_INT, PUB_ARRAY_LONG,
            PUB_ARRAY_FLOAT };

    public String    PUB_ARRAY_STRING[] =
        { "JavaScript", "LiveConnect", "Java" };

//   public JSObject  PUB_ARRAY_JSOBJECT[]
}