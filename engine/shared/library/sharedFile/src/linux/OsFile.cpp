// ======================================================================
//
// OsFile.cpp
// Copyright 2024, Sony Online Entertainment Inc.
// All Rights Reserved.
//
// ======================================================================

#include "sharedFile/FirstSharedFile.h"
#include "sharedFile/OsFile.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdexcept>
#include <memory>
#include <cerrno>
#include <cstring>
#include <iostream>

// ======================================================================

// Define a constant for invalid file handles
constexpr int INVALID_FILE_HANDLE = -1;

// Remove DuplicateString definition if already defined in included headers
// If it is used elsewhere and needs to be included, it should be done conditionally
/*
inline char* DuplicateString(const char* str) {
    if (!str) {
        return nullptr;
    }
    size_t length = std::strlen(str) + 1;
    char* duplicate = new char[length];
    std::memcpy(duplicate, str, length);
    return duplicate;
}
*/

// ----------------------------------------------------------------------

void OsFile::install() {
    // Installation code (if any) goes here
}

// ----------------------------------------------------------------------

bool OsFile::exists(const char* fileName) { // Matching header declaration
    struct stat statBuffer;
    if (stat(fileName, &statBuffer) != 0) {
        return false;
    }

    // Check if the file is a directory
    if (S_ISDIR(statBuffer.st_mode)) {
        return false;
    }

    return true;
}

// ----------------------------------------------------------------------

int OsFile::getFileSize(const char* fileName) { // Matching header declaration
    struct stat statBuffer;
    if (stat(fileName, &statBuffer) == 0) {
        return static_cast<int>(statBuffer.st_size);
    }

    // Return -1 to indicate that the file size could not be determined
    return -1;
}

// ----------------------------------------------------------------------

OsFile* OsFile::open(const char* fileName, bool randomAccess) { // Matching header declaration
    // Unused parameter warning can be suppressed using (void)randomAccess;
    (void)randomAccess;

    if (!exists(fileName)) {
        return nullptr;
    }

    // Attempt to open the file
    int handle = ::open(fileName, O_RDONLY);
    if (handle == INVALID_FILE_HANDLE) {
        std::cerr << "OsFile::open failed to open file " << fileName
                  << ", errno=" << errno << ": " << std::strerror(errno) << std::endl;
        return nullptr;
    }

    // Use raw pointer since the header signature uses raw pointer
    return new OsFile(handle, DuplicateString(fileName));
}

// ----------------------------------------------------------------------

OsFile::OsFile(int handle, char* fileName)
    : m_handle(handle),
      m_length(lseek(m_handle, 0, SEEK_END)), // Get file length
      m_offset(0),
      m_fileName(fileName) {
    if (m_length < 0) {
        throw std::runtime_error("Failed to determine file length");
    }

    // Reset file offset to the beginning
    if (lseek(m_handle, 0, SEEK_SET) < 0) {
        throw std::runtime_error("Failed to reset file offset");
    }
}

// ----------------------------------------------------------------------

OsFile::~OsFile() {
    if (m_handle != INVALID_FILE_HANDLE) {
        ::close(m_handle);
    }
    delete[] m_fileName;
}

// ----------------------------------------------------------------------

int OsFile::length() const {
    return m_length;
}

// ----------------------------------------------------------------------

void OsFile::seek(int newFilePosition) {
    if (m_offset != newFilePosition) {
        int result = lseek(m_handle, newFilePosition, SEEK_SET);
        if (result != newFilePosition) {
            throw std::runtime_error("SetFilePointer failed");
        }
        m_offset = newFilePosition;
    }
}

// ----------------------------------------------------------------------

int OsFile::read(void* destinationBuffer, int numberOfBytes) {
    int result = 0;
    while (true) {
        result = ::read(m_handle, destinationBuffer, numberOfBytes);
        if (result >= 0) {
            break;
        }
        if (errno != EAGAIN) {
            std::cerr << "Read failed for " << m_fileName << ": "
                      << errno << " " << std::strerror(errno) << std::endl;
            break;
        }
    }
    if (result >= 0) {
        m_offset += result;
    }
    return result;
}

// ======================================================================

