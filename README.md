# Pacm

> Simple C++ package manager

[![CI](https://github.com/sourcey/libsourcey/actions/workflows/ci.yml/badge.svg)](https://github.com/sourcey/libsourcey/actions/workflows/ci.yml)

**Repository**: [https://github.com/sourcey/libsourcey](https://github.com/sourcey/libsourcey)
**Dependencies**: [LibSourcey (base, net, json, http, archo, crypto)](https://github.com/sourcey/libsourcey)
**Licence**: LGPL-2.1+

## Installing

Pacm is part of the [LibSourcey](https://github.com/sourcey/libsourcey) project. To build:

~~~ bash
git clone --recurse-submodules https://github.com/sourcey/libsourcey.git
cd libsourcey
cmake -B build -DBUILD_MODULE_pacm=ON -DBUILD_TESTS=ON
cmake --build build
~~~

Pacm can also be used as an external module by cloning it into the LibSourcey `src` folder, where it will be auto-discovered by the build system.

## Overview

Pacm is your solution for a simple C++ package manager that can be embedded and redistributed with native applications. Pacm is designed to make short work of the following tasks:

* Auto-Updates
* Front-end Package Management
* Version Constrained Package Management
* Dependency Management
* Remote Plugin Management

Pacm should be familiar territory if you've ever used `rubygems` in Ruby, or `npm` in NodeJS. Basically, a [package list](#client-server-protocol) is downloaded from the server in JSON format, and the client then [issues commands](#supported-commands) to manage packages installed on the local system. You can work with the API one of two ways; by [embedding Pacm](#embedding-pacm) and compiling it with your application; or by redistributing it with your application and calling the [Pacm command line tool](#redistributable-command-line-tool) directly.

The Pacm code base is kept small and readable thanks to LibSourcey, which abstracts and handles complex cross-platform tasks such as networking, filesystem and cryptography. LibSourcey is built on top of `libuv`, and provides a modern C++20 interface for the native Pacm API.

There are a lot of open source [package managers](http://en.wikipedia.org/wiki/List_of_software_package_management_systems) out there, but there has been great need of a simple embeddable package manager in C++ for some time. For this reason we built Pacm, and we're pleased to contribute it to the open source community.

## Embedding Pacm

Pacm is written in simple and readable C++ code, so if you have some basic coding nouse then you'll be all over it like Barry White on a waterbed covered in hamburgers.

The example below shows how to use the C++ API to query, list, install and uninstall packages:

~~~ cpp
pacm::PackageManager::Options options;
// configuration options go here...
pacm::PackageManager pm(options);
pm.initialize();

// query the server for the latest packages
pm.queryRemotePackages();

// list local (installed) packages
for (auto& kv : pm.localPackages().map()) {
    std::cout << "Local package: "
    << kv.first << "\n"
    << kv.second->toString() << std::endl;
}

// list remote (available) packages
for (auto& kv : pm.remotePackages().map()) {
    std::cout << "Remote package: "
    << kv.first << "\n"
    << kv.second->toString() << std::endl;
}

// install a package
pm.installPackage("SomePackageName");

// update a package (will install if it doesn't exist)
pm.updatePackage("SomePackageName");

// update / install a list of packages.
// package states are available via callback events
std::vector<std::string> packages;
packages.push_back("SomePackageName");
packages.push_back("SomeOtherPackage");
pm.installPackages(packages);

// update all packages
pm.updateAllPackages();

// uninstall a package.
// uninstallPackages() can also be used for multiple packages.
pm.uninstallPackage("SomePackageName");
~~~

If you're planning on using the native API then the best place to start is the source code of the [pacm-cli command line tool](apps/pacm-cli/src/main.cpp), which can be easily reverse engineered and modified for your own purposes.

For all method definitions and further documentation the [source code](include/scy/pacm/packagemanager.h) is always the best reference.

## Redistributable Command Line Tool

Pacm comes with a redistributable command-line tool which can be compiled and redistributed with your existing applications. In many ways this is preferable to embedding Pacm as it allows you to decouple complex version management code from your main application, but depending on which platforms you are targeting there may also be security limitations to consider.

### Examples

Print help:

~~~ bash
pacm-cli -help
~~~

Install a package:

~~~ bash
pacm-cli -endpoint https://packages.example.com -uri /packages.json -print -install myplugin
~~~

Update all installed packages to the latest version:

~~~ bash
pacm-cli -endpoint https://packages.example.com -uri /packages.json -print -update
~~~

### Supported Commands

The following commands are currently supported by the Pacm console application:

~~~
General commands:
  -help           Print help
  -logfile        Log file output path

Server commands:
  -endpoint       HTTP server endpoint
  -uri            HTTP server package JSON index URI

Package commands:
  -install        Packages to install (comma separated)
  -uninstall      Packages to uninstall (comma separated)
  -update         Update all packages
  -print          Print all local and remote packages on exit
  -checksum-alg   Checksum algorithm for verifying packages (SHA256)

Filesystem commands:
  -install-dir    Directory where packages will be installed
  -data-dir       Directory where package manifests will be stored
  -temp-dir       Directory where intermediate package files will be stored
~~~

## Client-Server Protocol

The Pacm repository does not currently include a server module, but since everything is in JSON it should be a sinch to implement using your existing web framework. All that is required on the server-side is to list packages and facilitate downloads.

A server response to a Pacm query is illustrated below, and remember that since everything is in JSON you can easily add your own metadata as required.

Pacm will send a HTTP GET request to the server:

~~~ http
GET /packages.json HTTP/1.1
~~~

The server responds with an array of packages and available file assets in JSON format:

~~~ json
[{
    "id": "myplugin",
    "type": "Plugin",
    "name": "My Plugin",
    "author": "Example",
    "description": "An example plugin package.",
    "assets": [{
        "version": "1.0.0",
        "sdk-version": "2.0.0",
        "platform": "linux",
        "checksum": "e4d909c290d0fb1ca068ffaddf22cbd0",
        "file-name": "myplugin-1.0.0-sdk-2.0.0-linux.zip",
        "file-size": 432321,
        "mirrors": [{
            "url": "https://packages.example.com/myplugin-1.0.0-sdk-2.0.0-linux.zip"
        }]
    }]
}]
~~~

## Contributing

If you improve on the code base and want to contribute to the project then pull requests are always very welcome.

1. [Fork LibSourcey on Github](https://github.com/sourcey/libsourcey)
2. Create your feature branch (`git checkout -b my-new-feature`)
3. Commit your changes (`git commit -am 'Add some feature'`)
4. Push to the branch (`git push origin my-new-feature`)
5. Create new Pull Request
