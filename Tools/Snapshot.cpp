// Snapshot.cpp — dev tool: run Seyir without a host, render its panel to a
// PNG and, optionally, what it played to a WAV.
//   SeyirSnapshot out.png [out.wav] [seconds] [mode] [voice] [checks]
// It walks freely (RUN) at 100 BPM, so the path, ladder and LCD have
// something to show.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 2)
    {
        std::printf ("usage: SeyirSnapshot out.png [out.wav] [seconds] [mode] [voice]\n");
        return 1;
    }
    const double rate = 44100.0;
    const int block = 512;
    const double seconds = argc > 3 ? juce::String (argv[3]).getDoubleValue() : 8.0;

    SeyirProcessor proc;
    proc.setPlayConfigDetails (0, 2, rate, block);
    proc.prepareToPlay (rate, block);
    if (argc > 4) proc.selectMode (juce::String (argv[4]).getIntValue());
    if (argc > 5) proc.setParam (seyir::ids::voice, (float) juce::String (argv[5]).getIntValue());
    proc.setParam (seyir::ids::run, 1.0f);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);

    juce::AudioBuffer<float> audio (2, block);
    juce::AudioBuffer<float> all (2, (int) (rate * seconds) + block);
    juce::MidiBuffer midi;
    int written = 0;
    float peak = 0.0f;
    while (written < (int) (rate * seconds))
    {
        midi.clear();
        proc.processBlock (audio, midi);
        for (int ch = 0; ch < 2; ++ch)
            all.copyFrom (ch, written, audio, ch, 0, block);
        peak = juce::jmax (peak, audio.getMagnitude (0, block));
        written += block;
    }
    std::printf ("rendered %.1f s, peak %.3f, rms %.4f\n", written / rate, peak, all.getRMSLevel (0, 0, written));

    if (argc > 2)
    {
        juce::File wav (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        wav.deleteFile();
        juce::WavAudioFormat fmt;
        std::unique_ptr<juce::AudioFormatWriter> w (fmt.createWriterFor (new juce::FileOutputStream (wav), rate, 2, 24, {}, 0));
        if (w)
            w->writeFromAudioSampleBuffer (all, 0, written);
        std::printf ("wrote %s\n", wav.getFullPathName().toRawUTF8());
    }

    // Quick checks of the key modes: SeyirSnapshot out.png out.wav 8 1 0 checks
    if (argc > 6)
    {
        auto run = [&] (int blocks, std::function<void (juce::MidiBuffer&)> first)
        {
            for (int i = 0; i < blocks; ++i)
            {
                midi.clear();
                if (i == 0 && first) first (midi);
                proc.processBlock (audio, midi);
            }
        };
        std::vector<TrailNote> trail;
        proc.collectTrail (trail);
        trail.clear();

        proc.setParam (seyir::ids::play, (float) seyir::ids::Play);
        run (20, [] (juce::MidiBuffer& m) { m.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 0); });
        std::printf ("PLAY: key E3 (64) sounds degree %d at %.1f cents\n", proc.soundingDegree(), proc.soundingCents());
        run (20, [] (juce::MidiBuffer& m) { m.addEvent (juce::MidiMessage::noteOff (1, 64), 0); });
        std::printf ("PLAY: after release, degree %d\n", proc.soundingDegree());

        proc.setParam (seyir::ids::play, (float) seyir::ids::Follow);
        proc.setParam (seyir::ids::run, 0.0f);
        run (200, [] (juce::MidiBuffer& m) { m.addEvent (juce::MidiMessage::noteOn (1, 67, (juce::uint8) 100), 10); });
        proc.collectTrail (trail);
        std::printf ("FOLLOW: holding G3 for %.1f s played %d notes, first at %.0f cents\n", 200 * block / rate,
                     (int) trail.size(), trail.empty() ? -1.0 : trail.front().cents);
        run (20, [] (juce::MidiBuffer& m) { m.addEvent (juce::MidiMessage::noteOff (1, 67), 0); });
        trail.clear();
        run (100, nullptr);
        proc.collectTrail (trail);
        std::printf ("FOLLOW: after release, %d more notes\n", (int) trail.size());

        proc.setParam (seyir::ids::play, (float) seyir::ids::Walk);
        proc.setParam (seyir::ids::run, 1.0f);
    }

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize (SeyirPanel::designW, SeyirPanel::designH);
    // Keep walking while the panel's timer collects the path.
    for (int i = 0; i < 40; ++i)
    {
        midi.clear();
        for (int j = 0; j < 4; ++j) proc.processBlock (audio, midi);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (35);
    }
    const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);

    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    out.deleteFile();
    juce::FileOutputStream stream (out);
    juce::PNGImageFormat().writeImageToStream (img, stream);
    std::printf ("wrote %s\n", out.getFullPathName().toRawUTF8());
    ed.reset();
    return 0;
}
