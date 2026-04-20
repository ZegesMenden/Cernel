#include <iostream>
#include <fstream>
#include "elfparse.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <filename>\n";
        return 1;
    }
    std::ifstream file(argv[1], std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file: " << argv[1] << "\n";
        return 1;
    }
    std::cout << "File opened successfully: " << argv[1] << "\n";

    std::streampos filesize = 0;
    filesize = file.tellg();
    file.seekg( 0, std::ios::end );
    filesize = file.tellg() - filesize;
    std::cout << "File size: " << filesize << std::endl;

    uint8_t* filebuf = new uint8_t[filesize];
    file.read(reinterpret_cast<char*>(filebuf), filesize);
    if (!file) {
        std::cerr << "Failed to read ELF file.\n";
        return 1;
    }
    file.close();

    
    
    elf32_load_alloc_sections(filebuf, filesize, NULL);
    
    return 0;
}
