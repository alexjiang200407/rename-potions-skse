#include "RecipeMap.h"

static constexpr std::uint32_t MAX_NAME_SIZE = 1024;

namespace Log
{
	static void InitializeLog()
	{
		auto path = logger::log_directory();
		if (!path)
		{
			SKSE::stl::report_and_fail("Failed to find standard logging directory"sv);
		}

		*path /= Version::PROJECT;
		*path += ".log"sv;
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);

		auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

		auto logLevel = spdlog::level::trace;

		log->set_level(logLevel);
		log->flush_on(logLevel);

		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("[%H:%M:%S:%e] [thread %t] [%l] %v"s);

		logger::info(FMT_STRING("{} v{}"), Version::PROJECT, Version::NAME);
	}

	static void PrintInventoryEntryInfo(RE::FormID a_formID)
	{
		auto* a_actor = RE::PlayerCharacter::GetSingleton();
		for (const auto& x :
		     a_actor->GetInventory([=](RE::TESBoundObject& obj) { return obj.formID == a_formID; }))
		{
			logger::info(
				"count: {}, display name: {}",
				x.second.first,
				x.second.second->GetDisplayName());

			auto* invEntry = x.second.second.get();

			if (!invEntry || !invEntry->extraLists)
			{
				logger::info("No extra lists!");
				continue;
			}

			for (auto it : *invEntry->extraLists)
			{
				logger::info(
					"name: {}, owner: 0x{:x}, linked: 0x{:x}",
					it->GetDisplayName(x.first),
					it->GetOwner() ? it->GetOwner()->formID : 0x0,
					it->GetLinkedRef(nullptr) ? it->GetLinkedRef(nullptr)->formID : 0x0);
			}
		}
	}
}

namespace RE
{
	static void RenameInventoryEntry(
		RE::InventoryEntryData* a_entryData,
		RE::ExtraDataList*      a_extraList,
		const char*             a_name)
	{
		using func_t = decltype(&RE::CraftingSubMenus::EnchantConstructMenu::RenameItem_Impl);
		static REL::Relocation<func_t> func{
			RE::Offset::CraftingSubMenus::EnchantConstructMenu::RenameItem
		};
		return func(
			(RE::CraftingSubMenus::
		         EnchantConstructMenu*)nullptr,  // Function does not use `this` in the implementation
			a_entryData,
			a_extraList,
			a_name);
	}

	static void CraftingSubmenu__RefreshItemCard(
		RE::CraftingSubMenus::CraftingSubMenu* a_self,
		RE::InventoryEntryData*                a_invEntryData)
	{
		REL::Relocation<decltype(CraftingSubmenu__RefreshItemCard)> func{
			RELOCATION_ID(50297, 51218)
		};
		func(a_self, a_invEntryData);
	};
}

namespace Util
{
	static RE::CraftingSubMenus::AlchemyMenu*
		GetAlchemyMenu(RE::CraftingMenu** a_craftingMenuOut = nullptr)
	{
		if (!RE::UI::GetSingleton())
			return nullptr;

		auto* craftingMenu = RE::UI::GetSingleton()->GetMenu<RE::CraftingMenu>().get();

		if (!craftingMenu)
			return nullptr;

		if (a_craftingMenuOut)
			*a_craftingMenuOut = craftingMenu;
		return skyrim_cast<RE::CraftingSubMenus::AlchemyMenu*>(craftingMenu->subMenu);
	}
}

namespace Hooks
{
	struct AlchemyMenu__ProcessUserEvent
	{
		static void Install()
		{
			stl::write_vfunc<RE::CraftingSubMenus::AlchemyMenu, 5, AlchemyMenu__ProcessUserEvent>();
		}

		static bool thunk(RE::CraftingSubMenus::AlchemyMenu* a_self, RE::BSFixedString* a_control)
		{
			if (a_control)
				logger::info("{}", a_control->c_str());
			else
				return func(a_self, a_control);

			if (*a_control == "YButton" && a_self->resultPotion)
			{
				std::array<RE::GFxValue, 2> args;
				args[0] = RE::GFxValue(a_self->resultPotion->fullName);
				args[1] = MAX_NAME_SIZE - 1;

				RE::ControlMap::GetSingleton()->AllowTextInput(true);

				if (!a_self->craftingMenu.Invoke("EditItemName", nullptr, args))
				{
					logger::error("Could not rename item");
					return false;
				}
				return true;
			}

			return func(a_self, a_control);
		}

		using func_t = decltype(thunk);
		static inline REL::Relocation<func_t> func;
	};

	struct Actor__AddItem
	{
		static void InventoryEntryAddTextDisplayExtraData(
			RE::InventoryEntryData*& a_entry,
			int                      a_count,
			RE::TESBoundObject*      a_obj,
			const char*              a_name)
		{
			if (a_entry && a_entry->extraLists)
			{
				for (const auto& list : *a_entry->extraLists)
				{
					if (list->HasType(RE::ExtraDataType::kTextDisplayData) &&
					    strcmp(list->GetDisplayName(a_obj), a_name) == 0)
					{
						// Might overflow
						list->SetCount(static_cast<uint16_t>(list->GetCount() + a_count));
						return;
					}
				}
			}

			auto* xData = RE::BSExtraData::Create<RE::ExtraTextDisplayData>();

			if (!xData)
			{
				logger::error("Could not allocate ExtraTextDisplayData");
				return;
			}

			xData->SetName(a_name);
			auto* newList = new RE::ExtraDataList();

			if (!newList)
			{
				logger::error("Could not allocate ExtraDataList");
				return;
			}

			newList->Add(xData);
			newList->SetCount(static_cast<uint16_t>(a_count));  // Might overflow
			a_entry->AddExtraList(newList);
		}

		static void thunk(
			RE::Actor*          a_actor,
			RE::TESBoundObject* a_item,
			RE::ExtraDataList*  a_extraDataList,
			int                 a_count,
			RE::TESObjectREFR*  a_fromRefr)
		{
			auto* alchemyMenu = Util::GetAlchemyMenu();

			if (!alchemyMenu || !alchemyMenu->resultPotionEntry)
			{
				logger::error("Alchemy menu or result potion not found from Actor__AddItem");
				func(a_actor, a_item, a_extraDataList, a_count, a_fromRefr);
				return;
			}

			logger::info("Before AddItem");
			Log::PrintInventoryEntryInfo(a_item->formID);
			func(a_actor, a_item, a_extraDataList, a_count, a_fromRefr);
			logger::info("After AddItem");
			Log::PrintInventoryEntryInfo(a_item->formID);

			// Same as default
			if (alchemyMenu->resultPotion->fullName ==
			    alchemyMenu->resultPotionEntry->GetDisplayName())
			{
				logger::info("Same as default");
				return;
			}

			if (auto* changes = a_actor->GetInventoryChanges(false); changes && changes->entryList)
			{
				for (auto& entry : *changes->entryList)
				{
					if (entry && entry->object == a_item)
					{
						InventoryEntryAddTextDisplayExtraData(
							entry,
							a_count,
							entry->object,
							alchemyMenu->resultPotionEntry->GetDisplayName());
					}
				}
			}

			logger::info("After Hook");
			Log::PrintInventoryEntryInfo(a_item->formID);
		}

		static void Install()
		{
			REL::Relocation<uintptr_t> target{
				RELOCATION_ID(50449, 51354),
				OFFSET(0x1CF, 0x1CA)
			};  // AlchemyMenu::HandleXButton
			stl::write_thunk_call<Actor__AddItem>(target.address());
		}

		using func_t = decltype(thunk);
		static inline REL::Relocation<func_t> func;
	};

	struct AddedPotionNotification
	{
		static void thunk(
			RE::TESBoundObject* a_boundObj,
			int                 a_count,
			bool                a_unk1,
			bool                a_unk2,
			const char*         a_name)
		{
			if (auto* alchemyMenu = Util::GetAlchemyMenu();
			    alchemyMenu && alchemyMenu->resultPotionEntry && alchemyMenu->resultPotion &&
			    alchemyMenu->resultPotion->fullName !=
			        alchemyMenu->resultPotionEntry->GetDisplayName())
			{
				func(
					a_boundObj,
					a_count,
					a_unk1,
					a_unk2,
					alchemyMenu->resultPotionEntry->GetDisplayName());
			}
			else
				func(a_boundObj, a_count, a_unk1, a_unk2, a_name);
		}

		static void Install()
		{
			REL::Relocation<uintptr_t> target{
				RELOCATION_ID(50449, 51354),
				OFFSET(0x22E, 0x229)
			};  // AlchemyMenu::HandleXButton
			stl::write_thunk_call<AddedPotionNotification>(target.address());
		}

		using func_t = decltype(thunk);
		static inline REL::Relocation<func_t> func;
	};

	struct AlchemyMenu__SetClearSelectionsButtonText
	{
		static bool thunk(
			RE::GFxValue::ObjectInterface* a_self,
			void*                          a_data,
			std::uint32_t                  a_idx,
			RE::GFxValue&                  a_val)
		{
			if (a_val.IsString())
			{
				const char* str = a_val.GetString();

				if (str && str[0] != '\0')
				{
					a_val.SetString("Rename Potion");  // Add localization
				}
			}
			return func(a_self, a_data, a_idx, a_val);
		}

		using func_t = decltype(thunk);
		static inline REL::Relocation<func_t> func;

		static void Install()
		{
			REL::Relocation<uintptr_t> target1{ RELOCATION_ID(50526, 51412),
				                                OFFSET(0x260, 0x2A7) };  // Button Appears
			stl::write_thunk_call<AlchemyMenu__SetClearSelectionsButtonText>(target1.address());

			REL::Relocation<uintptr_t> target2{ RELOCATION_ID(50324, 51239), 0x207 };  // Open Menu
			stl::write_thunk_call<AlchemyMenu__SetClearSelectionsButtonText>(target2.address());

			REL::Relocation<uintptr_t> target3{ RELOCATION_ID(50541, 51424),
				                                0x139 };  // Hover over an element
			stl::write_thunk_call<AlchemyMenu__SetClearSelectionsButtonText>(target3.address());
		}
	};

	struct AlchemyMenu__SetItemCardInfo
	{
		static bool thunk(RE::CraftingSubMenus::AlchemyMenu* a_self, RE::ItemCard* a_itemCard)
		{
			auto* alchemyMenu = Util::GetAlchemyMenu();

			if (alchemyMenu && (alchemyMenu->resultPotion || alchemyMenu->unknownPotion))
			{
				if (const auto* name = RecipeMap::GetSingleton().GetCurrentRecipeName(a_self);
				    name && a_itemCard->obj.HasMember("name"))
				{
					a_itemCard->obj.SetMember("name", name);
					RE::RenameInventoryEntry(a_self->resultPotionEntry, nullptr, name);
				}
			}

			return func(a_self, a_itemCard);
		}

		static void Install()
		{
			stl::write_vfunc<RE::CraftingSubMenus::AlchemyMenu, 7, AlchemyMenu__SetItemCardInfo>();
		}
		using func_t = decltype(thunk);
		static inline REL::Relocation<func_t> func;
	};

	static void InstallAll()
	{
		AlchemyMenu__ProcessUserEvent::Install();
		Actor__AddItem::Install();
		AddedPotionNotification::Install();
		AlchemyMenu__SetClearSelectionsButtonText::Install();
		AlchemyMenu__SetItemCardInfo::Install();
	}
};

class MenuEventHandler : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
{
public:
	static void Register()
	{
		static MenuEventHandler singleton;
		RE::UI::GetSingleton()->AddEventSink(&singleton);
	}

private:
	MenuEventHandler() = default;

	void HandleAlchemyMenuOpen(
		RE::CraftingMenu*                  a_craftingMenu,
		RE::CraftingSubMenus::AlchemyMenu* a_alchemyMenu)
	{
		if (!a_craftingMenu->fxDelegate)
		{
			logger::error("Could not find fxDelegate for crafting menu");
			return;
		}

		RE::FxDelegate::CallbackDefn callback;

		// Not sure what this does but uncommented causes crashes when exit
		// callback.handler.reset(craftingMenu);

		callback.callback = [](const RE::FxDelegateArgs& a_arg) {
			logger::info("Ended Item Renaming");

			if (a_arg.GetArgCount() != 2)
				return;

			if (!a_arg[0].IsBool() || !a_arg[1].IsString())
				return;

			bool        useNewName = a_arg[0].GetBool();
			const char* newName    = a_arg[1].GetString();

			RE::ControlMap::GetSingleton()->AllowTextInput(false);

			auto* alchemyMenu = Util::GetAlchemyMenu();

			if (!alchemyMenu || !alchemyMenu->resultPotionEntry)
			{
				logger::error(
					"Could not find alchemy menu and result potion from EndItemRename callback");
				return;
			}

			if (useNewName)
			{
				RenameInventoryEntry(alchemyMenu->resultPotionEntry, nullptr, newName);
				RecipeMap::GetSingleton().AddCurrentRecipeName(alchemyMenu, newName);
			}

			RE::CraftingSubmenu__RefreshItemCard(alchemyMenu, alchemyMenu->resultPotionEntry);
		};
		a_craftingMenu->fxDelegate->callbacks.Add("EndItemRename", callback);
		logger::info("{}", std::uintptr_t(&a_alchemyMenu->buttonText));
	}

	RE::BSEventNotifyControl ProcessEvent(
		const RE::MenuOpenCloseEvent* a_event,
		RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
	{
		logger::info(
			"{} menu `{}`",
			a_event->opening ? "Opening" : "Closing",
			a_event->menuName.c_str());

		if (a_event->menuName == RE::CraftingMenu::MENU_NAME)
		{
			RE::CraftingMenu* craftingMenu;
			auto*             alchemyMenu = Util::GetAlchemyMenu(&craftingMenu);

			if (!alchemyMenu)
				return RE::BSEventNotifyControl::kContinue;

			if (a_event->opening)
			{
				HandleAlchemyMenuOpen(craftingMenu, alchemyMenu);
			}
			else
			{
				if (craftingMenu->fxDelegate)
					craftingMenu->fxDelegate->callbacks.Remove("EndItemRename");
				else
					logger::warn("Could not find fxDelegate for crafting menu");
			}
		}

		return RE::BSEventNotifyControl::kContinue;
	}
};

#ifdef SKYRIM_AE
extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []() {
	SKSE::PluginVersionData v;
	v.PluginVersion(Version::MAJOR);
	v.PluginName(Version::PROJECT);
	v.AuthorName("shdowraithe101");
	v.UsesAddressLibrary();
	v.UsesUpdatedStructs();
	v.CompatibleVersions({ SKSE::RUNTIME_LATEST });

	return v;
}();
#else
extern "C" DLLEXPORT bool SKSEAPI
	SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = Version::PROJECT_C_STR;
	a_info->version = Version::MAJOR;

	if (a_skse->IsEditor())
	{
		logger::critical("Loaded in editor, marking as incompatible"sv);
		return false;
	}

	const auto ver = a_skse->RuntimeVersion();
	if (ver
#	ifndef SKYRIMVR
	    < SKSE::RUNTIME_1_5_39
#	else
	    > SKSE::RUNTIME_VR_1_4_15_1
#	endif
	)
	{
		logger::critical(FMT_STRING("Unsupported runtime version {}"), ver.string());
		return false;
	}

	return true;
}
#endif

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);
	Log::InitializeLog();

	auto runtimeVer    = a_skse->RuntimeVersion();
	auto runtimeVerStr = runtimeVer.string();
	logger::info("Game version : {}", runtimeVerStr);

	Hooks::InstallAll();

	SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* message) {
		if (message->type == SKSE::MessagingInterface::kDataLoaded)
		{
			RE::ConsoleLog::GetSingleton()->Print("%s has been loaded", Version::PROJECT_C_STR);
			MenuEventHandler::Register();
		}
		else if (message->type == SKSE::MessagingInterface::kPostLoad)
		{
			RecipeMap::GetSingleton().RegisterSerialization();
		}
	});

	return true;
}
