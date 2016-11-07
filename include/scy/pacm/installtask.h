///
//
// LibSourcey
// Copyright (c) 2005, Sourcey <http://sourcey.com>
//
// SPDX-License-Identifier:	LGPL-2.1+
//
/// @addtogroup pacm
/// @{


#ifndef SCY_Pacm_InstallTask_H
#define SCY_Pacm_InstallTask_H


#include "scy/pacm/package.h"
#include "scy/http/client.h"
#include "scy/stateful.h"
#include "scy/logger.h"
#include "scy/idler.h"


namespace scy {
namespace pacm {


class PackageManager;


struct InstallationState: public State
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

    std::string str(unsigned int id) const
    {
        switch(id) {
        case None:                    return "None";
        case Downloading:            return "Downloading";
        case Extracting:            return "Extracting";
        case Finalizing:            return "Finalizing";
        case Installed:                return "Installed";
        case Cancelled:                return "Cancelled";
        case Failed:                return "Failed";
        default: assert(false);
        }
        return "undefined";
    }
};

/// Package installation options.
struct InstallOptions
{
    std::string version;            ///< If set then the given package version will be installed.
    std::string sdkVersion;         ///< If set then the latest package version for given SDK
                                    ///< version will be installed.
    std::string installDir;         ///< Install to the given location, otherwise the manager default
                                    ///< installDir will be used.
    //bool copyManifestToInstallDir;  ///< Copies the local package.json file to the install directory.

    InstallOptions() {
        version = "";
        sdkVersion = "";
        installDir = "";
    }
};


/// This class implements the package installation procedure.
class InstallTask:
    public async::Runnable,
    public Stateful<InstallationState>
{
public:
    typedef std::shared_ptr<InstallTask> Ptr;

    InstallTask(PackageManager& manager,
                LocalPackage* local, RemotePackage* remote,
                const InstallOptions& options = InstallOptions(),
                uv::Loop* loop = uv::defaultLoop());
    virtual ~InstallTask();

    virtual void start();
    virtual void cancel();

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

    virtual Package::Asset getRemoteAsset() const;

    virtual LocalPackage* local() const;
    virtual RemotePackage* remote() const;
    virtual InstallOptions& options();
    virtual uv::Loop* loop() const;

    virtual bool valid() const;
    virtual bool cancelled() const;
    virtual bool failed() const;
    virtual bool success() const;
    virtual bool complete() const;
    virtual int progress() const;

    /// Signals on progress update [0-100].
    Signal<int&> Progress;

    /// Signals on task completion for both
    /// success and failure cases.
    NullSignal Complete;

protected:
    /// Called asynchronously by the thread to
    /// do the work.
    virtual void run();

    virtual void onStateChange(InstallationState& state, const InstallationState& oldState);
    virtual void onDownloadProgress(void* sender, const double& progress);
    virtual void onDownloadComplete(void* sender, const http::Response& response);

    virtual void setProgress(int value);

protected:
    mutable Mutex    _mutex;

    Idler            _runner;
    //Thread          _thread;
    PackageManager& _manager;
    LocalPackage*    _local;
    RemotePackage*    _remote;
    InstallOptions    _options;
    int             _progress;
    bool            _downloading;
    http::ClientConnection::Ptr _dlconn;
    uv::Loop*       _loop;

    friend class PackageManager;
    friend class InstallMonitor;
};


typedef std::vector<InstallTask*> InstallTaskVec;
typedef std::vector<InstallTask::Ptr> InstallTaskPtrVec;


} } // namespace scy::pacm


#endif // SCY_Pacm_InstallTask_H

/// @\}
