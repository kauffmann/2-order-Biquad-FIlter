/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

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
    // Construct MultiFilter instances with FilterCoefficients reference
    mFilterLeft = std::make_unique<MultiFilter>(mFilterCoefficients);
    mFilterRight = std::make_unique<MultiFilter>(mFilterCoefficients);
    mFilter[0] = mFilterLeft.get();
    mFilter[1] = mFilterRight.get();
    
    // Register the processor as a listener to the parameters
    apvts.addParameterListener("CUTOFF", this);
    apvts.addParameterListener("RESONANCE", this);
    apvts.addParameterListener("GAIN", this);
    apvts.addParameterListener("FILTER", this);
    
    // Initialize FilterCoefficients with default values
    mFilterCoefficients.update(44100.0, 1000.0, 0.707, 0.0, 0);
}

FilterAudioProcessor::~FilterAudioProcessor()
{
    // unique_ptr will automatically call destructors
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
    
    mFilter[0]->setSamplingRate(sampleRate);
    mFilter[1]->setSamplingRate(sampleRate);
    
    // Update the coefficient cache with the current parameters and new sample rate
    auto* cutoffParam = apvts.getParameter("CUTOFF");
    auto* resonanceParam = apvts.getParameter("RESONANCE");
    auto* gainParam = apvts.getParameter("GAIN");
    auto* filterParam = apvts.getParameter("FILTER");
    
    float cutoff = cutoffParam->getValue();
    float resonance = resonanceParam->getValue();
    float gain = gainParam->getValue();
    float filterType = filterParam->getValue();
    
    mFilterCoefficients.update(sampleRate, cutoff, resonance, gain, static_cast<int>(filterType));
    
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
            mFilter[channel]->processSample(channelData[i]);
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



//void FilterAudioProcessor::getStateInformation(
//    juce::MemoryBlock& destData) {
//
//
//    juce::ValueTree params("Params");
//
//    for (auto& param : getParameters())
//    {
//        juce::ValueTree paramTree(getParamID(param));
//        paramTree.setProperty("Value", param->getValue(), nullptr);
//        params.appendChild(paramTree, nullptr);
//
//       
//    }
//
//
//    copyXmlToBinary(*params.createXml(), destData);
//
//
//}
//
//void FilterAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
//{
//
//
//    auto xml = getXmlFromBinary(data, sizeInBytes);
//
//    if (xml != nullptr)
//    {
//        auto preset = juce::ValueTree::fromXml(*xml);
//
//        for (auto& param : getParameters())
//        {
//            
//            auto paramTree = preset.getChildWithName(getParamID(param));
//
//            if (paramTree.isValid())
//                param->setValueNotifyingHost(paramTree["Value"]);
//        }
//    }
//
//    
//
//    
//}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FilterAudioProcessor();
}

void FilterAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    // Get current values from APVTS for all parameters
    auto* cutoffParam = apvts.getParameter("CUTOFF");
    auto* resonanceParam = apvts.getParameter("RESONANCE");
    auto* gainParam = apvts.getParameter("GAIN");
    auto* filterParam = apvts.getParameter("FILTER");
    
    float cutoff = cutoffParam->getValue();
    float resonance = resonanceParam->getValue();
    float gain = gainParam->getValue();
    float filterType = filterParam->getValue();
    
    // Update the shared coefficient cache
    // Note: We use the last known sample rate (from prepareToPlay)
    // This will be set to a default value initially and updated when prepareToPlay is called
    mFilterCoefficients.update(mFilter[0]->getSamplingRate(), cutoff, resonance, gain, static_cast<int>(filterType));
    
    // Also update the per-channel smoothed cutoff values in each filter
    mFilter[0]->setCutoffFrequency(cutoff);
    mFilter[1]->setCutoffFrequency(cutoff);
    mFilter[0]->setResonans(resonance);
    mFilter[1]->setResonans(resonance);
    mFilter[0]->setGain(gain);
    mFilter[1]->setGain(gain);
    mFilter[0]->setFilterType(static_cast<int>(filterType));
    mFilter[1]->setFilterType(static_cast<int>(filterType));
}