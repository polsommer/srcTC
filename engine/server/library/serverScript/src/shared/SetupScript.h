// ======================================================================
//
// SetupScript.h
// copyright 1998 Bootprint Entertainment
//
// ======================================================================

#pragma once

#include <cstdint>

// ======================================================================

class SetupScript
{
public:
        // **** INCREMENT THIS VERSION NUMBER WHENEVER THIS STRUCT CHANGES ****
        static constexpr std::int32_t dataVersion = 0;

        struct Data
        {
                std::int32_t version = dataVersion;

                // script data
                bool useRemoteDebugJava = false; // flag to use the jvm for remote debugging
        };

public:
        static void setupDefaultGameData(Data &data) noexcept;
        static Data createDefaultGameData() noexcept;

        static void install();
        static void install(Data const &data);
        static void remove() noexcept;
};

// ======================================================================

