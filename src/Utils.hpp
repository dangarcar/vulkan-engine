#pragma once

#include <chrono>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>

namespace fly {

    class Timer {
    public:
        Timer(const std::string& label): label{label} { 
            this->start = std::chrono::steady_clock::now(); 
        }
        void reset() { this->start = std::chrono::steady_clock::now(); }

        double elapsed() const {
            std::chrono::duration<double> dt = std::chrono::steady_clock::now() - start;
            return dt.count();
        }

        void printElapsed() const {
            std::cout << label << ": " << this->elapsed() * 1000 << "ms\n";
        }

    private:
        std::chrono::time_point<std::chrono::steady_clock> start;
        std::string label;
    };

    class ScopeTimer: public Timer {
    public:
        ScopeTimer(const std::string& label): Timer{label} {}
        
        ~ScopeTimer(){
            this->printElapsed();
        }    
    };

    inline std::vector<char> readFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
    
        if (!file.is_open()) {
            throw std::runtime_error("failed to open file!");
        }

        size_t fileSize = (size_t) file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);

        file.close();

        return buffer;
    }

}
