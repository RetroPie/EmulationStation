//EmulationStation, a graphical front-end for ROM browsing. Created by Alec "Aloshi" Lofquist.
//http://www.aloshi.com

#include "guis/GuiDetectDevice.h"
#include "guis/GuiMsgBox.h"
#include "utils/FileSystemUtil.h"
#include "utils/ProfilingUtil.h"
#include "views/ViewController.h"
#include "CollectionSystemManager.h"
#include "EmulationStation.h"
#include "InputManager.h"
#include "Log.h"
#include "MameNames.h"
#include "platform.h"
#include "PowerSaver.h"
#include "ScraperCmdLine.h"
#include "Settings.h"
#include "SystemData.h"
#include "SystemScreenSaver.h"
#include <SDL_events.h>
#include <SDL_main.h>
#include <SDL_timer.h>
#include <iostream>
#include <time.h>
#ifdef WIN32
#include <Windows.h>
#endif

#include <FreeImage.h>

// NUEVO: sistema de localización ES-X
#include "LocaleES.h"

// NUEVO: música de fondo ES-X
#include "audio/BackgroundMusicManager.h"

bool scrape_cmdline = false;

bool parseArgs(int argc, char* argv[])
{
	Utils::FileSystem::setExePath(argv[0]);

	// We need to process --home before any call to Settings::getInstance(), because settings are loaded from homepath
	for(int i = 1; i < argc; i++)
	{
		if(strcmp(argv[i], "--home") == 0)
		{
			if(i >= argc - 1)
			{
				std::cerr << "Invalid home path supplied.";
				return false;
			}

			Utils::FileSystem::setHomePath(argv[i + 1]);
			break;
		}
	}

	for(int i = 1; i < argc; i++)
	{
		if(strcmp(argv[i], "--monitor") == 0)
		{
			if (i >= argc - 1)
			{
				std::cerr << "Invalid monitor supplied.";
				return false;
			}

			int monitor = atoi(argv[i + 1]);
			i++; // skip the argument value
			Settings::getInstance()->setInt("MonitorID", monitor);
		}else if(strcmp(argv[i], "--resolution") == 0)
		{
			if(i >= argc - 2)
			{
				std::cerr << "Invalid resolution supplied.";
				return false;
			}

			int width = atoi(argv[i + 1]);
			int height = atoi(argv[i + 2]);
			i += 2; // skip the argument value
			Settings::getInstance()->setInt("WindowWidth", width);
			Settings::getInstance()->setInt("WindowHeight", height);
		}else if(strcmp(argv[i], "--screensize") == 0)
		{
			if(i >= argc - 2)
			{
				std::cerr << "Invalid screensize supplied.";
				return false;
			}

			int width = atoi(argv[i + 1]);
			int height = atoi(argv[i + 2]);
			i += 2; // skip the argument value
			Settings::getInstance()->setInt("ScreenWidth", width);
			Settings::getInstance()->setInt("ScreenHeight", height);
		}else if(strcmp(argv[i], "--screenoffset") == 0)
		{
			if(i >= argc - 2)
			{
				std::cerr << "Invalid screenoffset supplied.";
				return false;
			}

			int x = atoi(argv[i + 1]);
			int y = atoi(argv[i + 2]);
			i += 2; // skip the argument value
			Settings::getInstance()->setInt("ScreenOffsetX", x);
			Settings::getInstance()->setInt("ScreenOffsetY", y);
		}else if (strcmp(argv[i], "--screenrotate") == 0)
		{
			if (i >= argc - 1)
			{
				std::cerr << "Invalid screenrotate supplied.";
				return false;
			}

			int rotate = atoi(argv[i + 1]);
			++i; // skip the argument value
			Settings::getInstance()->setInt("ScreenRotate", rotate);
		}else if(strcmp(argv[i], "--gamelist-only") == 0)
		{
			Settings::getInstance()->setBool("ParseGamelistOnly", true);
		}else if(strcmp(argv[i], "--ignore-gamelist") == 0)
		{
			Settings::getInstance()->setBool("IgnoreGamelist", true);
		}else if(strcmp(argv[i], "--show-hidden-files") == 0)
		{
			Settings::getInstance()->setBool("ShowHiddenFiles", true);
		}else if(strcmp(argv[i], "--draw-framerate") == 0)
		{
			Settings::getInstance()->setBool("DrawFramerate", true);
		}else if(strcmp(argv[i], "--no-exit") == 0)
		{
			Settings::getInstance()->setBool("ShowExit", false);
		}else if(strcmp(argv[i], "--no-confirm-quit") == 0)
		{
			Settings::getInstance()->setBool("ConfirmQuit", false);
		}else if(strcmp(argv[i], "--no-splash") == 0)
		{
			Settings::getInstance()->setBool("SplashScreen", false);
		}else if(strcmp(argv[i], "--debug") == 0)
		{
			Settings::getInstance()->setBool("Debug", true);
			Settings::getInstance()->setBool("HideConsole", false);
			Log::setReportingLevel(LogDebug);
		}else if(strcmp(argv[i], "--fullscreen-borderless") == 0)
		{
			Settings::getInstance()->setBool("FullscreenBorderless", true);
		}else if(strcmp(argv[i], "--windowed") == 0)
		{
			Settings::getInstance()->setBool("Windowed", true);
		}else if(strcmp(argv[i], "--vsync") == 0)
		{
			bool vsync = (strcmp(argv[i + 1], "on") == 0 || strcmp(argv[i + 1], "1") == 0) ? true : false;
			Settings::getInstance()->setBool("VSync", vsync);
			i++; // skip vsync value
		}else if(strcmp(argv[i], "--scrape") == 0)
		{
			scrape_cmdline = true;
		}else if(strcmp(argv[i], "--max-vram") == 0)
		{
			int maxVRAM = atoi(argv[i + 1]);
			Settings::getInstance()->setInt("MaxVRAM", maxVRAM);
		}
		else if (strcmp(argv[i], "--force-kiosk") == 0)
		{
			Settings::getInstance()->setBool("ForceKiosk", true);
		}
		else if (strcmp(argv[i], "--force-kid") == 0)
		{
			Settings::getInstance()->setBool("ForceKid", true);
		}
		else if (strcmp(argv[i], "--force-disable-filters") == 0)
		{
			Settings::getInstance()->setBool("ForceDisableFilters", true);
		}
		else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
		{
#ifdef WIN32
			AttachConsole(ATTACH_PARENT_PROCESS);
			freopen("CONOUT$", "wb", stdout);
#endif
			std::cout <<
				"EmulationStation, a graphical front-end for ROM browsing.\n"
				"Written by Alec \"Aloshi\" Lofquist.\n"
				"Version " << PROGRAM_VERSION_STRING << ", built " << PROGRAM_BUILT_STRING << "\n"
				"Command line arguments:\n"
				"\nGeometry settings:\n"
				"--resolution WIDTH HEIGHT      try and force a particular resolution\n"
				"--screenrotate N               rotate a quarter turn clockwise for each N\n"
				"--screensize WIDTH HEIGHT      for a canvas smaller than the full resolution,\n"
				"                               or if rotating into portrait mode\n"
				"--screenoffset X Y             move the canvas by x,y pixels\n"
				"--fullscreen-borderless        borderless fullscreen window\n"
				"--windowed                     not fullscreen, should be used with --resolution\n"
				"--monitor N                    monitor index (0-)\n"
				"\nGame and settings visibility in ES and behaviour of ES:\n"
				"--force-disable-filters        force the UI to ignore applied filters on\n"
				"                               gamelist (p)\n"
				"--force-kid                    force the UI mode to be Kid\n"
				"--force-kiosk                  force the UI mode to be Kiosk\n"
				"--no-confirm-quit              omit confirm dialog on actions of quit menu\n"
				"--no-exit                      don't show the exit option in the menu\n"
				"--no-splash                    don't show the splash screen\n"
				"\nGamelist related:\n"
				"--gamelist-only                use gamelist.xml as trusted source and do not\n"
				"                               check any path entries of gamelist.xml (p)\n"
				"--ignore-gamelist              do not read gamelist.xml files (useful for\n"
				"                               troubleshooting)\n"
				"\nAdvanced settings:\n"
				"--debug                        more logging, show console on Windows. Enables\n"
				"                               these keyboard shortcuts with left CTRL-key:\n"
				"                               +G: Toggle Gridlayout boundary boxes\n"
				"                               +I: Toggle image boundary box\n"
				"                               +R: Reload all UI views (theme, gamelist, system)\n"
				"                               +T: Toggle textcomponent boundary box\n"
				"--draw-framerate               display the framerate (p)\n"
				"--max-vram SIZE                maximum VRAM to use in MB before swapping,\n"
				"                               use 0 for unlimited (p)\n"
				"--show-hidden-files            show also hidden files of filesystem, no effect\n"
				"                               if --gamelist-only is also set (p)\n"
				"--vsync 1|0                    turn vsync on (1) or off (0) (default is on)\n"
				"\nGeneric switches:\n"
				"--help, -h                     summon a sentient, angry tuba\n\n"
				"--home PATH                    directory to use as home folder for\n"
				"                               .emulationstation/es_settings.cfg, aso.\n"
				"                               Subfolder .emulationstation/ will be created.\n"
				"\nScrape mode:\n"
				"--scrape                       scrape using command line interface\n\n"
				"Note: Switches marked (p) will be persisted in es_settings.cfg when any\n"
				"setting is changed via EmulationStation UI.\n\n"
				"Please refer to the online documentation for additional information:\n"
				"https://retropie.org.uk/docs/EmulationStation/\n";
			return false;
		}
	}

	return true;
}

bool verifyHomeFolderExists()
{
	std::string home = Utils::FileSystem::getHomePath();
	std::string configDir = home + "/.emulationstation";
	if(!Utils::FileSystem::exists(configDir))
	{
		std::cout << "Creating config directory \"" << configDir << "\"\n";
		Utils::FileSystem::createDirectory(configDir);
		if(!Utils::FileSystem::exists(configDir))
		{
			std::cerr << "Config directory could not be created!\n";
			return false;
		}
	}

	return true;
}

bool loadSystemConfigFile(Window* window, const char** errorString)
{
	*errorString = NULL;

	if(!SystemData::loadConfig(window))
	{
		LOG(LogError) << "Error while parsing systems configuration file!";
		*errorString = "IT LOOKS LIKE YOUR SYSTEMS CONFIGURATION FILE HAS NOT BEEN SET UP OR IS INVALID. YOU'LL NEED TO DO THIS BY HAND, UNFORTUNATELY.\n\n"
			"VISIT EMULATIONSTATION.ORG FOR MORE INFORMATION.";
		return false;
	}

	if(SystemData::sSystemVector.size() == 0)
	{
		LOG(LogError) << "No systems found! Does at least one system have a game present? (check that extensions match!)\n(Also, make sure you've updated your es_systems.cfg for XML!)";
		*errorString = "WE CAN'T FIND ANY SYSTEMS!\n"
			"CHECK THAT YOUR PATHS ARE CORRECT IN THE SYSTEMS CONFIGURATION FILE, "
			"AND YOUR GAME DIRECTORY HAS AT LEAST ONE GAME WITH THE CORRECT EXTENSION.\n\n"
			"VISIT EMULATIONSTATION.ORG FOR MORE INFORMATION.";
		return false;
	}

	return true;
}

void onExit()
{
	Log::close();
}

int main(int argc, char* argv[])
{
	std::locale::global(std::locale("C"));

	if(!parseArgs(argc, argv))
		return 0;

#ifdef WIN32
	if(!Settings::getInstance()->getBool("HideConsole"))
	{
		if(AllocConsole())
		{
			freopen("CONIN$", "r", stdin);
			freopen("CONOUT$", "wb", stdout);
			freopen("CONOUT$", "wb", stderr);
		}
	}else{
		if(GetConsoleWindow())
			ShowWindow(GetConsoleWindow(), SW_HIDE);
	}
#endif

#ifdef FREEIMAGE_LIB
	FreeImage_Initialise();
#endif

	if(!verifyHomeFolderExists())
		return 1;

	Log::init();
	Log::open();
	LOG(LogInfo) << "EmulationStation - v" << PROGRAM_VERSION_STRING << ", built " << PROGRAM_BUILT_STRING;

	// 🔤 NUEVO: inicializar localización según Settings::Language
	LocaleES::getInstance().loadFromSettings();

	atexit(&onExit);

	Window window;
	SystemScreenSaver screensaver(&window);
	PowerSaver::init();
	ViewController::init(&window);
	CollectionSystemManager::init(&window);
	MameNames::init();
	window.pushGui(ViewController::get());

	bool splashScreen = Settings::getInstance()->getBool("SplashScreen");

	if(!scrape_cmdline)
	{
		if(!window.init())
		{
			LOG(LogError) << "Window failed to initialize!";
			return 1;
		}

		if (splashScreen)
		{
			std::string progressText = "Loading system config...";
			window.renderLoadingScreen(progressText);
		}
	}

	const char* errorMsg = NULL;
	if(!loadSystemConfigFile(splashScreen ? &window : nullptr, &errorMsg))
	{
		if(errorMsg == NULL)
		{
			LOG(LogError) << "Unknown error occured while parsing system config file.";
			if(!scrape_cmdline)
				Renderer::deinit();
			return 1;
		}

		window.pushGui(new GuiMsgBox(&window,
			errorMsg,
			"QUIT", [] {
				SDL_Event* quit = new SDL_Event();
				quit->type = SDL_QUIT;
				SDL_PushEvent(quit);
			}));
	}

	if(scrape_cmdline)
	{
		return run_scraper_cmdline();
	}

	ViewController::get()->preload();

	if(splashScreen)
		window.renderLoadingScreen("Done.");

	InputManager::getInstance()->init();

	// 🔊 NUEVO: inicializar música de fondo ES-X (si está habilitada)
	BackgroundMusicManager::getInstance().init();

	if(errorMsg == NULL)
	{
		if(Utils::FileSystem::exists(InputManager::getConfigPath()) && InputManager::getInstance()->getNumConfiguredDevices() > 0)
		{
			ViewController::get()->goToStart();
		}else{
			window.pushGui(new GuiDetectDevice(&window, true, [] { ViewController::get()->goToStart(); }));
		}
	}

	int lastTime = SDL_GetTicks();
	int ps_time = SDL_GetTicks();

	bool running = true;

	while(running)
	{
		SDL_Event event;
		bool ps_standby = PowerSaver::getState() && (int) SDL_GetTicks() - ps_time > PowerSaver::getMode();

		if(ps_standby ? SDL_WaitEventTimeout(&event, PowerSaver::getTimeout()) : SDL_PollEvent(&event))
		{
			do
			{
				InputManager::getInstance()->parseEvent(event, &window);

				if(event.type == SDL_QUIT)
					running = false;
			} while(SDL_PollEvent(&event));

			if (ps_standby)
				lastTime = SDL_GetTicks();

			ps_time = SDL_GetTicks();
		}
		else if (ps_standby)
		{
			ps_time = SDL_GetTicks();
		}

		if(window.isSleeping())
		{
			lastTime = SDL_GetTicks();
			SDL_Delay(1);
			continue;
		}

		int curTime = SDL_GetTicks();
		int deltaTime = curTime - lastTime;
		lastTime = curTime;

		if(deltaTime < 0)
			deltaTime = 1000;

		window.update(deltaTime);

		// ✅ BGM: procesar “resume con delay” al volver del juego
		BackgroundMusicManager::getInstance().update(deltaTime);

		window.render();
		Renderer::swapBuffers();

		Log::flush();
	}

	while(window.peekGui() != ViewController::get())
		delete window.peekGui();

	InputManager::getInstance()->deinit();

	// 🔊 NUEVO: apagar música antes de cerrar ventana / audio
	BackgroundMusicManager::getInstance().shutdown();

	window.deinit();

	MameNames::deinit();
	CollectionSystemManager::deinit();
	SystemData::deleteSystems();

#ifdef FREEIMAGE_LIB
	FreeImage_DeInitialise();
#endif

	processQuitMode();

	ProfileDump();

	LOG(LogInfo) << "EmulationStation cleanly shutting down.";

	return 0;
}
