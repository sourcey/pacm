///
//
// LibSourcey
// Copyright (c) 2005, Sourcey <http://sourcey.com>
//
// SPDX-License-Identifier:	LGPL-2.1+
//
/// @addtogroup pacm
/// @{


#ifndef SCY_Pacm_Package_H
#define SCY_Pacm_Package_H


#include "scy/json/json.h"


namespace scy {
namespace pacm {


/// This class is a JSON representation of an
/// package belonging to the PackageManager.
struct Package : public json::Value
{
    /// This class represents a archived file
    /// asset containing files belonging to
    /// the parent package.
    struct Asset
    {
        Asset(json::Value& src);
        virtual ~Asset();

        virtual std::string fileName() const;
        virtual std::string version() const;
        virtual std::string sdkVersion() const;
        virtual std::string checksum() const;
        virtual std::string url(int index = 0) const;
        virtual int fileSize() const;

        virtual bool valid() const;

        virtual void print(std::ostream& ost) const;

        virtual Asset& operator=(const Asset& r);
        virtual bool operator==(const Asset& r) const;

        json::Value& root;
    };

    Package();
    Package(const json::Value& src);
    virtual ~Package();

    virtual std::string id() const;
    virtual std::string name() const;
    virtual std::string type() const;
    virtual std::string author() const;
    virtual std::string description() const;

    virtual bool valid() const;

    virtual void print(std::ostream& ost) const;
};


//
// Remote Package
//

/// This class is a JSON representation of an
/// package existing on the remote server that
/// may be downloaded and installed.
struct RemotePackage : public Package
{
    RemotePackage();
    RemotePackage(const json::Value& src);
    virtual ~RemotePackage();

    virtual json::Value& assets();

    /// Returns the latest asset for this package.
    /// For local packages this is the currently installed version.
    /// For remote packages this is the latest available version.
    /// Throws an exception if no asset exists.
    virtual Asset latestAsset();

    /// Returns the latest asset for the given package version.
    /// Throws an exception if no asset exists.
    virtual Asset assetVersion(const std::string& version);

    /// Returns the latest asset for the given SDK version.
    /// This method is for safely installing plug-ins which
    /// must be compiled against a specific SDK version.
    /// The package JSON must have a "sdk-version" member
    /// for this function to work as intended.
    /// Throws an exception if no asset exists.
    virtual Asset latestSDKAsset(const std::string& version);
};


//
// Local Package
//

/// This class is a JSON representation of an
/// installed local package that exists on the
/// file system.
struct LocalPackage : public Package
{
    /// This class provides a list of all package
    /// files and their location on the file system.
    struct Manifest
    {
        Manifest(json::Value& src);
        virtual ~Manifest();

        virtual bool empty() const;

        // virtual void addDir(const std::string& path);
        virtual void addFile(const std::string& path);

        json::Value& root;

    private:
        // Manifest& operator = (const Manifest&) {}
    };

    LocalPackage();
    LocalPackage(const json::Value& src);

    /// Create the local package from the remote package
    /// reference with the following manipulations.
    ///    1) Add a local manifest element.
    ///    2) Remove asset mirror elements.
    LocalPackage(const RemotePackage& src);


    virtual ~LocalPackage();

    /// Set's the overall package state. Possible values are:
    /// Installing, Installed, Failed, Uninstalled.
    /// If the packages completes while still Installing,
    /// this means the package has yet to be finalized.
    virtual void setState(const std::string& state);

    /// Set's the package installation state.
    /// See InstallationState for possible values.
    virtual void setInstallState(const std::string& state);

    /// Set's the installation directory for this package.
    virtual void setInstallDir(const std::string& dir);

    /// Sets the installed asset, once installed.
    /// This method also sets the version.
    virtual void setInstalledAsset(const Package::Asset& installedRemoteAsset);

    /// Sets the current version of the local package.
    /// Installation must be complete.
    virtual void setVersion(const std::string& version);

    /// Locks the package at the given version.
    /// Once set this package will not be updated past
    /// the given version.
    /// Pass an empty string to remove the lock.
    virtual void setVersionLock(const std::string& version);

    /// Locks the package at the given SDK version.
    /// Once set this package will only update to the most
    /// recent version with given SDK version.
    /// Pass an empty string to remove the lock.
    virtual void setSDKVersionLock(const std::string& version);

    /// Returns the installed package version.
    virtual std::string version() const;

    /// Returns the current state of this package.
    virtual std::string state() const;

    /// Returns the installation state of this package.
    virtual std::string installState() const;

    /// Returns the installation directory for this package.
    virtual std::string installDir() const;


    virtual std::string versionLock() const;
    virtual std::string sdkLockedVersion() const;

    /// Returns the currently installed asset, if any.
    /// If none, the returned asset will be empty().
    virtual Asset asset();

    /// Returns true or false depending on weather or
    /// not the package is installed successfully.
    /// False if package is in Failed state.
    virtual bool isInstalled() const;

    virtual bool isFailed() const;

    /// Returns the installation manifest.
    virtual Manifest manifest();

    virtual bool verifyInstallManifest(bool allowEmpty = false);

    /// Returns the full full path of the installed file.
    /// Thrown an exception if the install directory is unset.
    virtual std::string getInstalledFilePath(const std::string& fileName,
                                             bool whiny = false);

    virtual json::Value& errors();
    virtual void addError(const std::string& message);
    virtual std::string lastError() const;
    virtual void clearErrors();

    virtual bool valid() const;
};


//
// Package Pair
//

/// This class provides pairing of a local and a
/// remote package.
struct PackagePair
{
    PackagePair(LocalPackage* local = nullptr, RemotePackage* remote = nullptr);

    virtual bool valid() const;

    std::string id() const;
    std::string name() const;
    std::string type() const;
    std::string author() const;

    /// Returns true if there are no possible updates for
    /// this package, false otherwise.
    // virtual bool hasAvailableUpdates();

    LocalPackage* local;
    RemotePackage* remote;
};


typedef std::vector<PackagePair> PackagePairVec;


} // namespace pacm
} // namespace scy


#endif // SCY_Pacm_Package_H


/// @\}
