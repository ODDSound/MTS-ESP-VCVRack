#include "plugin.hpp"


Plugin* pluginInstance;


void init(Plugin* p) {
	pluginInstance = p;

	// Add modules here
	p->addModel(modelMIDI_CV_MTS_ESP);
	p->addModel(modelCV_MIDI_MTS_ESP);
	p->addModel(modelQuantizer_MTS_ESP);
	p->addModel(modelInterval);

	// Any other plugin initialization may go here.
	// As an alternative, consider lazy-loading assets and lookup tables when your module is created to reduce startup times of Rack.
}
