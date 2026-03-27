///
//
// icey
// Copyright (c) 2005, icey <https://0state.com>
//
// SPDX-License-Identifier: LGPL-2.1+
//
/// @addtogroup pacm
/// @{


#pragma once


#include "icy/json/json.h"


namespace icy {
namespace pacm {


/// JSON-backed package metadata shared by local and remote package records.
struct Package : public json::Value
{
    /// Archive asset metadata for a specific package build.
    struct Asset
    {
        /// @param src JSON object node that backs this asset.
        Asset(json::Value& src);
        Asset(const Asset&) = default;
        virtual ~Asset() noexcept;

        /// Returns the archive file name (e.g. "my-plugin-1.0.0.zip").
        virtual std::string fileName() const;

        /// Returns the package version string (e.g. "1.0.0").
        virtual std::string version() const;

        /// Returns the SDK version this asset was built against (e.g. "2.0.0").
        virtual std::string sdkVersion() const;

        /// Returns the asset checksum string, or empty if none is set.
        virtual std::string checksum() const;

        /// Returns the download URL from the mirror list at @p index.
        /// @param index Zero-based index into the mirrors array.
        virtual std::string url(int index = 0) const;

        /// Returns the uncompressed file size in bytes, or 0 if not set.
        virtual int fileSize() const;

        /// Returns true if the asset has the minimum required fields
        /// (file-name, version, mirrors).
        virtual bool valid() const;

        /// Writes the raw JSON of this asset to @p ost.
        /// @param ost Output stream.
        virtual void print(std::ostream& ost) const;

        /// Copies the backing JSON node from @p r.
        /// @param r Source asset to copy from.
        virtual Asset& operator=(const Asset& r);

        /// Returns true if file name, version and checksum all match @p r.
        virtual bool operator==(const Asset& r) const;

        json::Value& root;
    };

    /// Constructs an empty package.
    Package();

    /// Constructs a package from an existing JSON value.
    /// @param src JSON object containing package fields.
    Package(const json::Value& src);
    virtual ~Package() noexcept;

    /// Returns the package unique identifier.
    virtual std::string id() const;

    /// Returns the package display name.
    virtual std::string name() const;

    /// Returns the package type (e.g. "plugin", "asset").
    virtual std::string type() const;

    /// Returns the package author string.
    virtual std::string author() const;

    /// Returns the package description string.
    virtual std::string description() const;

    /// Returns true if id, name and type are all non-empty.
    virtual bool valid() const;

    /// Returns a plain JSON copy of this package object.
    [[nodiscard]] virtual json::Value toJson() const;

    /// Dumps the JSON representation of this package to @p ost.
    /// @param ost Output stream.
    virtual void print(std::ostream& ost) const;
};


//
// Remote Package
//

/// Package metadata loaded from the remote package index.
struct RemotePackage : public Package
{
    /// Constructs an empty remote package.
    RemotePackage();

    /// Constructs a remote package from an existing JSON value.
    /// @param src JSON object containing remote package fields.
    RemotePackage(const json::Value& src);
    virtual ~RemotePackage() noexcept;

    /// Returns a reference to the "assets" JSON array node.
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

/// Package metadata for an installed package on the local filesystem.
struct LocalPackage : public Package
{
    /// Manifest of installed files recorded for a local package.
    struct Manifest
    {
        /// @param src JSON array node that backs this manifest.
        Manifest(json::Value& src);
        virtual ~Manifest() noexcept;

        /// Returns true if the manifest contains no file entries.
        virtual bool empty() const;

        /// Appends @p path to the manifest file list.
        /// @param path Relative path of an installed file.
        // virtual void addDir(const std::string& path);
        virtual void addFile(const std::string& path);

        json::Value& root;

    private:
        // Manifest& operator = (const Manifest&) {}
    };

    /// Constructs an empty local package.
    LocalPackage();

    /// Constructs a local package from an existing JSON value.
    /// @param src JSON object containing local package fields.
    LocalPackage(const json::Value& src);

    /// Create the local package from the remote package
    /// reference with the following manipulations.
    ///    1) Add a local manifest element.
    ///    2) Remove asset mirror elements.
    LocalPackage(const RemotePackage& src);


    virtual ~LocalPackage() noexcept;

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


    /// Returns the pinned version string, or empty if no lock is set.
    virtual std::string versionLock() const;

    /// Returns the pinned SDK version string, or empty if no lock is set.
    virtual std::string sdkLockedVersion() const;

    /// Returns the currently installed asset, if any.
    /// If none, the returned asset will be empty().
    virtual Asset asset();

    /// Returns true or false depending on weather or
    /// not the package is installed successfully.
    /// False if package is in Failed state.
    virtual bool isInstalled() const;

    /// Returns true if the package state is "Failed".
    virtual bool isFailed() const;

    /// Returns the installation manifest.
    virtual Manifest manifest();

    virtual bool verifyInstallManifest(bool allowEmpty = false);

    /// Returns the full full path of the installed file.
    /// Thrown an exception if the install directory is unset.
    virtual std::string getInstalledFilePath(const std::string& fileName,
                                             bool whiny = false);

    /// Returns a reference to the JSON array of accumulated error messages.
    virtual json::Value& errors();

    /// Appends @p message to the errors array.
    /// @param message Error description to record.
    virtual void addError(const std::string& message);

    /// Returns the most recently added error message, or empty if none.
    virtual std::string lastError() const;

    /// Clears all recorded error messages.
    virtual void clearErrors();

    virtual bool valid() const;
};


//
// Package Pair
//

/// Pairing of the installed and remote metadata for the same package ID.
struct PackagePair
{
    /// @param local  Pointer to the locally installed package, or nullptr if not installed.
    /// @param remote Pointer to the remote package record, or nullptr if not known.
    PackagePair(LocalPackage* local = nullptr, RemotePackage* remote = nullptr);
    virtual ~PackagePair() = default;

    /// Returns true if at least one of local/remote is set and that pointer is itself valid().
    virtual bool valid() const;

    /// Returns the package ID, preferring the local package if available.
    std::string id() const;

    /// Returns the package display name, preferring the local package if available.
    std::string name() const;

    /// Returns the package type, preferring the local package if available.
    std::string type() const;

    /// Returns the package author, preferring the local package if available.
    std::string author() const;

    /// Returns true if there are no possible updates for
    /// this package, false otherwise.
    // virtual bool hasAvailableUpdates();

    LocalPackage* local;
    RemotePackage* remote;
};


/// Vector of local/remote package pairs used for reconciliation and update checks.
using PackagePairVec = std::vector<PackagePair>;


} // namespace pacm
} // namespace icy


/// @}
