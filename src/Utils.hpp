#pragma once

#include <chrono>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <format>
#include <glm/glm.hpp>

#ifndef NDEBUG
    inline void _assert_format() {} // Empty overload

    template <typename... Args>
    inline void _assert_format(const std::string& fmt, Args&&... args) {
        std::cerr << '\n' << std::vformat(fmt, std::make_format_args(std::forward<Args>(args)...)) << '\n';
    }

    #define FLY_ASSERT(expr, ...) \
        do { \
            if(!(expr)) { \
                std::cerr << std::format("Assertion failed: {}\n \tat {}:{}", #expr, __FILE__, __LINE__); \
                _assert_format(__VA_ARGS__); \
                std::abort(); \
            } \
        } while (0)
#else
    #define FLY_ASSERT(expr, ...) ((void)0)
#endif

namespace fly {

    namespace print {

        // Overload operator << for glm::vec
        template <glm::length_t L, typename T, glm::qualifier Q>
        std::ostream& operator<<(std::ostream& os, const glm::vec<L, T, Q>& v) {
            os << "(";
            for (glm::length_t i = 0; i < L; i++) {
                os << v[i];
                if (i < L - 1) os << ", ";
            }
            os << ")";
            return os;
        }

    }

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
            throw std::runtime_error(std::format("failed to open file {}!", filename));
        }

        size_t fileSize = (size_t) file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);

        file.close();

        return buffer;
    }

}
