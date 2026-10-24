// Settings.h — per-user preferences shared by every Seyir on the machine:
// tooltips on or off, and the folder you last loaded a Scala file from.
//
//   macOS:   ~/Library/Application Support/Seyir/Seyir.settings
//   Windows: %APPDATA%\Seyir\Seyir.settings
//   Linux:   ~/.config/Seyir/Seyir.settings
//
// Message thread only.
#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>

namespace seyir
{
struct SettingsStore
{
    SettingsStore();
    juce::PropertiesFile props;
};

class Settings
{
public:
    bool tooltipsEnabled() const;          // default: on
    void setTooltipsEnabled (bool);

    juce::File scalaFolder() const;
    void setScalaFolder (const juce::File&);

    static juce::File file();

private:
    juce::SharedResourcePointer<SettingsStore> store;
};
} // namespace seyir
