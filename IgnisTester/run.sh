if [ ! -d "build" ]; then
    mkdir build
    cmake -E chdir build cmake ..
    ln -sf build/compile_commands.json ../compile_commands.json
fi
cd ./build
cmake --build .
cd ../Output
./IgnisTester
