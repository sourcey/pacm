# Pacm

Embeddable package manager for native applications, extensions, and packaged payloads.

- Namespace: `icy::pacm`
- CMake target: `icey::pacm`
- Primary headers: `include/icy/pacm/packagemanager.h`, `package.h`, `installtask.h`, `installmonitor.h`
- Directory layout: `include/` for the public API, `src/` for package/install logic, `apps/` for `pacm-cli`, `tests/` for metadata and lifecycle coverage

Pacm owns package delivery and install state:

- fetch package index JSON over HTTP
- compare local vs remote package state
- download and verify archives
- extract payloads through `archo`
- finalize installs into the target directory

The package format is generic, but it now has first-class extension metadata so installed payloads can describe:

- loader (`graft`, future worker launchers, etc.)
- runtime kind (`native`, `worker`)
- install-relative entrypoint
- ABI version
- declared capabilities

That makes `pacm` the distribution layer while `graft` handles native shared-library binding after install.

Read next:
- [Pacm module guide](../../docs/modules/pacm.md)
- [Graft module guide](../../docs/modules/graft.md)
- [Archo module guide](../../docs/modules/archo.md)
