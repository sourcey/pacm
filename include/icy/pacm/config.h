///
//
// icey
// Copyright (c) 2005, icey <https://0state.com>
//
// SPDX-License-Identifier: LGPL-2.1+
//
/// @defgroup pacm Pacm module
///
/// Package manager for distributing and installing packaged extensions and assets.
/// @{


#pragma once

#include "icy/base.h"


namespace icy {
/// Package manifests, install tasks, and repository management helpers.
namespace pacm {


#define DEFAULT_API_ENDPOINT "https://localhost:3000"
#define DEFAULT_API_INDEX_URI "/packages.json"
#define DEFAULT_PACKAGE_INSTALL_DIR "pacm/install"
#define DEFAULT_PACKAGE_DATA_DIR "pacm/data"
#define DEFAULT_PACKAGE_TEMP_DIR "pacm/tmp"
#define DEFAULT_CHECKSUM_ALGORITHM "SHA256"

#ifdef _WIN32
#define DEFAULT_PLATFORM "win32"
#elif __APPLE__
#define DEFAULT_PLATFORM "mac"
#elif __linux__
#define DEFAULT_PLATFORM "linux"
#else
#error "Unknown platform"
#endif

// Shared library exports
#if defined(Pacm_EXPORTS)
#define Pacm_API ICY_EXPORT
#else
#define Pacm_API ICY_IMPORT
#endif


} // namespace pacm
} // namespace icy


/// @}
