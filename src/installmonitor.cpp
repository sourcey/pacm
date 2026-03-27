///
//
// icey
// Copyright (c) 2005, icey <https://0state.com>
//
// SPDX-License-Identifier: LGPL-2.1+
//
/// @addtogroup pacm
/// @{


#include "icy/pacm/installmonitor.h"
#include "icy/logger.h"
#include "icy/pacm/packagemanager.h"


using namespace std;


namespace icy {
namespace pacm {


InstallMonitor::InstallMonitor()
{
}


InstallMonitor::~InstallMonitor() noexcept
{
}


void InstallMonitor::onInstallStateChange(void* sender,
                                          InstallationState& state,
                                          const InstallationState& oldState)
{
    auto task = reinterpret_cast<InstallTask*>(sender);

    SDebug << "onInstallStateChange: " << task << ": " << state << endl;

    InstallStateChange.emit(*task, state, oldState);
}


void InstallMonitor::onInstallComplete(InstallTask& task)
{
    // auto task = reinterpret_cast<InstallTask*>(sender);

    SDebug << "Package Install Complete: " << task.state().toString() << endl;

    // Notify listeners when each package completes.
    InstallComplete.emit(*task.local());

    int progress = 0;
    {
        std::lock_guard<std::mutex> guard(_mutex);

        // Remove the package task reference.
        for (auto it = _tasks.begin(); it != _tasks.end(); it++) {
            if (&task == it->get()) {
                task.StateChange -=
                    slot(this, &InstallMonitor::onInstallStateChange);
                task.Complete -= slot(this, &InstallMonitor::onInstallComplete);
                _tasks.erase(it);
                break;
            }
        }

        if (!_packages.empty())
            progress = static_cast<int>((_packages.size() - _tasks.size()) * 100 / _packages.size());

        SInfo << "Waiting on " << _tasks.size() << " packages to complete"
              << endl;
    }

    // Set progress
    setProgress(progress);

    if (isComplete())
        Complete.emit(_packages);
}


void InstallMonitor::addTask(InstallTask::Ptr task)
{
    std::lock_guard<std::mutex> guard(_mutex);
    if (!task->valid())
        throw std::runtime_error("Invalid package task");
    _tasks.push_back(task);
    _packages.push_back(task->_local);
    task->StateChange += slot(this, &InstallMonitor::onInstallStateChange);
    task->Complete += slot(this, &InstallMonitor::onInstallComplete);
}


void InstallMonitor::startAll()
{
    std::lock_guard<std::mutex> guard(_mutex);
    for (auto& task : _tasks)
        task->start();
}


void InstallMonitor::cancelAll()
{
    std::lock_guard<std::mutex> guard(_mutex);
    for (auto& task : _tasks)
        task->cancel();
}


void InstallMonitor::setProgress(int value)
{
    {
        std::lock_guard<std::mutex> guard(_mutex);
        _progress = value;
    }
    Progress.emit(value);
}


InstallTaskPtrVec InstallMonitor::tasks() const
{
    std::lock_guard<std::mutex> guard(_mutex);
    return _tasks;
}


LocalPackageVec InstallMonitor::packages() const
{
    std::lock_guard<std::mutex> guard(_mutex);
    return _packages;
}


bool InstallMonitor::isComplete() const
{
    std::lock_guard<std::mutex> guard(_mutex);
    return _tasks.empty();
}


} // namespace pacm
} // namespace icy


/// @}
