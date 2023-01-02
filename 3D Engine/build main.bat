
call "D:\Program Files (x86)\Microsoft Visual Studio\VC\Auxiliary\Build\vcvarsall.bat" x86_amd64
cl main.c -Fe:main.exe user32.lib gdi32.lib

@call main.exe



