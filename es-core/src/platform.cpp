#include "platform.h"

#include <SDL_events.h>
#ifdef WIN32
#include <windows.h>
#include "Shlwapi.h"
#pragma comment(lib, "Shlwapi.lib")
#else
#include <unistd.h>
#endif
#include <fcntl.h>

#include "Log.h"

int runShutdownCommand()
{
#ifdef WIN32 // windows
	return system("shutdown -s -t 0");
#else // osx / linux
	return system("sudo shutdown -h now");
#endif
}

int runRestartCommand()
{
#ifdef WIN32 // windows
	return system("shutdown -r -t 0");
#else // osx / linux
	return system("sudo shutdown -r now");
#endif
}

int runSystemCommand(const std::string& cmd_utf8)
{
#ifdef WIN32
	std::string args = std::string(PathGetArgs(cmd_utf8.c_str()));
	std::string program = cmd_utf8.substr(0, cmd_utf8.length() - args.length());
	SHELLEXECUTEINFO ShExecInfo = {0};
	ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
	ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
	ShExecInfo.hwnd = NULL;
	ShExecInfo.lpVerb = NULL;
	ShExecInfo.lpFile = program.c_str();        
	ShExecInfo.lpParameters = args.c_str();   
	ShExecInfo.lpDirectory = NULL;
	ShExecInfo.nShow = SW_SHOW;
	ShExecInfo.hInstApp = NULL; 
	ShellExecuteEx(&ShExecInfo);
	WaitForSingleObject(ShExecInfo.hProcess, INFINITE);
	CloseHandle(ShExecInfo.hProcess);
	DWORD dwExitCode = 0;
	GetExitCodeProcess(ShExecInfo.hProcess, &dwExitCode);
	return dwExitCode;
#else
	return system(cmd_utf8.c_str());
#endif
}

QuitMode quitMode = QuitMode::QUIT;

int quitES(QuitMode mode)
{
	quitMode = mode;

	SDL_Event *quit = new SDL_Event();
	quit->type = SDL_QUIT;
	SDL_PushEvent(quit);
	return 0;
}

void touch(const std::string& filename)
{
#ifdef WIN32
	FILE* fp = fopen(filename.c_str(), "ab+");
	if (fp != NULL)
		fclose(fp);
#else
	int fd = open(filename.c_str(), O_CREAT|O_WRONLY, 0644);
	if (fd >= 0)
		close(fd);
#endif
}

void processQuitMode()
{
	switch (quitMode)
	{
	case QuitMode::RESTART:
		LOG(LogInfo) << "Restarting EmulationStation";
		touch("/tmp/es-restart");
		break;
	case QuitMode::REBOOT:
		LOG(LogInfo) << "Rebooting system";
		touch("/tmp/es-sysrestart");
		runRestartCommand();
		break;
	case QuitMode::SHUTDOWN:
		LOG(LogInfo) << "Shutting system down";
		touch("/tmp/es-shutdown");
		runShutdownCommand();
		break;
	default:
		// No-op to prevent compiler warnings
		// If we reach here, it is not a RESTART, REBOOT,
		// or SHUTDOWN. Basically a normal exit.
		break;
	}
}
