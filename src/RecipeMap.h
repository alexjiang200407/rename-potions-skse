#pragma once

class RecipeMap
{
private:
	static constexpr size_t   MAX_INGREDIENTS          = 3;
	static constexpr size_t   MIN_INGREDIENTS_REQUIRED = 2;
	static constexpr uint32_t SERIALIZATION_KEY        = 'RPSE';
	static constexpr uint32_t SERIALIZATION_TYPE       = 'REC';
	static constexpr uint32_t SERIALIZATION_VER        = 0;

	class KeyHash;

	class Key
	{
	public:
		friend class KeyHash;

		Key() = default;
		Key(std::array<RE::FormID, MAX_INGREDIENTS> a_ingredients);

		bool operator==(const Key& rhs) const;
		operator bool();

	private:
		std::array<RE::FormID, MAX_INGREDIENTS> data;
	};

	class KeyHash
	{
	public:
		std::size_t operator()(const Key& a_key) const;
	};

	Key GetIngredientKey(RE::CraftingSubMenus::AlchemyMenu* a_alchemyMenu);

	void DeserializeRecipes(SKSE::SerializationInterface* a_intfc);

	static void Serialize(SKSE::SerializationInterface* a_intfc);
	static void Revert(SKSE::SerializationInterface* a_intfc);
	static void Deserialize(SKSE::SerializationInterface* a_intfc);

public:
	void RegisterSerialization();

	void AddCurrentRecipeName(RE::CraftingSubMenus::AlchemyMenu* a_alchemyMenu, const char* a_name);

	const char* GetCurrentRecipeName(RE::CraftingSubMenus::AlchemyMenu* a_alchemyMenu);

	static RecipeMap& GetSingleton();

private:
	std::unordered_map<Key, std::string, KeyHash> recipes;
};
