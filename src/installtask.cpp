///
//
// Icey
// Copyright (c) 2005, Icey <https://icey.com>
//
// SPDX-License-Identifier: LGPL-2.1+
//
/// @addtogroup pacm
/// @{


#include "icy/pacm/installtask.h"
#include "icy/archo/zipfile.h"
#include "icy/crypto/hash.h"
#include "icy/http/authenticator.h"
#include "icy/http/client.h"
#include "icy/logger.h"
#include "icy/packetio.h"
#include "icy/pacm/package.h"
#include "icy/pacm/packagemanager.h"

#include "icy/filesystem.h"

#include <algorithm>

using namespace std;


namespace icy {
namespace pacm {


InstallTask::InstallTask(PackageManager& manager, LocalPackage* local, RemotePackage* remote,
                         const InstallOptions& options, uv::Loop* loop)
    : _manager(manager)
    , _local(local)
    , _remote(remote)
    , _options(options)
    , _progress(0)
    , _downloading(false)
    , _dlconn(nullptr)
    , _loop(loop)
{
    LTrace("Create");
    if (!valid())
        throw std::runtime_error("Invalid install task configuration");
}


InstallTask::~InstallTask()
{
    LTrace("Destory");

    // :)
}


void InstallTask::start()
{
    STrace << "Starting: Name=" << _local->name()
           << ", Version= " << _options.version
           << ", SDK Version=" << _options.sdkVersion << endl;

    // Prepare environment and install options

    // Check against provided options to make sure that
    // we can proceed with task creation.
    if (!_options.version.empty()) {
        _remote->assetVersion(_options.version); // throw if none
    }
    if (!_options.sdkVersion.empty()) {
        _remote->latestSDKAsset(_options.sdkVersion); // throw if none
    }

    // Set default install directory if none was given
    if (_options.installDir.empty()) {

        // Use the current install dir if the local package already exists
        if (!_local->installDir().empty()) {
            _options.installDir = _local->installDir();
        }

        // Or use the manager default
        else {
            _options.installDir = _manager.options().installDir;
        }
    }

    // Normalize lazy windows paths
    _options.installDir = fs::normalize(_options.installDir);
    _local->setInstallDir(_options.installDir);

    // Create the directory
    fs::mkdirr(_options.installDir);

    // If the package failed previously we might need
    // to clear the file cache.
    if (_manager.options().clearFailedCache)
        _manager.clearPackageCache(*_local);

    _runner.start(std::bind(&InstallTask::run, this));

    // Increment the event loop while the task active
    _runner.handle().ref();
}


void InstallTask::cancel()
{
    setState(this, InstallationState::Cancelled);
}


void InstallTask::run()
{
    try {
        auto local = this->local();
        switch (state().id()) {
            case InstallationState::None:
                setProgress(0);
                doDownload();
                setState(this, InstallationState::Downloading);
                break;
            case InstallationState::Downloading:
                if (_downloading)
                    return; // skip until download completes

                setState(this, InstallationState::Extracting);
                break;
            case InstallationState::Extracting:
                setProgress(75);
                doExtract();
                setState(this, InstallationState::Finalizing);
                break;
            case InstallationState::Finalizing:
                setProgress(90);
                doFinalize();
                local->setState("Installed");
                local->clearErrors();
                local->setInstalledAsset(getRemoteAsset());
                setProgress(100); // set before state change

                // Transition the internal state if finalization was a success.
                // This will complete the installation process.
                setState(this, InstallationState::Installed);
                break;
            case InstallationState::Installed:
                setComplete(); // complete and destroy
                return;
            case InstallationState::Cancelled:
                local->setState("Failed");
                setProgress(100);
                setComplete(); // complete and destroy
                return;
            case InstallationState::Failed:
                local->setState("Failed");
                if (_error.any())
                    local->addError(_error.message);
                setProgress(100); // complete and destroy
                setComplete();
                return;
            default:
                throw std::runtime_error("Unexpected installation state");
        }
    } catch (std::exception& exc) {
        SError << "Installation failed: " << exc.what() << endl;
        _error.message = exc.what();
        setState(this, InstallationState::Failed);
    }
}


void InstallTask::onStateChange(InstallationState& state, const InstallationState& oldState)
{
    SDebug << "State changed: " << oldState << " => " << state << endl;

    // Set the package install task so we know from which state to
    // resume installation.
    // TODO: Should this be reset by the clearFailedCache option?
    local()->setInstallState(state.toString());

    Stateful<InstallationState>::onStateChange(state, oldState);
}


void InstallTask::doDownload()
{
    Package::Asset asset = getRemoteAsset();
    if (!asset.valid())
        throw std::runtime_error(
            "Package download failed: The remote asset is invalid.");

    // If the remote asset already exists in the cache, we can
    // skip the download.

    std::string outfile = _manager.getCacheFilePath(asset.fileName());
    _dlconn = http::Client::instance().createConnection(asset.url(), _loop);
    if (!_manager.options().httpUsername.empty()) {
        http::BasicAuthenticator cred(_manager.options().httpUsername,
                                      _manager.options().httpPassword);
        cred.authenticate(_dlconn->request());
    }

    SDebug << "Initializing download: URL=" << asset.url() << ", File path=" << outfile << endl;

    _dlconn->setReadStream(
        new std::ofstream(outfile, std::ios_base::out | std::ios_base::binary));
    _dlconn->IncomingProgress += slot(this, &InstallTask::onDownloadProgress);
    _dlconn->Complete += slot(this, &InstallTask::onDownloadComplete);
    _dlconn->submit();

    _downloading = true;
}


void InstallTask::onDownloadProgress(const double& progress)
{
    SDebug << "Download progress: " << progress << endl;

    // Progress 1 - 75 covers download
    // Increments of 10 or greater
    int prog = static_cast<int>(progress * 0.75);
    if (prog > 0 && prog > this->progress() + 10)
        setProgress(prog);
}


void InstallTask::onDownloadComplete(const http::Response& response)
{
    SDebug << "Download complete: " << response << endl;
    _dlconn->close();
    _dlconn = nullptr;
    _downloading = false;
}


void InstallTask::doExtract()
{
    setState(this, InstallationState::Extracting);

    Package::Asset asset = getRemoteAsset();
    if (!asset.valid())
        throw std::runtime_error("The package can't be extracted");

    // Get the input file and check veracity
    std::string archivePath(_manager.getCacheFilePath(asset.fileName()));
    if (!fs::exists(archivePath))
        throw std::runtime_error("The local package file does not exist: " + archivePath);
    if (!_manager.isSupportedFileType(asset.fileName()))
        throw std::runtime_error(
            "The local package has an unsupported file extension: " + fs::extname(archivePath));

    // Verify file checksum if one was provided
    std::string originalChecksum(asset.checksum());
    if (!originalChecksum.empty()) {
        std::string computedChecksum(crypto::checksum(
            _manager.options().checksumAlgorithm, archivePath));
        SDebug << "Verify checksum: original=" << originalChecksum
               << ", computed=" << computedChecksum << endl;
        if (originalChecksum != computedChecksum)
            throw std::runtime_error("Checksum verification failed: " + fs::extname(archivePath));
    }

    // Create the output directory
    std::string tempDir(_manager.getPackageDataDir(_local->id()));

    SDebug << "Unpacking archive: " << archivePath << " to " << tempDir << endl;

    // Reset the local installation manifest before extraction
    _local->manifest().root.clear();

    // Decompress the archive
    archo::ZipFile zip(archivePath);
    while (true) {
        // Validate zip entry name to prevent path traversal attacks
        std::string entryName = zip.currentFileName();
        if (entryName.find("..") != std::string::npos)
            throw std::runtime_error("Path traversal detected in archive entry: " + entryName);

        (void)zip.extractCurrentFile(tempDir, true);

        // Add the extracted file to the package install manifest
        // Note: Manifest stores relative paths
        _local->manifest().addFile(entryName);

        if (!zip.goToNextFile())
            break;
    }
}


void InstallTask::doFinalize()
{
    setState(this, InstallationState::Finalizing);

    bool errors = false;
    std::string tempDir(_manager.getPackageDataDir(_local->id()));
    std::string installDir = options().installDir;

    // Ensure the install directory exists
    fs::mkdirr(installDir);
    SDebug << "Finalizing to: " << installDir << endl;

    // Move all extracted files to the installation path
    std::vector<std::string> entries;
    fs::readdir(tempDir, entries);
    for (const auto& source : entries) {
        try {
            std::string target = fs::makePath(installDir, fs::filename(source));

            SDebug << "moving file: " << source << " => " << target << endl;
            fs::rename(source, target);
        } catch (std::exception& exc) {
            // The previous version files may be currently in use,
            // in which case PackageManager::finalizeInstallations()
            // must be called from an external process before the
            // installation can be completed.
            errors = true;
            SError << "finalize error: " << exc.what() << endl;
            _local->addError(exc.what());
        }
    }

    // The package requires finalizing at a later date.
    // The current task will be cancelled, and the package
    // saved with the Installing state.
    if (errors) {
        SDebug << "Finalization failed, cancelling task" << endl;
        cancel();
        return;
    }

    // Remove the temporary output folder if the installation
    // was successfully finalized.
    try {
        SDebug << "Removing temp directory: " << tempDir << endl;
        fs::rmdir(tempDir);
    } catch (std::exception& exc) {
        // While testing on a windows system this fails regularly
        // with a file sharing error, but since the package is already
        // installed we can just swallow it.
        SWarn << "cannot remove temp directory: " << exc.what() << endl;
    }

    SDebug << "finalization complete" << endl;
}


void InstallTask::setComplete()
{
    {
        SInfo << "Package installed: "
              << "Name=" << _local->name() << ", Version=" << _local->version()
              << ", Package State=" << _local->state()
              << ", Package Install State=" << _local->installState() << endl;
#ifdef _DEBUG
        _local->print(cout);
#endif
    }

    // Close the connection
    {
        if (_dlconn)
            _dlconn->close();
    }

    // Cancel the runner and schedule for deletion
    _runner.cancel();

    // The task will be destroyed
    // as a result of this signal.
    Complete.emit(*this);
}


void InstallTask::setProgress(int value)
{
    {
        _progress = value;
    }
    Progress.emit(*this, value);
}


Package::Asset InstallTask::getRemoteAsset() const
{
    return !_options.version.empty()
               ? _remote->assetVersion(_options.version)
           : !_options.sdkVersion.empty()
               ? _remote->latestSDKAsset(_options.sdkVersion)
               : _remote->latestAsset();
}


int InstallTask::progress() const
{
    return _progress;
}


bool InstallTask::valid() const
{
    return !stateEquals(InstallationState::Failed) && _local->valid() &&
           (!_remote || _remote->valid());
}


bool InstallTask::cancelled() const
{
    return stateEquals(InstallationState::Cancelled);
}


bool InstallTask::failed() const
{
    return stateEquals(InstallationState::Failed);
}


bool InstallTask::success() const
{
    return stateEquals(InstallationState::Installed);
}


bool InstallTask::complete() const
{
    return stateEquals(InstallationState::Installed) ||
           stateEquals(InstallationState::Cancelled) ||
           stateEquals(InstallationState::Failed);
}


LocalPackage* InstallTask::local() const
{
    return _local;
}


RemotePackage* InstallTask::remote() const
{
    return _remote;
}


InstallOptions& InstallTask::options()
{
    return _options;
}


uv::Loop* InstallTask::loop() const
{
    return _loop;
}


} // namespace pacm
} // namespace icy


/// @}
