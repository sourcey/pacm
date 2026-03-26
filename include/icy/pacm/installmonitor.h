///
//
// Icey
// Copyright (c) 2005, Icey <https://0state.com>
//
// SPDX-License-Identifier: LGPL-2.1+
//
/// @addtogroup pacm
/// @{


#pragma once


#include "icy/pacm/installtask.h"


namespace icy {
namespace pacm {


/// Vector of local package pointers used by install monitor progress snapshots.
using LocalPackageVec = std::vector<LocalPackage*>;


/// Aggregates multiple install tasks and reports overall progress.
class Pacm_API InstallMonitor
{
public:
    InstallMonitor();
    virtual ~InstallMonitor() noexcept;

    InstallMonitor(const InstallMonitor&) = delete;
    InstallMonitor& operator=(const InstallMonitor&) = delete;
    InstallMonitor(InstallMonitor&&) = delete;
    InstallMonitor& operator=(InstallMonitor&&) = delete;

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
    ThreadSignal<void(InstallTask&, const InstallationState&,
                      const InstallationState&)>
        InstallStateChange;

    /// Signals when a managed install task completes.
    ThreadSignal<void(LocalPackage&)> InstallComplete;

    /// Signals on overall progress update [0-100].
    ThreadSignal<void(int&)> Progress;

    /// Signals on all tasks complete.
    ThreadSignal<void(LocalPackageVec&)> Complete;

protected:
    virtual void onInstallStateChange(
        void* sender, InstallationState& state,
        const InstallationState&
            oldState); // Called when a monitored install task completes.
    virtual void onInstallComplete(InstallTask& task);

    virtual void setProgress(int value);

protected:
    mutable std::mutex _mutex;
    InstallTaskPtrVec _tasks;
    LocalPackageVec _packages;
    int _progress;
};


/// Returns a comma-delimited string of display names from @p packages.
/// @param packages Vector of LocalPackage pointers to format.
/// @return Comma-separated name string, e.g. "PluginA, PluginB".
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


} // namespace pacm
} // namespace icy


/// @}
