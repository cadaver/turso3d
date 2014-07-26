:: For conditions of distribution and use, see copyright notice in License.txt

@echo off
pushd %~dp0

:: Clean the CMake cache
if exist Build\CMakeCache.txt. del /F Build\CMakeCache.txt

:: Clean CMakeFiles directory
if exist Build\CMakeFiles. rd /S /Q Build\CMakeFiles

popd
