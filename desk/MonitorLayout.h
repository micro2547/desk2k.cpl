#pragma once
#include "pch.h"

// Non-owning arrangement view over the Settings page's staged configuration.
struct MonitorLayoutItem
{
	int number = 0;
	RECT desktop = {};
	bool attached = false;
	bool primary = false;
	DWORD orientation = DMDO_DEFAULT;
};

enum class MonitorLayoutAction
{
	Select,
	Move,
	Rotate,
	MakePrimary,
	ToggleAttached
};

constexpr UINT MLN_ACTION = NM_FIRST - 80;
struct MonitorLayoutNotification
{
	NMHDR header = {};
	MonitorLayoutAction action = MonitorLayoutAction::Select;
	size_t index = 0;
	POINT position = {};
	DWORD orientation = DMDO_DEFAULT;
};
