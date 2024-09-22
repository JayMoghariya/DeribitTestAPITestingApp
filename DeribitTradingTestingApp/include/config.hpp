#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <cstdlib>

std::string get_env_var(const std::string& var_name);

std::string get_api_key();
std::string get_api_secret();

#endif // CONFIG_HPP

// export DERIBIT_API_KEY="your_api_key"
// export DERIBIT_API_SECRET="your_api_secret"