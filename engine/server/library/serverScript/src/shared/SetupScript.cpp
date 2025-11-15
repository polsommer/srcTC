// ======================================================================
//
// SetupScript.cpp
//
// copyright 2001 Sony Online Entertainment
//
// ======================================================================

#include "serverScript/FirstServerScript.h"
#include "serverScript/SetupScript.h"

#include "serverScript/ConfigServerScript.h"

namespace
{
        bool s_installed = false;
}

// ----------------------------------------------------------------------
/**
 * Install the engine.
 *
 * The settings in the Data structure will determine which subsystems
 * get initialized.
 */

void SetupScript::install()
{
        install(createDefaultGameData());
}

void SetupScript::install(Data const &data)
{
        if (s_installed)
        {
                DEBUG_WARNING(true, ("SetupScript::install called more than once; ignoring subsequent call."));
                return;
        }

        if (data.version != dataVersion)
        {
                DEBUG_WARNING(true, ("SetupScript data version mismatch. Expected %d, received %d.", dataVersion, data.version));
        }

        // setup the engine configuration
        ConfigServerScript::install();
        s_installed = true;
}

// ----------------------------------------------------------------------
/**
 * Uninstall the engine.
 *
 * This routine will properly uninstall the engine components that were
 * installed by SetupScript::install().
 */

void SetupScript::remove() noexcept
{
        if (!s_installed)
        {
                return;
        }

        ConfigServerScript::remove();
        s_installed = false;
}

// ----------------------------------------------------------------------

void SetupScript::setupDefaultGameData(Data &data) noexcept
{
        data = createDefaultGameData();
}

// ----------------------------------------------------------------------

SetupScript::Data SetupScript::createDefaultGameData() noexcept
{
        return Data{};
}

// ----------------------------------------------------------------------

