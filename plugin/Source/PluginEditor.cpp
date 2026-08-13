/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginAssets.h"
#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
PluginEditor::PluginEditor (FilterAudioProcessor& p)
    : processorRef (p),
      mCutoffSlider (p.getApvts(), "CUTOFF", "Cutoff"),
      mResonanceSlider (p.getApvts(), "RESONANCE", "Resonance"),
      mGainSlider (p.getApvts(), "GAIN", "Self Gain"),
      mFilterComboBox(p.getApvts(),"FILTER"),
      mFilterCurve(p)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    //setSize (400, 300);
    backGround = juce::ImageCache::getFromMemory (BinaryData::background_final_png, BinaryData::background_final_pngSize);

    //setSize(750, 500);
    
    // Set the custom look and feel
    setLookAndFeel(&customLookAndFeel);

    // Add sliders to the editor
    addAndMakeVisible(mCutoffSlider);
    addAndMakeVisible(mResonanceSlider);
    addAndMakeVisible(mGainSlider);
    
    // Add filter curve visualization
    addAndMakeVisible(mFilterCurve);

    // Add filter combobox
    mFilterComboBox.addItemList(juce::StringArray{ "LowPass", "HighPass", "BPF", "Notch", "HighShelf", "LowShelf" }, 1);
    mFilterComboBox.setSelectedItemIndex(0);
    addAndMakeVisible(mFilterComboBox);

    cutoffLabel.setJustificationType(juce::Justification::centred);
    cutoffLabel.setInterceptsMouseClicks(false, false);
    cutoffLabel.setFont(customLookAndFeel.getSliderLabelFont());

    addAndMakeVisible(cutoffLabel);

    resonanceLabel.setJustificationType(juce::Justification::centred);
    resonanceLabel.setInterceptsMouseClicks(false, false);
    resonanceLabel.setFont(customLookAndFeel.getSliderLabelFont());
    addAndMakeVisible(resonanceLabel);

    gainLabel.setJustificationType(juce::Justification::centred);
    gainLabel.setInterceptsMouseClicks(false, false);
    gainLabel.setFont(customLookAndFeel.getSliderLabelFont());
    addAndMakeVisible(gainLabel);

    filterLabel.setJustificationType(juce::Justification::centred);
    filterLabel.setInterceptsMouseClicks(false, false);
    filterLabel.setFont(customLookAndFeel.getSliderLabelFont());
    addAndMakeVisible(filterLabel);
}

PluginEditor::~PluginEditor()
{
}

//==============================================================================
void PluginEditor::paint (juce::Graphics& g)
{
    //DBG( "Hello" << mFilterComboBox.getSelectedId() );

    if (backGround.isValid())
        g.drawImageWithin(backGround, 0, 0, getWidth(), getHeight(), juce::RectanglePlacement::stretchToFit, false);

    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    //g.drawFittedText ("Hello World!", getLocalBounds(), juce::Justification::centred, 1);
}

void PluginEditor::resized()
{
    const int slider_size = 70;

    int sliderXpos = static_cast<int>(120);
    int deltaSlidersXpos = 86;

    // Position filter curve visualization at (35, 16) with size 430x170
    mFilterCurve.setBounds(35, 16, 430, 170);

    mCutoffSlider.setBounds( static_cast<int>( sliderXpos - deltaSlidersXpos),
                         static_cast<int>( (getHeight() * 0.6422)),
                         slider_size, slider_size);


    //JUCE_LIVE_CONSTANT(0.50)
    mResonanceSlider.setBounds(static_cast<int>( sliderXpos * 2 - deltaSlidersXpos),
                         static_cast<int>( (getHeight() * 0.6422)),
                         slider_size, slider_size);


    mGainSlider.setBounds( static_cast<int>( sliderXpos * 3 - deltaSlidersXpos),
                         static_cast<int>( (getHeight() * 0.6422)),
                         slider_size, slider_size);

    cutoffLabel.setBounds(static_cast<int>(sliderXpos - deltaSlidersXpos),
                        static_cast<int>(getHeight() * 0.6422) + slider_size + 5,
                        slider_size, 20);

    resonanceLabel.setBounds(static_cast<int>(sliderXpos * 2 - deltaSlidersXpos),
                           static_cast<int>(getHeight() * 0.6422) + slider_size + 5,
                           slider_size, 20);

    gainLabel.setBounds(static_cast<int>(sliderXpos * 3 - deltaSlidersXpos),
                      static_cast<int>(getHeight() * 0.6422) + slider_size + 5,
                      slider_size, 20);

    mFilterComboBox.setBounds(static_cast<int>(sliderXpos * 4 - 97),
                            static_cast<int>(getHeight() * 0.7222),
                            slider_size * 1.245, 25);

    filterLabel.setBounds(static_cast<int>(sliderXpos * 4 - deltaSlidersXpos),
                         static_cast<int>(getHeight() * 0.5531) + 25 + 5,
                         slider_size, 20);


}



// Wrapper Editor implementation

WrappedRasterAudioProcessorEditor::WrappedRasterAudioProcessorEditor(FilterAudioProcessor& p)
: AudioProcessorEditor(p), rasterComponent(p), mProcessor(p)
{

    addAndMakeVisible(rasterComponent);



    if (auto* constrainer = getConstrainer())
    {
        constrainer->setFixedAspectRatio(static_cast<double> (originalWidth) / static_cast<double> (originalHeight)); //impotant, is used when resized. w ad h must always fit this ratio
        constrainer->setSizeLimits(originalWidth / 4, originalHeight / 4, originalWidth * 2, originalHeight * 2);
    }

    double sizeRatio = mProcessor.getResizeFactor();



    setResizable(true, true);
    setSize(static_cast<int> (originalWidth * sizeRatio), static_cast<int> (originalHeight * sizeRatio));
    

}

void WrappedRasterAudioProcessorEditor::resized()
{
    const auto scaleFactor = static_cast<float> (getWidth()) / originalWidth;
    mProcessor.setResizeFactor(scaleFactor);


    rasterComponent.setTransform(juce::AffineTransform::scale(scaleFactor)); // this is the actual scale transforming
    rasterComponent.setBounds(0, 0, originalWidth, originalHeight); // unintuitive, but need this as transform use original bounds to transform from.

}