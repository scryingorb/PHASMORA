#include "PluginProcessor.h"
#include "Presets/GhostPresetManager.h"
#include <fstream>
#include <juce_cryptography/juce_cryptography.h>
#include <iostream>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace {
void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
void set(GhostAudioProcessor& p,const char* id,float value){p.apvts.getParameter(id)->setValueNotifyingHost(value);}
}
int main(int argc,char** argv)
{
 juce::ScopedJuceInitialiser_GUI gui;
 try {
  const bool capture=argc>1 && juce::String(argv[1])=="--capture";
  const auto folder=juce::File::getCurrentWorkingDirectory().getChildFile("BrandingBaseline");
  const juce::File fixtures(GHOST_BRANDING_FIXTURES);
  check(folder.createDirectory().wasOk(),"baseline directory");
  for(int held=0;held<2;++held) {
   GhostAudioProcessor p; const float values[]={.82f,.60f,.50f,.42f,.25f,.45f,1.0f,static_cast<float>(held)};
   const auto stateFile=(capture ? folder : fixtures).getChildFile("legacy-state-"+juce::String(held)+".bin");
   juce::MemoryBlock state;
   if(capture) {
    for(size_t i=0;i<8;++i)set(p,GhostPresetManager::ids[i],values[i]);
    p.getStateInformation(state);check(stateFile.replaceWithData(state.getData(),state.getSize()),"write legacy state");
   } else {
    check(stateFile.loadFileAsData(state),"read pre-rename state");
    p.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
   }
   for(size_t i=0;i<8;++i)check(std::abs(p.apvts.getParameter(GhostPresetManager::ids[i])->getValue()-values[i])<1e-6,"all legacy values restore");
  }
  for(double rate:{44100.0,48000.0,88200.0,96000.0}) {
   GhostAudioProcessor p;juce::MemoryBlock state;
   check((capture ? folder : fixtures).getChildFile("legacy-state-0.bin").loadFileAsData(state),"legacy state exists");
   p.setStateInformation(state.getData(),static_cast<int>(state.getSize()));p.prepareToPlay(rate,256);
   auto path=folder.getChildFile("audio-"+juce::String(static_cast<int>(rate))+".f32");
   std::fstream file(path.getFullPathName().toStdString(),std::ios::binary|(capture ? std::ios::out|std::ios::trunc : std::ios::in));
   if(capture)check(file.is_open(),"baseline audio opened");
   juce::MemoryOutputStream rendered;
   juce::AudioBuffer<float> audio(2,256);juce::MidiBuffer midi;float expected[256];
   const int blocks=static_cast<int>(rate*16/256);bool audible=false;
   for(int block=0;block<blocks;++block){
    if(block==blocks/4)set(p,"freeze",1);
    if(block==blocks/2){set(p,"freeze",0);set(p,"ghost",1);set(p,"blur",1);set(p,"drift",1);set(p,"decay",1);}
    if(block==blocks*3/4)GhostPresetManager::load(p.apvts,3);
    for(int i=0;i<256;++i){auto v=static_cast<float>(.4*std::sin((block*256+i)*.031));audio.setSample(0,i,v);audio.setSample(1,i,-.7f*v);}
    p.processBlock(audio,midi);
    for(int ch=0;ch<2;++ch){
     const auto* samples=audio.getReadPointer(ch);rendered.write(samples,sizeof(expected));audible=audible||audio.getMagnitude(ch,0,256)>0.001f;
     if(capture)file.write(reinterpret_cast<const char*>(samples),sizeof(expected));
     else if(file.is_open()) {file.read(reinterpret_cast<char*>(expected),sizeof(expected));check(file.gcount()==sizeof(expected),"baseline complete");check(std::memcmp(samples,expected,sizeof(expected))==0,"renamed audio must be bit-identical");}
    }
   }
   check(audible,"comparison contains audio");
   if(!capture)check(juce::SHA256(rendered.getData(),rendered.getDataSize()).toHexString()==
       fixtures.getChildFile("audio-"+juce::String(static_cast<int>(rate))+".sha256").loadFileAsString().trim(),
       "pre-rename audio SHA-256 exact match");
   std::cout<<rate<<" Hz: "<<(capture?"captured pre-rename output":"BIT-IDENTICAL to pre-rename output")<<"; 16 seconds stereo, state/automation/reverse/freeze/presets.\n";
  }
  std::cout<<"Legacy state: all eight IDs and values verified with FREEZE off/on.\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
