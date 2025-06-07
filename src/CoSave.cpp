#include "CoSave.h"

bool CoSave::Write(SKSE::SerializationInterface* a_intfc, const std::string& a_data)
{
	const std::size_t size = a_data.length();

	if (size > std::numeric_limits<uint32_t>::max())
	{
		logger::error("String size too large");
		return false;
	}

	return a_intfc->WriteRecordData(size) &&
	       a_intfc->WriteRecordData(a_data.data(), static_cast<std::uint32_t>(size));
}

bool CoSave::Read(SKSE::SerializationInterface* a_intfc, std::string& a_result)
{
	std::size_t size = 0;
	if (!a_intfc->ReadRecordData(size))
	{
		return false;
	}
	if (size > 0)
	{
		if (size > std::numeric_limits<uint32_t>::max())
		{
			logger::error("String size too large");
			return false;
		}

		a_result.resize(size);
		if (!a_intfc->ReadRecordData(a_result.data(), static_cast<std::uint32_t>(size)))
		{
			return false;
		}
	}
	else
	{
		a_result = "";
	}
	return true;
}

bool CoSave::Load(SKSE::SerializationInterface* a_intfc, RE::FormID& a_formID)
{
	RE::FormID tempID = 0;
	a_formID          = 0;

	if (!CoSave::Read(a_intfc, tempID))
	{
		return false;
	}

	if (!tempID)
	{
		return false;
	}

	a_formID = tempID;  // save the originally read a_formID

	if (!a_intfc->ResolveFormID(tempID, tempID))
	{
		return false;
	}

	a_formID = tempID;  // save the resolved a_formID
	return true;
}
