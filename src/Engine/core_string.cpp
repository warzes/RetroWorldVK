#include "stdafx.h"
#include "core_string.h"
//=============================================================================
void core::AddUnique(std::vector<std::string>& arr, const std::string& val)
{
	bool found = false;
	for (size_t i = 0; i < arr.size(); ++i)
	{
		if (arr[i] == val)
		{
			found = true;
			break;
		}
	}
	if (!found)
		arr.push_back(val);
}
//=============================================================================
void core::AddUnique(std::vector<const char*>& arr, const char* val)
{
	bool found = false;
	for (size_t i = 0; i < arr.size(); ++i)
	{
		if (strcmp(arr[i], val) == 0)
		{
			found = true;
			break;
		}
	}
	if (!found)
		arr.push_back(val);
}
//=============================================================================