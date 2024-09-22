#include "config.hpp"
#include <iostream>

std::string get_env_var(const std::string& var_name) {
    const char* value = std::getenv(var_name.c_str());
    return value ? std::string(value) : std::string("");
}

std::string get_api_key() {
    return get_env_var("DERIBIT_API_KEY");
}

std::string get_api_secret() {
    return get_env_var("DERIBIT_API_SECRET");
}