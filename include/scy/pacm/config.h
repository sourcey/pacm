///
//
// LibSourcey
// Copyright (c) 2005, Sourcey <http://sourcey.com>
//
// SPDX-License-Identifier:	LGPL-2.1+
//
/// @addtogroup pacm
/// @{


#ifndef SCY_Pacm_Config_H
#define SCY_Pacm_Config_H


namespace scy {
namespace pacm {


#define DEFAULT_API_ENDPOINT "http://localhost:3000"
#define DEFAULT_API_INDEX_URI "/packages.json"
#define DEFAULT_PACKAGE_INSTALL_DIR "pacm/install"
#define DEFAULT_PACKAGE_DATA_DIR "pacm/data"
#define DEFAULT_PACKAGE_TEMP_DIR "pacm/tmp"
#define DEFAULT_CHECKSUM_ALGORITHM "MD5"

#ifdef _WIN32
#define DEFAULT_PLATFORM "win32"
#elif __APPLE__
#define DEFAULT_PLATFORM "mac"
#elif __linux__
#define DEFAULT_PLATFORM "linux"
#else
#error "Unknown platform"
#endif


} // namespace pacm
} // namespace scy


#endif // SCY_Pacm_Config_H


/// @\}
