///
//
// LibSourcey
// Copyright (c) 2005, Sourcey <https://sourcey.com>
//
// SPDX-License-Identifier: LGPL-2.1+
//
/// @addtogroup pacm
/// @{


#include "scy/pacm/package.h"
#include "scy/filesystem.h"
#include "scy/logger.h"
#include "scy/util.h"

#include "assert.h"


namespace scy {
namespace pacm {


//
// Base Package
//


Package::Package()
{
}


Package::Package(const json::value& src)
    : json::value(src)
{
}


Package::~Package()
{
}


bool Package::valid() const
{
    return !id().empty() && !name().empty() && !type().empty();
}


std::string Package::id() const
{
    return (*this)["id"].get<std::string>();
}


std::string Package::type() const
{
    return (*this)["type"].get<std::string>();
}


std::string Package::name() const
{
    return (*this)["name"].get<std::string>();
}


std::string Package::author() const
{
    return (*this)["author"].get<std::string>();
}


std::string Package::description() const
{
    return (*this)["description"].get<std::string>();
}


void Package::print(std::ostream& ost) const
{
    ost << dump();
}


//
// Package Asset
//


Package::Asset::Asset(json::value& src)
    : root(src)
{
}


Package::Asset::~Asset()
{
}


std::string Package::Asset::fileName() const
{
    return root["file-name"].get<std::string>();
}


std::string Package::Asset::version() const
{
    return root.value("version", "0.0.0");
}


std::string Package::Asset::sdkVersion() const
{
    return root.value("sdk-version", "0.0.0");
}


std::string Package::Asset::checksum() const
{
    return root.value("checksum", "");
}


std::string Package::Asset::url(int index) const
{
    return root["mirrors"][index]["url"].get<std::string>();
}


int Package::Asset::fileSize() const
{
    return root.value("file-size", 0);
}


bool Package::Asset::valid() const
{
    return root.find("file-name") != root.end()
        && root.find("version") != root.end()
        && root.find("mirrors") != root.end();
}


void Package::Asset::print(std::ostream& ost) const
{
    ost << root.dump();
}


Package::Asset& Package::Asset::operator=(const Asset& r)
{
    root = r.root;
    return *this;
}


bool Package::Asset::operator==(const Asset& r) const
{
    return fileName() == r.fileName() 
        && version() == r.version() 
        && checksum() == r.checksum();
}


//
// Remote Package
//


RemotePackage::RemotePackage()
{
}


RemotePackage::RemotePackage(const json::value& src)
    : Package(src)
{
}


RemotePackage::~RemotePackage()
{
}


json::value& RemotePackage::assets()
{
    return (*this)["assets"];
}


Package::Asset RemotePackage::latestAsset()
{
    json::value& assets = this->assets();
    if (assets.empty())
        throw std::runtime_error("Package has no assets");

    // The latest asset may not be in order, so
    // make sure we always return the latest one.
    int index = 0;
    if (assets.size() > 1) {
        for (int i = 1; i < static_cast<int>(assets.size()); i++) {
            if (util::compareVersion(assets[i]["version"].get<std::string>(),
                                     assets[index]["version"].get<std::string>())) {
                index = i;
            }
        }
    }

    return Asset(assets[index]);
}


Package::Asset RemotePackage::assetVersion(const std::string& version)
{
    json::value& assets = this->assets();
    if (assets.empty())
        throw std::runtime_error("Package has no assets");

    int index = -1;
    for (int i = 1; i < static_cast<int>(assets.size()); i++) {
        if (assets[i]["version"].get<std::string>() == version) {
            index = i;
            break;
        }
    }

    if (index == -1)
        throw std::runtime_error("No package asset with version " + version);

    return Asset(assets[index]);
}


Package::Asset RemotePackage::latestSDKAsset(const std::string& version)
{
    json::value& assets = this->assets();
    if (assets.empty())
        throw std::runtime_error("Package has no assets");

    int index = -1;
    for (int i = 1; i < static_cast<int>(assets.size()); i++) {
        if (assets[i]["sdk-version"].get<std::string>() == version &&
            (index == -1 ||
             (assets[index]["sdk-version"].get<std::string>() != version ||
              util::compareVersion(assets[i]["version"].get<std::string>(),
                                   assets[index]["version"].get<std::string>())))) {
            index = i;
        }
    }

    if (index == -1)
        throw std::runtime_error("No package asset with SDK version " +
                                 version);

    return Asset(assets[index]);
}


//
// Local Package
//


LocalPackage::LocalPackage()
{
}


LocalPackage::LocalPackage(const json::value& src)
    : Package(src)
{
}


LocalPackage::LocalPackage(const RemotePackage& src)
    : Package(src)
{
    assert(src.valid());

    // Clear unwanted remote package fields
    erase("assets");
    assert(valid());
}


LocalPackage::~LocalPackage()
{
}


Package::Asset LocalPackage::asset()
{
    return Package::Asset((*this)["asset"]);
}


LocalPackage::Manifest LocalPackage::manifest()
{
    return Manifest((*this)["manifest"]);
}


void LocalPackage::setState(const std::string& state)
{
    assert(state == "Installing" || state == "Installed" || state == "Failed" ||
           state == "Uninstalled");

    (*this)["state"] = state;
}


void LocalPackage::setInstallState(const std::string& state)
{
    (*this)["install-state"] = state;
}


void LocalPackage::setVersion(const std::string& version)
{
    if (state() != "Installed")
        throw std::runtime_error(
            "Package must be installed before the version is set.");

    (*this)["version"] = version;
}


bool LocalPackage::isInstalled() const
{
    return state() == "Installed";
}


bool LocalPackage::isFailed() const
{
    return state() == "Failed";
}


std::string LocalPackage::state() const
{
    return value("state", "Installing");
}


std::string LocalPackage::installState() const
{
    return value("install-state", "None");
}


std::string LocalPackage::installDir() const
{
    return value("install-dir", "");
}


std::string LocalPackage::getInstalledFilePath(const std::string& fileName, bool whiny)
{
    std::string dir = installDir();
    if (whiny && dir.empty())
        throw std::runtime_error("Package install directory is not set.");

    // TODO: What about sub directories?
    fs::addnode(dir, fileName);
    return dir;
}


void LocalPackage::setVersionLock(const std::string& version)
{
    if (version.empty())
        (*this).erase("version-lock");
    else
        (*this)["version-lock"] = version;
}


void LocalPackage::setSDKVersionLock(const std::string& version)
{
    if (version.empty())
        (*this).erase("sdk-version-lock");
    else
        (*this)["sdk-version-lock"] = version;
}


std::string LocalPackage::version() const
{
    return value("version", "0.0.0");
}


std::string LocalPackage::versionLock() const
{
    return value("version-lock", "");
}


std::string LocalPackage::sdkLockedVersion() const
{
    return value("sdk-version-lock", "");
}


bool LocalPackage::verifyInstallManifest(bool allowEmpty)
{
    DebugS(this) << name() << ": Verifying install manifest" << std::endl;

    // Check file system for each manifest file
    LocalPackage::Manifest manifest = this->manifest();
    for (auto it = manifest.root.begin(); it != manifest.root.end(); it++) {
        std::string path = this->getInstalledFilePath((*it).get<std::string>(), false);
        DebugS(this) << name() << ": Checking exists: " << path << std::endl;

        if (!fs::exists(fs::normalize(path))) {
            ErrorS(this) << name() << ": Missing file: " << path << std::endl;
            return false;
        }
    }

    return allowEmpty ? true : !manifest.empty();
}


void LocalPackage::setInstalledAsset(const Package::Asset& installedRemoteAsset)
{
    if (state() != "Installed")
        throw std::runtime_error(
            "Package must be installed before asset can be set.");

    if (!installedRemoteAsset.valid())
        throw std::runtime_error("Remote asset is invalid.");

    (*this)["asset"] = installedRemoteAsset.root;
    setVersion(installedRemoteAsset.version());
}


void LocalPackage::setInstallDir(const std::string& dir)
{
    (*this)["install-dir"] = dir;
}


json::value& LocalPackage::errors()
{
    return (*this)["errors"];
}


void LocalPackage::addError(const std::string& message)
{
    errors().push_back(message);
}


std::string LocalPackage::lastError() const
{
    json::value errors = (*this)["errors"];
    return errors.empty() ? "" : errors[errors.size() - 1].get<std::string>();
}


void LocalPackage::clearErrors()
{
    errors().clear();
}


bool LocalPackage::valid() const
{
    return Package::valid();
}


//
// Local Package Manifest
//


LocalPackage::Manifest::Manifest(json::value& src)
    : root(src)
{
}


LocalPackage::Manifest::~Manifest()
{
}


void LocalPackage::Manifest::addFile(const std::string& path)
{
    // Do not allow duplicates
    // if (!find_child_by_(*this)["file", "path", path.c_str()).empty())
    //    return;

    // json::value node(path);
    root.push_back(path);
}


bool LocalPackage::Manifest::empty() const
{
    return root.empty();
}


//
// Package Pair
//


PackagePair::PackagePair(LocalPackage* local, RemotePackage* remote)
    : local(local)
    , remote(remote)
{
}


std::string PackagePair::id() const
{
    return local ? local->id() : remote ? remote->id() : "";
}


std::string PackagePair::name() const
{
    return local ? local->name() : remote ? remote->name() : "";
}


std::string PackagePair::type() const
{
    return local ? local->type() : remote ? remote->type() : "";
}


std::string PackagePair::author() const
{
    return local ? local->author() : remote ? remote->author() : "";
}


bool PackagePair::valid() const
{
    // Packages must be valid, and
    // must have at least one package.
    return (!local || local->valid()) && (!remote || remote->valid()) &&
           (local || remote);
}


} // namespace pacm
} // namespace scy


/// @\}
