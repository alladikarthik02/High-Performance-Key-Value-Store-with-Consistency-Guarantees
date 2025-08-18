#pragma once

#include <libnuraft/nuraft.hxx>
#include <iostream>

class RaftLogger : public nuraft::logger {
public:
    RaftLogger() = default;

    void put_details(int level,
                    const char* source_file,
                    const char* func_name,
                    size_t line_number,
                    const std::string& msg) override {
        // Simple console logging
        std::cout << "[" << level << "] " << source_file << ":" << line_number 
                  << " " << func_name << "() - " << msg << std::endl;
    }

    void flush() override {
        std::cout.flush();
    }
};