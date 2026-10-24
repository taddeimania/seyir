#include "Settings.h"

namespace seyir
{
namespace
{
juce::PropertiesFile::Options options()
{
    juce::PropertiesFile::Options o;
    o.applicationName         = "Seyir";
    o.filenameSuffix          = ".settings";
   #if JUCE_LINUX
    o.folderName              = ".config/Seyir";
   #else
    o.folderName              = "Seyir";
   #endif
    o.osxLibrarySubFolder     = "Application Support";
    o.storageFormat           = juce::PropertiesFile::storeAsXML;
    o.millisecondsBeforeSaving = -1;   // every change is saved explicitly
    return o;
}

constexpr const char* kTooltips = "tooltips";
constexpr const char* kScala    = "scalaFolder";
} // namespace

SettingsStore::SettingsStore() : props (options()) {}

juce::File Settings::file() { return options().getDefaultFile(); }

bool Settings::tooltipsEnabled() const
{
    return store->props.getBoolValue (kTooltips, true);
}

void Settings::setTooltipsEnabled (bool on)
{
    store->props.setValue (kTooltips, on);
    store->props.saveIfNeeded();
}

juce::File Settings::scalaFolder() const
{
    const auto path = store->props.getValue (kScala);
    juce::File f (path);
    if (path.isNotEmpty() && f.isDirectory())
        return f;
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory);
}

void Settings::setScalaFolder (const juce::File& f)
{
    store->props.setValue (kScala, f.getFullPathName());
    store->props.saveIfNeeded();
}
} // namespace seyir
