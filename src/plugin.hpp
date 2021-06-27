#pragma once
#include <rack.hpp>


using namespace rack;

// Declare the Plugin, defined in plugin.cpp
extern Plugin* pluginInstance;

// Declare each Model, defined in each module source file
extern Model* modelMIDI_CV_MTS_ESP;
extern Model* modelCV_MIDI_MTS_ESP;
extern Model* modelQuantizer_MTS_ESP;
extern Model* modelInterval;
