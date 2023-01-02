#include <iostream>
#include "vms/simple.cpp"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: stemu <executable_binary_path>" << std::endl;
        return 1;
    }
    scsmExec(readBinaryFile(argv[1]));
    return 0;
}