#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
FilterAudioProcessor::FilterAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ), apvts(*this, nullptr, "Parameters", mParameterLayout.createParameterLayout())
#endif
{
    // Register the processor as a listener to the parameters
    apvts.addParameterListener("CUTOFF", this);
    apvts.addParameterListener("RESONANCE", this);
    apvts.addParameterListener("GAIN", this);
    apvts.addParameterListener("FILTER", this);
    
}

FilterAudioProcessor::~FilterAudioProcessor()
{
    apvts.removeParameterListener("CUTOFF", this);
    apvts.removeParameterListener("RESONANCE", this);
    apvts.removeParameterListener("GAIN", this);
    apvts.removeParameterListener("FILTER", this);

}






//==============================================================================
const juce::String FilterAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool FilterAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool FilterAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool FilterAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double FilterAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int FilterAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int FilterAudioProcessor::getCurrentProgram()
{
    return 0;
}

void FilterAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String FilterAudioProcessor::getProgramName (int index)
{
    return {};
}

void FilterAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void FilterAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    
    mFilter[0].setSamplingRate(sampleRate);
    mFilter[1].setSamplingRate(sampleRate);
    
}

void FilterAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool FilterAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void FilterAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

   
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer (channel);

      
        for (size_t i = 0; i < buffer.getNumSamples(); i++)
        {
            mFilter[channel].processSample(channelData[i]);
        }

    }
}

//==============================================================================
bool FilterAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* FilterAudioProcessor::createEditor()
{
     return new WrappedRasterAudioProcessorEditor(*this);
}

//==============================================================================

void FilterAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // Serialize the APVTS state directly
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void FilterAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // Deserialize the APVTS state directly
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));

    if (xml != nullptr)
    {
        juce::ValueTree tree = juce::ValueTree::fromXml(*xml);
        apvts.replaceState(tree);
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FilterAudioProcessor();
}

void FilterAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == "CUTOFF")
    {

        mFilter[0].setCutoffFrequency(newValue);
        mFilter[1].setCutoffFrequency(newValue);  
    }
    else if (parameterID == "RESONANCE")
    {
        mFilter[0].setResonans(newValue);
        mFilter[1].setResonans(newValue);
    }

    else if (parameterID == "GAIN")
    {
        mFilter[0].setGain(newValue);
        mFilter[1].setGain(newValue);
    }

    else if (parameterID == "FILTER")
    {
        mFilter[0].setFilterType(newValue);
        mFilter[1].setFilterType(newValue);   
    }
}

// Return coefficients snapshot from selected channel
std::array<double, MultiFilter::COEFF_COUNT> FilterAudioProcessor::getFilterCoefficients(int channel) const
{
    if (channel < 0 || channel > 1)
        channel = 0;
    return mFilter[channel].getCoefficients();
}
