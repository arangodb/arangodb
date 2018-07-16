package org.tartarus.snowball;

import java.lang.reflect.Method;

public class Among {
    public Among (String s, int substring_i, int result) {
        this.s = s.toCharArray();
        this.substring_i = substring_i;
	this.result = result;
	this.method = null;
    }

    public Among (String s, int substring_i, int result, String methodname,
		  Class<? extends SnowballProgram> programclass) {
        this.s = s.toCharArray();
        this.substring_i = substring_i;
	this.result = result;
	try {
	    this.method = programclass.getDeclaredMethod(methodname);
	} catch (NoSuchMethodException e) {
	    throw new RuntimeException(e);
	}
    }

    public final char[] s; /* search string */
    public final int substring_i; /* index to longest matching substring */
    public final int result; /* result of the lookup */
    public final Method method; /* method to use if substring matches */
};
