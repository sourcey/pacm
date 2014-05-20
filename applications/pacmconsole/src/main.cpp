#include "scy/application.h"
#include "scy/pacm/packagemanager.h"
#include "scy/net/sslmanager.h"
#include "scy/util.h"


using std::cout;
using std::endl;
using namespace scy;


//
// Pacm Console Application
//
// Examples:
//
// pacm -help
// pacm -endpoint https://anionu.com -uri /packages.json -install surveillancemodeplugin,recordingmodeplugin -print
// pacm -endpoint https://anionu.com -uri /packages.json -uninstall surveillancemodeplugin,recordingmodeplugin -print
// pacm -endpoint https://anionu.com -uri /packages.json -update -print
//
// TODO:
// - Better error and package install failure output
//


class PacmApplication: public scy::Application	
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

		Options() {
			update = false;
			print = false;
			help = false;
		}
	} options;

	PacmApplication()
	{
	}

	virtual ~PacmApplication()
	{
	}
	
	void printHelp()
	{
		cout << 
			"\nPacm v0.1.0"
			"\n(c) Sourcey"
			"\nhttp://sourcey.com/pacm"
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
			"\n  -checksum-alg   Checksum algorithm for verifying packages (MDS/SHA1)"
			"\n"
			"\nFilesystem commands:"
			"\n  -install-dir    Directory where packages will be installed"
			"\n  -data-dir       Directory where package manifests will be stored"
			"\n  -temp-dir       Directory where intermediate package files will be stored"
			<< endl;
	}
	
	void parseOptions(int argc, char* argv[])
	{
		OptionParser optparse(argc, argv, "-");
		for (auto& kv : optparse.args) {	
			const std::string& key = kv.first;
			const std::string& value = kv.second;
			DebugL << "Setting option: " << key << ": " << value << std::endl;

			if (key == "help") {
				options.help = true;
			}
			else if (key == "endpoint") {
				manager.options().endpoint = value;
			}
			else if (key == "uri") {
				manager.options().indexURI = value;
			}
			else if (key == "install-dir") {
				manager.options().installDir = value;
			}
			else if (key == "data-dir") {
				manager.options().dataDir = value;
			}
			else if (key == "temp-dir") {
				manager.options().tempDir = value;
			}
			else if (key == "packages") {
				manager.options().endpoint = value;
			}
			else if (key == "install") {
				options.install = util::split(value, ",");
			}
			else if (key == "uninstall") {
				options.uninstall = util::split(value, ",");
			}
			else if (key == "update") {
				options.update = true;
			}
			else if (key == "print") {
				options.print = true;
			}
			else if (key == "logfile") {
				auto log = dynamic_cast<FileChannel*>(scy::Logger::instance().get("Pacm"));
				log->setPath(value);
			}
			else {
				cout << "Unrecognized command: " << key << "=" << value << endl;
			}
		}
	}

	void work()
	{		
		// Print help
		if (options.help) {
			printHelp();
			return;
		}

		// Initialize Pacman and query remote packages from the server
		manager.initialize();
		manager.queryRemotePackages();
		scy::Application::run(); 
		assert(manager.initialized());

		// Uninstall packages if requested
		if (!options.uninstall.empty()) {
			cout << "# Uninstall packages: " << options.install.size() << endl;
			manager.uninstallPackages(options.uninstall);
			scy::Application::run(); 
		}

		// Install packages if requested
		if (!options.install.empty()) {
			cout << "# Install packages: " << options.install.size() << endl;
			manager.installPackages(options.install);
			scy::Application::run(); 
		}

		// Update all packages if requested
		if (options.update) {
			cout << "# Update all packages" << endl;
			manager.updateAllPackages();
			scy::Application::run(); 
		}

		// Print packages to stdout
		if (options.print) {
			cout << "# Print packages" << endl;

			// Print local packages
			{
				cout << "Local packages: " << manager.localPackages().size() << endl;
				for (auto& kv : manager.localPackages().map()) {			
					cout << "  - " << kv.first << ": version=" 
						<< kv.second->version() << ", state=" 
						<< kv.second->state() << endl;
				}
			}

			// Print remote packages
			{
				cout << "Remote packages: " << manager.remotePackages().size() << endl;
				for (auto& kv : manager.remotePackages().map()) {	
					cout << "  - " << kv.first << ": version=" 
						<< kv.second->latestAsset().version() << ", author=" 
						<< kv.second->author() << endl;
				}
			}
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
		std::string logPath(getCwd());
		fs::addnode(logPath, "logs");
		fs::addnode(logPath, util::format("Pacm_%Ld.log", static_cast<long>(Timestamp().epochTime())));	
		Logger::instance().add(new FileChannel("Pacm", logPath, LDebug));

		// Init SSL client context 	
		net::SSLManager::initNoVerifyClient();
	
		// Run the application
		{
			PacmApplication app;	
			app.parseOptions(argc, argv);
			app.work();
		}
	
		// Cleanup all singletons
		http::Client::destroy();
		net::SSLManager::destroy();
		GarbageCollector::destroy();
		Logger::destroy();
	}

	return 0;
}


#if 0
	Thread console([](void* arg) 
	{
		auto app = reinterpret_cast<PacmApplication*>(arg);		
	
		char o = 0;
		while (o != 'Q') 
		{	
			cout << 
				"COMMANDS:\n"
				"  A	Set Active Packages."
				"  L	List Local Packages."
				"  K	List Remote Packages."
				"  J	Display Latest Remote Package Asset."
				"  R	Reload Package List."
				"  I	Install Packages."
				"  U	Uninstall Packages."
				"  D	Update All Packages."
				"  Q	Quit.\n";
		
			o = toupper(getch());		

			// Set Active Packages
			if (o == 'A') {	
				cout << "Enter packages names separated by commas: " << endl;
				string s;
				getline(cin,s);
				packages = util::split(s, ",");
			}

			// List Local Packages
			else if (o == 'L') {
				cout << "Listing local packages: " << app->manager.localPackages().size() << endl;
				auto items = app->manager.localPackages().map();
				for (auto it = items.begin(); it != items.end(); ++it) {				
					cout << "Package: " << it->first << endl;
				}
			} 

			// List Remote Packages
			else if (o == 'K') {
				cout << "Listing remote packages: " << app->manager.remotePackages().size() << endl;
				auto items = app->manager.remotePackages().map();
				for (auto it = items.begin(); it != items.end(); ++it) {				
					cout << "Package: " << it->first << endl;
				}
			} 

			// Display Latest Remote Package Asset
			else if (o == 'J') {
				auto  items = app->manager.remotePackages().map();
				for (auto it = items.begin(); it != items.end(); ++it) {			
					it->second->latestAsset().print(InfoL);	
				}
			} 

			// Reload Package List
			else if (o == 'R') {
				app->manager.uninitialize();
				app->manager.initialize();
			} 

			// Install Packages
			else if (o == 'I') {
				assert(!packages.empty());
				app->manager.install(packages);
			} 

			// Uninstall Packages
			else if (o == 'U') {
				assert(!packages.empty());
				app->manager.uninstall(packages);
			} 

			// Update All Packages
			else if (o == 'D') {
				app->manager.updateAllPackages();
			}
		}
	}, &app);

	app.waitForShutdown([](void* arg) {			
		reinterpret_cast<PacmApplication*>(arg)->stop();
	}, &app);
#endif