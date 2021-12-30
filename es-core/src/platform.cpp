#include "platform.h"

#include <SDL_events.h>
#ifdef WIN32
#include <codecvt>
#else
#include <unistd.h>
#endif
#include <fcntl.h>

#include "Log.h"
#ifdef WIN32
#include <Windows.h>
#include "utils/FileSystemUtil.h"
#endif

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
	STARTUPINFOW
		si;
	PROCESS_INFORMATION
		pi;
	DWORD
		rcode = 0;

	memset(&si, 0, sizeof si);
	memset(&pi, 0, sizeof pi);
	si.cb = sizeof si;

	if(!CreateProcessW(NULL, (LPWSTR)Utils::FileSystem::convertToWideString(cmd_utf8).c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
		return 9009;

	CloseHandle(pi.hThread);
	WaitForSingleObject(pi.hProcess, INFINITE);
	GetExitCodeProcess(pi.hProcess, &rcode);
	CloseHandle(pi.hProcess);

	return rcode;
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

#ifdef WIN32
// Windows hasn't /tmp directory usualy so nothing to touch.
inline void touch(const std::string&) {}
#else
void touch(const std::string& filename)
{
	int fd = open(filename.c_str(), O_CREAT|O_WRONLY, 0644);
	if (fd >= 0)
		close(fd);
}
#endif

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
	}
}
