//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// system_utils.h: declaration of OS-specific utility functions

#ifndef COMMON_SYSTEM_UTILS_H_
#define COMMON_SYSTEM_UTILS_H_

#include "platform.h"
#include <string>

namespace angle
{

class Library
{
public:
    Library() = default;
    virtual ~Library() {}
    virtual void *getSymbol(const char *symbolName) = 0;
    virtual void *getNative() const                 = 0;

    template <typename FuncT>
    void getAs(const char *symbolName, FuncT *funcOut)
    {
        *funcOut = reinterpret_cast<FuncT>(getSymbol(symbolName));
    }
private:
    Library(const Library &) = delete;
    void operator=(const Library &) = delete;
};

// Use SYSTEM_DIR to bypass loading ANGLE libraries with the same name as system DLLS
// (e.g. opengl32.dll)
enum class SearchType
{
    ApplicationDir,
    SystemDir
};

Library *OpenSharedLibrary(const char *libraryName, SearchType searchType);

void CloseSharedLibrary(Library *library);

}  // namespace angle

#endif  // COMMON_SYSTEM_UTILS_H_
