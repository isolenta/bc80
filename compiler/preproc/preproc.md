## (Reduced) C Preprocessor

As soon as my own language is inspired by C but it is not standard C actually, the preprocessor also has a few restrictions and limitations comparing with "standard C preprocessor":

- digraphs, trigraphs are not supported
- you can't substitute constants as part of #include argument, i.e. recursive constant evaluation is not supported.
- __FUNCTION_, __STDC__ (and family) are not supported
- stringify and concat operators are not supported


### TODO:

- define: add function-like macro
- substitute function-like macro call
- ifdef-if-else-elif-endif and expressions
- sysroot, include path list
- makefile dependencies issues (rebuild cpp if common/ changed)
- preproc as library
