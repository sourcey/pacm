///
//
// LibSourcey
// Copyright (c) 2005, Sourcey <http://sourcey.com>
//
// SPDX-License-Identifier:	LGPL-2.1+
//
/// @addtogroup pacm
/// @{


#ifndef SCY_Pacm_InstallMonitor_H
#define SCY_Pacm_InstallMonitor_H


#include "scy/pacm/installtask.h"


namespace scy {
namespace pacm {


typedef std::vector<LocalPackage*> LocalPackageVec;


class InstallMonitor
{
public:
    InstallMonitor();
    virtual ~InstallMonitor();

    /// Adds a task to monitor.
    virtual void addTask(InstallTask::Ptr task);

    /// Starts all monitored tasks.
    virtual void startAll();

    /// Cancels all monitored tasks.
    virtual void cancelAll();

    /// Returns true if all install tasks have completed,
    /// either successfully or unsuccessfully.
    virtual bool isComplete() const;

    /// Returns the list of monitored package tasks.
    virtual InstallTaskPtrVec tasks() const;

    /// Returns the list of monitored packages.
    virtual LocalPackageVec packages() const;

    /// Proxies state change events from managed packages
    Signal<void(InstallTask&, const InstallationState&, const InstallationState&)> InstallStateChange;

    /// Signals when a managed install task completes.
    Signal<void(LocalPackage&)> InstallComplete;

    /// Signals on overall progress update [0-100].
    Signal<void(int&)> Progress;

    /// Signals on all tasks complete.
    Signal<void(LocalPackageVec&)> Complete;

protected:
    virtual void onInstallStateChange(void* sender, InstallationState& state, const InstallationState& oldState);    // Called when a monitored install task completes.
    virtual void onInstallComplete(InstallTask& task);

    virtual void setProgress(int value);

protected:
    mutable Mutex    _mutex;
    InstallTaskPtrVec _tasks;
    LocalPackageVec _packages;
    int _progress;
};


/// Returns a comma delimited package name string.
inline std::string getInstallTaskNamesString(LocalPackageVec& packages)
{
    std::string names;
    for (auto it = packages.begin(); it != packages.end();) {
        names += (*it)->name();
        ++it;
        if (it != packages.end())
            names += ", ";
    }
    return names;
}


} } // namespace scy::pacm


#endif // SCY_Pacm_InstallMonitor_H

/// @\}
