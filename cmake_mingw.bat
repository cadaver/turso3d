cmake -E make_directory Build
cmake -E chdir Build cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ..
