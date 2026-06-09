#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <iostream>

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto output = argc > 1
        ? juce::File (argv[1])
        : juce::File::getCurrentWorkingDirectory()
              .getChildFile ("docs")
              .getChildFile ("assets")
              .getChildFile ("vst3-screenshot.png");

    if (! output.getParentDirectory().createDirectory())
    {
        std::cerr << "Could not create output directory: " << output.getParentDirectory().getFullPathName()
                  << '\n';
        return 1;
    }

    HemerodromosDroneAudioProcessor processor;
    processor.prepareToPlay (44100.0, 512);

    HemerodromosDroneAudioProcessorEditor editor (processor);
    editor.setVisible (true);
    editor.resized();

    juce::Timer::callPendingTimersSynchronously();

    juce::Image image (juce::Image::RGB, editor.getWidth(), editor.getHeight(), true);
    juce::Graphics graphics (image);
    editor.paintEntireComponent (graphics, true);

    juce::PNGImageFormat png;
    if (auto stream = output.createOutputStream())
    {
        if (png.writeImageToStream (image, *stream))
        {
            std::cout << "Wrote " << output.getFullPathName() << '\n';
            return 0;
        }
    }

    std::cerr << "Could not write screenshot: " << output.getFullPathName() << '\n';
    return 1;
}
