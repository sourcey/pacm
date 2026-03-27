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


#include "icy/http/client.h"
#include "icy/idler.h"
#include "icy/logger.h"
#include "icy/pacm/config.h"
#include "icy/pacm/package.h"
#include "icy/stateful.h"


namespace icy {
namespace pacm {


class Pacm_API PackageManager;


/// State machine states for package installation.
struct InstallationState : public State
{
    enum Type
    {
        None = 0,
        Downloading,
        Extracting,
        Finalizing,
        Installed,
        Cancelled,
        Failed
    };

    /// Converts a state ID to its string representation.
    /// @param id One of the Type enum values.
    /// @return Human-readable state name, or "undefined" for unknown values.
    std::string str(unsigned int id) const
    {
        switch (id) {
            case None:
                return "None";
            case Downloading:
                return "Downloading";
            case Extracting:
                return "Extracting";
            case Finalizing:
                return "Finalizing";
            case Installed:
                return "Installed";
            case Cancelled:
                return "Cancelled";
            case Failed:
                return "Failed";
            default:
                return "undefined";
        }
    }
};

/// Package installation options.
struct InstallOptions
{
    std::string version;    ///< If set then the given package version will be installed.
    std::string sdkVersion; ///< If set then the latest package version for given SDK
                            ///< version will be installed.
    std::string installDir; ///< Install to the given location, otherwise the
                            ///< manager default `installDir` will be used.

    InstallOptions()
    {
        version = "";
        sdkVersion = "";
        installDir = "";
    }
};


/// Downloads, extracts, and finalizes a single package installation.
class Pacm_API InstallTask : public basic::Runnable
    , public Stateful<InstallationState>
{
public:
    using Ptr = std::shared_ptr<InstallTask>;

    /// @param manager Owning PackageManager instance.
    /// @param local   Local package record (must not be null).
    /// @param remote  Remote package record to install from (may be null for local-only ops).
    /// @param options Version and path overrides for this installation.
    /// @param loop    libuv event loop to use for async HTTP downloads.
    /// @throws std::runtime_error if the task configuration is invalid.
    InstallTask(PackageManager& manager, LocalPackage* local, RemotePackage* remote,
                const InstallOptions& options = InstallOptions(),
                uv::Loop* loop = uv::defaultLoop());
    virtual ~InstallTask() noexcept;

    InstallTask(const InstallTask&) = delete;
    InstallTask& operator=(const InstallTask&) = delete;
    InstallTask(InstallTask&&) = delete;
    InstallTask& operator=(InstallTask&&) = delete;

    /// Validates options, resolves the install directory, and launches the background runner.
    /// @throws std::runtime_error if the requested version or SDK version asset is unavailable.
    virtual void start();

    /// Transitions the task to the Cancelled state.
    void cancel(bool flag = true) override;

    /// Downloads the package archive from the server.
    virtual void doDownload();

    /// Extracts the downloaded package files
    /// to the intermediate directory.
    virtual void doExtract();

    /// Moves extracted files from the intermediate
    /// directory to the installation directory.
    virtual void doFinalize();

    /// Called when the task completes either
    /// successfully or in error.
    /// This will trigger destruction.
    virtual void setComplete();

    /// Returns the remote asset selected by the current InstallOptions.
    /// Respects version and sdkVersion overrides; falls back to latestAsset().
    virtual Package::Asset getRemoteAsset() const;

    /// Returns a pointer to the local package record.
    virtual LocalPackage* local() const;

    /// Returns a pointer to the remote package record.
    virtual RemotePackage* remote() const;

    /// Returns a read-only view of the installation options for this task.
    [[nodiscard]] virtual const InstallOptions& options() const;

    /// Returns the libuv event loop used for async operations.
    virtual uv::Loop* loop() const;

    /// Returns true if the task is not in a Failed state and both local and remote
    /// (if set) packages are valid.
    virtual bool valid() const;

    /// Returns true if the task is in the Cancelled state.
    bool cancelled() const override;

    /// Returns true if the task is in the Failed state.
    virtual bool failed() const;

    /// Returns true if the task is in the Installed (success) state.
    virtual bool success() const;

    /// Returns true if the task has reached a terminal state
    /// (Installed, Cancelled, or Failed).
    virtual bool complete() const;

    /// Returns the current progress value in the range [0, 100].
    virtual int progress() const;

    /// Signals on progress update [0-100].
    Signal<void(InstallTask&, int&)> Progress;

    /// Signals on task completion for both
    /// success and failure cases.
    Signal<void(InstallTask&)> Complete;

protected:
    /// Called asynchronously by the thread to do the work.
    void run() override;

    void onStateChange(InstallationState& state,
                       const InstallationState& oldState) override;
    virtual void onDownloadProgress(const double& progress);
    virtual void onDownloadComplete(const http::Response& response);

    virtual void setProgress(int value);

protected:
    mutable std::mutex _mutex;

    Idler _runner;
    icy::Error _error;
    PackageManager& _manager;
    LocalPackage* _local;
    RemotePackage* _remote;
    InstallOptions _options;
    int _progress;
    bool _downloading;
    http::ClientConnection::Ptr _dlconn;
    uv::Loop* _loop;

    friend class PackageManager;
    friend class InstallMonitor;
};


/// Vector of raw install task pointers used for transient iteration.
using InstallTaskVec = std::vector<InstallTask*>;

/// Vector of shared install task handles retained across async workflows.
using InstallTaskPtrVec = std::vector<InstallTask::Ptr>;


} // namespace pacm
} // namespace icy


/// @}
