///
//
// icey
// Copyright (c) 2005, icey <https://0state.com>
//
// SPDX-License-Identifier: LGPL-2.1+
//
/// @addtogroup pacm
/// @{


#include "icy/pacm/package.h"
#include "icy/filesystem.h"
#include "icy/logger.h"
#include "icy/util.h"


namespace icy {
namespace pacm {


//
// Base Package
//


Package::Extension::Extension(const json::Value& src)
    : root(src)
{
}


Package::Extension::~Extension() noexcept
{
}


std::string Package::Extension::loader() const
{
    return root.value("loader", "");
}


std::string Package::Extension::runtime() const
{
    return root.value("runtime", "");
}


std::string Package::Extension::entryPoint() const
{
    return root.value("entrypoint", "");
}


int Package::Extension::abiVersion() const
{
    return root.value("abi-version", 0);
}


std::vector<std::string> Package::Extension::capabilities() const
{
    std::vector<std::string> values;
    auto it = root.find("capabilities");
    if (it == root.end() || it->is_null())
        return values;
    if (!it->is_array())
        return values;

    for (const auto& value : *it) {
        if (!value.is_string())
            return {};
        values.push_back(value.get<std::string>());
    }

    return values;
}


bool Package::Extension::valid() const
{
    if (!root.is_object())
        return false;

    const auto runtimeValue = runtime();
    const auto entryPointValue = entryPoint();
    if (runtimeValue.empty() || entryPointValue.empty())
        return false;

    if (runtimeValue == "native" && loader().empty())
        return false;

    auto abi = abiVersion();
    if (abi < 0)
        return false;

    auto it = root.find("capabilities");
    if (it != root.end()) {
        if (!it->is_array())
            return false;
        for (const auto& value : *it) {
            if (!value.is_string() || value.get<std::string>().empty())
                return false;
        }
    }

    return true;
}


bool Package::Extension::hasCapability(std::string_view capability) const
{
    for (const auto& value : capabilities()) {
        if (value == capability)
            return true;
    }
    return false;
}


Package::Package()
{
}


Package::Package(const json::Value& src)
    : json::Value(src)
{
}


Package::~Package() noexcept
{
}


bool Package::valid() const
{
    return !id().empty() && !name().empty() && !type().empty();
}


json::Value Package::toJson() const
{
    return json::Value(static_cast<const json::Value&>(*this));
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


bool Package::hasExtension() const
{
    auto it = find("extension");
    return it != end() && it->is_object();
}


Package::Extension Package::extension() const
{
    auto it = find("extension");
    if (it == end() || !it->is_object())
        throw std::runtime_error("Package does not contain extension metadata");
    return Extension(*it);
}


void Package::print(std::ostream& ost) const
{
    ost << dump();
}


//
// Package Asset
//


Package::Asset::Asset(json::Value& src)
    : root(src)
{
}


Package::Asset::~Asset() noexcept
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
    return root.find("file-name") != root.end() && root.find("version") != root.end() && root.find("mirrors") != root.end();
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
    return fileName() == r.fileName() && version() == r.version() && checksum() == r.checksum();
}


//
// Remote Package
//


RemotePackage::RemotePackage()
{
}


RemotePackage::RemotePackage(const json::Value& src)
    : Package(src)
{
}


RemotePackage::~RemotePackage() noexcept
{
}


json::Value& RemotePackage::assets()
{
    return (*this)["assets"];
}


Package::Asset RemotePackage::latestAsset()
{
    json::Value& assets = this->assets();
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
    json::Value& assets = this->assets();
    if (assets.empty())
        throw std::runtime_error("Package has no assets");

    int index = -1;
    for (int i = 0; i < static_cast<int>(assets.size()); i++) {
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
    json::Value& assets = this->assets();
    if (assets.empty())
        throw std::runtime_error("Package has no assets");

    // Find the asset with the matching SDK version and highest package version
    int index = -1;
    for (int i = 0; i < static_cast<int>(assets.size()); i++) {
        if (assets[i]["sdk-version"].get<std::string>() == version &&
            (index == -1 ||
             util::compareVersion(assets[i]["version"].get<std::string>(),
                                  assets[index]["version"].get<std::string>()))) {
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


LocalPackage::LocalPackage(const json::Value& src)
    : Package(src)
{
}


LocalPackage::LocalPackage(const RemotePackage& src)
    : Package(src)
{
    if (!src.valid())
        throw std::runtime_error("Cannot create local package from invalid remote package");

    // Clear unwanted remote package fields
    erase("assets");
}


LocalPackage::~LocalPackage() noexcept
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
    if (state != "Installing" && state != "Installed" && state != "Failed" && state != "Uninstalled")
        throw std::invalid_argument("Invalid package state: " + state);

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
    dir = fs::makePath(dir, fileName);
    return dir;
}


std::string LocalPackage::extensionEntryPointPath(bool whiny) const
{
    if (!hasExtension())
        return "";

    const auto ext = extension();
    if (!ext.valid()) {
        if (whiny)
            throw std::runtime_error("Package extension metadata is invalid.");
        return "";
    }

    const auto dir = installDir();
    if (whiny && dir.empty())
        throw std::runtime_error("Package install directory is not set.");

    return dir.empty() ? ext.entryPoint() : fs::makePath(dir, ext.entryPoint());
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
    SDebug << name() << ": Verifying install manifest" << std::endl;

    // Check file system for each manifest file
    LocalPackage::Manifest manifest = this->manifest();
    for (const auto& entry : manifest.root) {
        std::string path = this->getInstalledFilePath(entry.get<std::string>(), false);
        SDebug << name() << ": Checking exists: " << path << std::endl;

        if (!fs::exists(fs::normalize(path))) {
            SError << name() << ": Missing file: " << path << std::endl;
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


json::Value& LocalPackage::errors()
{
    json::Value& node = (*this)["errors"];
    if (node.is_null())
        node = json::Value::array();
    else if (!node.is_array())
        throw std::runtime_error("Package errors must be an array.");
    return node;
}


void LocalPackage::addError(const std::string& message)
{
    errors().push_back(message);
}


std::string LocalPackage::lastError() const
{
    auto it = find("errors");
    if (it == end())
        return "";
    if (!it->is_array())
        throw std::runtime_error("Package errors must be an array.");
    return it->empty() ? "" : it->back().get<std::string>();
}


void LocalPackage::clearErrors()
{
    auto it = find("errors");
    if (it == end())
        return;
    if (!it->is_array())
        throw std::runtime_error("Package errors must be an array.");
    it->clear();
}


bool LocalPackage::valid() const
{
    return Package::valid();
}


//
// Local Package Manifest
//


LocalPackage::Manifest::Manifest(json::Value& src)
    : root(src)
{
}


LocalPackage::Manifest::~Manifest() noexcept
{
}


void LocalPackage::Manifest::addFile(const std::string& path)
{
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
    return local ? local->id() : remote ? remote->id()
                                        : "";
}


std::string PackagePair::name() const
{
    return local ? local->name() : remote ? remote->name()
                                          : "";
}


std::string PackagePair::type() const
{
    return local ? local->type() : remote ? remote->type()
                                          : "";
}


std::string PackagePair::author() const
{
    return local ? local->author() : remote ? remote->author()
                                            : "";
}


bool PackagePair::hasExtension() const
{
    return (local && local->hasExtension()) || (remote && remote->hasExtension());
}


bool PackagePair::valid() const
{
    // Packages must be valid, and
    // must have at least one package.
    return (!local || local->valid()) && (!remote || remote->valid()) &&
           (local || remote);
}


} // namespace pacm
} // namespace icy


/// @}
