///
//
// Icey
// Copyright (c) 2005, Icey <https://0state.com>
//
// SPDX-License-Identifier: LGPL-2.1+
//
/// @addtogroup pacm
/// @{


#include "icy/pacm/packagemanager.h"
#include "icy/filesystem.h"
#include "icy/http/authenticator.h"
#include "icy/http/client.h"
#include "icy/json/json.h"
#include "icy/packetio.h"
#include "icy/pacm/package.h"
#include "icy/util.h"

#include <memory>


using namespace std;


namespace icy {
namespace pacm {


PackageManager::PackageManager(const Options& options)
    : _options(options)
{
}


PackageManager::~PackageManager()
{
}


void PackageManager::initialize()
{
    createDirectories();
    loadLocalPackages();
}


bool PackageManager::initialized() const
{
    std::lock_guard<std::mutex> guard(_mutex);
    return !_remotePackages.empty() || !_localPackages.empty();
}


void PackageManager::uninitialize()
{
    cancelAllTasks();

    std::lock_guard<std::mutex> guard(_mutex);
    _remotePackages.clear();
    _localPackages.clear();
}


void PackageManager::cancelAllTasks()
{
    std::lock_guard<std::mutex> guard(_mutex);
    auto it = _tasks.begin();
    while (it != _tasks.end()) {
        (*it)->cancel();
        it = _tasks.erase(it);
    }
}


void PackageManager::createDirectories()
{
    std::lock_guard<std::mutex> guard(_mutex);
    fs::mkdirr(_options.tempDir);
    fs::mkdirr(_options.dataDir);
    fs::mkdirr(_options.installDir);
}


void PackageManager::queryRemotePackages()
{
    std::lock_guard<std::mutex> guard(_mutex);
    SDebug << "Querying server: " << _options.endpoint << _options.indexURI << endl;

    if (!_tasks.empty())
        throw std::runtime_error("Cannot load packages while tasks are active.");

    try {
        auto conn = http::Client::instance().createConnection(_options.endpoint + _options.indexURI);
        conn->request().setMethod("GET");
        conn->request().setKeepAlive(false);
        conn->setReadStream(new std::stringstream);

        // OAuth authentication
        if (!_options.httpOAuthToken.empty()) {
            conn->request().add("Authorization",
                                "Bearer " + _options.httpOAuthToken);
        }

        // HTTP Basic authentication
        else if (!_options.httpUsername.empty()) {
            http::BasicAuthenticator cred(_options.httpUsername,
                                          _options.httpPassword);
            cred.authenticate(conn->request());
        }

        conn->Complete += [&](const http::Response& response) {
            STrace << "On package response complete: " << response << endl;

            parseRemotePackages(conn->readStream<std::stringstream>().str());
            RemotePackageResponse.emit(response);
            conn->close();
        };

        conn->submit();
    } catch (std::exception& exc) {
        SError << "Package Query Error: " << exc.what() << endl;
        throw exc;
    }
}


void PackageManager::parseRemotePackages(const std::string& data)
{
    try {
        json::Value root = json::Value::parse(data.begin(), data.end());
        _remotePackages.clear();

        for (const auto& elem : root) {
            auto package = std::make_unique<RemotePackage>(elem);
            if (!package->valid()) {
                SError << "Invalid package: " << package->id() << endl;
                continue;
            }
            auto id = package->id();
            _remotePackages.tryAdd(id, std::move(package));
        }
    } catch (std::invalid_argument& exc) {
        SError << "Invalid server JSON response: " << exc.what() << endl;
        throw exc;
    }
}


void PackageManager::loadLocalPackages()
{
    std::string dir;
    {
        std::lock_guard<std::mutex> guard(_mutex);

        if (!_tasks.empty())
            throw std::runtime_error(
                "Cannot load packages while there are active tasks.");

        _localPackages.clear();
        dir = _options.dataDir;
    }
    loadLocalPackages(dir);
}


void PackageManager::loadLocalPackages(const std::string& dir)
{
    SDebug << "Loading manifests: " << dir << endl;

    std::vector<std::string> dirEntries;
    fs::readdir(dir, dirEntries);
    for (const auto& path : dirEntries) {
        std::string filename = fs::filename(path);
        if (filename.find(".json") != std::string::npos) {
            try {
                json::Value root;
                json::loadFile(path, root);

                SDebug << "Loading package manifest: " << path << endl;
                auto package = std::make_unique<LocalPackage>(root);
                if (!package->valid()) {
                    throw std::runtime_error("The local package is invalid.");
                }

                SDebug << "local package added: " << package->name() << endl;
                auto id = package->id();
                localPackages().tryAdd(id, std::move(package));
            } catch (std::exception& exc) {
                SError << "Cannot load local package: " << exc.what() << endl;
            }
        }
    }
}


bool PackageManager::saveLocalPackages(bool whiny)
{
    STrace << "Saving local packages" << endl;

    bool res = true;
    auto& toSave = localPackages();
    for (auto& [key, pkg] : toSave) {
        if (!saveLocalPackage(static_cast<LocalPackage&>(*pkg), whiny))
            res = false;
    }
    return res;
}


bool PackageManager::saveLocalPackage(LocalPackage& package, bool whiny)
{
    bool res = false;
    try {
        validatePathComponent(package.id(), "saveLocalPackage");
        std::string path(util::format("%s/%s.json", options().dataDir.c_str(),
                                      package.id().c_str()));
        SDebug << "Saving local package: " << package.id() << endl;
        json::saveFile(path, package);
        res = true;
    } catch (std::exception& exc) {
        SError << "Save error: " << exc.what() << endl;
        if (whiny)
            throw exc;
    }
    return res;
}


//
//    Package installation methods
//

InstallTask::Ptr
PackageManager::installPackage(const std::string& name,
                               const InstallOptions& options) //, bool whiny
{
    SDebug << "Install package: " << name << endl;

    // Try to update our remote package list if none exist.
    // TODO: Consider storing a remote package cache file.
    // if (remotePackages().empty())
    // queryRemotePackages();

    // Get the asset to install or throw
    PackagePair pair = getOrCreatePackagePair(name);

    // Get the asset to install or return a nullptr
    InstallOptions opts(options);
    try {
        Package::Asset asset = getLatestInstallableAsset(pair, options);

        // Set the version option as the version to install.
        // KLUDGE: Instead of modifying options, we should have
        // a better way of sending the asset/version to the InstallTask.
        opts.version = asset.version();

        SDebug << "Installing asset: " << asset.root.dump(4) << endl;
    } catch (std::exception& exc) {
        SWarn << "No installable assets: " << exc.what() << endl;
        return nullptr;
    }

    return createInstallTask(pair, opts);
}


Package::Asset
PackageManager::getLatestInstallableAsset(const PackagePair& pair,
                                          const InstallOptions& options) const
{
    if (!pair.local || !pair.remote)
        throw std::runtime_error("Must have a local and remote package to "
                                 "determine installable assets.");

    bool isInstalledAndVerified =
        pair.local->isInstalled() && pair.local->verifyInstallManifest();

    SDebug << "Get asset to install:"
           << "\n\tName: " << pair.local->name()
           << "\n\tDesired Version: " << options.version
           << "\n\tDesired SDK Version: " << options.sdkVersion
           << "\n\tLocal Version: " << pair.local->version()
           << "\n\tLocal Version Lock: " << pair.local->versionLock()
           << "\n\tLocal SDK Version Lock: " << pair.local->sdkLockedVersion()
           << "\n\tLocal Verified: " << isInstalledAndVerified << endl;

    // Return a specific asset version if requested
    std::string version(options.version.empty() ? pair.local->versionLock()
                                                : options.version);
    if (!version.empty()) {
        SDebug << "Get specific asset version: " << version << endl;

        // Ensure the version lock option doesn't conflict with the saved
        // package
        // TODO: Is this really necessary, perhaps the option should override
        // the saved package?
        if (!pair.local->versionLock().empty() &&
            options.version != pair.local->versionLock())
            throw std::runtime_error(
                "Invalid version option: Package already locked at version: " +
                pair.local->versionLock());

        // Get the latest asset for locked SDK version or throw
        Package::Asset asset = pair.remote->assetVersion(version);

        // Throw if we are already running the locked version
        if (isInstalledAndVerified &&
            !util::compareVersion(asset.version(), pair.local->version()))
            throw std::runtime_error(
                "Package is up-to-date at locked version: " + asset.version());

        // Return the requested asset or throw
        return asset;
    }

    // Return the latest asset for a specific SDK asset version if requested
    std::string sdkVersion(options.sdkVersion.empty()
                               ? pair.local->sdkLockedVersion()
                               : options.sdkVersion);
    if (!sdkVersion.empty()) {
        SDebug << "Get latest asset for SDK version: " << sdkVersion << endl;

        // Ensure the SDK version lock option doesn't conflict with the saved
        // package
        if (!pair.local->sdkLockedVersion().empty() &&
            sdkVersion != pair.local->sdkLockedVersion())
            throw std::runtime_error("Invalid SDK version option: Package "
                                     "already locked at SDK version: " +
                                     pair.local->sdkLockedVersion());

        // Get the latest asset for SDK version or throw
        Package::Asset sdkAsset = pair.remote->latestSDKAsset(sdkVersion);

        // Throw if there are no newer assets for the locked version
        if (isInstalledAndVerified &&
            !util::compareVersion(sdkAsset.version(), pair.local->version()))
            throw std::runtime_error("Package is up-to-date at SDK version: " +
                                     options.sdkVersion);

        // Return the newer asset for the locked SDK version
        return sdkAsset;
    }

    // Try to return an asset which is newer than the current one or throw
    Package::Asset latestAsset = pair.remote->latestAsset();
    if (isInstalledAndVerified &&
        !util::compareVersion(latestAsset.version(), pair.local->version()))
        throw std::runtime_error("Package is up-to-date at version: " +
                                 pair.local->version());

    // Return the newer asset
    return latestAsset;
}


bool PackageManager::hasAvailableUpdates(const PackagePair& pair) const
{
    try {
        getLatestInstallableAsset(pair); // has updates or throw
        return true;                     // has updates
    } catch (std::exception&) {
    }
    return false; // up-to-date
}


bool PackageManager::installPackages(const StringVec& ids,
                                     const InstallOptions& options,
                                     InstallMonitor* monitor, bool whiny)
{
    bool res = false;
    try {
        for (const auto& id : ids) {
            auto task = installPackage(id, options); //, whiny
            if (task) {
                if (monitor)
                    monitor->addTask(task); // manual start
                else
                    task->start(); // auto start
                res = true;
            }
        }
    } catch (std::exception& exc) {
        SError << "Installation failed: " << exc.what() << endl;
        if (whiny)
            throw exc;
    }
    return res;
}


InstallTask::Ptr PackageManager::updatePackage(const std::string& name,
                                               const InstallOptions& options)
{
    // An update action is essentially the same as an install action,
    // except we make sure local package exists before continuing.
    {
        if (!localPackages().contains(name)) {
            std::string error("Update Failed: " + name + " is not installed.");
            SError << "" << error << endl;
            throw std::runtime_error(error);
            // if (whiny)
            // else
            //    return nullptr;
        }
    }

    return installPackage(name, options); //, whiny
}


bool PackageManager::updatePackages(const StringVec& ids,
                                    const InstallOptions& options,
                                    InstallMonitor* monitor, bool whiny)
{
    // An update action is essentially the same as an install action,
    // except we make sure local package exists before continuing.
    StringVec toUpdate(ids);
    {
        for (const auto& id : ids) {
            if (!localPackages().contains(id)) {
                std::string error("Cannot update " + id +
                                  " because it's not installed.");
                SError << error << endl;
                if (whiny)
                    throw std::runtime_error(error);
                // else
                //    return nullptr;
            } else
                toUpdate.push_back(id);
        }
    }

    return installPackages(toUpdate, options, monitor, whiny);
}


bool PackageManager::updateAllPackages(bool whiny)
{
    StringVec toUpdate;
    for (const auto& [id, pkg] : localPackages()) {
        toUpdate.push_back(id);
    }

    InstallOptions options;
    return installPackages(toUpdate, options, nullptr, whiny);
}


bool PackageManager::uninstallPackage(const std::string& id, bool whiny)
{
    SDebug << "Uninstalling: " << id << endl;

    try {
        auto* package = localPackages().get(id);
        if (!package)
            throw std::runtime_error("Package not found: " + id);
        try {
            // Delete package files from manifest
            // NOTE: If some files fail to delete we still consider the
            // uninstall a success.
            LocalPackage::Manifest manifest = package->manifest();
            if (!manifest.empty()) {
                for (const auto& entry : manifest.root) {
                    std::string path(package->getInstalledFilePath(entry.get<std::string>()));
                    SDebug << "Delete file: " << path << endl;
                    try {
                        fs::unlink(path);
                    } catch (std::exception& exc) {
                        SError << "Error deleting file: " << exc.what() << ": "
                               << path << endl;
                    }
                }
                manifest.root.clear();
            } else {
                SDebug << "Uninstall: Empty package manifest: " << id << endl;
            }

            // Delete package manifest file
            validatePathComponent(package->id(), "uninstallPackage");
            std::string path(options().dataDir);
            path = fs::makePath(path, package->id() + ".json"); // manifest_

            SDebug << "Delete manifest: " << path << endl;
            fs::unlink(path);
        } catch (std::exception& exc) {
            SError << "Nonfatal uninstall error: " << exc.what() << endl;
            // Swallow and continue...
        }

        // Set the package as Uninstalled
        package->setState("Uninstalled");

        // Notify the outside application
        PackageUninstalled.emit(*package);

        // Free package reference from memory (unique_ptr handles deletion)
        localPackages().erase(package->id());
    } catch (std::exception& exc) {
        SError << "Fatal uninstall error: " << exc.what() << endl;
        if (whiny)
            throw exc;
        else
            return false;
    }

    return true;
}


bool PackageManager::uninstallPackages(const StringVec& ids, bool whiny)
{
    SDebug << "Uninstall packages: " << ids.size() << endl;
    bool res = true;
    for (const auto& id : ids) {
        if (!uninstallPackage(id, whiny))
            res = false;
    }
    return res;
}


InstallTask::Ptr PackageManager::createInstallTask(PackagePair& pair, const InstallOptions& options)
{
    SInfo << "Create install task: " << pair.name() << endl;

    // Ensure we only have one task per package
    if (getInstallTask(pair.remote->id()))
        throw std::runtime_error(pair.remote->name() + " is already installing.");

    auto task = std::make_shared<InstallTask>(*this, pair.local, pair.remote, options);
    task->Complete += slot(this, &PackageManager::onPackageInstallComplete, -1, -1); // lowest priority to remove task
    {
        std::lock_guard<std::mutex> guard(_mutex);
        _tasks.push_back(task);
    }
    InstallTaskCreated.emit(*task);
    return task; // must call task->start()
}


bool PackageManager::hasUnfinalizedPackages()
{
    SDebug << "checking for unfinalized packages" << endl;

    bool res = false;
    auto& packages = localPackages();
    for (auto& [key, pkg] : packages) {
        if (pkg->state() == "Installing" &&
            pkg->installState() == "Finalizing") {
            SDebug << "finalization required: " << pkg->name() << endl;
            res = true;
        }
    }

    return res;
}


bool PackageManager::finalizeInstallations(bool whiny)
{
    SDebug << "Finalizing installations" << endl;

    bool res = true;
    auto& packages = localPackages();
    for (auto& [key, pkg] : packages) {
        try {
            if (pkg->state() == "Installing" &&
                pkg->installState() == "Finalizing") {
                SDebug << "Finalizing: " << pkg->name() << endl;

                // Create an install task on the stack - we only have
                // to move some files around so no async required.
                InstallTask task(*this, pkg.get(), nullptr);
                task.doFinalize();

                if (pkg->state() != "Installed" || pkg->installState() != "Installed")
                    LWarn("Package not in expected state after finalization");

                // Manually emit the install complete signal.
                InstallTaskComplete.emit(task);
            }
        } catch (std::exception& exc) {
            SError << "Finalize Error: " << exc.what() << endl;
            res = false;
            if (whiny)
                throw exc;
        }

        // Always save the package.
        saveLocalPackage(*pkg, false);
    }

    return res;
}

void PackageManager::onPackageInstallComplete(InstallTask& task)
{
    STrace << "Install complete: " << task.state().toString() << endl;

    // Save the local package
    saveLocalPackage(*task.local());

    // PackageInstallationComplete.emit(*task.local());
    InstallTaskComplete.emit(task);

    // Remove the task reference
    {
        std::lock_guard<std::mutex> guard(_mutex);
        for (auto it = _tasks.begin(); it != _tasks.end(); it++) {
            if (it->get() == &task) {
                _tasks.erase(it);
                break;
            }
        }
    }
}


//
// Task helper methods
//

InstallTask::Ptr PackageManager::getInstallTask(const std::string& id) const
{
    std::lock_guard<std::mutex> guard(_mutex);
    for (const auto& task : _tasks) {
        if (task->remote()->id() == id)
            return task;
    }
    return nullptr;
}


InstallTaskPtrVec PackageManager::tasks() const
{
    std::lock_guard<std::mutex> guard(_mutex);
    return _tasks;
}


//
// Package helper methods
//

PackagePair PackageManager::getPackagePair(const std::string& id,
                                           bool whiny) const
{
    std::lock_guard<std::mutex> guard(_mutex);
    auto* local = _localPackages.get(id);
    auto* remote = _remotePackages.get(id);

    if (whiny && local && !local->valid())
        throw std::runtime_error("The local package is invalid");

    if (whiny && remote && !remote->valid())
        throw std::runtime_error("The remote package is invalid");

    return PackagePair(local, remote);
}


PackagePairVec PackageManager::getPackagePairs() const
{
    PackagePairVec pairs;
    std::lock_guard<std::mutex> guard(_mutex);
    auto& lpackages = _localPackages;
    auto& rpackages = _remotePackages;
    for (auto& [key, pkg] : lpackages) {
        pairs.push_back(PackagePair(pkg.get()));
    }
    for (auto& [key, pkg] : rpackages) {
        bool exists = false;
        for (unsigned i = 0; i < pairs.size(); i++) {
            if (pairs[i].id() == pkg->id()) {
                pairs[i].remote = pkg.get();
                exists = true;
                break;
            }
        }
        if (!exists)
            pairs.push_back(PackagePair(nullptr, pkg.get()));
    }
    return pairs;
}


PackagePairVec PackageManager::getUpdatablePackagePairs() const
{
    PackagePairVec pairs = getPackagePairs();
    for (auto it = pairs.begin(); it != pairs.end();) {
        if (!hasAvailableUpdates(*it)) {
            it = pairs.erase(it);
        } else
            ++it;
    }
    return pairs;
}


PackagePair PackageManager::getOrCreatePackagePair(const std::string& id)
{
    std::lock_guard<std::mutex> guard(_mutex);
    auto* remote = _remotePackages.get(id);
    if (!remote)
        throw std::runtime_error("The remote package does not exist.");

    if (remote->assets().empty())
        throw std::runtime_error("The remote package has no file assets.");

    if (!remote->latestAsset().valid())
        throw std::runtime_error("The remote package has invalid file assets.");

    if (!remote->valid())
        throw std::runtime_error("The remote package is invalid.");

    // Get or create the local package description.
    auto* local = _localPackages.get(id);
    if (!local) {
        auto pkg = std::make_unique<LocalPackage>(*remote);
        local = pkg.get();
        _localPackages.tryAdd(id, std::move(pkg));
    }

    if (!local->valid())
        throw std::runtime_error("The local package is invalid.");

    return PackagePair(local, remote);
}


string PackageManager::installedPackageVersion(const std::string& id) const
{
    std::lock_guard<std::mutex> guard(_mutex);
    auto* local = _localPackages.get(id);
    if (!local)
        throw std::runtime_error("Package not found: " + id);

    if (!local->valid())
        throw std::runtime_error("The local package is invalid.");
    if (!local->isInstalled())
        throw std::runtime_error("The local package is not installed.");

    return local->version();
}


// ---------------------------------------------------------------------
// File Helper Methods
//
void PackageManager::clearCache()
{
    std::string dir(options().tempDir);
    fs::rmdir(dir); // remove it
    if (fs::exists(dir))
        LWarn("Failed to fully remove cache directory: ", dir);
}


bool PackageManager::clearPackageCache(LocalPackage& package)
{
    bool res = true;
    json::Value& assets = package["assets"];
    for (unsigned i = 0; i < assets.size(); i++) {
        Package::Asset asset(assets[i]);
        if (!clearCacheFile(asset.fileName(), false))
            res = false;
    }
    return res;
}


bool PackageManager::clearCacheFile(std::string_view fileName, bool whiny)
{
    try {
        std::string path = fs::makePath(options().tempDir, fileName);
        fs::unlink(path);
        return true;
    } catch (std::exception& exc) {
        SError << "Clear Cache Error: " << fileName << ": " << exc.what()
               << endl;
        if (whiny)
            throw exc;
    }
    return false;
}


bool PackageManager::hasCachedFile(Package::Asset& asset)
{
    std::string path = fs::makePath(options().tempDir, asset.fileName());
    if (!fs::exists(path))
        return false;

    // Validate file size if the asset specifies one
    int expectedSize = asset.fileSize();
    if (expectedSize > 0) {
        auto actualSize = fs::filesize(path);
        if (static_cast<std::int64_t>(expectedSize) != actualSize)
            return false;
    }

    return true;
}


bool PackageManager::isSupportedFileType(std::string_view fileName)
{
    return fileName.find(".zip") != std::string_view::npos ||
           fileName.find(".tar.gz") != std::string_view::npos;
}


std::string PackageManager::getCacheFilePath(std::string_view fileName)
{
    validatePathComponent(fileName, "getCacheFilePath");
    return fs::makePath(options().tempDir, fileName);
}


std::string PackageManager::getPackageDataDir(std::string_view id)
{
    validatePathComponent(id, "getPackageDataDir");
    std::string dir = fs::makePath(options().dataDir, id);
    fs::mkdirr(dir); // create it
    return dir;
}


PackageManager::Options& PackageManager::options()
{
    std::lock_guard<std::mutex> guard(_mutex);
    return _options;
}


RemotePackageStore& PackageManager::remotePackages()
{
    std::lock_guard<std::mutex> guard(_mutex);
    return _remotePackages;
}


LocalPackageStore& PackageManager::localPackages()
{
    std::lock_guard<std::mutex> guard(_mutex);
    return _localPackages;
}


} // namespace pacm
} // namespace icy


/// @}
