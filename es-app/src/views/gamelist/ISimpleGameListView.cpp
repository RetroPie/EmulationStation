#include "views/gamelist/ISimpleGameListView.h"

#include "views/UIModeController.h"
#include "views/ViewController.h"
#include "CollectionSystemManager.h"
#include "Scripting.h"
#include "Settings.h"
#include "Sound.h"
#include "SystemData.h"
#include "LocaleES.h"         // soporte de idioma
#include "NavigationSounds.h" // helper central para navigationsounds

ISimpleGameListView::ISimpleGameListView(Window* window, FileData* root) 
	: IGameListView(window, root),
	mHeaderText(window), mHeaderImage(window), mBackground(window)
{
	// Localización del texto por defecto del encabezado
	LocaleES& loc = LocaleES::getInstance();
	loc.loadFromSettings();

	mHeaderText.setText(loc.translate("LOGO TEXT")); // ← Traducción desde .ini
	mHeaderText.setSize(mSize.x(), 0);
	mHeaderText.setPosition(0, 0);
	mHeaderText.setHorizontalAlignment(ALIGN_CENTER);
	mHeaderText.setDefaultZIndex(50);

	mHeaderImage.setResize(0, mSize.y() * 0.185f);
	mHeaderImage.setOrigin(0.5f, 0.0f);
	mHeaderImage.setPosition(mSize.x() / 2, 0);
	mHeaderImage.setDefaultZIndex(50);

	mBackground.setResize(mSize.x(), mSize.y());
	mBackground.setDefaultZIndex(0);

	addChild(&mHeaderText);
	addChild(&mBackground);
}

void ISimpleGameListView::onThemeChanged(const std::shared_ptr<ThemeData>& theme)
{
	using namespace ThemeFlags;
	mBackground.applyTheme(theme, getName(), "background", ALL);
	mHeaderImage.applyTheme(theme, getName(), "logo", ALL);
	mHeaderText.applyTheme(theme, getName(), "logoText", ALL);

	// Remove old theme extras
	for (auto extra : mThemeExtras)
	{
		removeChild(extra);
		delete extra;
	}
	mThemeExtras.clear();

	// Add new theme extras
	mThemeExtras = ThemeData::makeExtras(theme, getName(), mWindow);
	for (auto extra : mThemeExtras)
	{
		addChild(extra);
	}

	// Mostrar imagen si está disponible, sino texto
	if (mHeaderImage.hasImage())
	{
		removeChild(&mHeaderText);
		addChild(&mHeaderImage);
	}
	else
	{
		addChild(&mHeaderText);
		removeChild(&mHeaderImage);
	}
}

void ISimpleGameListView::onFileChanged(FileData* /*file*/, FileChangeType /*change*/)
{
	FileData* cursor = getCursor();
	if (!cursor->isPlaceHolder()) {
		populateList(cursor->getParent()->getChildrenListToDisplay());
		setCursor(cursor);
	}
	else
	{
		populateList(mRoot->getChildrenListToDisplay());
		setCursor(cursor);
	}
}

bool ISimpleGameListView::input(InputConfig* config, Input input)
{
	if (input.value != 0)
	{
		// LANZAR JUEGO / ENTRAR A CARPETA
		if(config->isMappedTo("a", input))
		{
			FileData* cursor = getCursor();
			if(cursor->getType() == GAME)
			{
				// SONIDO DE LAUNCH (compatible con navigationsounds → "launch")
				std::shared_ptr<Sound> launchSnd;

				SystemData* sys = mRoot ? mRoot->getSystem() : nullptr;
				if (sys != nullptr)
				{
					const std::shared_ptr<ThemeData>& theme = sys->getTheme();
					if (theme)
					{
						// 1) esquema nuevo
						launchSnd = NavigationSounds::getFromTheme(theme, "launch");
					}
				}

				// 2) fallback clásico (por si acaso)
				if (!launchSnd)
					launchSnd = Sound::getFromTheme(getTheme(), getName(), "launch");

				if (launchSnd)
					launchSnd->play();

				launch(cursor);
			}
			else
			{
				if(cursor->getChildren().size() > 0)
				{
					mCursorStack.push(cursor);
					populateList(cursor->getChildrenListToDisplay());
					setCursor(getCursor());
				}
			}

			return true;
		}
		// VOLVER (dentro de carpetas o a SystemView)
		else if(config->isMappedTo("b", input))
		{
			// 🎵 SONIDO DE BACK SIEMPRE (carpeta o salida a SystemView)
			std::shared_ptr<Sound> backSnd;
			SystemData* sys = mRoot ? mRoot->getSystem() : nullptr;
			if (sys != nullptr)
			{
				const std::shared_ptr<ThemeData>& theme = sys->getTheme();
				if (theme)
				{
					// 1) esquema nuevo: navigationsounds → "back"
					backSnd = NavigationSounds::getFromTheme(theme, "back");
				}
			}
			// 2) fallback clásico (por vista basic/detailed/video)
			if (!backSnd)
				backSnd = Sound::getFromTheme(getTheme(), getName(), "back");

			if (backSnd)
				backSnd->play();

			// Lógica de navegación
			if(mCursorStack.size())
			{
				// Volver una carpeta atrás en la misma lista
				populateList(mCursorStack.top()->getParent()->getChildrenListToDisplay());
				setCursor(mCursorStack.top());
				mCursorStack.pop();
			}
			else
			{
				// Salir a SystemView
				onFocusLost();
				SystemData* systemToView = getCursor()->getSystem();
				if (systemToView->isCollection())
				{
					systemToView = CollectionSystemManager::get()->getSystemToView(systemToView);
				}
				ViewController::get()->goToSystemView(systemToView);
			}

			return true;
		}
		// QUICK SYSTEM SELECT → LB/RB (sonido quicksysselect)
		else if(config->isMappedLike(getQuickSystemSelectRightButton(), input))
		{
			if(Settings::getInstance()->getBool("QuickSystemSelect"))
			{
				// sonido quicksysselect (Batocera-like)
				SystemData* sys = mRoot ? mRoot->getSystem() : nullptr;
				if (sys != nullptr)
				{
					const std::shared_ptr<ThemeData>& theme = sys->getTheme();
					if (theme)
					{
						auto snd = NavigationSounds::getFromTheme(theme, "quicksysselect");
						// fallback opcional al browse
						if (!snd)
							snd = NavigationSounds::getFromTheme(theme, "systembrowse");
						if (snd)
							snd->play();
					}
				}

				onFocusLost();
				ViewController::get()->goToNextGameList();
				return true;
			}
		}
		else if(config->isMappedLike(getQuickSystemSelectLeftButton(), input))
		{
			if(Settings::getInstance()->getBool("QuickSystemSelect"))
			{
				SystemData* sys = mRoot ? mRoot->getSystem() : nullptr;
				if (sys != nullptr)
				{
					const std::shared_ptr<ThemeData>& theme = sys->getTheme();
					if (theme)
					{
						auto snd = NavigationSounds::getFromTheme(theme, "quicksysselect");
						if (!snd)
							snd = NavigationSounds::getFromTheme(theme, "systembrowse");
						if (snd)
							snd->play();
					}
				}

				onFocusLost();
				ViewController::get()->goToPrevGameList();
				return true;
			}
		}
		// RANDOM
		else if (config->isMappedTo("x", input))
		{
			if (mRoot->getSystem()->isGameSystem())
			{
				FileData* randomGame = getCursor()->getSystem()->getRandomGame();
				if (randomGame)
				{
					setCursor(randomGame);
				}
				return true;
			}
		}
		// FAVORITO / COLECCIÓN (sonido "favorite")
		else if (config->isMappedTo("y", input) && !UIModeController::getInstance()->isUIModeKid())
		{
			if(mRoot->getSystem()->isGameSystem())
			{
				if (CollectionSystemManager::get()->toggleGameInCollection(getCursor()))
				{
					// sonido favorite al togglear colección (normalmente Favoritos)
					SystemData* sys = mRoot ? mRoot->getSystem() : nullptr;
					if (sys != nullptr)
					{
						const std::shared_ptr<ThemeData>& theme = sys->getTheme();
						if (theme)
						{
							auto favSnd = NavigationSounds::getFromTheme(theme, "favorite");
							// fallback suave a "select" si el tema no tiene favorite
							if (!favSnd)
								favSnd = NavigationSounds::getFromTheme(theme, "select");
							if (favSnd)
								favSnd->play();
						}
					}
					return true;
				}
			}
		}
	}

	// Evento game-select para scripts
	FileData* cursor = getCursor();
	SystemData* system = this->mRoot->getSystem();
	if (system != NULL) {
		Scripting::fireEvent("game-select", system->getName(), cursor->getPath(), cursor->getName(), "input");
	}
	else
	{
		Scripting::fireEvent("game-select", "NULL", "NULL", "NULL", "input");
	}
	return IGameListView::input(config, input);
}
