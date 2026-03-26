#include "icy/application.h"
#include "icy/filesystem.h"
#include "icy/net/sslmanager.h"
#include "icy/pacm/packagemanager.h"
#include "icy/util.h"


using std::cerr;
using std::cout;
using std::endl;
using namespace icy;


//
// Pacm Console Application
//
// Examples:
//
// pacm -help
// pacm -endpoint https://anionu.com -uri /packages.json -install
// surveillancemodeplugin,recordingmodeplugin -print
// pacm -endpoint https://anionu.com -uri /packages.json -uninstall
// surveillancemodeplugin,recordingmodeplugin -print
// pacm -endpoint https://anionu.com -uri /packages.json -update -print
//


class PacmApplication : public icy::Application
{
public:
    pacm::PackageManager manager;

    struct Options
    {
        StringVec install;
        StringVec uninstall;
        bool update;
        bool print;
        bool help;

        Options()
        {
            update = false;
            print = false;
            help = false;
        }
    } options;

    PacmApplication() {}

    virtual ~PacmApplication() {}

    void printHelp()
    {
        cout
            << "\nPacm v0.2.0"
               "\n(c) icey"
               "\nhttps://0state.com/pacm"
               "\n"
               "\nGeneral commands:"
               "\n  -help           Print help"
               "\n  -logfile        Log file path"
               "\n"
               "\nServer commands:"
               "\n  -endpoint       HTTP server endpoint"
               "\n  -uri            HTTP server package JSON index URI"
               "\n"
               "\nPackage commands:"
               "\n  -install        Packages to install (comma separated)"
               "\n  -uninstall      Packages to uninstall (comma separated)"
               "\n  -update         Update all packages"
               "\n  -print          Print all local and remote packages on exit"
               "\n  -checksum-alg   Checksum algorithm for verifying packages "
               "(MDS/SHA1)"
               "\n"
               "\nFilesystem commands:"
               "\n  -install-dir    Directory where packages will be installed"
               "\n  -data-dir       Directory where package manifests will be "
               "stored"
               "\n  -temp-dir       Directory where intermediate package files "
               "will be stored"
            << endl;
    }

    void parseOptions(int argc, char* argv[])
    {
        OptionParser optparse(argc, argv, "-");
        for (auto& kv : optparse.args) {
            const std::string& key = kv.first;
            const std::string& value = kv.second;
            SDebug << "Setting option: " << key << ": " << value << std::endl;

            if (key == "help") {
                options.help = true;
            } else if (key == "endpoint") {
                manager.options().endpoint = value;
            } else if (key == "uri") {
                manager.options().indexURI = value;
            } else if (key == "install-dir") {
                manager.options().installDir = value;
            } else if (key == "data-dir") {
                manager.options().dataDir = value;
            } else if (key == "temp-dir") {
                manager.options().tempDir = value;
            } else if (key == "packages") {
                manager.options().endpoint = value;
            } else if (key == "install") {
                options.install = util::split(value, ",");
            } else if (key == "uninstall") {
                options.uninstall = util::split(value, ",");
            } else if (key == "update") {
                options.update = true;
            } else if (key == "print") {
                options.print = true;
            } else if (key == "logfile") {
                auto log = dynamic_cast<FileChannel*>(
                    icy::Logger::instance().get("Pacm"));
                log->setPath(value);
            } else {
                cerr << "Unrecognized command: " << key << "=" << value << endl;
            }
        }
    }

    void work()
    {
        try {
            // Print help
            if (options.help) {
                printHelp();
                return;
            }

            // Initialize Pacman and query remote packages from the server
            manager.initialize();
            manager.queryRemotePackages();
            icy::Application::run();
            if (!manager.initialized()) {
                cerr << "Package manager failed to initialize" << endl;
                return;
            }

            // Uninstall packages if requested
            if (!options.uninstall.empty()) {
                cout << "# Uninstall packages: " << options.install.size()
                     << endl;
                manager.uninstallPackages(options.uninstall);
                icy::Application::run();
            }

            // Install packages if requested
            if (!options.install.empty()) {
                cout << "# Install packages: " << options.install.size()
                     << endl;
                manager.installPackages(options.install);
                icy::Application::run();
            }

            // Update all packages if requested
            if (options.update) {
                cout << "# Update all packages" << endl;
                manager.updateAllPackages();
                icy::Application::run();
            }

            // Print packages to stdout
            if (options.print) {
                cout << "# Print packages" << endl;

                // Print local packages
                {
                    cout << "Local packages: " << manager.localPackages().size()
                         << endl;
                    for (auto& kv : manager.localPackages().map()) {
                        cout << "  - " << kv.first
                             << ": version=" << kv.second->version()
                             << ", state=" << kv.second->state() << endl;
                    }
                }

                // Print remote packages
                {
                    cout << "Remote packages: "
                         << manager.remotePackages().size() << endl;
                    for (auto& kv : manager.remotePackages().map()) {
                        cout << "  - " << kv.first << ": version="
                             << kv.second->latestAsset().version()
                             << ", author=" << kv.second->author() << endl;
                    }
                }
            }
        } catch (std::exception& exc) {
            cerr << "Pacm runtime error: " << exc.what() << endl;
        }
    }
};


int main(int argc, char** argv)
{
    {
#ifdef _MSC_VER
        _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

        // Setup the file logger
        std::string logPath = fs::makePath(fs::makePath(getCwd(), "logs"), util::format("Pacm_%Ld.log", static_cast<long>(Timestamp().epochTime())));
        Logger::instance().add(std::make_unique<FileChannel>("Pacm", logPath, Level::Debug));

        // Init SSL client context with certificate verification
        net::SSLManager::instance().initializeClient(
            std::make_shared<net::SSLContext>(
                net::SSLContext::CLIENT_USE, "", "", "",
                net::SSLContext::VERIFY_RELAXED, 9, true,
                "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH"));

        // Run the application
        {
            PacmApplication app;
            app.parseOptions(argc, argv);
            app.work();
        }

        // Cleanup all singletons
        http::Client::destroy();
        net::SSLManager::destroy();
        Logger::destroy();
    }

    return 0;
}