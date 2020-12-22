astyle.exe -v --formatted --options=astyle-options.ini --recursive *.h
astyle.exe -v --formatted --options=astyle-options.ini --exclude="idlib/math/Simd_SSE.cpp" --exclude="sys/win32/win_cpu.cpp" --exclude="sys/win32/win_main.cpp" --recursive *.cpp

pause