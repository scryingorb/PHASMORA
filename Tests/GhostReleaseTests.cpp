#include "PluginProcessor.h"
#include "Presets/GhostPresetManager.h"
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <windows.h>
#include <psapi.h>

namespace { thread_local bool countAllocations = false; thread_local size_t callbackAllocations = 0; }
void* operator new(std::size_t size)
{
    if (countAllocations) ++callbackAllocations;
    if (auto* p = std::malloc(size == 0 ? 1 : size)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
namespace {
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void set(GhostAudioProcessor& p, const char* id, float value)
{ p.apvts.getParameter(id)->setValueNotifyingHost(value); }
void maximum(GhostAudioProcessor& p, bool reverse, bool freeze)
{
    for (auto* id : {"ghost","memory","decay","blur","drift","mix"}) set(p,id,1);
    set(p,"reverse",reverse ? 1.0f : 0.0f); set(p,"freeze",freeze ? 1.0f : 0.0f);
}
size_t privateBytes()
{
    PROCESS_MEMORY_COUNTERS_EX counters {}; counters.cb=sizeof(counters);
    check(GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),sizeof(counters)) != 0,"memory query");
    return counters.PrivateUsage;
}
void process(GhostAudioProcessor& p, juce::AudioBuffer<float>& a, juce::MidiBuffer& midi)
{
    countAllocations=true; p.processBlock(a,midi); countAllocations=false;
    check(callbackAllocations==0,"audio callback C++ allocation");
}
void tailMetadata()
{
    GhostAudioProcessor p;
    check(p.getName()=="PHASMORA" && juce::String(JucePlugin_Manufacturer)=="PHASMORA","public branding");
    check(juce::String(JucePlugin_VersionString)=="1.0.0","release version");
    check(JucePlugin_ManufacturerCode==0x47687374 && JucePlugin_PluginCode==0x47683031,"stable identifiers");
    check(!JucePlugin_IsSynth && !JucePlugin_WantsMidiInput,"stereo effect metadata");
   #ifdef NDEBUG
    std::cout << "Release NDEBUG enabled.\n";
   #else
    throw std::runtime_error("release tests require NDEBUG");
   #endif
    check(p.getParameters().size()==8,"eight stable parameter IDs");
    const std::array<float,8> defaults {65,40,40,20,10,35,0,0};
    for (size_t i=0;i<defaults.size();++i)
    {
        auto* parameter=p.apvts.getParameter(GhostPresetManager::ids[i]);
        check(parameter && parameter->isAutomatable(),"stable automatable parameter");
        check(std::abs(parameter->convertFrom0to1(parameter->getDefaultValue())-defaults[i])<0.0001f,"release defaults");
    }
    std::cout << "Default finite tail seconds=" << p.getTailLengthSeconds() << '\n';
    set(p,"decay",0); check(p.getTailLengthSeconds()==12,"zero decay reserves history");
    maximum(p,false,false); check(p.getTailLengthSeconds()==686,"maximum practical finite tail");
    for (int d=0;d<=100;++d)
    {
        set(p,"decay",d*0.01f);
        auto t=p.getTailLengthSeconds(); check(std::isfinite(t)&&t>=12&&t<=686,"finite decay range");
    }
    set(p,"freeze",1); check(std::isinf(p.getTailLengthSeconds())&&p.getTailLengthSeconds()>0,"positive infinite freeze sentinel");
    juce::MemoryBlock state; p.getStateInformation(state);
    GhostAudioProcessor restored; restored.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
    check(std::isinf(restored.getTailLengthSeconds()),"restored freeze metadata");
    set(restored,"freeze",0); check(restored.getTailLengthSeconds()==686,"freeze release metadata");
    set(restored,"ghost",0); check(restored.getTailLengthSeconds()==12,"macro zero history allowance");
}
void benchmark()
{
    using Clock=std::chrono::steady_clock;
    for (int block : {64,256}) for (int mode=0;mode<5;++mode)
    {
        GhostAudioProcessor p;
        if(mode>0) maximum(p,mode==2 || mode==4,false);
        p.prepareToPlay(48000,block);
        juce::AudioBuffer<float> a(2,block); juce::MidiBuffer midi;
        double seconds=0, worst=0;
        const int blocks=48000*5/block;
        for(int n=0;n<blocks+48000*11/block;++n)
        {
            if(n==48000*11/block && mode>=3) set(p,"freeze",1);
            for(int i=0;i<block;++i)
            { auto v=static_cast<float>(0.25*std::sin((n*block+i)*0.019)); a.setSample(0,i,v); a.setSample(1,i,-v); }
            const auto begin=Clock::now(); process(p,a,midi);
            const auto elapsed=std::chrono::duration<double>(Clock::now()-begin).count();
            if(n>=48000*11/block) {seconds+=elapsed;worst=(std::max)(worst,elapsed);}
        }
        const char* names[] {"defaults","maximum normal","reverse","freeze","all maximum"};
        std::cout << "Benchmark 48000/" << block << ' ' << names[mode] << ": "
                  << seconds*1000 << " ms / 5 audio seconds, mean block " << seconds/blocks*1e6
                  << " us, worst " << worst*1e6 << " us (wall clock, not DAW CPU).\n";
    }
}
void stress()
{
    for(double rate : {44100.0,48000.0,88200.0,96000.0})
    {
        GhostAudioProcessor p; maximum(p,true,false); p.prepareToPlay(rate,256);
        juce::AudioBuffer<float> a(2,256);juce::MidiBuffer midi;
        float peak=0; size_t startMemory=0,endMemory=0;
        // Two minutes each: full history, repeated capture/release, reversal,
        // macro extremes, presets, and editor lifetime events outside processing.
        const int blocks=static_cast<int>(rate*120/256);
        for(int block=0;block<blocks;++block)
        {
            const auto time=block*256/rate;
            if(block%static_cast<int>(rate*5/256)==0)
            {
                const int phase=static_cast<int>(time/5)%6;
                maximum(p,true,phase!=0);
                if(phase==2) set(p,"reverse",0);
                if(phase==3) set(p,"ghost",0);
                if(phase==4) GhostPresetManager::load(p.apvts,(block/100)%10);
                if(time<30) { std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor()); }
            }
            if(startMemory==0 && time>=40) startMemory=privateBytes();
            for(int i=0;i<256;++i)
            { const auto v=static_cast<float>(0.5*std::sin((block*256+i)*0.031)); a.setSample(0,i,v); a.setSample(1,i,-0.8f*v); }
            process(p,a,midi);
            for(int ch=0;ch<2;++ch)for(int i=0;i<256;++i)
            { const auto v=a.getSample(ch,i);check(std::isfinite(v),"stress output finite");peak=(std::max)(peak,std::abs(v)); }
        }
        endMemory=privateBytes();
        check(peak<2.0f,"integrated stress bounded");
        check(endMemory<=startMemory+2*1024*1024,"no sustained private memory growth >2 MiB");
        std::cout << rate << " Hz 120s integrated stress peak=" << peak << " (" << juce::Decibels::gainToDecibels(peak)
                  << " dBFS), warmed private bytes " << startMemory << " -> " << endMemory << ", callback new allocations=0.\n";
    }
}
void practicalTail()
{
    // Near-DC worst-case slow mode: fill 10 seconds, then let the full practical
    // horizon run. Forward clean and all-max reverse/diffusion are both covered.
    for(int mode=0;mode<2;++mode)
    {
        GhostAudioProcessor p; maximum(p,mode==1,false);
        if(mode==0) {set(p,"blur",0);set(p,"drift",0);}
        p.prepareToPlay(48000,1024); juce::AudioBuffer<float> a(2,1024);juce::MidiBuffer midi;
        float lastPeak=0;
        for(int n=0;n<48000*697/1024;++n)
        {
            const auto time=n*1024.0/48000;
            for(int ch=0;ch<2;++ch)for(int i=0;i<1024;++i)a.setSample(ch,i,time<10 ? 0.5f : 0.0f);
            process(p,a,midi);
            if(time>=686) lastPeak=(std::max)(lastPeak,a.getMagnitude(0,1024));
        }
        check(lastPeak<0.001f,"practical tail endpoint below -60 dBFS");
        std::cout << "686s finite-tail validation mode=" << mode << " last-window peak=" << lastPeak
                  << " (" << juce::Decibels::gainToDecibels(lastPeak) << " dBFS).\n";
    }
}
}
int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    try {tailMetadata();benchmark();stress();practicalTail();std::cout << "All RC release tests passed.\n";return 0;}
    catch(const std::exception& e) {countAllocations=false;std::cerr << e.what() << '\n';return 1;}
}

