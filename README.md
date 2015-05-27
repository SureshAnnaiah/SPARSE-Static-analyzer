Enhancing features supported by SPARSE static analyzer using SMATCH wrapper
=========================================================================================================
I added one patch and one supporting header file for SMATCH ( wrapper tool for SPARSE static analyser).

SMATCH added many functionality(memory leaks,invalid function parameters,invalid pointer usage) for existing SPARSE features.

Detection after memory leak : SMATCH is able to detect few basic memory flaws.The patch that uploaded have implementation
of detecting pointer assignment after free feature. To implement this feature i had using hashing datastructure that will maintain
dependency list for every malloced memory.
