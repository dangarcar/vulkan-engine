cd build

cmake ..
if(-not $?) {
    cd ..
    echo "Cmake went wrong!!!"
    exit 1
}

make
if(-not $?) {
    cd ..
    echo "Make process was finished unsuccessfully!!!!"
    exit 1
}

cd ..
./build/EngineTest.exe
