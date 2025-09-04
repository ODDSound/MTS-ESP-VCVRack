#include "plugin.hpp"
#include "libMTSClient.h"
#include <algorithm>

struct Quantizer_MTS_ESP : Module {
	enum ParamIds {
		ROUNDING_PARAM,
        MODE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CV_IN_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		CV_OUT_OUTPUT,
		TRIGGER_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		CONNECTED_LIGHT,
		NUM_LIGHTS
	};

	dsp::PulseGenerator pulseGenerators[16];

	MTSClient *mtsClient = 0;
	
	bool hasMaster = false;
    bool bypassed = false;
	int roundingMode = 0;
    int mode = 0;
	double freqs[128];
	float cv_out[16];
	float last_cv_in[16] = { 0.f };
	float last_cv_out[16] = { 0.f };
	float rateLimiterPhase = 0.f;
	const double ln2 = 0.693147180559945309417;
	
	Quantizer_MTS_ESP() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configSwitch(ROUNDING_PARAM, -1.f, 1.f, 0.f, "Rounding", {"Down", "Nearest", "Up"});
		getParamQuantity(ROUNDING_PARAM)->randomizeEnabled = false;
        configSwitch(MODE_PARAM, 0.f, 1.f, 1.f, "Mode", {"Retune", "Quant"});
        getParamQuantity(MODE_PARAM)->randomizeEnabled = false;
        configInput(CV_IN_INPUT, "1V/oct pitch");
        configOutput(CV_OUT_OUTPUT, "1V/oct pitch");
        configOutput(TRIGGER_OUTPUT, "Trigger");
        configLight(CONNECTED_LIGHT, "MTS-ESP Connected");
        configBypass(CV_IN_INPUT, CV_OUT_OUTPUT);
		mtsClient = MTS_RegisterClient();
		for (int i = 0; i < 128; i++) freqs[i] = 440. * pow(2., (i-69.) / 12.);
	}
	
	virtual ~Quantizer_MTS_ESP() {
		MTS_DeregisterClient(mtsClient);
	}

	void process(const ProcessArgs& args) override {
		bool lastHasMaster = hasMaster;
		hasMaster = mtsClient && MTS_HasMaster(mtsClient);
		
		int lastRoundingMode = roundingMode;
		roundingMode = std::round(params[ROUNDING_PARAM].getValue());
        
        int lastMode = mode;
        mode = std::round(params[MODE_PARAM].getValue());

		lights[CONNECTED_LIGHT].setBrightness(hasMaster ? 1.f : 0.1f);

		bool throttle = false;
		const float rateLimiterPeriod = 0.005f;
		rateLimiterPhase += args.sampleTime / rateLimiterPeriod;
		if (rateLimiterPhase >= 1.f) {
			rateLimiterPhase -= 1.f;
		}
		else {
			throttle = hasMaster && (hasMaster == lastHasMaster) && (roundingMode == lastRoundingMode) && (mode == lastMode) && !bypassed;
		}

        bypassed = false;
        
		int channels = inputs[CV_IN_INPUT].getChannels();

		if (throttle) {
			for (int c = 0; c < channels; c++) {
				outputs[CV_OUT_OUTPUT].setVoltage(last_cv_out[c], c);
			}
		}
		else if (hasMaster) {
			
			bool freqsUpdated = (hasMaster != lastHasMaster) || (roundingMode != lastRoundingMode) || (mode != lastMode);
			for (int i = 0; i < 128; i++) {
				double f = MTS_NoteToFrequency(mtsClient, i, -1);
				if (f != freqs[i]) {
					freqs[i] = MTS_NoteToFrequency(mtsClient, i, -1);
					freqsUpdated = true;
				}
			}

			for (int c = 0; c < channels; c++) {
				double vin = inputs[CV_IN_INPUT].getVoltage(c);
				if (std::isnan(vin) || std::isinf(vin)) vin = 0.f;
				if (!freqsUpdated && vin==last_cv_in[c]) {
					cv_out[c] = last_cv_out[c];
				}
				else if (mode == 1) {
					double freq = dsp::FREQ_C4 * pow(2., vin);
					double qf = freq;
					unsigned char iLower, iUpper; iLower=0; iUpper=0;
					double dLower, dUpper; dLower=0; dUpper=0;
					bool found = false;
					for (int i = 0; i < 128; i++) {
						if (MTS_ShouldFilterNote(mtsClient, i, -1)) continue;
						double d = freqs[i] - freq;
						if (abs(d) < 1e-7) {found = true; break;}
						if (d < 0) {if (!dLower || d>dLower) {dLower = d; iLower = i;}}
						else if (!dUpper || d<dUpper) {dUpper = d; iUpper = i;}
					}
					if (!found) {
						if (!dLower) qf = freqs[iUpper];
						else if (!dUpper || iLower==iUpper) qf = freqs[iLower];
						else if (roundingMode == -1) qf = freqs[iLower];
						else if (roundingMode == 1) qf = freqs[iUpper];
						else {
							double fLower = freqs[iLower];
							double fUpper = freqs[iUpper];
							double fmid = fLower * pow(2., 0.5 * (log(fUpper/fLower) / ln2));
							qf = freq<fmid?fLower:fUpper;
						}
					}			
					cv_out[c] = log(qf/dsp::FREQ_C4) / ln2;
				}
                else {
                    double pitch = (vin * 12.) + 60.;
                    int note;
                    if (roundingMode == -1) note = std::floor(pitch);
                    else if (roundingMode == 1) note = std::ceil(pitch);
                    else note = std::round(pitch);
                    note = std::min(std::max(0, note), 127);
                    cv_out[c] = log(freqs[note]/dsp::FREQ_C4) / ln2;
                }
				
				last_cv_in[c] = vin;
				
				outputs[CV_OUT_OUTPUT].setVoltage(cv_out[c], c);
		
				if (cv_out[c] != last_cv_out[c]) {
					pulseGenerators[c].trigger(1e-3f);
					last_cv_out[c] = cv_out[c];
				}
				bool pulse = pulseGenerators[c].process(args.sampleTime);
				outputs[TRIGGER_OUTPUT].setVoltage((pulse ? 10.f : 0.f), c);
			}
		}
		else {
			for (int c = 0; c < channels; c++) {
				double vin = inputs[CV_IN_INPUT].getVoltage(c);
				if (std::isnan(vin) || std::isinf(vin)) vin = 0.f;
				outputs[CV_OUT_OUTPUT].setVoltage(vin, c);
				last_cv_in[c] = last_cv_out[c] = vin;
			}
		}
        
		outputs[CV_OUT_OUTPUT].setChannels(channels);
		outputs[TRIGGER_OUTPUT].setChannels(channels);
	}
    
    void processBypass(const ProcessArgs& args) override {
        hasMaster = mtsClient && MTS_HasMaster(mtsClient);
        lights[CONNECTED_LIGHT].setBrightness(hasMaster ? 1.f : 0.1f);
        bypassed = true;
        Module::processBypass(args);
    }
};

struct Quantizer_MTS_ESPWidget : ModuleWidget {
	Quantizer_MTS_ESPWidget(Quantizer_MTS_ESP* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Quantizer_MTS_ESP.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(7.526, 18.)), module, Quantizer_MTS_ESP::CONNECTED_LIGHT));

        addParam(createParam<CKSS>(mm2px(Vec(1., 41.59)), module, Quantizer_MTS_ESP::MODE_PARAM));
		addParam(createParam<CKSSThree>(mm2px(Vec(1., 53.679)), module, Quantizer_MTS_ESP::ROUNDING_PARAM));
		
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.526, 73.409)), module, Quantizer_MTS_ESP::CV_IN_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.526, 91.386)), module, Quantizer_MTS_ESP::CV_OUT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.526, 109.34)), module, Quantizer_MTS_ESP::TRIGGER_OUTPUT));
	}
};


Model* modelQuantizer_MTS_ESP = createModel<Quantizer_MTS_ESP, Quantizer_MTS_ESPWidget>("Quantizer_MTS_ESP");
