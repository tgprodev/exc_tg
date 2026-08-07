/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "settings/settings_type.h"

namespace Settings {

// The "Exc" settings section: Exctg-fork-specific settings.
// Currently hosts the "Check for updates" action (Sparkle on macOS).
[[nodiscard]] Type ExcId();

} // namespace Settings
