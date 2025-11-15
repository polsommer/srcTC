// MultiFractalReaderWriter.h
// asommers
//
// copyright 2001, sony online entertainment
//

#ifndef INCLUDED_MultiFractalReaderWriter_H
#define INCLUDED_MultiFractalReaderWriter_H

//-------------------------------------------------------------------

class Iff;
class MultiFractal;

//-------------------------------------------------------------------

class MultiFractalReaderWriter
{
public:
    // Save the MultiFractal data into an Iff object
    static void save(Iff& iff, const MultiFractal& multiFractal);
    
    // Load the MultiFractal data from an Iff object
    static void load(Iff& iff, MultiFractal& multiFractal);

private:
    // Helper functions for reading/writing MultiFractal data
    static void writeMultiFractalData(Iff& iff, const MultiFractal& multiFractal);
    static void readMultiFractalData(Iff& iff, MultiFractal& multiFractal);

    // Unified load function for both versions
    static void loadData(Iff& iff, MultiFractal& multiFractal);
};

//-------------------------------------------------------------------

#endif
