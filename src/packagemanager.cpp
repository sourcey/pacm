///
//
// LibSourcey
// Copyright (c) 2005, Sourcey <https://sourcey.com>
//
// SPDX-License-Identifier:	LGPL-2.1+
//
/// @addtogroup pacm
/// @{


#include "scy/pacm/packagemanager.h"
#include "scy/http/authenticator.h"
#include "scy/http/client.h"
#include "scy/json/json.h"
#include "scy/packetio.h"
#include "scy/pacm/package.h"
#include "scy/util.h"


using namespace std;


namespace scy {
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
    DebugL << "Querying server: " << _options.endpoint << _options.indexURI
           << endl;

    if (!_tasks.empty())
        throw std::runtime_error(
            "Cannot load packages while tasks are active.");

    try {
        auto conn = http::Client::instance().createConnection(
            _options.endpoint + _options.indexURI);
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
            TraceL << "On package response complete: " << response
                   // << conn->readStream<std::stringstream>().str()
                   << endl;

            parseRemotePackages(conn->readStream<std::stringstream>().str());
            RemotePackageResponse.emit(response);
            conn->close();
        };

        conn->send();
    } catch (std::exception& exc) {
        ErrorL << "Package Query Error: " << exc.what() << endl;
        throw exc;
    }
}


void PackageManager::parseRemotePackages(const std::string& data)
{
    json::Value root;
    json::Reader reader;
    bool ok = reader.parse(data, root);
    if (ok) {
        _remotePackages.clear();

        for (auto it = root.begin(); it != root.end(); it++) {
            auto package = new RemotePackage(*it);
            if (!package->valid()) {
                ErrorL << "Invalid package: " << package->id() << endl;
                delete package;
                continue;
            }
            _remotePackages.add(package->id(), package);
        }
    } else {
        // TODO: Set error state
        ErrorL << "Invalid server JSON response: "
               << reader.getFormattedErrorMessages() << endl;
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
    DebugL << "Loading manifests: " << dir << endl;

    StringVec nodes;
    fs::readdir(dir, nodes);
    for (unsigned i = 0; i < nodes.size(); i++) {
        if (nodes[i].find(".json") != std::string::npos) {
            LocalPackage* package = nullptr;
            try {
                std::string path(dir);
                fs::addnode(path, nodes[i]);
                json::Value root;
                json::loadFile(path, root);

                DebugL << "Loading package manifest: " << path << endl;
                package = new LocalPackage(root);
                if (!package->valid()) {
                    throw std::runtime_error("The local package is invalid.");
                }

                DebugL << "local package added: " << package->name() << endl;
                localPackages().add(package->id(), package);
            } catch (std::exception& exc) {
                ErrorL << "Cannot load local package: " << exc.what() << endl;
                if (package)
                    delete package;
            }
        }
    }
}


bool PackageManager::saveLocalPackages(bool whiny)
{
    TraceL << "Saving local packages" << endl;

    bool res = true;
    LocalPackageMap toSave = localPackages().map();
    for (auto it = toSave.begin(); it != toSave.end(); ++it) {
        if (!saveLocalPackage(static_cast<LocalPackage&>(*it->second), whiny))
            res = false;
    }
    return res;
}


bool PackageManager::saveLocalPackage(LocalPackage& package, bool whiny)
{
    bool res = false;
    try {
        std::string path(util::format("%s/%s.json", options().dataDir.c_str(),
                                      package.id().c_str()));
        DebugL << "Saving local package: " << package.id() << endl;
        json::saveFile(path, package);
        res = true;
    } catch (std::exception& exc) {
        ErrorL << "Save error: " << exc.what() << endl;
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
    DebugL << "Install package: " << name << endl;

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

        DebugL << "Installing asset: " << asset.root.toStyledString() << endl;
    } catch (std::exception& exc) {
        WarnL << "No installable assets: " << exc.what() << endl;
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

    DebugL << "Get asset to install:"
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
        DebugL << "Get specific asset version: " << version << endl;

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
        assert(asset.version() == version);

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
        DebugL << "Get latest asset for SDK version: " << sdkVersion << endl;

        // Ensure the SDK version lock option doesn't conflict with the saved
        // package
        if (!pair.local->sdkLockedVersion().empty() &&
            sdkVersion != pair.local->sdkLockedVersion())
            throw std::runtime_error("Invalid SDK version option: Package "
                                     "already locked at SDK version: " +
                                     pair.local->sdkLockedVersion());

        // Get the latest asset for SDK version or throw
        Package::Asset sdkAsset = pair.remote->latestSDKAsset(sdkVersion);
        assert(sdkAsset.sdkVersion() == sdkVersion);

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

#if 0
    // Return true if the locked version is already installed
    if (!pair.local->versionLock().empty() || !options.version.empty()) {
        std::string versionLock = options.version.empty() ? pair.local->versionLock() : options.version;
        //assert();
        // TODO: assert valid version?

        // Make sure the version option matches the lock, if both were set
        if (!options.version.empty() && !pair.local->versionLock().empty() &&
            options.version != pair.local->versionLock())
            throw std::runtime_error("Invalid version option: Package locked at version: " + pair.local->versionLock());

        // If everything is in order there is nothing to install
        if (isInstalledAndVerified && pair.local->versionLock() == pair.local->version())
            throw std::runtime_error("Package is up-to-date. Locked at version: " + pair.local->versionLock());

        // Return the locked asset, or throw
        return pair.remote->assetVersion(versionLock);
    }

    // Get the best asset from the locked SDK version, if applicable
    if (!pair.local->sdkLockedVersion().empty() || !options.sdkVersion.empty()) {

        // Make sure the version option matches the lock, if both were set
        if (!options.sdkVersion.empty() && !pair.local->sdkLockedVersion().empty() &&
            options.sdkVersion != pair.local->sdkLockedVersion())
            throw std::runtime_error("Invalid SDK version option: Package locked at SDK version: " + pair.local->sdkLockedVersion());

        // Get the latest asset for SDK version
        Package::Asset sdkAsset = pair.remote->latestSDKAsset(pair.local->sdkLockedVersion()); // throw if none

        // If everything is in order there is nothing to install
        if (isInstalledAndVerified && pair.local->asset().sdkVersion() == pair.local->sdkLockedVersion() &&
            !util::compareVersion(sdkAsset.version(), pair.local->version()))
            throw std::runtime_error("Package is up-to-date for SDK: " + pair.local->sdkLockedVersion());

        return sdkAsset;
    }

    // If all else fails return the latest asset!
    Package::Asset latestAsset = pair.remote->latestAsset();
    if (isInstalledAndVerified && !util::compareVersion(latestAsset.version(), pair.local->version()))
        throw std::runtime_error("Package is up-to-date.");

    return latestAsset;
#endif
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
        for (auto it = ids.begin(); it != ids.end(); ++it) {
            auto task = installPackage(*it, options); //, whiny
            if (task) {
                if (monitor)
                    monitor->addTask(task); // manual start
                else
                    task->start(); // auto start
                res = true;
            }
        }
    } catch (std::exception& exc) {
        ErrorL << "Installation failed: " << exc.what() << endl;
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
        if (!localPackages().exists(name)) {
            std::string error("Update Failed: " + name + " is not installed.");
            ErrorL << "" << error << endl;
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
        for (auto it = ids.begin(); it != ids.end(); ++it) {
            if (!localPackages().exists(*it)) {
                std::string error("Cannot update " + *it +
                                  " because it's not installed.");
                ErrorL << error << endl;
                if (whiny)
                    throw std::runtime_error(error);
                // else
                //    return nullptr;
            } else
                toUpdate.push_back(*it);
        }
    }

    return installPackages(toUpdate, options, monitor, whiny);
}


bool PackageManager::updateAllPackages(bool whiny)
{
    StringVec toUpdate;
    auto& packages = localPackages().map();
    for (auto it = packages.begin(); it != packages.end(); ++it) {
        toUpdate.push_back(it->first);
    }

    InstallOptions options;
    return installPackages(toUpdate, options, nullptr, whiny);
}


bool PackageManager::uninstallPackage(const std::string& id, bool whiny)
{
    DebugL << "Uninstalling: " << id << endl;

    try {
        LocalPackage* package = localPackages().get(id, true);
        try {
            // Delete package files from manifest
            // NOTE: If some files fail to delete we still consider the
            // uninstall a success.
            LocalPackage::Manifest manifest = package->manifest();
            if (!manifest.empty()) {
                for (auto it = manifest.root.begin(); it != manifest.root.end();
                     it++) {
                    std::string path(
                        package->getInstalledFilePath((*it).asString()));
                    DebugL << "Delete file: " << path << endl;
                    try {
                        fs::unlink(path);
                    } catch (std::exception& exc) {
                        ErrorL << "Error deleting file: " << exc.what() << ": "
                               << path << endl;
                    }
                }
                manifest.root.clear();
            } else {
                DebugL << "Uninstall: Empty package manifest: " << id << endl;
            }

            // Delete package manifest file
            std::string path(options().dataDir);
            fs::addnode(path, package->id() + ".json"); // manifest_

            DebugL << "Delete manifest: " << path << endl;
            fs::unlink(path);
        } catch (std::exception& exc) {
            ErrorL << "Nonfatal uninstall error: " << exc.what() << endl;
            // Swallow and continue...
        }

        // Set the package as Uninstalled
        package->setState("Uninstalled");

        // Notify the outside application
        PackageUninstalled.emit(*package);

        // Free package reference from memory
        localPackages().remove(package);
        delete package;
    } catch (std::exception& exc) {
        ErrorL << "Fatal uninstall error: " << exc.what() << endl;
        if (whiny)
            throw exc;
        else
            return false;
    }

    return true;
}


bool PackageManager::uninstallPackages(const StringVec& ids, bool whiny)
{
    DebugL << "Uuninstall packages: " << ids.size() << endl;
    bool res = true;
    for (auto it = ids.begin(); it != ids.end(); ++it) {
        if (!uninstallPackage(*it, whiny))
            res = false;
    }
    return res;
}


InstallTask::Ptr
PackageManager::createInstallTask(PackagePair& pair,
                                  const InstallOptions& options)
{
    InfoL << "Create install task: " << pair.name() << endl;

    // Ensure we only have one task per package
    if (getInstallTask(pair.remote->id()))
        throw std::runtime_error(pair.remote->name() +
                                 " is already installing.");

    auto task =
        std::make_shared<InstallTask>(*this, pair.local, pair.remote, options);
    task->Complete += slot(this, &PackageManager::onPackageInstallComplete, -1,
                           -1); // lowest priority to remove task
    {
        std::lock_guard<std::mutex> guard(_mutex);
        _tasks.push_back(task);
    }
    InstallTaskCreated.emit(*task);
    return task; // must call task->start()
}


bool PackageManager::hasUnfinalizedPackages()
{
   

    DebugL << "checking for unfinalized packages" << endl;

    bool res = false;
    auto& packages = localPackages().map();
    for (auto it = packages.begin(); it != packages.end(); ++it) {
        if (it->second->state() == "Installing" &&
            it->second->installState() == "Finalizing") {
            DebugL << "finalization required: " << it->second->name() << endl;
            res = true;
        }
    }

    return res;
}


bool PackageManager::finalizeInstallations(bool whiny)
{
    DebugL << "Finalizing installations" << endl;

    bool res = true;
    auto& packages = localPackages().map();
    for (auto it = packages.begin(); it != packages.end(); ++it) {
        try {
            if (it->second->state() == "Installing" &&
                it->second->installState() == "Finalizing") {
                DebugL << "Finalizing: " << it->second->name() << endl;

                // Create an install task on the stack - we only have
                // to move some files around so no async required.
                InstallTask task(*this, it->second, nullptr);
                task.doFinalize();

                assert(it->second->state() == "Installed" &&
                       it->second->installState() == "Installed");

                // Manually emit the install complete signal.
                InstallTaskComplete.emit(task);

                // if (it->second->state() != "Installed" ||
                //     it->second->installState() != "Installed") {
                //     res = false;
                //     if (whiny)
                //         throw std::runtime_error(it->second->name() + ":
                // Finalization failed");
                // }
            }
        } catch (std::exception& exc) {
            ErrorL << "Finalize Error: " << exc.what() << endl;
            res = false;
            if (whiny)
                throw exc;
        }

        // Always save the package.
        saveLocalPackage(*it->second, false);
    }

    return res;
}

void PackageManager::onPackageInstallComplete(InstallTask& task)
{
    TraceL << "Install complete: " << task.state().toString() << endl;

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
    for (auto it = _tasks.begin(); it != _tasks.end(); it++) {
        if ((*it)->remote()->id() == id)
            return *it;
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
    auto local = _localPackages.get(id, false);
    auto remote = _remotePackages.get(id, false);

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
    auto lpackages = _localPackages.map();  // copy
    auto rpackages = _remotePackages.map(); // copy
    for (auto lit = lpackages.begin(); lit != lpackages.end(); ++lit) {
        pairs.push_back(PackagePair(lit->second));
    }
    for (auto rit = rpackages.begin(); rit != rpackages.end(); ++rit) {
        bool exists = false;
        for (unsigned i = 0; i < pairs.size(); i++) {
            if (pairs[i].id() == rit->second->id()) {
                pairs[i].remote = rit->second;
                exists = true;
                break;
            }
        }
        if (!exists)
            pairs.push_back(PackagePair(nullptr, rit->second));
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
    auto remote = _remotePackages.get(id, false);
    if (!remote)
        throw std::runtime_error("The remote package does not exist.");

    if (remote->assets().empty())
        throw std::runtime_error("The remote package has no file assets.");

    if (!remote->latestAsset().valid())
        throw std::runtime_error("The remote package has invalid file assets.");

    if (!remote->valid())
        throw std::runtime_error("The remote package is invalid.");

    // Get or create the local package description.
    auto local = _localPackages.get(id, false);
    if (!local) {
        local = new LocalPackage(*remote);
        _localPackages.add(id, local);
    }

    if (!local->valid())
        throw std::runtime_error("The local package is invalid.");

    return PackagePair(local, remote);
}


string PackageManager::installedPackageVersion(const std::string& id) const
{
    std::lock_guard<std::mutex> guard(_mutex);
    auto local = _localPackages.get(id, true);

    if (!local->valid())
        throw std::runtime_error("The local package is invalid.");
    if (!local->isInstalled())
        throw std::runtime_error("The local package is not installed.");

    return local->version();
}


#if 0
bool PackageManager::hasAvailableUpdates(PackagePair& pair)
{
    assert(pair.valid());
    return pair.local && pair.remote &&
        pair.local->hasAvailableUpdates(*pair.remote);
}


bool PackageManager::verifyInstallManifest(LocalPackage& package)
{
    DebugL << package.name()
        << ": Verifying install manifest" << endl;

    // Check file system for each manifest file
    LocalPackage::Manifest manifest = package.manifest();
    for (auto it = manifest.root.begin(); it != manifest.root.end(); it++) {
        std::string path = package.getInstalledFilePath((*it).asString(), false);
        DebugL << package.name()
            << ": Checking: " << path << endl;
        File file(path);
        if (!file.exists()) {
            ErrorL << package.name()
                << ": Missing package file: " << path << endl;
            return false;
        }
    }

    return true;
}
#endif


// ---------------------------------------------------------------------
// File Helper Methods
//
void PackageManager::clearCache()
{
    std::string dir(options().tempDir);
    fs::addsep(dir);
    fs::rmdir(dir); // remove it
    assert(!fs::exists(dir));
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


bool PackageManager::clearCacheFile(const std::string& fileName, bool whiny)
{
    try {
        std::string path(options().tempDir);
        fs::addnode(path, fileName);
        fs::unlink(path);
        return true;
    } catch (std::exception& exc) {
        ErrorL << "Clear Cache Error: " << fileName << ": " << exc.what()
               << endl;
        if (whiny)
            throw exc;
    }
    return false;
}


bool PackageManager::hasCachedFile(Package::Asset& asset)
{
    std::string path(options().tempDir);
    fs::addnode(path, asset.fileName());
    return fs::exists(path); // TODO: crc and size check
}


bool PackageManager::isSupportedFileType(const std::string& fileName)
{
    return fileName.find(".zip") != std::string::npos ||
           fileName.find(".tar.gz") != std::string::npos;
}


std::string PackageManager::getCacheFilePath(const std::string& fileName)
{
    std::string dir(options().tempDir);
    fs::addnode(dir, fileName);
    return dir;
}


std::string PackageManager::getPackageDataDir(const std::string& id)
{
    std::string dir(options().dataDir);
    fs::addnode(dir, id);
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
} // namespace scy


/// @\}
