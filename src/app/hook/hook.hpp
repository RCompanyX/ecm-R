#pragma once

#include <psapi.h>

#include "defs.hpp"
#include "global.hpp"

class hook
{
public:
	static bool IsPackageLoaded(const char* package)
	{
		return package != nullptr && ((bool(*)(char*))0x0052CF60)(const_cast<char*>(package));
	}

	static bool IsFrontendChyronReady()
	{
		return IsPackageLoaded("UI_PC_Help_Bar.fng") ||
			IsPackageLoaded("GarageMain.fng") ||
			IsPackageLoaded("Chyron_FE.fng");
	}

	static bool SummonChyron(const char* title, const char* artist, const char* album)
	{
		switch (global::game)
		{
		case game_t::NFSU2:
			if (game_state == GameFlowState::None ||
				game_state == GameFlowState::LoadingFrontend ||
				game_state == GameFlowState::LoadingRegion ||
				game_state == GameFlowState::LoadingTrack ||
				game_state == GameFlowState::ExitDemoDisc)
			{
				return false;
			}

			if ((game_state == GameFlowState::UnloadingFrontend || game_state == GameFlowState::InFrontend) && !IsFrontendChyronReady())
			{
				return false;
			}

			reinterpret_cast<void(__cdecl*)(const char*, const char*, const char*)>(0x004AC950)(title, artist, album);
			return true;
		}

		return false;
	}

	static void HideChyron()
	{
		switch (global::game)
		{
		case game_t::NFSU2:
		{
			constexpr auto remove_fng_from_ui_object = 0x005379A0;
			const auto remove_package = [remove_fng_from_ui_object](char* package)
			{
				if (((bool(*)(char*))0x0052CF60)(package))
				{
					((void(__cdecl*)(char*))remove_fng_from_ui_object)(package);
				}
			};

			remove_package(const_cast<char*>("Chyron_FE.fng"));
			remove_package(const_cast<char*>("Chyron_IG.fng"));
			break;
		}
		}
	}

	template <typename T> static void jump(std::uint32_t address, T function)
	{
		*(std::uint8_t*)(address) = 0xE9;
		*(std::uint32_t*)(address + 1) = (std::uint32_t(function) - address - 5);
	};

	template <typename T> static void retn_value(std::uint32_t address, T value)
	{
		*(std::uint8_t*)(address) = 0xB8;
		*(std::uint32_t*)(address + 1) = std::uint32_t(value);
		*(std::uint8_t*)(address + 5) = 0xC3;
	}

	static void retn(std::uint32_t address)
	{
		*(std::uint8_t*)(address) = 0xC3;
	};
};
