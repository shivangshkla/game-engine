@echo off

set Wildcard=*.h *.cpp *.inl *.c

echo --------
echo --------

echo STATICS_FOUND:
findstr -s -n -i -l "static" %Wildcard%

echo --------
echo --------

echo GLOBALS_FOUND:
findstr -s -n -i -l "local_persist" %Wildcard%
findstr -s -n -i -l "global" %Wildcard%

echo --------
echo --------
