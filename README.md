# Pacm

**Homepage**: [http://sourcey.com/pacm](http://sourcey.com/pacm)  
**Licence**: LGPL  
**Dependencies**: [LibSourcey (base, uv, net, json, crypto, archo, http)](http://sourcey.com/libsourcey)  
    
Pacm is your solution for a simple front-end package manager that can be easily embedded and redistributed with native C++ applications. Pacm is designed to make short work of the following tasks:

* Auto-Updates
* Version Constrained Package Management
* Dependency Management
* Remote Plugin Management

The Pacm API should a familiar territory if you're familiar with Ruby's `rubygems` and NodeJS's `npm`. Basically, a [package list](#client-server-protocol) is downloaded from the server in JSON format, and the client then [issues commands](#supported-commands) to manage packages installed on the local system. You can work with the API one of two ways; you can [embed Pacm](#embedding-pacm) by including and compiling the source into your application; or you can just call the [redistributable Pacm binary](#redistributable-command-line-tool) directly.

The Pacm code base is kept small and readable thanks to LibSourcey, which abstracts and handles complex cross-platform tasks such as networking, filesystem and cryptography. LibSourcey is built on to of `libuv`, NodeJS's super fast networking layer, and provides a modern C++ interface for the native Pacm API.

There are a lot of open source [package managers](http://en.wikipedia.org/wiki/List_of_software_package_management_systems) out there, but there has been great need of a simple embeddable package manager in C++ for some time. We built Pacm for this reason, and we're pleased to contribute it to the open source community.

## Embedding Pacm

Pacm is written in simple, readable C++11 code, so if you have some basic coding nouse then you'll be all over it like Barry White on a waterbed full of hamburgers. 

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
    std::cout << "Local package: " 
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

If you're planning on using the native API then the best place to start is the source code of the [Pacm command line tool](https://github.com/sourcey/pacm/blob/master/applications/pacmconsole/src/main.cpp), which can be easily reverse engineered and modified for your own purposes.

For all method definitions and further documentation the [source code](https://github.com/sourcey/pacm/blob/master/include/scy/pacm/packagemanager.h) is always the best reference.

## Redistributable Command Line Tool

Pacm comes with a redistributable command-line tool which can be compiled and redistributed with your existing applications. In many ways this is preferable to embedding Pacm as it allows you to decouple complex version management code from your main application, but depending on which platforms you are targeting there may also be security limitations you need to consider.

### Examples

Print help:

~~~ bash
pacm -help
~~~

Install the Anionu `surveillancemodeplugin`:

~~~ bash
pacm -endpoint https://anionu.com -uri /packages.json -print -install surveillancemodeplugin
~~~

Uninstall the Anionu `surveillancemodeplugin`:

~~~ bash
pacm -endpoint https://anionu.com -uri /packages.json -print -install surveillancemodeplugin
~~~

Update all installed packages to the latest version:

~~~ bash
pacm -endpoint https://anionu.com -uri /packages.json -print -update
~~~

### Supported Commands

The following commands are implemented by the Pacm application:

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
  -checksum-alg   Checksum algorithm for verifying packages (MDS/SHA1)

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

The server responds with an array of packages and available file assets in JSON format like so:

~~~ javascript
[{
    "id": "surveillancemodeplugin",
    "type": "Plugin",
    "name": "Surveillance Mode Plugin",
    "author": "Sourcey",
    "description": "Surveillance mode provides Spot with real-time motion detection capabilities. Surveillance mode is ideal for when you are away from the surveilled premises, and want to protect yourself against unwanted intruders. You can also configure SMS alerts and record videos during intervals of motion.",
    "assets": [{
        "version": "0.9.3",
        "sdk-version": "0.6.2",
        "platform": "win32",
        "checksum": "e4d909c290d0fb1ca068ffaddf22cbd0",
        "file-name": "surveillancemodeplugin-0.9.4-sdk-0.6.2-win32.zip",
        "file-size": 432321,
        "mirrors": [{
            "url": "https://anionu.com/packages/surveillancemodeplugin/download/surveillancemodeplugin-0.9.4-sdk-0.6.2-win32.zip"
        }]
    }, {
        "version": "0.9.3",
        "sdk-version": "0.6.0",
        "platform": "win32",
        "checksum": "c290d0fb1ca068ffaddf22cbd0e4d909",
        "file-name": "surveillancemodeplugin-0.9.3-sdk-0.6.0-win32-debug.zip",
        "file-size": 432221,
        "mirrors": [{
            "url": "https://anionu.com/packages/surveillancemodeplugin/download/surveillancemodeplugin-0.9.3-sdk-0.6.0-win32-debug.zip"
        }]
    }]
}, {
    "id": "mediaplugin",
    "type": "Plugin",
    "name": "Media Plugin",
    "author": "Sourcey",
    "description": "The Media Plugin implements audio and video encoders for recording and real-time media streaming. If you want to enable different media formats in Spot, you can do so by modifying this plugin.",
    "assets": [{
        "version": "0.8.9",
        "sdk-version": "0.6.2",
        "platform": "win32",
        "checksum": "fb1ca068ffaddf22cbd0e4d909c290d0",
        "file-name": "mediaplugin-0.8.9-sdk-0.6.2-win32.zip",
        "file-size": 1352888,
        "mirrors": [{
            "url": "https://anionu.com/packages/mediaplugin/download/mediaplugin-0.8.9-sdk-0.6.2-win32.zip"
        }]
    }, {
        "version": "0.8.8",
        "sdk-version": "0.6.0",
        "platform": "win32",
        "checksum": "068ffaddf22cbd0e4d909c290d0fb1ca",
        "file-name": "mediaplugin-0.8.8-sdk-0.6.0-win32-debug.zip",
        "file-size": 1352818,
        "mirrors": [{
            "url": "https://anionu.com/packages/mediaplugin/download/mediaplugin-0.8.8-sdk-0.6.0-win32-debug.zip"
        }]
    }]
}, {
    "id": "webrtcplugin",
    "type": "Plugin",
    "name": "WebRTC Plugin",
    "author": "Sourcey",
    "description": "This plugin provides Spot with WebRTC support, so you can view high quality video surveillance streams in a modern web browser.",
    "assets": [{
        "version": "0.1.1",
        "sdk-version": "0.6.2",
        "platform": "win32",
        "checksum": "068ffaddc290d0fb1caf22cbd0e4d909",
        "file-name": "webrtcplugin-0.1.1-sdk-0.6.2-win32.zip",
        "file-size": 0,
        "mirrors": [{
            "url": "https://anionu.com/packages/webrtcplugin/download/webrtcplugin-0.1.1-sdk-0.6.2-win32.zip"
        }]
    }, {
        "version": "0.1.0",
        "sdk-version": "0.6.0",
        "platform": "win32",
        "checksum": "addc290d0fb1caf22cbd0e4d909068ff",
        "file-name": "webrtcplugin-0.1.0-sdk-0.6.0-win32-debug.zip",
        "file-size": 3888157,
        "mirrors": [{
            "url": "https://anionu.com/packages/webrtcplugin/download/webrtcplugin-0.1.0-sdk-0.6.0-win32-debug.zip"
        }]
    }]
}]
~~~    

## Contributing

If you improve on the code base and want to contribute to the project then pull requests are always very welcome.

1. [Fork Pacm on Github](https://github.com/sourcey/pacm)
2. Create your feature branch (`git checkout -b my-new-feature`)
3. Commit your changes (`git commit -am 'Add some feature'`)
4. Push to the branch (`git push origin my-new-feature`)
5. Create new Pull Request

## Issues

If you find any bugs or issues please use the new [Github issue tracker](https://github.com/sourcey/pacm/issues).