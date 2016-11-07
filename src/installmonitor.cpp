///
//
// LibSourcey
// Copyright (c) 2005, Sourcey <http://sourcey.com>
//
// SPDX-License-Identifier:	LGPL-2.1+
//
/// @addtogroup pacm
/// @{


#include "scy/pacm/installmonitor.h"
#include "scy/pacm/packagemanager.h"
#include "scy/logger.h"


using namespace std;


namespace scy {
namespace pacm {


InstallMonitor::InstallMonitor()
{
}


InstallMonitor::~InstallMonitor()
{
}


void InstallMonitor::onInstallStateChange(void* sender, InstallationState& state, const InstallationState& oldState)
{
    auto task = reinterpret_cast<InstallTask*>(sender);

    DebugL << "[InstallMonitor] onInstallStateChange: " << task << ": " << state << endl;

    InstallStateChange.emit(this, *task, state, oldState);
}


void InstallMonitor::onInstallComplete(void* sender)
{
    auto task = reinterpret_cast<InstallTask*>(sender);

    DebugL << "[InstallMonitor] Package Install Complete: " << task->state().toString() << endl;

    // Notify listeners when each package completes.
    InstallComplete.emit(this, *task->local());

    int progress = 0;
    {
        Mutex::ScopedLock lock(_mutex);

        // Remove the package task reference.
        for (auto it = _tasks.begin(); it != _tasks.end(); it++) {
            if (task == it->get()) {
                task->StateChange -= sdelegate(this, &InstallMonitor::onInstallStateChange);
                task->Complete -= sdelegate(this, &InstallMonitor::onInstallComplete);
                _tasks.erase(it);
                break;
            }
        }

        progress = (_packages.size() - _tasks.size()) / _packages.size();

        InfoL << "[InstallMonitor] Waiting on "
            << _tasks.size() << " packages to complete" << endl;
    }

    // Set progress
    setProgress(progress);

    if (isComplete())
        Complete.emit(this, _packages);
}


void InstallMonitor::addTask(InstallTask::Ptr task)
{
    Mutex::ScopedLock lock(_mutex);
    if (!task->valid())
        throw std::runtime_error("Invalid package task");
    _tasks.push_back(task);
    _packages.push_back(task->_local);
    task->StateChange += sdelegate(this, &InstallMonitor::onInstallStateChange);
    task->Complete += sdelegate(this, &InstallMonitor::onInstallComplete);
}


void InstallMonitor::startAll()
{
    Mutex::ScopedLock lock(_mutex);
    for (auto it = _tasks.begin(); it != _tasks.end(); it++)
        (*it)->start();
}


void InstallMonitor::cancelAll()
{
    Mutex::ScopedLock lock(_mutex);
    for (auto it = _tasks.begin(); it != _tasks.end(); it++)
        (*it)->cancel();
}


void InstallMonitor::setProgress(int value)
{
    {
        Mutex::ScopedLock lock(_mutex);
        _progress = value;
    }
    Progress.emit(this, value);
}


InstallTaskPtrVec InstallMonitor::tasks() const
{
    Mutex::ScopedLock lock(_mutex);
    return _tasks;
}


LocalPackageVec InstallMonitor::packages() const
{
    Mutex::ScopedLock lock(_mutex);
    return _packages;
}


bool InstallMonitor::isComplete() const
{
    Mutex::ScopedLock lock(_mutex);
    return _tasks.empty();
}


} } // namespace scy::pacm

/// @\}
