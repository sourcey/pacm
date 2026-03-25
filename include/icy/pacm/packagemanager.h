///
//
// Icey
// Copyright (c) 2005, Icey <https://icey.com>
//
// SPDX-License-Identifier: LGPL-2.1+
//
/// @addtogroup pacm
/// @{


#pragma once


#include "icy/collection.h"
#include "icy/json/json.h"
#include "icy/pacm/config.h"
#include "icy/pacm/installmonitor.h"
#include "icy/pacm/installtask.h"
#include "icy/pacm/package.h"
#include "icy/platform.h"
#include "icy/stateful.h"
#include "icy/filesystem.h"
#include "icy/task.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>


namespace icy {
namespace pacm {


/// Validates that a string is safe to use as a path component.
/// Rejects path traversal sequences (..), directory separators (/ and \),
/// null bytes, and empty strings.
/// @param name    The path component to validate.
/// @param context Caller description included in the exception message.
/// @throws std::invalid_argument if @p name fails any validation check.
inline void validatePathComponent(std::string_view name, std::string_view context)
{
    if (name.empty())
        throw std::invalid_argument(std::string(context) + ": path component must not be empty");
    if (name.find("..") != std::string_view::npos)
        throw std::invalid_argument(std::string(context) + ": path traversal detected in '" + std::string(name) + "'");
    if (name.find('/') != std::string_view::npos || name.find('\\') != std::string_view::npos)
        throw std::invalid_argument(std::string(context) + ": directory separator in '" + std::string(name) + "'");
    if (name.find('\0') != std::string_view::npos)
        throw std::invalid_argument(std::string(context) + ": null byte in path component");
}


/// Keyed store of installed packages indexed by package ID.
using LocalPackageStore = KeyedStore<std::string, LocalPackage>;

/// Keyed store of remote package metadata indexed by package ID.
using RemotePackageStore = KeyedStore<std::string, RemotePackage>;


/// Loads package manifests and coordinates install, update, and uninstall workflows.
class Pacm_API PackageManager
{
public:
    /// Startup configuration for repository endpoints, credentials, and directories.
    struct Options
    {
        std::string endpoint;       ///< The HTTP server endpoint
        std::string indexURI;       ///< The HTTP server URI for querying packages JSON
        std::string httpUsername;   ///< Username for HTTP basic auth
        std::string httpPassword;   ///< PAssword for HTTP basic auth
        std::string httpOAuthToken; ///< Will be used instead of HTTP basic if provided

        std::string tempDir;    ///< Directory where package files will be downloaded and extracted
        std::string dataDir;    ///< Directory where package manifests will be kept
        std::string installDir; ///< Directory where packages will be installed

        std::string platform;          ///< Platform (win32, linux, mac)
        std::string checksumAlgorithm; ///< Checksum algorithm (MDS/SHA1)

        bool clearFailedCache; ///< This flag tells the package manager weather or not
                               ///< to clear the package cache if installation fails.

        Options(const std::string& root = getCwd())
        {
            tempDir = fs::makePath(root, DEFAULT_PACKAGE_TEMP_DIR);
            dataDir = fs::makePath(root, DEFAULT_PACKAGE_DATA_DIR);
            installDir = fs::makePath(root, DEFAULT_PACKAGE_INSTALL_DIR);
            endpoint = DEFAULT_API_ENDPOINT;
            indexURI = DEFAULT_API_INDEX_URI;
            platform = DEFAULT_PLATFORM;
            checksumAlgorithm = DEFAULT_CHECKSUM_ALGORITHM;
            clearFailedCache = true;
        }
    };

public:
    /// @param options Configuration for directories, endpoints, and credentials.
    PackageManager(const Options& options = Options());
    virtual ~PackageManager() noexcept;

    PackageManager(const PackageManager&) = delete;
    PackageManager& operator=(const PackageManager&) = delete;
    PackageManager(PackageManager&&) = delete;
    PackageManager& operator=(PackageManager&&) = delete;

    //
    /// Initialization Methods

    /// Initializes the package manager: creates directories, loads local manifests,
    /// and queries the remote package index.
    virtual void initialize();

    /// Releases resources and cancels any in-progress tasks.
    virtual void uninitialize();

    /// Returns true if initialize() has been called successfully.
    virtual bool initialized() const;

    /// Creates the package manager directory structure
    /// if it does not already exist.
    virtual void createDirectories();

    /// Queries the server for a list of available packages.
    virtual void queryRemotePackages();

    /// Loads all local package manifests from file system.
    /// Clears all in memory package manifests.
    virtual void loadLocalPackages();

    /// Loads all local package manifests residing the the
    /// given directory. This method may be called multiple
    /// times for different paths because it does not clear
    /// in memory package manifests.
    virtual void loadLocalPackages(const std::string& dir);

    /// Saves all local package manifests to the data directory.
    /// @param whiny If true, re-throws on write error; otherwise returns false.
    /// @return true on success.
    virtual bool saveLocalPackages(bool whiny = false);

    /// Saves the local package manifest to the file system.
    virtual bool saveLocalPackage(LocalPackage& package, bool whiny = false);

    /// Parse the remote packages from the given JSON data string.
    virtual void parseRemotePackages(const std::string& data);

    //
    /// Package Installation Methods

    /// Installs a single package.
    /// The returned InstallTask must be started.
    /// If the package is already up-to-date, a nullptr will be returned.
    /// Any other error will throw a std::runtime_error.
    virtual InstallTask::Ptr
    installPackage(const std::string& name,
                   const InstallOptions& options = InstallOptions());

    /// Installs multiple packages.
    /// The same options will be passed to each task.
    /// If a InstallMonitor instance was passed in the tasks will need to
    /// be started, otherwise they will be auto-started.
    /// The PackageManager does not take ownership of the InstallMonitor.
    virtual bool
    installPackages(const StringVec& ids,
                    const InstallOptions& options = InstallOptions(),
                    InstallMonitor* monitor = nullptr, bool whiny = false);

    /// Updates a single package.
    /// Throws an exception if the package does not exist.
    /// The returned InstallTask must be started.
    virtual InstallTask::Ptr
    updatePackage(const std::string& name,
                  const InstallOptions& options = InstallOptions());

    /// Updates multiple packages.
    /// Throws an exception if the package does not exist.
    /// If a InstallMonitor instance was passed in the tasks will need to
    /// be started, otherwise they will be auto-started.
    /// The PackageManager does not take ownership of the InstallMonitor.
    virtual bool
    updatePackages(const StringVec& ids,
                   const InstallOptions& options = InstallOptions(),
                   InstallMonitor* monitor = nullptr, bool whiny = false);

    /// Updates all installed packages.
    virtual bool updateAllPackages(bool whiny = false);

    /// Uninstalls multiple packages.
    virtual bool uninstallPackages(const StringVec& ids, bool whiny = false);

    /// Uninstalls a single package.
    virtual bool uninstallPackage(const std::string& id, bool whiny = false);

    /// Returns true if there are updates available that have
    /// not yet been finalized. Packages may be unfinalized if
    /// there were files in use at the time of installation.
    virtual bool hasUnfinalizedPackages();

    /// Finalizes active installations by moving all package
    /// files to their target destination. If files are to be
    /// overwritten they must not be in use or finalization
    /// will fail.
    virtual bool finalizeInstallations(bool whiny = false);

    //
    /// Task Helper Methods

    /// Gets the install task for the given package ID.
    virtual InstallTask::Ptr getInstallTask(const std::string& id) const;

    /// Returns a list of all tasks.
    virtual InstallTaskPtrVec tasks() const;

    /// Aborts all package installation tasks. All tasks must
    /// be aborted before clearing local or remote manifests.
    virtual void cancelAllTasks();

    //
    /// Package Helper Methods

    /// Returns all package pairs, valid or invalid.
    /// Some pairs may not have both local and remote package pointers.
    virtual PackagePairVec getPackagePairs() const;

    /// Returns a list of package pairs which may be updated.
    /// All pairs will have both local and remote package pointers,
    /// and the remote version will be newer than the local version.
    virtual PackagePairVec getUpdatablePackagePairs() const;

    /// Returns a local and remote package pair.
    /// An exception will be thrown if either the local or
    /// remote packages aren't available or are invalid.
    virtual PackagePair getPackagePair(const std::string& id,
                                       bool whiny = false) const;

    /// Returns a local and remote package pair.
    /// If the local package doesn't exist it will be created
    /// from the remote package.
    /// If the remote package doesn't exist a NotFoundException
    /// will be thrown.
    virtual PackagePair getOrCreatePackagePair(const std::string& id);

    /// Creates a package installation task for the given pair.
    virtual InstallTask::Ptr createInstallTask(PackagePair& pair,
                                               const InstallOptions& options = InstallOptions());

    /// Returns the version number of an installed package.
    /// Exceptions will be thrown if the package does not exist,
    /// or is not fully installed.
    virtual std::string installedPackageVersion(const std::string& id) const;

    /// Returns the best asset to install, or throws a descriptive exception
    /// if no updates are available, or if the package is already up-to-date.
    /// This method takes version and SDK locks into consideration.
    virtual Package::Asset getLatestInstallableAsset(
        const PackagePair& pair,
        const InstallOptions& options = InstallOptions()) const;

    /// Returns true if there are updates available for this package, false
    /// otherwise.
    virtual bool hasAvailableUpdates(const PackagePair& pair) const;

    //
    /// File Helper Methods

    /// Clears all files in the cache directory.
    void clearCache();

    /// Clears a package archive from the local cache.
    bool clearPackageCache(LocalPackage& package);

    /// Clears a file from the local cache.
    bool clearCacheFile(std::string_view fileName, bool whiny = false);

    /// Checks if a package archive exists in the local cache.
    bool hasCachedFile(Package::Asset& asset);

    /// Checks if the file type is a supported package archive.
    bool isSupportedFileType(std::string_view fileName);

    /// Returns the full path of the cached file if it exists,
    /// or an empty path if the file doesn't exist.
    std::string getCacheFilePath(std::string_view fileName);

    /// Returns the package data directory for the
    /// given package ID.
    std::string getPackageDataDir(std::string_view id);

    //
    /// Accessors

    /// Returns a reference to the current options.
    virtual Options& options();

    /// Returns a reference to the in-memory remote package store.
    virtual RemotePackageStore& remotePackages();

    /// Returns a reference to the in-memory local package store.
    virtual LocalPackageStore& localPackages();

    //
    /// Events

    /// Signals when the remote package list have been
    /// downloaded from the server.
    Signal<void(const http::Response&)> RemotePackageResponse;

    /// Signals when a package is uninstalled.
    Signal<void(LocalPackage&)> PackageUninstalled;

    /// Signals when an installation task is created,
    /// before it is started.
    Signal<void(InstallTask&)> InstallTaskCreated;

    /// Signals when a package installation tasks completes,
    /// either successfully or in error.
    Signal<void(const InstallTask&)> InstallTaskComplete;

protected:
    //
    /// Callbacks

    void onPackageInstallComplete(InstallTask& task);

protected:
    mutable std::mutex _mutex;
    LocalPackageStore _localPackages;
    RemotePackageStore _remotePackages;
    InstallTaskPtrVec _tasks;
    Options _options;
};


} // namespace pacm
} // namespace icy


/// @}
