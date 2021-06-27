#include "plugin.hpp"
#include <iomanip>

struct RoundedDisplayParamQuantity : ParamQuantity {
	float getDisplayValue() override {
		return std::round(getValue());
	}
};

struct RatioParamQuantity : RoundedDisplayParamQuantity {
	int getDisplayPrecision() override {
		return 7;
	}
};

struct TransposeParamQuantity : RoundedDisplayParamQuantity {
	int getDisplayPrecision() override {
		return 4;
	}
};

struct CKSSNoRandom : CKSS {
	void randomize () override {
	}
};

struct WhiteKnobLarge : RoundKnob {
	WhiteKnobLarge() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/WhiteKnobLarge.svg")));
	}
};

struct Interval : Module {
	enum ParamIds {
		RATIO_NUM_PARAM,
		RATIO_DEN_PARAM,
		CENTS_PARAM,
		BEND_PARAM,
		TRANSPOSE_PARAM,
		INTERVAL_MODE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CV_IN_INPUT,
		BEND_INPUT,
		TRANSPOSE_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		CV_OUT_OUTPUT,
		THRU_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};
	
	bool useCents = false;
	float ratioNum = 2.f;
	float ratioDen = 1.f;
	float ratioVolts = 1.f;
	float transposeCvs[16];
	float bendCvs[16];
	int cvTranspose = 0;
	
	Interval() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam<RatioParamQuantity>(RATIO_NUM_PARAM, 1.f, 9999999.f, 2.f, "Numerator");
		configParam<RatioParamQuantity>(RATIO_DEN_PARAM, 1.f, 9999999.f, 1.f, "Denominator");
		configParam(CENTS_PARAM, -9999.9f, 9999.9f, 1200.f, "Cents", " cents");
		configParam<TransposeParamQuantity>(TRANSPOSE_PARAM, -9999.f, 9999.f, 0.f, "Transpose");
		configParam(BEND_PARAM, -1.f, 1.f, 0.f, "Bend");
		configParam(INTERVAL_MODE_PARAM, 0.f, 1.f, 1.f, "Interval Mode");
	}
	
	virtual ~Interval() {
	}
	
	void getCvs(Input &input, float *cvs) {
	 	int channels = input.getChannels();
 		if (channels == 0) {
 			for (int i = 0; i < 16; i++)
				cvs[i] = 1.f;
		}
 		else if (channels == 1) {
 			float vin = input.getVoltage(0);
 			if (std::isnan(vin) || std::isinf(vin)) vin = 0.f;
 			vin = clamp(vin, -5.f, 5.f) * 0.2f;
 			for (int i = 0; i < 16; i++)
 				cvs[i] = vin;
 		}
 		else {
 		 	for (int i = 0; i < channels; i++) {
				float vin = input.getVoltage(0);
 				if (std::isnan(vin) || std::isinf(vin)) vin = 0.f;
 				cvs[i] = clamp(vin, -5.f, 5.f) * 0.2f;
 			}
 			for (int i = channels; i < 16; i++)
 				cvs[i] = 1.f;
 		}
 	}

	void process(const ProcessArgs& args) override {	
		bool newUseCents = !(params[INTERVAL_MODE_PARAM].getValue());
 		float num = std::round(params[RATIO_NUM_PARAM].getValue());
 		float den = std::round(params[RATIO_DEN_PARAM].getValue());
 		float intervalVolts = 0.f;
 		
 		if  ((!newUseCents && useCents!=newUseCents) ||
 			(!useCents && (num!=ratioNum || den!=ratioDen))) {
 			const double ratioToSemitones=17.31234049066756088832;
 			ratioVolts = ratioToSemitones * log(num/den) / 12.;
 		}
 		
 		useCents = newUseCents;
 		ratioNum = num;
 		ratioDen = den;
 		
 		if (!useCents)
 			intervalVolts = ratioVolts;
 		else
 			intervalVolts = params[CENTS_PARAM].getValue() / 1200.f;
 		
 		getCvs(inputs[TRANSPOSE_INPUT], transposeCvs);
		getCvs(inputs[BEND_INPUT], bendCvs);
 		cvTranspose = std::round(params[TRANSPOSE_PARAM].getValue()) * transposeCvs[0];
 		
 		int channels = inputs[CV_IN_INPUT].getChannels();
 		for (int c = 0; c < channels; c++) {
			double vin = inputs[CV_IN_INPUT].getVoltage(c);
			if (std::isnan(vin) || std::isinf(vin)) vin = 0.f;	
			outputs[THRU_OUTPUT].setVoltage(vin, c);
			int tranpose = std::round(params[TRANSPOSE_PARAM].getValue()) * transposeCvs[c];
			float bend = params[BEND_PARAM].getValue() * bendCvs[c];
			float off = intervalVolts * (tranpose + bend);
			double vout = clamp(vin + off, -5.f, 5.f);
			outputs[CV_OUT_OUTPUT].setVoltage(vout, c);
		}
		outputs[THRU_OUTPUT].setChannels(channels);
		outputs[CV_OUT_OUTPUT].setChannels(channels);
	}
};

struct TransposeDisplayWidget : TransparentWidget {

	int *value = 0;
	std::shared_ptr<Font> font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Segment7Standard.ttf"));

	void draw(const DrawArgs& args) override {
		// Background
		NVGcolor backgroundColor = nvgRGB(0x16, 0x2e, 0x40);
		NVGcolor borderColor = nvgRGB(0x10, 0x10, 0x10);
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.0, 0.0, box.size.x, box.size.y, 4.0);
		nvgFillColor(args.vg, backgroundColor);
		nvgFill(args.vg);
		nvgStrokeWidth(args.vg, 1.5);
		nvgStrokeColor(args.vg, borderColor);
		nvgStroke(args.vg);
		// text
		if (value) {
			nvgFontSize(args.vg, 16);
			nvgFontFaceId(args.vg, font->handle);
			nvgTextLetterSpacing(args.vg, 2);
			std::stringstream to_display;
			to_display << std::right << std::setw(5) << *value;
			Vec textPos = Vec(1.f, 16.5f);
			NVGcolor textColor = nvgRGB(0x80, 0x80, 0x80);
			nvgFillColor(args.vg, textColor);
			nvgText(args.vg, textPos.x, textPos.y, to_display.str().c_str(), NULL);
		}
	}
};

struct ParamDisplayWidget : Knob {

	float textX = 4.f;
	std::shared_ptr<Font> font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Segment7Standard.ttf"));

	virtual void get_display_string(std::stringstream& to_display) {
		if (paramQuantity)
			to_display << (paramQuantity->getValue());
	}

	void draw(const DrawArgs& args) override {
		// Background
		NVGcolor backgroundColor = nvgRGB(0x16, 0x2e, 0x40);
		NVGcolor borderColor = nvgRGB(0x10, 0x10, 0x10);
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.0, 0.0, box.size.x, box.size.y, 4.0);
		nvgFillColor(args.vg, backgroundColor);
		nvgFill(args.vg);
		nvgStrokeWidth(args.vg, 1.5);
		nvgStrokeColor(args.vg, borderColor);
		nvgStroke(args.vg);
		// text
		nvgFontSize(args.vg, 16);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, 2);
		std::stringstream to_display;
		get_display_string(to_display);
		Vec textPos = Vec(textX, 16.5f);
		NVGcolor textColor = nvgRGB(0x4c, 0xa0, 0xdf);
		nvgFillColor(args.vg, textColor);
		nvgText(args.vg, textPos.x, textPos.y, to_display.str().c_str(), NULL);
	}
};

struct RatioParamDisplayWidget : ParamDisplayWidget {
	RatioParamDisplayWidget() {
		speed = 0.00001;
	};
	
	void get_display_string(std::stringstream& to_display) override {
		if (paramQuantity)
			to_display << std::right << std::setw(7) << std::setprecision(7) << std::round(paramQuantity->getValue());
	}
};

struct CentsParamDisplayWidget : ParamDisplayWidget {
	CentsParamDisplayWidget() {
		speed = 0.1;
	};
	
	void get_display_string(std::stringstream& to_display) override {
		if (paramQuantity)
			to_display << std::right << std::setw(7) << std::setprecision(1) << std::fixed << (paramQuantity->getValue());
	}
};

struct TransposeParamDisplayWidget : ParamDisplayWidget {
	TransposeParamDisplayWidget() {
		speed = 0.001;
		textX = 1.f;
	};
	
	void get_display_string(std::stringstream& to_display) override {
		if (paramQuantity)
			to_display << std::right << std::setw(5) << (int) std::round(paramQuantity->getValue());
	}
};

struct IntervalWidget : ModuleWidget {

	ParamDisplayWidget *displayNum;
	ParamDisplayWidget *displayDen;
	ParamDisplayWidget *displayCents;

	IntervalWidget(Interval* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Interval.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		
		addParam(createParam<CKSSNoRandom>(mm2px(Vec(10., 19.6)), module, Interval::INTERVAL_MODE_PARAM));

		displayNum = new RatioParamDisplayWidget();
		displayNum->box.pos = Vec(9, 85);
		displayNum->box.size = Vec(87, 20);
    	if (module)
			displayNum->paramQuantity = module->paramQuantities[Interval::RATIO_NUM_PARAM];
		addChild(displayNum);
		
		displayDen = new RatioParamDisplayWidget();
		displayDen->box.pos = Vec(9, 110);
		displayDen->box.size = Vec(87, 20);
    	if (module)
			displayDen->paramQuantity = module->paramQuantities[Interval::RATIO_DEN_PARAM];
		addChild(displayDen);
		
		displayCents = new CentsParamDisplayWidget();
		displayCents->box.pos = Vec(9, 85);
		displayCents->box.size = Vec(87, 20);
		if (module)
			displayCents->paramQuantity = module->paramQuantities[Interval::CENTS_PARAM];
		addChild(displayCents);
		
		ParamDisplayWidget *displayTranspose = new TransposeParamDisplayWidget();
		displayTranspose->box.pos = Vec(35, 166);
		displayTranspose->box.size = Vec(61, 20);
		if (module)
			displayTranspose->paramQuantity = module->paramQuantities[Interval::TRANSPOSE_PARAM];
		addChild(displayTranspose);
		
		TransposeDisplayWidget *displayCVTranspose = new TransposeDisplayWidget();
		displayCVTranspose->box.pos = Vec(35, 191);
		displayCVTranspose->box.size = Vec(61, 20);
		if (module)
			displayCVTranspose->value = &(module->cvTranspose);
		addChild(displayCVTranspose);
		
		addParam(createParamCentered<WhiteKnobLarge>(Vec(69, 266), module, Interval::BEND_PARAM));
		
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.475, 67.916)), module, Interval::TRANSPOSE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.475, 90.130)), module, Interval::BEND_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.475, 112.344)), module, Interval::CV_IN_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(17.780, 112.344)), module, Interval::THRU_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(29.084, 112.344)), module, Interval::CV_OUT_OUTPUT));
	}
	
	void step() override {
		ModuleWidget::step();

		Interval* module = dynamic_cast<Interval*>(this->module);
		if (!module)
			return;
		
		if (module->useCents) {
			displayNum->hide();
			displayDen->hide();
			displayCents->show();
		}
		else {
			displayNum->show();
			displayDen->show();
			displayCents->hide();
		}
	}
};


Model* modelInterval = createModel<Interval, IntervalWidget>("Interval");