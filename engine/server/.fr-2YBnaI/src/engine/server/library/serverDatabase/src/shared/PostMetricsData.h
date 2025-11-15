#ifndef POSTMETRICSDATA_H
#define POSTMETRICSDATA_H

#include "serverDatabase/Query.h" // Base class for database queries
#include "CentralServerMetricsData.h" // Include the metrics data class

// ======================================================================

class PostMetricsData : public DB::Query
{
public:
    // Constructor that accepts metrics data
    PostMetricsData(const CentralServerMetricsData &metricsData);

    // Override getSQL to construct the SQL command
    virtual void getSQL(std::string &sql) override;

    // Override bindParameters to bind the metrics data
    virtual bool bindParameters() override;

    // Override bindColumns; no columns to bind for insert
    virtual bool bindColumns() override;

    // Override to set the execution mode
    virtual DB::Query::QueryMode getExecutionMode() const override;

    // Execute the query
    bool execute();

private:
    CentralServerMetricsData m_metricsData; // Metrics data to be posted
};

// ======================================================================

#endif // POSTMETRICSDATA_H
