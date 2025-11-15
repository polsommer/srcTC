// ======================================================================
//
// SwgDatabaseServer.cpp
// copyright (c) 2001 Sony Online Entertainment
//
// ======================================================================

#include "SwgDatabaseServer/FirstSwgDatabaseServer.h"
#include "SwgDatabaseServer.h"

// Additional necessary includes
#include "SwgDatabaseServer/DataCleanupManager.h"
#include "SwgDatabaseServer/ObjvarNameManager.h"
#include "SwgDatabaseServer/SwgLoader.h"
#include "SwgDatabaseServer/SwgPersister.h"
#include "serverDatabase/ConfigServerDatabase.h"
#include "serverDatabase/DataLookup.h"
#include "serverDatabase/LazyDeleter.h"
#include "serverDatabase/MessageToManager.h"
#include "SwgDatabaseServer/CMLoader.h"
#include "serverMetrics/MetricsData.h"
#include "sharedLog/Log.h"
#include "SwgSnapshot.h"
#include "../queries/CommoditiesSchema.h"
#include "../tasks/TaskLoadObjvarNames.h"
#include "sharedFoundation/Os.h"

// ======================================================================

void SwgDatabaseServer::install()
{
    DatabaseProcess::installDerived(new SwgDatabaseServer);
}

// ----------------------------------------------------------------------

SwgDatabaseServer::SwgDatabaseServer() :
        DatabaseProcess()
{
}

// ----------------------------------------------------------------------

SwgDatabaseServer::~SwgDatabaseServer()
{
}

// ----------------------------------------------------------------------

void SwgDatabaseServer::run()
{
    SwgPersister::install();
    SwgLoader::install();
    DataLookup::install();
    LazyDeleter::install();
    ObjvarNameManager::install();
    MessageToManager::install();
    CMLoader::install();

    setupSwgMetrics();  // Game-specific metrics

    DataCleanupManager cleanupManager;
    cleanupManager.runDailyCleanup();

    DatabaseProcess::run();
}

// ----------------------------------------------------------------------
void SwgDatabaseServer::frameTick()
{
    tickSwgMetrics();
}

// ----------------------------------------------------------------------

/**
 * This function is needed to be able to use callbackWithExceptionHandling()
 */
void SwgDatabaseServer::runStatic()
{
    getInstance().run();
}

// ----------------------------------------------------------------------
void SwgDatabaseServer::setupSwgMetrics()
{
    MetricsData* p_met = MetricsData::getInstance();
    if (p_met)
    {
        // Metric initialization with scoped variables
        m_metricTableBufferBattlefieldMarkerObjectBufferCreated = p_met->addMetric("Buffers.Created.BattlefieldMarkerObjectBuffer");
        m_metricTableBufferBattlefieldMarkerObjectBufferActive = p_met->addMetric("Buffers.Active.BattlefieldMarkerObjectBuffer");

        m_metricTableBufferBuildingObjectBufferCreated = p_met->addMetric("Buffers.Created.BuildingObjectBuffer");
        m_metricTableBufferBuildingObjectBufferActive = p_met->addMetric("Buffers.Active.BuildingObjectBuffer");

        // Remaining buffer metrics setup
        m_metricTableBufferCellObjectBufferCreated = p_met->addMetric("Buffers.Created.CellObjectBuffer");
        m_metricTableBufferCellObjectBufferActive = p_met->addMetric("Buffers.Active.CellObjectBuffer");

        m_metricTableBufferCityObjectBufferCreated = p_met->addMetric("Buffers.Created.CityObjectBuffer");
        m_metricTableBufferCityObjectBufferActive = p_met->addMetric("Buffers.Active.CityObjectBuffer");

        m_metricTableBufferCreatureObjectBufferCreated = p_met->addMetric("Buffers.Created.CreatureObjectBuffer");
        m_metricTableBufferCreatureObjectBufferActive = p_met->addMetric("Buffers.Active.CreatureObjectBuffer");

        m_metricTableBufferFactoryObjectBufferCreated = p_met->addMetric("Buffers.Created.FactoryObjectBuffer");
        m_metricTableBufferFactoryObjectBufferActive = p_met->addMetric("Buffers.Active.FactoryObjectBuffer");

        m_metricTableBufferGuildObjectBufferCreated = p_met->addMetric("Buffers.Created.GuildObjectBuffer");
        m_metricTableBufferGuildObjectBufferActive = p_met->addMetric("Buffers.Active.GuildObjectBuffer");

        m_metricTableBufferHarvesterInstallationObjectBufferCreated = p_met->addMetric("Buffers.Created.HarvesterInstallationObjectBuffer");
        m_metricTableBufferHarvesterInstallationObjectBufferActive = p_met->addMetric("Buffers.Active.HarvesterInstallationObjectBuffer");

        m_metricTableBufferInstallationObjectBufferCreated = p_met->addMetric("Buffers.Created.InstallationObjectBuffer");
        m_metricTableBufferInstallationObjectBufferActive = p_met->addMetric("Buffers.Active.InstallationObjectBuffer");

        m_metricTableBufferIntangibleObjectBufferCreated = p_met->addMetric("Buffers.Created.IntangibleObjectBuffer");
        m_metricTableBufferIntangibleObjectBufferActive = p_met->addMetric("Buffers.Active.IntangibleObjectBuffer");

        m_metricTableBufferManufactureInstallationObjectBufferCreated = p_met->addMetric("Buffers.Created.ManufactureInstallationObjectBuffer");
        m_metricTableBufferManufactureInstallationObjectBufferActive = p_met->addMetric("Buffers.Active.ManufactureInstallationObjectBuffer");

        m_metricTableBufferManufactureSchematicObjectBufferCreated = p_met->addMetric("Buffers.Created.ManufactureSchematicObjectBuffer");
        m_metricTableBufferManufactureSchematicObjectBufferActive = p_met->addMetric("Buffers.Active.ManufactureSchematicObjectBuffer");

        m_metricTableBufferMissionObjectBufferCreated = p_met->addMetric("Buffers.Created.MissionObjectBuffer");
        m_metricTableBufferMissionObjectBufferActive = p_met->addMetric("Buffers.Active.MissionObjectBuffer");

        m_metricTableBufferObjectTableBufferCreated = p_met->addMetric("Buffers.Created.ObjectTableBuffer");
        m_metricTableBufferObjectTableBufferActive = p_met->addMetric("Buffers.Active.ObjectTableBuffer");

        m_metricTableBufferPlanetObjectBufferCreated = p_met->addMetric("Buffers.Created.PlanetObjectBuffer");
        m_metricTableBufferPlanetObjectBufferActive = p_met->addMetric("Buffers.Active.PlanetObjectBuffer");

        m_metricTableBufferPlayerObjectBufferCreated = p_met->addMetric("Buffers.Created.PlayerObjectBuffer");
        m_metricTableBufferPlayerObjectBufferActive = p_met->addMetric("Buffers.Active.PlayerObjectBuffer");

        m_metricTableBufferResourceContainerObjectBufferCreated = p_met->addMetric("Buffers.Created.ResourceContainerObjectBuffer");
        m_metricTableBufferResourceContainerObjectBufferActive = p_met->addMetric("Buffers.Active.ResourceContainerObjectBuffer");

        m_metricTableBufferShipObjectBufferCreated = p_met->addMetric("Buffers.Created.ShipObjectBuffer");
        m_metricTableBufferShipObjectBufferActive = p_met->addMetric("Buffers.Active.ShipObjectBuffer");

        m_metricTableBufferStaticObjectBufferCreated = p_met->addMetric("Buffers.Created.StaticObjectBuffer");
        m_metricTableBufferStaticObjectBufferActive = p_met->addMetric("Buffers.Active.StaticObjectBuffer");

        m_metricTableBufferTangibleObjectBufferCreated = p_met->addMetric("Buffers.Created.TangibleObjectBuffer");
        m_metricTableBufferTangibleObjectBufferActive = p_met->addMetric("Buffers.Active.TangibleObjectBuffer");

        m_metricTableBufferUniverseObjectBufferCreated = p_met->addMetric("Buffers.Created.UniverseObjectBuffer");
        m_metricTableBufferUniverseObjectBufferActive = p_met->addMetric("Buffers.Active.UniverseObjectBuffer");

        m_metricTableBufferVehicleObjectBufferCreated = p_met->addMetric("Buffers.Created.VehicleObjectBuffer");
        m_metricTableBufferVehicleObjectBufferActive = p_met->addMetric("Buffers.Active.VehicleObjectBuffer");

        m_metricTableBufferWeaponObjectBufferCreated = p_met->addMetric("Buffers.Created.WeaponObjectBuffer");
        m_metricTableBufferWeaponObjectBufferActive = p_met->addMetric("Buffers.Active.WeaponObjectBuffer");

        // Auction metrics
        m_metricAuctionLocationsRowCreated = p_met->addMetric("CBuffers.Created.AuctionLocationsRow");
        m_metricAuctionLocationsRowActive = p_met->addMetric("CBuffers.Active.AuctionLocationsRow");

        m_metricMarketAuctionsRowCreated = p_met->addMetric("CBuffers.Created.MarketAuctionsRow");
        m_metricMarketAuctionsRowActive = p_met->addMetric("CBuffers.Active.MarketAuctionsRow");

        m_metricMarketAuctionBidsRowCreated = p_met->addMetric("CBuffers.Created.MarketAuctionBidsRow");
        m_metricMarketAuctionBidsRowActive = p_met->addMetric("CBuffers.Active.MarketAuctionBidsRow");

        // Objvar metrics
        m_metricObjvarNameRowsActive = p_met->addMetric("ObjvarRowsActive");
        m_metricGoldObjvarNameRowsActive = p_met->addMetric("ObjvarGoldRowsActive");
    }
    else
    {
        LOG("SwgDatabaseServer::setupSwgMetrics", ("Metrics create FAILED!"));
    }
}

// ----------------------------------------------------------------------
void SwgDatabaseServer::tickSwgMetrics()
{
    static time_t lastTime = 0;
    time_t currentTime = time(0);

    if (currentTime == lastTime)
        return;

    lastTime = currentTime;

    MetricsData* p_met = MetricsData::getInstance();
    if (p_met == nullptr)
        return;

    // Update metrics with scoped variables for each metric
    p_met->updateMetric(m_metricTableBufferBattlefieldMarkerObjectBufferCreated, BattlefieldMarkerObjectBuffer::getRowsCreated());
    p_met->updateMetric(m_metricTableBufferBattlefieldMarkerObjectBufferActive, BattlefieldMarkerObjectBuffer::getRowsActive());

    p_met->updateMetric(m_metricTableBufferBuildingObjectBufferCreated, BuildingObjectBuffer::getRowsCreated());
    p_met->updateMetric(m_metricTableBufferBuildingObjectBufferActive, BuildingObjectBuffer::getRowsActive());

    // Remaining buffer updates
    p_met->updateMetric(m_metricTableBufferCellObjectBufferCreated, CellObjectBuffer::getRowsCreated());
    p_met->updateMetric(m_metricTableBufferCellObjectBufferActive, CellObjectBuffer::getRowsActive());

    p_met->updateMetric(m_metricTableBufferCityObjectBufferCreated, CityObjectBuffer::getRowsCreated());
    p_met->updateMetric(m_metricTableBufferCityObjectBufferActive, CityObjectBuffer::getRowsActive());

    p_met->updateMetric(m_metricTableBufferCreatureObjectBufferCreated, CreatureObjectBuffer::getRowsCreated());
    p_met->updateMetric(m_metricTableBufferCreatureObjectBufferActive, CreatureObjectBuffer::getRowsActive());

    p_met->updateMetric(m_metricTableBufferFactoryObjectBufferCreated, FactoryObjectBuffer::getRowsCreated());
    p_met->updateMetric(m_metricTableBufferFactoryObjectBufferActive, FactoryObjectBuffer::getRowsActive());

    // Auction updates
    p_met->updateMetric(m_metricAuctionLocationsRowCreated, DBSchema::AuctionLocationsRow::getRowsCreated());
    p_met->updateMetric(m_metricAuctionLocationsRowActive, DBSchema::AuctionLocationsRow::getRowsActive());

    p_met->updateMetric(m_metricMarketAuctionsRowCreated, DBSchema::MarketAuctionsRow::getRowsCreated());
    p_met->updateMetric(m_metricMarketAuctionsRowActive, DBSchema::MarketAuctionsRow::getRowsActive());

    p_met->updateMetric(m_metricMarketAuctionBidsRowCreated, DBSchema::MarketAuctionBidsRow::getRowsCreated());
    p_met->updateMetric(m_metricMarketAuctionBidsRowActive, DBSchema::MarketAuctionBidsRow::getRowsActive());

    // Objvars
    p_met->updateMetric(m_metricObjvarNameRowsActive, TaskLoadObjvarNames::getObjvarNamesCount());
    p_met->updateMetric(m_metricGoldObjvarNameRowsActive, TaskLoadObjvarNames::getGoldObjvarNamesCount());
}

// ======================================================================
