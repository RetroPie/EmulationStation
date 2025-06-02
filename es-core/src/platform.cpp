#include "platform.h"

#include <SDL_events.h>
#ifdef WIN32
#include <SDL.h>
#include <Windows.h>
#include "renderers/Renderer.h"
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
	return system(cmd_utf8.c_str());
}

int launchGameCommand(const std::string& cmd_utf8)
{
#ifdef _WIN32
	STARTUPINFO
		si;
	PROCESS_INFORMATION
		pi;
	SDL_Event
		event;
	DWORD
		rcode = 0;
	Uint32
		wf;

	wf = SDL_GetWindowFlags(Renderer::getSDLWindow());
	SDL_SetWindowFullscreen(Renderer::getSDLWindow(), 0);
	SDL_SetWindowBordered(Renderer::getSDLWindow(), SDL_TRUE);

	Renderer::swapBuffers();

	memset(&si, 0, sizeof si);
	memset(&pi, 0, sizeof pi);
	si.cb = sizeof si;

	if(!CreateProcess(NULL, (LPSTR)cmd_utf8.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
		return 9009;

	while(WaitForSingleObject(pi.hProcess, 200) == WAIT_TIMEOUT)
		while(SDL_PollEvent(&event))
			; // NOP

	GetExitCodeProcess(pi.hProcess, &rcode);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	if(wf & SDL_WINDOW_FULLSCREEN)
		SDL_SetWindowFullscreen(Renderer::getSDLWindow(), SDL_WINDOW_FULLSCREEN);
	if(wf & SDL_WINDOW_BORDERLESS)
		SDL_SetWindowBordered(Renderer::getSDLWindow(), SDL_FALSE);

	wf = SDL_GetWindowFlags(Renderer::getSDLWindow());
	if(wf & SDL_WINDOW_MINIMIZED)
		SDL_RestoreWindow(Renderer::getSDLWindow());

	return rcode;
#else
	return runSystemCommand(cmd_utf8);
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
	// Windows hasn't /tmp directory usualy so nothing to touch.
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
