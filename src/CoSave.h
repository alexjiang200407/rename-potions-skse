#pragma once

namespace CoSave
{
	template <typename T>
	bool Write(SKSE::SerializationInterface* a_intfc, const T& a_data)
	{
		return a_intfc->WriteRecordData(&a_data, sizeof(T));
	}

	bool Write(SKSE::SerializationInterface* a_intfc, const std::string& a_data);

	template <typename T>
	bool Read(SKSE::SerializationInterface* a_interface, T& a_result)
	{
		return a_interface->ReadRecordData(&a_result, sizeof(T));
	}

	bool Read(SKSE::SerializationInterface* a_intfc, std::string& a_result);

	bool Load(SKSE::SerializationInterface* a_intfc, RE::FormID& a_formID);

	template <typename T>
	bool Load(SKSE::SerializationInterface* a_intfc, T*& a_out, RE::FormID& a_formID)
	{
		a_out = nullptr;

		if (!Load(a_intfc, a_formID))
		{
			return false;
		}

		if (const auto form = RE::TESForm::LookupByID<T>(a_formID); form)
		{
			a_out = form;
			return true;
		}

		return false;
	}
}
