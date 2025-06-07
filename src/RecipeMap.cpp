#include "RecipeMap.h"
#include "CoSave.h"

RecipeMap::Key RecipeMap::GetIngredientKey(RE::CraftingSubMenus::AlchemyMenu* a_alchemyMenu)
{
	uint32_t                                idx = 0;
	std::array<RE::FormID, MAX_INGREDIENTS> ingredientIDs{};

	for (const auto& i : a_alchemyMenu->selectedIndexes)
	{
		if (i >= a_alchemyMenu->ingredientEntries.size())
		{
			logger::error("selected idx of {} is invalid", i);
			return {};
		}

		const auto& ing = a_alchemyMenu->ingredientEntries[i];

		if (ing.ingredient && ing.isSelected)
		{
			logger::info("{} is selected", ing.ingredient->GetDisplayName());

			if (!ing.ingredient->GetObject())
			{
				logger::error("Could not get ingredient object");
				break;
			}

			if (idx >= MAX_INGREDIENTS)
			{
				logger::warn("Too many ingredients selected");
				break;
			}

			ingredientIDs[idx++] = ing.ingredient->GetObject()->formID;
		}
	}

	if (idx < MIN_INGREDIENTS_REQUIRED)
	{
		logger::warn(
			"Could not add recipe as selected ingredients is less than ingredients required");
		return {};
	}

	return { ingredientIDs };
}

void RecipeMap::DeserializeRecipes(SKSE::SerializationInterface* a_intfc)
{
	size_t items = 0;
	if (!CoSave::Read(a_intfc, items))
	{
		logger::error("Could not read number of recipes");
		return;
	}

	recipes.reserve(items);

	for (size_t i = 0; i < items; i++)
	{
		std::array<RE::FormID, MAX_INGREDIENTS>          ingredientIDs{};
		std::array<RE::IngredientItem*, MAX_INGREDIENTS> ingredientTesForms{};
		size_t                                           unresolved = 0;

		for (size_t j = 0; j < MAX_INGREDIENTS; j++)
		{
			if (!CoSave::Load(a_intfc, ingredientTesForms[j], ingredientIDs[j]))
			{
				unresolved++;

				if ((MAX_INGREDIENTS - unresolved) < MIN_INGREDIENTS_REQUIRED)
				{
					logger::error("Could not resolve recipe formids");
					return;
				}
			}
		}

		std::string name;
		if (!CoSave::Read(a_intfc, name))
		{
			logger::info("Could not read name");
			return;
		}

		recipes[ingredientIDs] = name;

		logger::info(
			"{}, {}, {} => {}",
			ingredientTesForms[0] ? ingredientTesForms[0]->GetName() : "<null>",
			ingredientTesForms[1] ? ingredientTesForms[1]->GetName() : "<null>",
			ingredientTesForms[2] ? ingredientTesForms[2]->GetName() : "<null>",
			name);
	}
}

void RecipeMap::Serialize(SKSE::SerializationInterface* a_intfc)
{
	logger::info("Serializing...");
	const auto& recipes = GetSingleton().recipes;
	if (!a_intfc->OpenRecord(SERIALIZATION_TYPE, SERIALIZATION_VER))
	{
		logger::warn("Could not open record");
		return;
	}

	if (!CoSave::Write(a_intfc, recipes.size()))
	{
		logger::error("Failed to serialize recipe");
		return;
	}

	for (const auto& [key, name] : recipes)
	{
		if (!CoSave::Write(a_intfc, key) || !CoSave::Write(a_intfc, name))
		{
			logger::error("Failed to serialize recipe");
			break;
		}
	}
}

void RecipeMap::Revert(SKSE::SerializationInterface*)
{
	logger::info("Reverting...");
	GetSingleton().recipes.clear();
}

void RecipeMap::Deserialize(SKSE::SerializationInterface* a_intfc)
{
	logger::info("Deserializing...");

	std::uint32_t type, version, length;
	while (a_intfc->GetNextRecordInfo(type, version, length))
	{
		if (type == SERIALIZATION_TYPE && version == SERIALIZATION_VER)
		{
			GetSingleton().DeserializeRecipes(a_intfc);
		}
	}
}

void RecipeMap::RegisterSerialization()
{
	auto* intfc = SKSE::GetSerializationInterface();
	intfc->SetUniqueID(SERIALIZATION_KEY);
	intfc->SetLoadCallback(Deserialize);
	intfc->SetRevertCallback(Revert);
	intfc->SetSaveCallback(Serialize);
}

void RecipeMap::AddCurrentRecipeName(
	RE::CraftingSubMenus::AlchemyMenu* a_alchemyMenu,
	const char*                        a_name)
{
	auto key = GetIngredientKey(a_alchemyMenu);

	if (key)
		recipes[key] = a_name;
}

const char* RecipeMap::GetCurrentRecipeName(RE::CraftingSubMenus::AlchemyMenu* a_alchemyMenu)
{
	if (a_alchemyMenu->selectedIndexes.size() < MIN_INGREDIENTS_REQUIRED)
	{
		return nullptr;
	}

	auto key = GetIngredientKey(a_alchemyMenu);

	if (!key)
		return nullptr;

	auto it = recipes.find(key);
	if (it != recipes.end())
	{
		return it->second.c_str();
	}
	return nullptr;
}

inline RecipeMap& RecipeMap::GetSingleton()
{
	static RecipeMap singleton;
	return singleton;
}

RecipeMap::Key::Key(std::array<RE::FormID, MAX_INGREDIENTS> a_ingredients) :
	data(std::move(a_ingredients))
{
	std::sort(data.begin(), data.end());
}

bool RecipeMap::Key::operator==(const Key& rhs) const { return rhs.data == data; }

RecipeMap::Key::operator bool() { return data[0] != 0 || data[1] != 0; }

std::size_t RecipeMap::KeyHash::operator()(const Key& a_key) const
{
	std::size_t hash = 0;
	for (auto id : a_key.data)
	{
		hash ^= std::hash<uint32_t>{}(id) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
	}
	return hash;
}
