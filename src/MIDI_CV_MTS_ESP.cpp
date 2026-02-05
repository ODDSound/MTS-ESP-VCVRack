#include "plugin.hpp"
#include "libMTSClient.h"
#include <algorithm>


struct MIDI_CV_MTS_ESP : Module {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		NUM_INPUTS
	};
	enum OutputIds {
		PITCH_OUTPUT,
		GATE_OUTPUT,
		VELOCITY_OUTPUT,
		AFTERTOUCH_OUTPUT,
		PW_OUTPUT,
		MOD_OUTPUT,
		RETRIGGER_OUTPUT,
		CLOCK_OUTPUT,
		CLOCK_DIV_OUTPUT,
		START_OUTPUT,
		STOP_OUTPUT,
		CONTINUE_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		CONNECTED_LIGHT,
		NUM_LIGHTS
	};

	midi::InputQueue midiInput;
	dsp::MidiParser<16> midiParser;

	MTSClient *mtsClient = 0;

	MIDI_CV_MTS_ESP() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configOutput(PITCH_OUTPUT, "1V/octave pitch");
		configOutput(GATE_OUTPUT, "Gate");
		configOutput(VELOCITY_OUTPUT, "Velocity");
		configOutput(AFTERTOUCH_OUTPUT, "Aftertouch");
		configOutput(PW_OUTPUT, "Pitch wheel");
		configOutput(MOD_OUTPUT, "Mod wheel");
		configOutput(RETRIGGER_OUTPUT, "Retrigger");
		configOutput(CLOCK_OUTPUT, "Clock");
		configOutput(CLOCK_DIV_OUTPUT, "Clock divider");
		configOutput(START_OUTPUT, "Start trigger");
		configOutput(STOP_OUTPUT, "Stop trigger");
		configOutput(CONTINUE_OUTPUT, "Continue trigger");
		configLight(CONNECTED_LIGHT, "MTS-ESP connected");
		
		mtsClient = MTS_RegisterClient();
	}

	virtual ~MIDI_CV_MTS_ESP() {
		MTS_DeregisterClient(mtsClient);
	}

	void onReset() override {
		midiParser.reset();
		midiInput.reset();
	}

	void process(const ProcessArgs& args) override {

		lights[CONNECTED_LIGHT].setBrightness(MTS_HasMaster(mtsClient) ? 1.f : 0.1f);
		
		midi::Message msg;
		while (midiInput.tryPop(&msg, args.frame)) {
			if (msg.getStatus() == 0x9 && msg.getValue() > 0 && MTS_ShouldFilterNote(mtsClient, msg.getNote(), -1))
				continue;
			midiParser.processMessage(msg);
		}

		midiParser.processFilters(args.sampleTime);

		// Set note outputs
		outputs[PITCH_OUTPUT].setChannels(midiParser.channels);
		outputs[GATE_OUTPUT].setChannels(midiParser.channels);
		outputs[VELOCITY_OUTPUT].setChannels(midiParser.channels);
		outputs[AFTERTOUCH_OUTPUT].setChannels(midiParser.channels);
		outputs[RETRIGGER_OUTPUT].setChannels(midiParser.channels);
		for (uint8_t c = 0; c < midiParser.channels; c++) {
			float note = midiParser.notes[c];
			if (mtsClient) note += MTS_RetuningInSemitones(mtsClient, midiParser.notes[c], -1);
			uint8_t wheelChannel = midiParser.getWheelChannels() == 1 ? 0 : c;
			outputs[PITCH_OUTPUT].setVoltage((note - 60.f + midiParser.getPw(wheelChannel) * midiParser.pwRange) / 12.f, c);
			outputs[GATE_OUTPUT].setVoltage(midiParser.gates[c] ? 10.f : 0.f, c);
			outputs[VELOCITY_OUTPUT].setVoltage(midiParser.velocities[c] / 127.f * 10.f, c);
			outputs[AFTERTOUCH_OUTPUT].setVoltage(midiParser.aftertouches[c] / 127.f * 10.f, c);
			outputs[RETRIGGER_OUTPUT].setVoltage(midiParser.retriggerPulses[c].isHigh() ? 10.f : 0.f, c);
		}

		// Pitch and mod wheel outputs
		uint8_t wheelChannels = midiParser.getWheelChannels();
		outputs[PW_OUTPUT].setChannels(wheelChannels);
		outputs[MOD_OUTPUT].setChannels(wheelChannels);
		for (uint8_t c = 0; c < wheelChannels; c++) {
			outputs[PW_OUTPUT].setVoltage(midiParser.getPw(c) * 5.f, c);
			outputs[MOD_OUTPUT].setVoltage(midiParser.getMod(c) * 10.f, c);
		}

		// Set clock and transport outputs
		outputs[CLOCK_OUTPUT].setVoltage(midiParser.clockPulse.isHigh() ? 10.f : 0.f);
		outputs[CLOCK_DIV_OUTPUT].setVoltage(midiParser.clockDividerPulse.isHigh() ? 10.f : 0.f);
		outputs[START_OUTPUT].setVoltage(midiParser.startPulse.isHigh() ? 10.f : 0.f);
		outputs[STOP_OUTPUT].setVoltage(midiParser.stopPulse.isHigh() ? 10.f : 0.f);
		outputs[CONTINUE_OUTPUT].setVoltage(midiParser.continuePulse.isHigh() ? 10.f : 0.f);
		
		midiParser.processPulses(args.sampleTime);

	}

	void processBypass(const ProcessArgs& args) override {
		lights[CONNECTED_LIGHT].setBrightness(MTS_HasMaster(mtsClient) ? 1.f : 0.1f);
		Module::processBypass(args);
	}

	json_t* dataToJson() override {
		json_t* rootJ = midiParser.toJson();
		json_object_set_new(rootJ, "midi", midiInput.toJson());
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		// For backwards compatibility, set to 0 if undefined in JSON.
		midiParser.pwRange = 0;

		midiParser.fromJson(rootJ);

		json_t* midiJ = json_object_get(rootJ, "midi");
		if (midiJ)
			midiInput.fromJson(midiJ);
	}
};


struct MIDI_CV_MTS_ESP_MidiDisplay : MidiDisplay {
	void setMidiPort(midi::Port* port) {
		MidiDisplay::setMidiPort(port);
		driverChoice->bgColor = nvgRGB(0x16, 0x2e, 0x40);
		driverChoice->color = nvgRGB(0xf0, 0xf0, 0xf0);
		deviceChoice->bgColor = nvgRGB(0x16, 0x2e, 0x40);
		deviceChoice->color = nvgRGB(0xf0, 0xf0, 0xf0);
		channelChoice->bgColor = nvgRGB(0x16, 0x2e, 0x40);
		channelChoice->color = nvgRGB(0xf0, 0xf0, 0xf0);
	}
};


struct MIDI_CV_MTS_ESPWidget : ModuleWidget {
	MIDI_CV_MTS_ESPWidget(MIDI_CV_MTS_ESP* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/MIDI_CV_MTS_ESP.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(11.4, 13.7)), module, MIDI_CV_MTS_ESP::CONNECTED_LIGHT));

		addOutput(createOutput<PJ301MPort>(mm2px(Vec(4.61505, 60.1445)), module, MIDI_CV_MTS_ESP::PITCH_OUTPUT));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(16.214, 60.1445)), module, MIDI_CV_MTS_ESP::GATE_OUTPUT));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(27.8143, 60.1445)), module, MIDI_CV_MTS_ESP::VELOCITY_OUTPUT));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(4.61505, 76.1449)), module, MIDI_CV_MTS_ESP::AFTERTOUCH_OUTPUT));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(16.214, 76.1449)), module, MIDI_CV_MTS_ESP::PW_OUTPUT));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(27.8143, 76.1449)), module, MIDI_CV_MTS_ESP::MOD_OUTPUT));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(4.61505, 92.1439)), module, MIDI_CV_MTS_ESP::CLOCK_OUTPUT));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(16.214, 92.1439)), module, MIDI_CV_MTS_ESP::CLOCK_DIV_OUTPUT));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(27.8143, 92.1439)), module, MIDI_CV_MTS_ESP::RETRIGGER_OUTPUT));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(4.61505, 108.144)), module, MIDI_CV_MTS_ESP::START_OUTPUT));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(16.214, 108.144)), module, MIDI_CV_MTS_ESP::STOP_OUTPUT));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(27.8143, 108.144)), module, MIDI_CV_MTS_ESP::CONTINUE_OUTPUT));

		MIDI_CV_MTS_ESP_MidiDisplay* midiDisplay = createWidget<MIDI_CV_MTS_ESP_MidiDisplay>(mm2px(Vec(3.41891, 17.8373)));
		midiDisplay->box.size = mm2px(Vec(33.840, 28));
		midiDisplay->setMidiPort(module ? &module->midiInput : NULL);
		addChild(midiDisplay);
	}

	void appendContextMenu(Menu* menu) override {
		MIDI_CV_MTS_ESP* module = dynamic_cast<MIDI_CV_MTS_ESP*>(this->module);

		menu->addChild(new MenuSeparator);

		menu->addChild(createSubmenuItem("Polyphony channels", string::f("%d", module->midiParser.channels), [=](Menu* menu) {
			for (int c = 1; c <= 16; c++) {
				std::string channelsLabel = (c == 1) ? "Monophonic" : string::f("%d", c);
				menu->addChild(createCheckMenuItem(channelsLabel, "",
					[=]() {return module->midiParser.channels == c;},
					[=]() {module->midiParser.setChannels(c);}
				));
			}
		}));

		menu->addChild(createIndexSubmenuItem("Monophonic priority", {
			"Last",
			"First",
			"Lowest",
			"Highest",
		}, [=]() {
			return module->midiParser.monoMode;
		}, [=](size_t monoMode) {
			module->midiParser.setMonoMode(decltype(module->midiParser)::MonoMode(monoMode));
		}));

		menu->addChild(createIndexSubmenuItem("Polyphony allocation", {
			"Rotate",
			"Reuse",
			"Reset",
			"MPE",
		}, [=]() {
			return module->midiParser.polyMode;
		}, [=](size_t polyMode) {
			module->midiParser.setPolyMode(decltype(module->midiParser)::PolyMode(polyMode));
		}));

		menu->addChild(createBoolPtrMenuItem("Retrigger on legato release", "", &module->midiParser.retriggerOnResume));

		menu->addChild(new MenuSeparator);

		menu->addChild(createBoolPtrMenuItem("Use release velocity", "", &module->midiParser.releaseVelocityEnabled));

		static const std::vector<float> pwRanges = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 24, 36, 48};
		auto getPwRangeLabel = [](float pwRange) -> std::string {
			if (pwRange == 0)
				return "Off";
			else if (std::fmod(pwRange, 12) == 0.f)
				return string::f("%g octave%s", pwRange / 12, pwRange / 12 == 1 ? "" : "s");
			else
				return string::f("%g semitone%s", pwRange, pwRange == 1 ? "" : "s");
		};
		menu->addChild(createSubmenuItem("Pitch bend range", getPwRangeLabel(module->midiParser.pwRange), [=](Menu* menu) {
			for (size_t i = 0; i < pwRanges.size(); i++) {
				menu->addChild(createCheckMenuItem(getPwRangeLabel(pwRanges[i]), "",
					[=]() {return module->midiParser.pwRange == pwRanges[i];},
					[=]() {module->midiParser.pwRange = pwRanges[i];}
				));
			}
		}));

		menu->addChild(createBoolPtrMenuItem("Smooth pitch/mod wheel", "", &module->midiParser.smooth));

		static const std::vector<uint32_t> clockDivisions = {24 * 4, 24 * 2, 24, 24 / 2, 24 / 4, 24 / 8, 2, 1};
		static const std::vector<std::string> clockDivisionLabels = {"Whole", "Half", "Quarter", "8th", "16th", "32nd", "12 PPQN", "24 PPQN"};
		size_t clockDivisionIndex = std::find(clockDivisions.begin(), clockDivisions.end(), module->midiParser.clockDivision) - clockDivisions.begin();
		std::string clockDivisionLabel = (clockDivisionIndex < clockDivisionLabels.size()) ? clockDivisionLabels[clockDivisionIndex] : "";
		menu->addChild(createSubmenuItem("CLK/N divider", clockDivisionLabel, [=](Menu* menu) {
			for (size_t i = 0; i < clockDivisions.size(); i++) {
				menu->addChild(createCheckMenuItem(clockDivisionLabels[i], "",
					[=]() {return module->midiParser.clockDivision == clockDivisions[i];},
					[=]() {module->midiParser.clockDivision = clockDivisions[i];}
				));
			}
		}));

		menu->addChild(new MenuSeparator);

		menu->addChild(createMenuItem("Reset MIDI (Panic)", "",
			[=]() {module->midiParser.panic();}
		));
	}
};


Model* modelMIDI_CV_MTS_ESP = createModel<MIDI_CV_MTS_ESP, MIDI_CV_MTS_ESPWidget>("MIDI_CV_MTS_ESP");
