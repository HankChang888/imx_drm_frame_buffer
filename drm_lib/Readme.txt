1.Compiler
gcc -shared -o libdrm.so -fPIC libdrm.c $(pkg-config --cflags --libs libdrm)
