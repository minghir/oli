/*
SPDX-License-Identifier: BSD-3-Clause
SPDX-FileCopyrightText: Copyright (c) 2018, Kenneth Troldal Balslev

   ____                               ____      ___ ____       ____  ____      ___
  6MMMMb                              `MM(      )M' `MM'      6MMMMb\`MM(      )M'
 8P    Y8                              `MM.     d'   MM      6M'    ` `MM.     d'
6M      Mb __ ____     ____  ___  __    `MM.   d'    MM      MM        `MM.   d'
MM      MM `M6MMMMb   6MMMMb `MM 6MMb    `MM. d'     MM      YM.        `MM. d'
MM      MM  MM'  `Mb 6M'  `Mb MMM9 `Mb    `MMd       MM       YMMMMb     `MMd
MM      MM  MM    MM MM    MM MM'   MM     dMM.      MM           `Mb     dMM.
MM      MM  MM    MM MMMMMMMM MM    MM    d'`MM.     MM            MM    d'`MM.
YM      M9  MM    MM MM       MM    MM   d'  `MM.    MM            MM   d'  `MM.
 8b    d8   MM.  ,M9 YM    d9 MM    MM  d'    `MM.   MM    / L    ,M9  d'    `MM.
  YMMMM9    MMYMMM9   YMMMM9 _MM_  _MM_M(_    _)MM_ _MMMMMMM MYMMMM9 _M(_    _)MM_
            MM
            MM
           _MM_

  Copyright (c) 2018, Kenneth Troldal Balslev

  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
  - Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  - Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  - Neither the name of the author nor the
    names of any contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 */

#ifndef OPENXLSX_TOOLS_H
#define OPENXLSX_TOOLS_H

#ifdef _WIN32
    #include <algorithm>            // std::replace
#endif
#ifdef ENABLE_NOWIDE
    #include <nowide/cstdio.hpp>    // nowide::fopen, nowide::remove, nowide::rename
    #include <nowide/stat.hpp>
    namespace boost {}              // ensure that namespace exists, even if boost doesn't define it
    using namespace boost;          // depending on library version, nowide namespace is hidden in boost::nowide
#else
    #include <cstdio>       // std::fopen
    #include <filesystem>   // std::filesystem::remove, std::filesystem::rename
    #include <sys/stat.h>   // for stat, to test if a file exists and whether a file is a directory
#endif
#include <random>       // std::random_device, std::mt19937, std::uniform_int_distribution
#include <string>       // std::string
#include <sys/types.h>  // off_t

// stat shenanigans configured *below* includes to make it absolutely clear that this is module-local
#ifdef ENABLE_NOWIDE
    #define STAT nowide::stat                       // use nowide version of stat, same name for win and non-win
    #define STATSTRUCT nowide::posix_stat_t         // platform-specific struct stat and struct _stat get mapped to nowide::posix_stat_t (without the struct keyword)
#else
    // don't use "stat" directly because windows has compatibility-breaking defines
    #if defined(_WIN32)
        // NOTE: this branch should never be triggered, because on _WIN32, ENABLE_NOWIDE should be always true
        #define STAT _stat                  // _stat should be available in standard environment on Windows
        #define STATSTRUCT struct _stat     // struct _stat also exists - split the two names in case the struct _stat must not be used on windows
    #else
        #define STAT stat
        #define STATSTRUCT struct stat
    #endif
#endif


namespace OpenXLSX
{
    /* 2025-05-04: moved a set of file system related utility functions to this new header file so that Unicode support on windows (nowide)
     * can be implemented in a central location */

    /**
     * @brief Test if path exists as either a file or a directory
     * @param path Check for existence of this
     * @return true if path exists as a file or directory
     */
    inline bool pathExists(const std::string& path)
    {
        STATSTRUCT info;
        if (STAT(path.c_str(), &info ) == 0)    // test if path exists
            return true;
        return false;
    }

#ifdef __GNUC__    // conditionally enable GCC specific pragmas to suppress unused function warning
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wunused-function"
#endif // __GNUC__
    /**
     * @brief Test if fileName exists and is not a directory
     * @param fileName The path to check for existence (as a file)
     * @return true if fileName exists and is a file, otherwise false
     */
    inline bool fileExists(const std::string& fileName)
    {
        STATSTRUCT info;
        if (STAT(fileName.c_str(), &info ) == 0)   // test if path exists
            if ((info.st_mode & S_IFDIR) == 0)        // test if it is NOT a directory
                return true;
        return false;
    }

    /**
     * @brief Test if fileName exists and is a directory
     * @param dirName The path to check for existence (as a directory)
     * @return true if fileName exists and is a directory, otherwise false
     */
    inline bool isDirectory(const std::string& dirName)
    {
        STATSTRUCT info;
        if (STAT(dirName.c_str(), &info ) == 0)    // test if path exists
            if ((info.st_mode & S_IFDIR) != 0)        // test if it is a directory
                return true;
        return false;
    }
#ifdef __GNUC__    // conditionally enable GCC specific pragmas to suppress unused function warning
#   pragma GCC diagnostic pop
#endif // __GNUC__

    /**
     * @brief determine the size of a file if it exists
     * @param fileName file to obtain info about
     * @return size of the file in Bytes (as returned by struct stat field st_size, or -1 if file info could not be obtained
     */
    inline off_t fileSize(const std::string& fileName)
    {
        STATSTRUCT info;
        if (STAT(fileName.c_str(), &info ) == 0)    // if file info could be obtained
            return info.st_size;                       // then return the file size
        return -1;  // default: return -1 to indicate failure
    }

    /**
     * @brief opens a file with support for unicode filenames on Windows
     * @param filename (unicode) name of file to open
     * @param mode how to open the file - passed through to fopen
     * @return FILE * to the open file on success
     * @return nullptr on failure
     */
    inline FILE* fopen( std::string filename, std::string mode )
    {
#       ifdef ENABLE_NOWIDE
            return nowide::fopen(filename.c_str(), mode.c_str());
#       else
            return std::fopen(filename.c_str(), mode.c_str());
#       endif
    }

    /**
     * @brief Generates a random filename, which is used to generate a temporary archive when modifying and saving
     * archive files.
     * @param length The length of the filename to create.
     * @return Returns the generated filename, appended with '.tmp'.
     */
    inline std::string GenerateRandomName(int length)
    {
        std::string letters = "abcdefghijklmnopqrstuvwxyz0123456789";

        std::random_device                 rand_dev;
        std::mt19937                       generator(rand_dev());
        std::uniform_int_distribution<int> distr(0, letters.size() - 1);

        std::string result;
        for (int i = 0; i < length; ++i) {
            result += letters[distr(generator)];
        }

        return result + ".tmp";
    }

    /**
     * @brief Generates a random filename with a call to GenerateRandomName, prepending the same path in which filename is located
     * @param filename determine "same path" from this file
     * @param length The length of the filename to create.
     * @return Returns the generated random filename in the same path as filename, appended with '.tmp'.
     * @note this function accounts for amigaos and windows specific drive / directory separators
     */
    inline std::string GenerateRandomNameInSamePath(std::string filename, int length)
    {
        if (filename.empty()) filename = "./"; // local folder

#       ifdef _WIN32
            std::replace( filename.begin(), filename.end(), '\\', '/' ); // pull request #210, alternate fix: fopen etc work fine with forward slashes
#       endif

        // ===== Determine path of the current file
        size_t pathPos = filename.rfind('/');

        // pull request #191, support AmigaOS style paths
#       ifdef __amigaos__
            constexpr const char * localFolder = "";    // local folder on AmigaOS can not be explicitly expressed in a path
            if (pathPos == std::string::npos) pathPos = filename.rfind(':'); // if no '/' found, attempt to find amiga drive root path
#       else
            constexpr const char * localFolder = "./"; // local folder on _WIN32 && __linux__ is .
#       endif
        std::string tempPath{};
        if (pathPos != std::string::npos) tempPath = filename.substr(0, pathPos + 1);
        else tempPath = localFolder; // prepend explicit identification of local folder in case path did not contain a folder

        // ===== Generate a random file name with the same path as the current file
        return tempPath + GenerateRandomName(length);
    }

    /**
     * @warning DRAFT VERSION, TO BE TESTED!
     * @brief Portable remove function to delete a file with unicode characters in the filename even on Windows
     * @param p (unicode) std::string name of file to delete
     * @return true upon success, false upon failure
     */
    inline bool remove(std::string const &p )
    {
#       ifdef ENABLE_NOWIDE
            return ( nowide::remove(p.c_str()) == 0 );    // nowide::remove / std::remove return an int, and 0 means success
#       else
            return std::filesystem::remove(p.c_str());
#       endif
    }

    /**
     * @warning DRAFT VERSION, TO BE TESTED!
     * @brief Portable rename function to save a filename with unicode characters even on Windows
     * @param old_p (unicode) std::string source filename
     * @param new_p (unicode) std::string destination filename
     * @return N/A
     */
    inline void rename(std::string const &old_p, std::string const &new_p )
    {
#       ifdef ENABLE_NOWIDE
            nowide::rename(old_p.c_str(), new_p.c_str());
#       else
            std::filesystem::rename(old_p.c_str(), new_p.c_str());
#       endif
    }
}    // namespace OpenXLSX

#endif // OPENXLSX_TOOLS_H
