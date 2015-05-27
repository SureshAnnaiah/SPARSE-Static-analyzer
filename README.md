Enhancing features supported by SPARSE static analyzer using SMATCH wrapper
=========================================================================================================
Added Detection after memory leak functionality for SMATCH ( wrapper tool for SPARSE static analyser).

SMATCH provides many functionality(like memory leaks,invalid function parameters,invalid pointer usage) on top of existing features of SPARSE static analyzer.

Detection after memory leak : SMATCH is able to detect few basic memory flaws.The patch that have uploaded has implementation of detecting pointer assignment after free feature. To implement this feature i had using hashing datastructure that will maintain dependency list for every malloced memory.
