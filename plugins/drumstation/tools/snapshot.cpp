// Renders the editor to a PNG for the manual: drumstation_snapshot out.png [width]
// Built only with -DVDC_BUILD_DOC_TOOLS=ON.
#include "../Source/PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "drumstation-panel.png");
    const int width = argc > 2 ? std::atoi (argv[2]) : 2000;

    DrumstationProcessor proc;
    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
    editor->setSize (width, juce::roundToInt (width * (double) DrumstationPanel::designHeight / DrumstationPanel::designWidth));
    juce::MessageManager::getInstance()->runDispatchLoopUntil (300);

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    out.deleteFile();
    juce::FileOutputStream stream (out);
    juce::PNGImageFormat().writeImageToStream (image, stream);
    std::printf ("wrote %s (%d x %d)\n", out.getFullPathName().toRawUTF8(), image.getWidth(), image.getHeight());
    editor.reset();
    return 0;
}
