// es-app/src/guis/GuiThemeOptions.cpp

#include "guis/GuiThemeOptions.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiTextEditPopup.h"

#include "LocaleES.h"
#include "Settings.h"
#include "utils/FileSystemUtil.h"
#include "resources/Font.h"
#include "views/ViewController.h"
#include "SystemData.h"
#include "Window.h"
#include "renderers/Renderer.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <map>

namespace
{
	inline std::string _(const std::string& key)
	{
		return LocaleES::getInstance().translate(key);
	}

	struct ThemeOption
	{
		std::string id;
		std::string type;
		std::string label;
		std::string path;
		std::string applyTo;

		// Optional output key for theme.ini (e.g. key=USER_NAME)
		std::string key;

		std::vector<std::pair<std::string, std::string>> values;
		std::string defaultValue;
	};

	// -----------------------------
	// Small helpers (ES-X compatible)
	// -----------------------------
	static std::string trim(const std::string& s)
	{
		size_t start = s.find_first_not_of(" \t\r\n");
		if(start == std::string::npos)
			return "";
		size_t end = s.find_last_not_of(" \t\r\n");
		return s.substr(start, end - start + 1);
	}

	static bool isAbsolutePathSimple(const std::string& p)
	{
		// Linux/Unix absolute
		return !p.empty() && p[0] == '/';
	}

	static std::string joinPathSimple(const std::string& a, const std::string& b)
	{
		if(a.empty()) return b;
		if(b.empty()) return a;

		if(a.back() == '/' && b.front() == '/')
			return a + b.substr(1);
		if(a.back() != '/' && b.front() != '/')
			return a + "/" + b;
		return a + b;
	}

	static std::string normalizeRelative(const std::string& p)
	{
		// remove leading "./"
		if(p.rfind("./", 0) == 0)
			return p.substr(2);
		return p;
	}

	static std::string resolveThemePath(const std::string& themeDir, const std::string& maybeRelative)
	{
		if(maybeRelative.empty())
			return "";

		if(isAbsolutePathSimple(maybeRelative))
			return maybeRelative;

		return joinPathSimple(themeDir, normalizeRelative(maybeRelative));
	}

	static bool copyFileForce(const std::string& src, const std::string& dst)
	{
		std::ifstream in(src.c_str(), std::ios::binary);
		if(!in)
			return false;

		std::ofstream out(dst.c_str(), std::ios::binary | std::ios::trunc);
		if(!out)
			return false;

		out << in.rdbuf();
		return out.good();
	}

	static void parseValues(const std::string& str, std::vector<std::pair<std::string, std::string>>& out)
	{
		out.clear();
		std::stringstream ss(str);
		std::string item;

		while(std::getline(ss, item, ','))
		{
			item = trim(item);
			if(item.empty())
				continue;

			auto pos = item.find('|');
			if(pos == std::string::npos)
				out.emplace_back(item, item);
			else
			{
				std::string internal = trim(item.substr(0, pos));
				std::string label    = trim(item.substr(pos + 1));
				if(!internal.empty() && !label.empty())
					out.emplace_back(internal, label);
			}
		}
	}

	// ------------------------------------------------------------
	// Read INI sections into a map: section -> (key->value)
	// ------------------------------------------------------------
	static std::map<std::string, std::map<std::string, std::string>>
	loadIniSections(const std::string& iniPath)
	{
		std::map<std::string, std::map<std::string, std::string>> data;

		std::ifstream file(iniPath.c_str());
		if(!file)
			return data;

		std::string line;
		std::string currentSection;

		while(std::getline(file, line))
		{
			line = trim(line);
			if(line.empty() || line[0] == ';' || line[0] == '#')
				continue;

			if(line.front() == '[' && line.back() == ']')
			{
				currentSection = trim(line.substr(1, line.size() - 2));
				continue;
			}

			auto pos = line.find('=');
			if(pos == std::string::npos)
				continue;

			std::string key = trim(line.substr(0, pos));
			std::string val = trim(line.substr(pos + 1));

			if(!currentSection.empty() && !key.empty())
				data[currentSection][key] = val;
		}

		return data;
	}

	// ------------------------------------------------------------
	// Read top-level key=value (before first [section])
	// ------------------------------------------------------------
	static std::string readTopIniValue(const std::string& iniPath, const std::string& key)
	{
		std::ifstream in(iniPath.c_str());
		if(!in)
			return "";

		std::string line;
		bool inTop = true;

		while(std::getline(in, line))
		{
			std::string t = trim(line);

			if(inTop && !t.empty() && t.front() == '[')
				break;

			if(t.empty() || t[0] == ';' || t[0] == '#')
				continue;

			auto posEq = t.find('=');
			if(posEq == std::string::npos)
				continue;

			std::string thisKey = trim(t.substr(0, posEq));
			if(thisKey == key)
				return trim(t.substr(posEq + 1));
		}

		return "";
	}

	static void updateThemeIniValue(const std::string& iniPath, const std::string& key, const std::string& value)
	{
		std::ifstream in(iniPath.c_str());
		if(!in)
			return;

		std::vector<std::string> lines;
		std::string line;
		bool updated = false;
		bool inTop   = true;

		while(std::getline(in, line))
		{
			std::string t = trim(line);

			if(inTop && !t.empty() && t.front() == '[')
				inTop = false;

			if(inTop)
			{
				auto posEq = t.find('=');
				if(posEq != std::string::npos)
				{
					std::string thisKey = trim(t.substr(0, posEq));
					if(thisKey == key)
					{
						line    = key + " = " + value;
						updated = true;
					}
				}
			}
			lines.push_back(line);
		}
		in.close();

		if(!updated)
		{
			size_t insertPos = lines.size();
			for(size_t i = 0; i < lines.size(); ++i)
			{
				std::string t = trim(lines[i]);
				if(!t.empty() && t.front() == '[')
				{
					insertPos = i;
					break;
				}
			}
			lines.insert(lines.begin() + insertPos, key + " = " + value);
		}

		std::ofstream out(iniPath.c_str(), std::ios::trunc);
		if(!out)
			return;
		for(const auto& l : lines)
			out << l << "\n";
	}

	// ------------------------------------------------------------
	// Auto-detect /layout subfolders and add [layout] option if missing
	// ------------------------------------------------------------
	static void ensureLayoutOptionIfLayoutFolderExists(const std::string& themeDir, std::vector<ThemeOption>& options)
	{
		const std::string layoutDir = themeDir + "/layout";
		if(!Utils::FileSystem::isDirectory(layoutDir))
			return;

		for(const auto& o : options)
			if(o.id == "layout")
				return;

		Utils::FileSystem::stringList lst = Utils::FileSystem::getDirContent(layoutDir);
		std::vector<std::string> dirs(lst.begin(), lst.end());

		std::vector<std::string> layouts;
		for(const auto& name : dirs)
		{
			if(name.empty() || name[0] == '.')
				continue;

			const std::string full = layoutDir + "/" + name;
			if(Utils::FileSystem::isDirectory(full))
				layouts.push_back(name);
		}

		if(layouts.empty())
			return;

		std::sort(layouts.begin(), layouts.end());

		ThemeOption layoutOpt;
		layoutOpt.id = "layout";
		layoutOpt.type = "select";
		layoutOpt.applyTo = "layout";
		layoutOpt.label = _("THEME LAYOUT");

		for(const auto& l : layouts)
			layoutOpt.values.emplace_back(l, l);

		const std::string current = Settings::getInstance()->getString("ThemeLayout");
		layoutOpt.defaultValue = !current.empty() ? current : layouts.front();

		options.insert(options.begin(), layoutOpt);
	}

	// ------------------------------------------------------------
	// Auto-detect /_inc/avatars images and add [avatar] option if missing
	// ------------------------------------------------------------
	static bool hasImageExt(const std::string& f)
	{
		auto lower = f;
		std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

		return (lower.size() >= 4 && lower.rfind(".png") == lower.size() - 4) ||
		       (lower.size() >= 4 && lower.rfind(".jpg") == lower.size() - 4) ||
		       (lower.size() >= 5 && lower.rfind(".jpeg") == lower.size() - 5);
	}

	static std::string stripExt(const std::string& f)
	{
		auto pos = f.find_last_of('.');
		if(pos == std::string::npos) return f;
		return f.substr(0, pos);
	}

	static void ensureAvatarOptionIfAvatarFolderExists(const std::string& themeDir, std::vector<ThemeOption>& options)
	{
		// If theme.ini already declared [avatar], do nothing.
		for(const auto& o : options)
			if(o.id == "avatar")
				return;

		const std::string avatarDir = themeDir + "/_inc/avatars";
		if(!Utils::FileSystem::isDirectory(avatarDir))
			return;

		Utils::FileSystem::stringList lst = Utils::FileSystem::getDirContent(avatarDir);
		std::vector<std::string> files(lst.begin(), lst.end());

		std::vector<std::string> imgs;
		for(const auto& name : files)
		{
			if(name.empty() || name[0] == '.')
				continue;

			const std::string full = avatarDir + "/" + name;
			if(Utils::FileSystem::isRegularFile(full) && hasImageExt(name))
				imgs.push_back(name);
		}

		if(imgs.empty())
			return;

		std::sort(imgs.begin(), imgs.end());

		ThemeOption opt;
		opt.id = "avatar";
		opt.type = "select";
		opt.applyTo = "theme_ini";
		opt.key = "AVATAR_PATH";
		opt.label = _("AVATAR");

		for(const auto& f : imgs)
		{
			const std::string relPath = "./_inc/avatars/" + f;
			const std::string nice    = stripExt(f);
			opt.values.emplace_back(relPath, nice);
		}

		const std::string iniPath = themeDir + "/theme.ini";
		std::string current = readTopIniValue(iniPath, opt.key);
		if(!current.empty())
			opt.defaultValue = current;
		else
			opt.defaultValue = opt.values.front().first;

		options.push_back(opt);
	}

	// ------------------------------------------------------------
	// Load theme.ini options
	// ------------------------------------------------------------
	static std::vector<ThemeOption> loadThemeOptionsFromIni(const std::string& themeDir)
	{
		std::vector<ThemeOption> options;
		const std::string iniPath = themeDir + "/theme.ini";

		if(Utils::FileSystem::isRegularFile(iniPath))
		{
			std::ifstream file(iniPath.c_str());
			if(file)
			{
				ThemeOption current;
				std::string line;

				auto pushIfValid = [&options](const ThemeOption& opt)
				{
					if(opt.id.empty() || opt.type.empty())
						return;

					std::string t = opt.type;
					std::transform(t.begin(), t.end(), t.begin(), ::tolower);

					// select requires values, input does not
					if(t == "select")
					{
						if(!opt.values.empty())
							options.push_back(opt);
					}
					else if(t == "input")
					{
						options.push_back(opt);
					}
				};

				while(std::getline(file, line))
				{
					line = trim(line);
					if(line.empty() || line[0] == ';' || line[0] == '#')
						continue;

					if(line.front() == '[' && line.back() == ']')
					{
						pushIfValid(current);
						current = ThemeOption();
						current.id = trim(line.substr(1, line.size() - 2));
						continue;
					}

					auto pos = line.find('=');
					if(pos != std::string::npos)
					{
						std::string key   = trim(line.substr(0, pos));
						std::string value = trim(line.substr(pos + 1));

						if(key == "label") current.label = value;
						else if(key == "type") current.type = value;
						else if(key == "path") current.path = value;
						else if(key == "apply_to" || key == "applyTo") current.applyTo = value;
						else if(key == "values") parseValues(value, current.values);
						else if(key == "default") current.defaultValue = value;
						else if(key == "key") current.key = value;
					}
				}

				pushIfValid(current);
			}
		}

		ensureLayoutOptionIfLayoutFolderExists(themeDir, options);
		ensureAvatarOptionIfAvatarFolderExists(themeDir, options);
		return options;
	}

	// ------------------------------------------------------------
	// Apply layout by COPYING layout files to theme root (dialog-style)
	// ------------------------------------------------------------
	static bool applyLayoutByCopy(Window* win, const std::string& themeDir, const std::string& layoutId)
	{
		const std::string iniPath = themeDir + "/theme.ini";
		std::map<std::string, std::map<std::string, std::string>> ini;
		if(Utils::FileSystem::isRegularFile(iniPath))
			ini = loadIniSections(iniPath);

		// Defaults: ./layout/<layoutId>/<file>.xml
		auto defPath = [&](const std::string& fileName) -> std::string
		{
			return "./layout/" + layoutId + "/" + fileName;
		};

		const std::string section = "layout_" + layoutId;

		std::string srcTheme  = defPath("theme.xml");
		std::string srcSwatch = defPath("Swatch.xml");
		std::string srcAvatar = defPath("avatar.xml");
		std::string srcUser   = defPath("user.xml");
		std::string srcStart  = defPath("start.xml");

		// If mapping exists in ini, override
		if(!ini.empty() && ini.count(section))
		{
			auto& s = ini[section];
			if(s.count("theme"))  srcTheme  = s["theme"];
			if(s.count("swatch")) srcSwatch = s["swatch"];
			if(s.count("avatar")) srcAvatar = s["avatar"];
			if(s.count("user"))   srcUser   = s["user"];
			if(s.count("start"))  srcStart  = s["start"];
		}

		// Resolve paths
		const std::string absTheme  = resolveThemePath(themeDir, srcTheme);
		const std::string absSwatch = resolveThemePath(themeDir, srcSwatch);
		const std::string absAvatar = resolveThemePath(themeDir, srcAvatar);
		const std::string absUser   = resolveThemePath(themeDir, srcUser);
		const std::string absStart  = resolveThemePath(themeDir, srcStart);

		bool okAny = false;
		bool okAll = true;

		auto doCopy = [&](const std::string& srcAbs, const std::string& dstName)
		{
			if(srcAbs.empty())
				return;

			if(!Utils::FileSystem::isRegularFile(srcAbs))
			{
				// Not fatal; just skip
				return;
			}

			const std::string dstAbs = themeDir + "/" + dstName;
			if(!copyFileForce(srcAbs, dstAbs))
				okAll = false;
			else
				okAny = true;
		};

		doCopy(absTheme,  "theme.xml");
		doCopy(absSwatch, "Swatch.xml");
		doCopy(absAvatar, "avatar.xml");
		doCopy(absUser,   "user.xml");
		doCopy(absStart,  "start.xml");

		if(!okAny)
		{
			if(win)
				win->pushGui(new GuiMsgBox(win, _("No se encontró ningún XML para copiar en este layout."), _("BACK")));
			return false;
		}

		if(!okAll)
		{
			if(win)
				win->pushGui(new GuiMsgBox(win, _("Algunos archivos no se pudieron copiar (revisá permisos/rutas)."), _("BACK")));
		}

		return true;
	}

	static void applyThemeOption(Window* win, const std::string& themeDir, const ThemeOption& opt, const std::string& value)
	{
		if(opt.id.empty() || value.empty())
			return;

		if(opt.applyTo == "layout" || opt.id == "layout")
		{
			Settings::getInstance()->setString("ThemeLayout", value);
			Settings::getInstance()->saveFile();

			// IMPORTANT: copy layout files to theme root (dialog behavior)
			applyLayoutByCopy(win, themeDir, value);
		}
		else
		{
			const std::string iniPath = themeDir + "/theme.ini";
			const std::string outKey  = !opt.key.empty() ? opt.key : opt.id;
			updateThemeIniValue(iniPath, outKey, value);
		}

		auto vc = ViewController::get();
		if(vc != nullptr)
			vc->reloadAll();
	}

	class GuiThemeOptionSelect : public GuiComponent
	{
	public:
		GuiThemeOptionSelect(Window* window,
		                     const std::string& themeDir,
		                     const ThemeOption& opt,
		                     const std::string& title)
			: GuiComponent(window)
			, mMenu(window, title.c_str())
			, mThemeDir(themeDir)
			, mOption(opt)
		{
			for(const auto& v : mOption.values)
			{
				ComponentListRow row;

				auto text = std::make_shared<TextComponent>(
					mWindow,
					v.second,
					Font::get(FONT_SIZE_MEDIUM),
					0x777777FF
				);

				text->setColor(0x777777FF);
				row.addElement(text, true);

				row.makeAcceptInputHandler([this, v]()
				{
					applyThemeOption(mWindow, mThemeDir, mOption, v.first);
					delete this;
				});

				mMenu.addRow(row);
			}

			addChild(&mMenu);

			setSize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
			mMenu.setPosition(
				(mSize.x() - mMenu.getSize().x()) / 2.0f,
				Renderer::getScreenHeight() * 0.15f);
		}

		bool input(InputConfig* config, Input input) override
		{
			if(GuiComponent::input(config, input))
				return true;

			if(config->isMappedTo("b", input) && input.value != 0)
			{
				delete this;
				return true;
			}
			return false;
		}

		std::vector<HelpPrompt> getHelpPrompts() override
		{
			std::vector<HelpPrompt> prompts;
			prompts.push_back(HelpPrompt("a", _("SELECT")));
			prompts.push_back(HelpPrompt("b", _("BACK")));
			return prompts;
		}

		HelpStyle getHelpStyle() override
		{
			HelpStyle style;
			auto vc = ViewController::get();
			if(vc)
			{
				auto system = vc->getState().getSystem();
				if(system)
					style.applyTheme(system->getTheme(), "system");
			}
			return style;
		}

	private:
		MenuComponent mMenu;
		std::string   mThemeDir;
		ThemeOption   mOption;
	};

	static void openThemeInput(Window* win, const std::string& themeDir, const ThemeOption& opt, const std::string& title)
	{
		if(win == nullptr)
			return;

		const std::string iniPath = themeDir + "/theme.ini";
		const std::string outKey  = !opt.key.empty() ? opt.key : opt.id;

		std::string current = readTopIniValue(iniPath, outKey);
		if(current.empty())
			current = opt.defaultValue;

		// NOTE: acceptBtnText is const char* and may be stored as pointer.
		// Use static std::string to guarantee lifetime and allow translation.
		static std::string okLabel = _("OK");

		win->pushGui(new GuiTextEditPopup(
			win,
			title,
			current,
			[win, themeDir, opt](const std::string& newValue)
			{
				if(newValue.empty())
					return;

				applyThemeOption(win, themeDir, opt, newValue);
			},
			false,              // multiline
			okLabel.c_str()     // safe lifetime
		));
	}
}

GuiThemeOptions::GuiThemeOptions(Window* window)
	: GuiComponent(window)
	, mMenu(window, _("THEME OPTIONS").c_str())
{
	std::string themeSet = Settings::getInstance()->getString("ThemeSet");
	std::string themeDir;

	if(!themeSet.empty())
		themeDir = Utils::FileSystem::getHomePath() + "/.emulationstation/themes/" + themeSet;

	auto options = loadThemeOptionsFromIni(themeDir);

	if(options.empty())
	{
		ComponentListRow row;
		row.addElement(
			std::make_shared<TextComponent>(
				mWindow,
				_("NO THEME OPTIONS AVAILABLE"),
				Font::get(FONT_SIZE_MEDIUM),
				0x777777FF),
			true);
		mMenu.addRow(row);
	}
	else
	{
		for(const auto& opt : options)
		{
			std::string entryLabel = !opt.label.empty() ? opt.label : opt.id;

			ComponentListRow row;

			auto text = std::make_shared<TextComponent>(
				mWindow,
				entryLabel,
				Font::get(FONT_SIZE_MEDIUM),
				0x777777FF
			);
			text->setColor(0x777777FF);

			row.addElement(text, true);

			row.makeAcceptInputHandler([this, themeDir, opt, entryLabel]
			{
				std::string t = opt.type;
				std::transform(t.begin(), t.end(), t.begin(), ::tolower);

				if(t == "select" && !opt.values.empty())
				{
					mWindow->pushGui(new GuiThemeOptionSelect(mWindow, themeDir, opt, entryLabel));
				}
				else if(t == "input")
				{
					openThemeInput(mWindow, themeDir, opt, entryLabel);
				}
				else
				{
					std::string msg = entryLabel + "\n\n" + _("(Feature not yet implemented)");
					mWindow->pushGui(new GuiMsgBox(mWindow, msg, _("BACK")));
				}
			});

			mMenu.addRow(row);
		}
	}

	addChild(&mMenu);

	setSize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
	mMenu.setPosition(
		(mSize.x() - mMenu.getSize().x()) / 2.0f,
		Renderer::getScreenHeight() * 0.15f);
}

bool GuiThemeOptions::input(InputConfig* config, Input input)
{
	if(GuiComponent::input(config, input))
		return true;

	if(config->isMappedTo("b", input) && input.value != 0)
	{
		delete this;
		return true;
	}
	return false;
}

std::vector<HelpPrompt> GuiThemeOptions::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	prompts.push_back(HelpPrompt("b", _("BACK")));
	return prompts;
}

HelpStyle GuiThemeOptions::getHelpStyle()
{
	HelpStyle style;
	auto vc = ViewController::get();
	if(vc)
	{
		auto system = vc->getState().getSystem();
		if(system)
			style.applyTheme(system->getTheme(), "system");
	}
	return style;
}
