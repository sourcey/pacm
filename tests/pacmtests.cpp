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
#include "icy/pacm/installtask.h"
#include "icy/json/json.h"
#include "icy/logger.h"
#include "icy/test.h"


using namespace std;
using namespace icy;
using namespace icy::test;


static const char* REMOTE_PACKAGE_JSON = R"({
    "id": "test-plugin",
    "name": "Test Plugin",
    "type": "Plugin",
    "author": "Test Author",
    "description": "A test package",
    "assets": [
        {
            "version": "1.0.0",
            "sdk-version": "2.0.0",
            "platform": "linux",
            "checksum": "abc123",
            "file-name": "test-1.0.0.zip",
            "file-size": 1024,
            "mirrors": [{"url": "https://example.com/test-1.0.0.zip"}]
        },
        {
            "version": "1.1.0",
            "sdk-version": "2.0.0",
            "platform": "linux",
            "checksum": "def456",
            "file-name": "test-1.1.0.zip",
            "file-size": 2048,
            "mirrors": [{"url": "https://example.com/test-1.1.0.zip"}]
        },
        {
            "version": "2.0.0",
            "sdk-version": "3.0.0",
            "platform": "linux",
            "checksum": "ghi789",
            "file-name": "test-2.0.0.zip",
            "file-size": 4096,
            "mirrors": [{"url": "https://example.com/test-2.0.0.zip"}]
        }
    ]
})";


int main(int argc, char** argv)
{
    // Logger::instance().add(std::make_unique<ConsoleChannel>("debug", Level::Trace));
    test::init();

    // =========================================================================
    // Package JSON Round-Trip
    //
    describe("package json round-trip", []() {
        json::Value j = json::Value::parse(REMOTE_PACKAGE_JSON);
        pacm::RemotePackage pkg(j);

        expect(pkg.id() == "test-plugin");
        expect(pkg.name() == "Test Plugin");
        expect(pkg.type() == "Plugin");
        expect(pkg.author() == "Test Author");
        expect(pkg.description() == "A test package");
        expect(pkg.valid());

        // Serialize back and verify key fields survive the round-trip
        json::Value serialized = pkg.toJson();
        expect(serialized["id"].get<std::string>() == "test-plugin");
        expect(serialized["name"].get<std::string>() == "Test Plugin");
        expect(serialized["type"].get<std::string>() == "Plugin");
        expect(serialized["author"].get<std::string>() == "Test Author");
        expect(serialized["description"].get<std::string>() == "A test package");
        expect(serialized["assets"].size() == 3);
    });

    // =========================================================================
    // Asset Fields
    //
    describe("asset fields", []() {
        json::Value j = json::Value::parse(REMOTE_PACKAGE_JSON);
        pacm::RemotePackage pkg(j);

        pacm::Package::Asset asset = pkg.assetVersion("1.0.0");
        expect(asset.fileName() == "test-1.0.0.zip");
        expect(asset.version() == "1.0.0");
        expect(asset.sdkVersion() == "2.0.0");
        expect(asset.checksum() == "abc123");
        expect(asset.fileSize() == 1024);
        expect(asset.url() == "https://example.com/test-1.0.0.zip");
        expect(asset.valid());
    });

    // =========================================================================
    // RemotePackage Version Selection
    //
    describe("remote package version selection", []() {
        json::Value j = json::Value::parse(REMOTE_PACKAGE_JSON);
        pacm::RemotePackage pkg(j);

        // latestAsset should return the highest version (2.0.0)
        pacm::Package::Asset latest = pkg.latestAsset();
        expect(latest.version() == "2.0.0");
        expect(latest.fileName() == "test-2.0.0.zip");

        // assetVersion should return exact match
        pacm::Package::Asset v1 = pkg.assetVersion("1.0.0");
        expect(v1.version() == "1.0.0");

        pacm::Package::Asset v11 = pkg.assetVersion("1.1.0");
        expect(v11.version() == "1.1.0");

        // latestSDKAsset should return the highest version for a given SDK
        // SDK 2.0.0 has versions 1.0.0 and 1.1.0, so should return 1.1.0
        pacm::Package::Asset sdkAsset = pkg.latestSDKAsset("2.0.0");
        expect(sdkAsset.version() == "1.1.0");
        expect(sdkAsset.sdkVersion() == "2.0.0");

        // SDK 3.0.0 has only version 2.0.0
        pacm::Package::Asset sdk3Asset = pkg.latestSDKAsset("3.0.0");
        expect(sdk3Asset.version() == "2.0.0");

        // Missing version should throw
        bool threw = false;
        try {
            pkg.assetVersion("9.9.9");
        } catch (const std::runtime_error&) {
            threw = true;
        }
        expect(threw);

        // Missing SDK version should throw
        threw = false;
        try {
            pkg.latestSDKAsset("99.0.0");
        } catch (const std::runtime_error&) {
            threw = true;
        }
        expect(threw);
    });

    // =========================================================================
    // LocalPackage from RemotePackage
    //
    describe("local package from remote package", []() {
        json::Value j = json::Value::parse(REMOTE_PACKAGE_JSON);
        pacm::RemotePackage remote(j);
        pacm::LocalPackage local(remote);

        // Core fields should be preserved
        expect(local.id() == "test-plugin");
        expect(local.name() == "Test Plugin");
        expect(local.type() == "Plugin");
        expect(local.valid());

        // Assets field should be cleared
        expect(local.find("assets") == local.end());
    });

    // =========================================================================
    // LocalPackage State Management
    //
    describe("local package state management", []() {
        json::Value j = json::Value::parse(REMOTE_PACKAGE_JSON);
        pacm::RemotePackage remote(j);
        pacm::LocalPackage local(remote);

        // Default state is "Installing"
        expect(local.state() == "Installing");
        expect(!local.isInstalled());
        expect(!local.isFailed());

        // Set to Installed
        local.setState("Installed");
        expect(local.state() == "Installed");
        expect(local.isInstalled());
        expect(!local.isFailed());

        // Set to Failed
        local.setState("Failed");
        expect(local.state() == "Failed");
        expect(local.isFailed());
        expect(!local.isInstalled());

        // Invalid state should throw
        bool threw = false;
        try {
            local.setState("Bogus");
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        expect(threw);
    });

    // =========================================================================
    // LocalPackage Version Management
    //
    describe("local package version management", []() {
        json::Value j = json::Value::parse(REMOTE_PACKAGE_JSON);
        pacm::RemotePackage remote(j);
        pacm::LocalPackage local(remote);

        // Default version
        expect(local.version() == "0.0.0");

        // setVersion requires Installed state
        local.setState("Installed");
        local.setVersion("1.2.3");
        expect(local.version() == "1.2.3");

        // Version lock
        expect(local.versionLock() == "");
        local.setVersionLock("1.0.0");
        expect(local.versionLock() == "1.0.0");

        // Clear version lock
        local.setVersionLock("");
        expect(local.versionLock() == "");

        // SDK version lock
        expect(local.sdkLockedVersion() == "");
        local.setSDKVersionLock("2.0.0");
        expect(local.sdkLockedVersion() == "2.0.0");

        // Clear SDK version lock
        local.setSDKVersionLock("");
        expect(local.sdkLockedVersion() == "");
    });

    // =========================================================================
    // Manifest Operations
    //
    describe("manifest operations", []() {
        json::Value j = json::Value::parse(REMOTE_PACKAGE_JSON);
        pacm::RemotePackage remote(j);
        pacm::LocalPackage local(remote);

        pacm::LocalPackage::Manifest manifest = local.manifest();

        // Initially empty
        expect(manifest.empty());

        // Add files
        manifest.addFile("lib/plugin.so");
        manifest.addFile("config/plugin.json");
        expect(!manifest.empty());

        // Verify files stored in the manifest
        pacm::LocalPackage::Manifest manifest2 = local.manifest();
        expect(manifest2.root.size() == 2);
        expect(manifest2.root[0].get<std::string>() == "lib/plugin.so");
        expect(manifest2.root[1].get<std::string>() == "config/plugin.json");
    });

    // =========================================================================
    // Error Handling
    //
    describe("error handling", []() {
        json::Value j = json::Value::parse(REMOTE_PACKAGE_JSON);
        pacm::RemotePackage remote(j);
        pacm::LocalPackage local(remote);

        // No errors initially
        expect(local.lastError() == "");

        // Add errors
        local.addError("Download failed");
        expect(local.lastError() == "Download failed");

        local.addError("Checksum mismatch");
        expect(local.lastError() == "Checksum mismatch");

        // Errors accumulate
        expect(local.errors().size() == 2);

        // Clear errors
        local.clearErrors();
        expect(local.lastError() == "");
        expect(local.errors().empty());
    });

    // =========================================================================
    // InstallationState Strings
    //
    describe("installation state strings", []() {
        pacm::InstallationState state;

        expect(state.str(pacm::InstallationState::None) == "None");
        expect(state.str(pacm::InstallationState::Downloading) == "Downloading");
        expect(state.str(pacm::InstallationState::Extracting) == "Extracting");
        expect(state.str(pacm::InstallationState::Finalizing) == "Finalizing");
        expect(state.str(pacm::InstallationState::Installed) == "Installed");
        expect(state.str(pacm::InstallationState::Cancelled) == "Cancelled");
        expect(state.str(pacm::InstallationState::Failed) == "Failed");

        // Unknown state
        expect(state.str(999) == "undefined");
    });

    test::runAll();
    return test::finalize();
}


/// @}
