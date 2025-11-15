#include "FirstTaskManager.h"
#include <stdlib.h>
#include <string>
#include <iostream>

#ifdef _WIN32
    #include <windows.h>
#endif

namespace EnvironmentVariable
{
    // Helper function to handle appending values to existing environment variables
    std::string appendToExistingValue(const std::string& oldValue, const std::string& newValue)
    {
        std::string result = oldValue;
        result += ":"; // Add the separator
        result += newValue;

        // Handle case where the first character happens to be ':'
        if (result[0] == ':')
            result.erase(0, 1); // Remove leading colon if it exists

        return result;
    }

#ifdef _WIN32
    // Windows-specific function to set environment variables using _putenv
    bool setEnvWindows(const char* key, const char* value)
    {
        std::string envString = std::string(key) + "=" + value;
        return (_putenv(envString.c_str()) == 0);
    }
#else
    // Unix-like systems function using setenv
    bool setEnvUnix(const char* key, const char* value, bool overwrite)
    {
        return (setenv(key, value, overwrite) == 0);
    }
#endif

    // Adds the provided value to an existing environment variable or creates a new one
    bool addToEnvironmentVariable(const char* key, const char* value)
    {
        bool retval = false;
        const char* oldValue = getenv(key);

        if (oldValue)
        {
            // Append the new value to the existing one
            std::string newEnvValue = appendToExistingValue(oldValue, value);

#ifdef _WIN32
            retval = setEnvWindows(key, newEnvValue.c_str());
#else
            retval = setEnvUnix(key, newEnvValue.c_str(), true);
#endif
        }
        else
        {
            // No existing value, just set the new one
#ifdef _WIN32
            retval = setEnvWindows(key, value);
#else
            retval = setEnvUnix(key, value, false);
#endif
        }

        return retval;
    }

    // Sets an environment variable, overwriting any existing value
    bool setEnvironmentVariable(const char* key, const char* value)
    {
#ifdef _WIN32
        return setEnvWindows(key, value);
#else
        return setEnvUnix(key, value, true);
#endif
    }
};
