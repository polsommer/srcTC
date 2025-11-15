// ======================================================================
//
// SwgLoaderSnapshotGroup.cpp
// copyright (c) 2001 Sony Online Entertainment
//
// ======================================================================

#include "SwgDatabaseServer/FirstSwgDatabaseServer.h"
#include "SwgLoaderSnapshotGroup.h"

#include "SwgDatabaseServer/SwgSnapshot.h"

// ======================================================================

SwgLoaderSnapshotGroup::SwgLoaderSnapshotGroup(uint32 requestingProcess)
    : LoaderSnapshotGroup(requestingProcess, new SwgSnapshot(DB::ModeQuery::mode_SELECT, false))
{
}

// ----------------------------------------------------------------------

SwgLoaderSnapshotGroup::~SwgLoaderSnapshotGroup()
{
    // No explicit cleanup required, relying on base class cleanup and smart memory management
}

// ----------------------------------------------------------------------

Snapshot* SwgLoaderSnapshotGroup::makeGoldSnapshot()
{
    // Creates a new SwgSnapshot in gold mode
    return new SwgSnapshot(DB::ModeQuery::mode_SELECT, true);
}

// ======================================================================
