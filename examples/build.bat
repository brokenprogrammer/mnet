@echo off

if not exist build mkdir build

pushd build

set compile_flags=-nologo /Zi /Od /FC /W3
start /b /wait "" "cl.exe"  %compile_flags% ../simple_client.c
start /b /wait "" "cl.exe"  %compile_flags% ../tcp_echo_server.c
start /b /wait "" "cl.exe"  %compile_flags% ../simple_http.c

popd