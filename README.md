# Pacm

**Homepage**: [http://sourcey.com/pacm](http://sourcey.com/pacm)  
**Licence**: LGPL  
**Dependencies**: [LibSourcey (base, uv, net, json, crypto, archo, http)](http://sourcey.com/libsourcey)  
    
Pacm is a front-end package manager written in C++ that can be embedded or redistributed with native applications to make short work of the following tasks:

* Auto-Updates
* Version Constrained Package Management
* Dependency Management
* Remote Plugin Management

There are a lot of open source [package managers](http://en.wikipedia.org/wiki/List_of_software_package_management_systems) out there, but none of them quite fit the bill for a minimal and flexible native package manager that could be easily distributed with our apps, so we built Pacm.

The Pacm codebase is kept quite small and readable thanks to LibSourcey which handles and abstracts all the complex cross-platform tasks such as networking, filesystem and cryptography.

Pacm speaks to the server using a basic [client-server JSON protocol](#client-server-protocol) which is very straight forward to integrate with your existing web server architecture. 

## Embedding Pacm

If you have some basic C++ nouse then you'll discover that Pacm is very easy to embed and use within your own applications. Some examples of the Pacm API are illustrated below:

~~~ cpp
PackageManager::Options options;
// configuration options go here...
PackageManager pacm(options);
pacm.initialize();
        
// query the server for the latest packages
pacm.queryRemotePackages();                
        
// list local (installed) packages
auto litems = pacm.localPackages().items();
for (auto it = litems.begin(); it != litems.end(); ++it) {				
  std::cout << "Package: " << it->first << std::endl;
}

// list remote (available) packages
auto ritems = app.remotePackages().items();
for (auto it = ritems.begin(); it != ritems.end(); ++it) {				
  std::cout << "Package: " << it->first << std::endl;          
  
  // output the latest asset information
  it->second->latestAsset().print(std::cout);	
}                
        
// install a package
pacm.installPackage("SomePackageName");

// update a package (will install if it doesn't exist)
pacm.updatePackage("SomePackageName");

// update / install a list of packages.
// package states are available via callback events
std::vector<std::string> packages;
packages.push_back("SomePackageName");
packages.push_back("SomeOtherPackage");        
pacm.installPackages(packages);

// update all packages
pacm.updateAllPackages();

// uninstall a package.
// uninstallPackages() can also be used for multiple packages.
pacm.uninstallPackage("SomePackageName");
~~~ 

That covers basic Pacm usage. These are plenty of other methods and features available, just check out the documentation for the method definitions in the [source code](https://github.com/sourcey/pacm/blob/master/include/scy/pacm/packagemanager.h) for a reference.

Another great place to start when working with the native API is the [Pacm command line tool](https://github.com/sourcey/pacm/blob/master/applications/pacmconsole/src/main.cpp) which can be easily reverse engineered and modified for your own purposes.

## Redistributable Command Line Tool

Pacm comes with a redistributable command-line tool which can be compiled and redistributed with your existing applications. In many ways this is preferable to embedding Pacm as it allows you to decouple complex version management code from your main application.

### Examples

Print help:

~~~ bash
pacm --help
~~~

Install the Anionu `surveillancemodeplugin`:

~~~ bash
pacm --endpoint https://anionu.com --uri /packages.json --print --install surveillancemodeplugin
~~~

Uninstall the Anionu `surveillancemodeplugin`:

~~~ bash
pacm --endpoint https://anionu.com --uri /packages.json --print --install surveillancemodeplugin
~~~

Update installed packages to the latest version:

~~~ bash
pacm --endpoint https://anionu.com --uri /packages.json --print --update
~~~

### Supported Commands

The following commands are implemented by the redistributable application. This is likely to be expanded upon in the near future, and in the mean time pull requests are very welcome:

~~~
General commands:

  --help           Print help
  --logfile        Log file output path

Server commands:

  --endpoint       HTTP server endpoint
  --uri            HTTP server package JSON index URI

Package commands:

  --install        Packages to install (comma separated)
  --uninstall      Packages to uninstall (comma separated)
  --update         Update all packages
  --print          Print all local and remote packages on exit
  --checksum-alg   Checksum algorithm for verifying packages (MDS/SHA1)

Filesystem commands:

  --install-dir    Directory where packages will be installed
  --data-dir       Directory where package manifests will be stored
  --temp-dir       Directory where intermediate package files will be stored
~~~

## Client-Server Protocol

The Pacm repository does not currently include a server module, but since everything is in JSON it should be a sinch to implement using your existing web framework. All that is required on the server-side is to list packages and facilitate downloads.

An example of a server response to a Pacm query is illustrated below. Pacm will send a HTTP GET request to the server which will reply with a JSON response that looks like this:
      
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
        "file-name": "surveillancemodeplugin-0.9.4-sdk-0.6.2-win32.zip",
        "file-size": 432321,
        "mirrors": [{
            "url": "https://anionu.com/packages/surveillancemodeplugin/download/surveillancemodeplugin-0.9.4-sdk-0.6.2-win32.zip"
        }]
    }, {
        "version": "0.9.3",
        "sdk-version": "0.6.0",
        "platform": "win32",
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
        "file-name": "mediaplugin-0.8.9-sdk-0.6.2-win32.zip",
        "file-size": 1352888,
        "mirrors": [{
            "url": "https://anionu.com/packages/mediaplugin/download/mediaplugin-0.8.9-sdk-0.6.2-win32.zip"
        }]
    }, {
        "version": "0.8.8",
        "sdk-version": "0.6.0",
        "platform": "win32",
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
        "file-name": "webrtcplugin-0.1.1-sdk-0.6.2-win32.zip",
        "file-size": 0,
        "mirrors": [{
            "url": "https://anionu.com/packages/webrtcplugin/download/webrtcplugin-0.1.1-sdk-0.6.2-win32.zip"
        }]
    }, {
        "version": "0.1.0",
        "sdk-version": "0.6.0",
        "platform": "win32",
        "file-name": "webrtcplugin-0.1.0-sdk-0.6.0-win32-debug.zip",
        "file-size": 3888157,
        "mirrors": [{
            "url": "https://anionu.com/packages/webrtcplugin/download/webrtcplugin-0.1.0-sdk-0.6.0-win32-debug.zip"
        }]
    }]
}]
~~~    

## Contributing

1. [Fork Pacm on Github](https://github.com/sourcey/pacm)
2. Create your feature branch (`git checkout -b my-new-feature`)
3. Commit your changes (`git commit -am 'Add some feature'`)
4. Push to the branch (`git push origin my-new-feature`)
5. Create new Pull Request

## Issues

If you find any bugs or issues please use the new [Github issue tracker](https://github.com/sourcey/pacm/issues).
