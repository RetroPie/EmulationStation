#include "FileSorts.h"

#include "utils/StringUtil.h"
#include "Settings.h"
#include "Log.h"
#include "LocaleESHook.h"   // 🔹 para es_translate()

namespace FileSorts
{

	const FileData::SortType typesArr[] = {
		// Nombre
		FileData::SortType(&compareName, true,  es_translate("name, ascending")),
		FileData::SortType(&compareName, false, es_translate("name, descending")),

		// Rating
		FileData::SortType(&compareRating, true,  es_translate("rating, ascending")),
		FileData::SortType(&compareRating, false, es_translate("rating, descending")),

		// Veces jugado
		FileData::SortType(&compareTimesPlayed, true,  es_translate("times played, ascending")),
		FileData::SortType(&compareTimesPlayed, false, es_translate("times played, descending")),

		// Última vez jugado
		FileData::SortType(&compareLastPlayed, true,  es_translate("last played, ascending")),
		FileData::SortType(&compareLastPlayed, false, es_translate("last played, descending")),

		// Jugadores
		FileData::SortType(&compareNumPlayers, true,  es_translate("number players, ascending")),
		FileData::SortType(&compareNumPlayers, false, es_translate("number players, descending")),

		// Fecha de lanzamiento
		FileData::SortType(&compareReleaseDate, true,  es_translate("release date, ascending")),
		FileData::SortType(&compareReleaseDate, false, es_translate("release date, descending")),

		// Género
		FileData::SortType(&compareGenre, true,  es_translate("genre, ascending")),
		FileData::SortType(&compareGenre, false, es_translate("genre, descending")),

		// Desarrollador
		FileData::SortType(&compareDeveloper, true,  es_translate("developer, ascending")),
		FileData::SortType(&compareDeveloper, false, es_translate("developer, descending")),

		// Distribuidor
		FileData::SortType(&comparePublisher, true,  es_translate("publisher, ascending")),
		FileData::SortType(&comparePublisher, false, es_translate("publisher, descending")),

		// Sistema
		FileData::SortType(&compareSystem, true,  es_translate("system, ascending")),
		FileData::SortType(&compareSystem, false, es_translate("system, descending"))
	};

	const std::vector<FileData::SortType> SortTypes(typesArr, typesArr + sizeof(typesArr)/sizeof(typesArr[0]));

	//returns if file1 should come before file2
	bool compareName(const FileData* file1, const FileData* file2)
	{
		// we compare the actual metadata name, as collection files have the system appended which messes up the order
		std::string name1 = Utils::String::toUpper(file1->metadata.get("sortname"));
		std::string name2 = Utils::String::toUpper(file2->metadata.get("sortname"));
		if(name1.empty()){
			name1 = Utils::String::toUpper(file1->metadata.get("name"));
		}
		if(name2.empty()){
			name2 = Utils::String::toUpper(file2->metadata.get("name"));
		}

		ignoreLeadingArticles(name1, name2);

		return name1.compare(name2) < 0;
	}

	bool compareRating(const FileData* file1, const FileData* file2)
	{
		return file1->metadata.getFloat("rating") < file2->metadata.getFloat("rating");
	}

	bool compareTimesPlayed(const FileData* file1, const FileData* file2)
	{
		//only games have playcount metadata
		if(file1->metadata.getType() == GAME_METADATA && file2->metadata.getType() == GAME_METADATA)
		{
			return (file1)->metadata.getInt("playcount") < (file2)->metadata.getInt("playcount");
		}

		return false;
	}

	bool compareLastPlayed(const FileData* file1, const FileData* file2)
	{
		// since it's stored as an ISO string (YYYYMMDDTHHMMSS), we can compare as a string
		// as it's a lot faster than the time casts and then time comparisons
		return (file1)->metadata.get("lastplayed") < (file2)->metadata.get("lastplayed");
	}

	bool compareNumPlayers(const FileData* file1, const FileData* file2)
	{
		return (file1)->metadata.getInt("players") < (file2)->metadata.getInt("players");
	}

	bool compareReleaseDate(const FileData* file1, const FileData* file2)
	{
		// since it's stored as an ISO string (YYYYMMDDTHHMMSS), we can compare as a string
		// as it's a lot faster than the time casts and then time comparisons
		return (file1)->metadata.get("releasedate") < (file2)->metadata.get("releasedate");
	}

	bool compareGenre(const FileData* file1, const FileData* file2)
	{
		std::string genre1 = Utils::String::toUpper(file1->metadata.get("genre"));
		std::string genre2 = Utils::String::toUpper(file2->metadata.get("genre"));
		return genre1.compare(genre2) < 0;
	}

	bool compareDeveloper(const FileData* file1, const FileData* file2)
	{
		std::string developer1 = Utils::String::toUpper(file1->metadata.get("developer"));
		std::string developer2 = Utils::String::toUpper(file2->metadata.get("developer"));
		return developer1.compare(developer2) < 0;
	}

	bool comparePublisher(const FileData* file1, const FileData* file2)
	{
		std::string publisher1 = Utils::String::toUpper(file1->metadata.get("publisher"));
		std::string publisher2 = Utils::String::toUpper(file2->metadata.get("publisher"));
		return publisher1.compare(publisher2) < 0;
	}

	bool compareSystem(const FileData* file1, const FileData* file2)
	{
		std::string system1 = Utils::String::toUpper(file1->getSystemName());
		std::string system2 = Utils::String::toUpper(file2->getSystemName());
		return system1.compare(system2) < 0;
	}

	//If option is enabled, ignore leading articles by temporarily modifying the name prior to sorting
	//(Articles are defined within the settings config file)
	void ignoreLeadingArticles(std::string &name1, std::string &name2) {

		if (Settings::getInstance()->getBool("IgnoreLeadingArticles"))
		{

			std::vector<std::string> articles = Utils::String::delimitedStringToVector(Settings::getInstance()->getString("LeadingArticles"), ",");

			for(Utils::String::stringVector::iterator it = articles.begin(); it != articles.end(); it++)
			{
				if (Utils::String::startsWith(Utils::String::toUpper(name1), Utils::String::toUpper(it[0]) + " ")) {
					name1 = Utils::String::replace(Utils::String::toUpper(name1), Utils::String::toUpper(it[0]) + " ", "");
				}

				if (Utils::String::startsWith(Utils::String::toUpper(name2), Utils::String::toUpper(it[0]) + " ")) {
					name2 = Utils::String::replace(Utils::String::toUpper(name2), Utils::String::toUpper(it[0]) + " ", "");
				}
			}
		}
	}

};
