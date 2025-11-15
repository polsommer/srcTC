// MultiFractalReaderWriter.cpp
// asommers
//
// copyright 2001, sony online entertainment
//

#include "sharedFractal/FirstSharedFractal.h"
#include "sharedFractal/MultiFractalReaderWriter.h"

#include "sharedFile/Iff.h"
#include "sharedFractal/MultiFractal.h"

// Helper function to write MultiFractal data into Iff
void MultiFractalReaderWriter::writeMultiFractalData(Iff& iff, const MultiFractal& multiFractal)
{
    iff.insertChunkData(multiFractal.getSeed());
    iff.insertChunkData(multiFractal.getUseBias() ? static_cast<int32>(1) : static_cast<int32>(0));
    iff.insertChunkData(multiFractal.getBias());
    iff.insertChunkData(multiFractal.getUseGain() ? static_cast<int32>(1) : static_cast<int32>(0));
    iff.insertChunkData(multiFractal.getGain());
    iff.insertChunkData(multiFractal.getNumberOfOctaves());
    iff.insertChunkData(multiFractal.getFrequency());
    iff.insertChunkData(multiFractal.getAmplitude());
    iff.insertChunkData(multiFractal.getScaleX());
    iff.insertChunkData(multiFractal.getScaleY());
    iff.insertChunkData(multiFractal.getOffsetX());
    iff.insertChunkData(multiFractal.getOffsetY());
    iff.insertChunkData(static_cast<int32>(multiFractal.getCombinationRule()));
}

// Save MultiFractal to Iff
void MultiFractalReaderWriter::save(Iff& iff, const MultiFractal& multiFractal)
{
    iff.insertForm(TAG(M,F,R,C));

    iff.insertForm(TAG_0001);

    iff.insertChunk(TAG_DATA);
    writeMultiFractalData(iff, multiFractal);
    iff.exitChunk();

    iff.exitForm();

    iff.exitForm();
}

// Helper function to read MultiFractal data from Iff
void MultiFractalReaderWriter::readMultiFractalData(Iff& iff, MultiFractal& multiFractal)
{
    multiFractal.setSeed(iff.read_uint32());

    bool useBias = iff.read_int32() != 0;
    real bias = iff.read_float();
    multiFractal.setBias(useBias, bias);

    bool useGain = iff.read_int32() != 0;
    real gain = iff.read_float();
    multiFractal.setGain(useGain, gain);

    multiFractal.setNumberOfOctaves(iff.read_int32());
    multiFractal.setFrequency(iff.read_float());
    multiFractal.setAmplitude(iff.read_float());

    real scaleX = iff.read_float();
    real scaleY = iff.read_float();
    multiFractal.setScale(scaleX, scaleY);

    real offsetX = iff.read_float();
    real offsetY = iff.read_float();
    multiFractal.setOffset(offsetX, offsetY);

    multiFractal.setCombinationRule(static_cast<MultiFractal::CombinationRule>(iff.read_int32()));
}

// Load MultiFractal from Iff
void MultiFractalReaderWriter::load(Iff& iff, MultiFractal& multiFractal)
{
    iff.enterForm(TAG(M,F,R,C));

    switch (iff.getCurrentName())
    {
    case TAG_0000:
    case TAG_0001:
        loadData(iff, multiFractal);
        break;

    default:
    {
        char tagBuffer[5];
        ConvertTagToString(iff.getCurrentName(), tagBuffer);

        char buffer[128];
        iff.formatLocation(buffer, sizeof(buffer));

        std::string errorMessage = "Unknown affector type at " + std::string(buffer) + "/" + std::string(tagBuffer);
        throw std::runtime_error(errorMessage);
    }
    }

    iff.exitForm();
}

// Unified function to load data for both versions
void MultiFractalReaderWriter::loadData(Iff& iff, MultiFractal& multiFractal)
{
    iff.enterForm(iff.getCurrentName());

    iff.enterChunk(TAG_DATA);
    readMultiFractalData(iff, multiFractal);
    iff.exitChunk();

    iff.exitForm();
}


