#include "plugin.hpp"
#include "libMTSClient.h"


struct MidiOutput : dsp::MidiGenerator<PORT_MAX_CHANNELS>, midi::Output {
	void onMessage(midi::Message message) override {
		midi::Output::sendMessage(message);
	}

	void reset() {
		Output::reset();
		MidiGenerator::reset();
	}
};


struct CV_MIDI_MTS_ESP : Module {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		PITCH_INPUT,
		GATE_INPUT,
		VEL_INPUT,
		AFT_INPUT,
		PW_INPUT,
		MW_INPUT,
		CLK_INPUT,
		VOL_INPUT,
		PAN_INPUT,
		START_INPUT,
		STOP_INPUT,
		CONTINUE_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	MidiOutput midiOutput;
	float rateLimiterPhase = 0.f;
	
	MTSClient *mtsClient;

	CV_MIDI_MTS_ESP() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		mtsClient = MTS_RegisterClient();
		onReset();
	}
	
	virtual ~CV_MIDI_MTS_ESP() {
		MTS_DeregisterClient(mtsClient);
	}


	void onReset() override {
		midiOutput.reset();
	}

	void process(const ProcessArgs& args) override {
		const float rateLimiterPeriod = 0.005f;
		rateLimiterPhase += args.sampleTime / rateLimiterPeriod;
		if (rateLimiterPhase >= 1.f) {
			rateLimiterPhase -= 1.f;
		}
		else {
			return;
		}

		for (int c = 0; c < inputs[PITCH_INPUT].getChannels(); c++) {
			int vel = (int) std::round(inputs[VEL_INPUT].getNormalPolyVoltage(10.f * 100 / 127, c) / 10.f * 127);
			vel = clamp(vel, 0, 127);
			midiOutput.setVelocity(vel, c);

            
            double freq = 440. * pow(2., inputs[PITCH_INPUT].getVoltage(c) - 0.75);
            int note;
            if (mtsClient && MTS_HasMaster(mtsClient)) note = MTS_FrequencyToNote(mtsClient, freq, c);
            else note = (int) std::round(inputs[PITCH_INPUT].getVoltage(c) * 12.f + 60.f);
			note = clamp(note, 0, 127);
			bool gate = inputs[GATE_INPUT].getPolyVoltage(c) >= 1.f;
			midiOutput.setNoteGate(note, gate, c);

			int aft = (int) std::round(inputs[AFT_INPUT].getPolyVoltage(c) / 10.f * 127);
			aft = clamp(aft, 0, 127);
			midiOutput.setKeyPressure(aft, c);
		}

		int pw = (int) std::round((inputs[PW_INPUT].getVoltage() + 5.f) / 10.f * 0x4000);
		pw = clamp(pw, 0, 0x3fff);
		midiOutput.setPitchWheel(pw);

		int mw = (int) std::round(inputs[MW_INPUT].getVoltage() / 10.f * 127);
		mw = clamp(mw, 0, 127);
		midiOutput.setModWheel(mw);

		int vol = (int) std::round(inputs[VOL_INPUT].getNormalVoltage(10.f) / 10.f * 127);
		vol = clamp(vol, 0, 127);
		midiOutput.setVolume(vol);

		int pan = (int) std::round((inputs[PAN_INPUT].getVoltage() + 5.f) / 10.f * 127);
		pan = clamp(pan, 0, 127);
		midiOutput.setPan(pan);

		bool clk = inputs[CLK_INPUT].getVoltage() >= 1.f;
		midiOutput.setClock(clk);

		bool start = inputs[START_INPUT].getVoltage() >= 1.f;
		midiOutput.setStart(start);

		bool stop = inputs[STOP_INPUT].getVoltage() >= 1.f;
		midiOutput.setStop(stop);

		bool cont = inputs[CONTINUE_INPUT].getVoltage() >= 1.f;
		midiOutput.setContinue(cont);
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "midi", midiOutput.toJson());
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* midiJ = json_object_get(rootJ, "midi");
		if (midiJ)
			midiOutput.fromJson(midiJ);
	}
};


struct CV_MIDI_MTS_ESPPanicItem : MenuItem {
	CV_MIDI_MTS_ESP* module;
	void onAction(const event::Action& e) override {
		module->midiOutput.panic();
	}
};


struct CV_MIDI_MTS_ESPWidget : ModuleWidget {
	CV_MIDI_MTS_ESPWidget(CV_MIDI_MTS_ESP* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/CV_MIDI_MTS_ESP.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(9, 64)), module, CV_MIDI_MTS_ESP::PITCH_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20, 64)), module, CV_MIDI_MTS_ESP::GATE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(32, 64)), module, CV_MIDI_MTS_ESP::VEL_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(9, 80)), module, CV_MIDI_MTS_ESP::AFT_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20, 80)), module, CV_MIDI_MTS_ESP::PW_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(32, 80)), module, CV_MIDI_MTS_ESP::MW_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(9, 96)), module, CV_MIDI_MTS_ESP::CLK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20, 96)), module, CV_MIDI_MTS_ESP::VOL_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(32, 96)), module, CV_MIDI_MTS_ESP::PAN_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(9, 112)), module, CV_MIDI_MTS_ESP::START_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20, 112)), module, CV_MIDI_MTS_ESP::STOP_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(32, 112)), module, CV_MIDI_MTS_ESP::CONTINUE_INPUT));

		MidiWidget* midiWidget = createWidget<MidiWidget>(mm2px(Vec(3.41891, 14.8373)));
		midiWidget->box.size = mm2px(Vec(33.840, 28));
		midiWidget->setMidiPort(module ? &module->midiOutput : NULL);
		addChild(midiWidget);
	}

	void appendContextMenu(Menu* menu) override {
		CV_MIDI_MTS_ESP* module = dynamic_cast<CV_MIDI_MTS_ESP*>(this->module);

		menu->addChild(new MenuEntry);

		CV_MIDI_MTS_ESPPanicItem* panicItem = new CV_MIDI_MTS_ESPPanicItem;
		panicItem->text = "Panic";
		panicItem->module = module;
		menu->addChild(panicItem);
	}
};


Model* modelCV_MIDI_MTS_ESP = createModel<CV_MIDI_MTS_ESP, CV_MIDI_MTS_ESPWidget>("CV_MIDI_MTS_ESP");

