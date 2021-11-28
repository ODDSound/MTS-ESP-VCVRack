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
        getParamQuantity(INTERVAL_MODE_PARAM)->randomizeEnabled = false;
        configInput(CV_IN_INPUT, "1V/oct pitch");
        configInput(BEND_INPUT, "Bend CV");
        configInput(TRANSPOSE_INPUT, "Transpose CV");
        configOutput(THRU_OUTPUT, "1V/oct pitch thru");
        configOutput(CV_OUT_OUTPUT, "1V/oct pitch");
        configBypass(CV_IN_INPUT, CV_OUT_OUTPUT);
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

	void draw(const DrawArgs& args) override {
		NVGcolor backgroundColor = nvgRGB(0x16, 0x2e, 0x40);
		NVGcolor borderColor = nvgRGB(0x10, 0x10, 0x10);
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.0, 0.0, box.size.x, box.size.y, 4.0);
		nvgFillColor(args.vg, backgroundColor);
		nvgFill(args.vg);
		nvgStrokeWidth(args.vg, 1.5);
		nvgStrokeColor(args.vg, borderColor);
		nvgStroke(args.vg);
	}
    
    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer == 1 && value) {
            std::shared_ptr<Font> font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Segment7Standard.ttf"));
            if (font) {
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
        Widget::drawLayer(args, layer);
    }
};

struct ParamDisplayWidget : Knob {

	float textX = 4.f;

	virtual void get_display_string(std::stringstream& to_display) {
        ParamQuantity* paramQuantity = getParamQuantity();
		if (paramQuantity)
			to_display << (paramQuantity->getValue());
	}
    
    void onDragMove(const event::DragMove& e) override {
        if (e.button != GLFW_MOUSE_BUTTON_LEFT)
            return;
        
        ParamQuantity* paramQuantity = getParamQuantity();
        if (paramQuantity) {
            float delta = (horizontal ? e.mouseDelta.x : -e.mouseDelta.y);
            delta *= 0.0015f;
            delta *= speed;
            delta *= paramQuantity->getRange();
            
            int mods = APP->window->getMods();
            if ((mods & RACK_MOD_MASK) == RACK_MOD_CTRL) {
                delta /= 16.f;
            }
            if ((mods & RACK_MOD_MASK) == (RACK_MOD_CTRL | GLFW_MOD_SHIFT)) {
                delta /= 256.f;
            }
            
            paramQuantity->setValue(paramQuantity->getValue() + delta);
        }
        
        ParamWidget::onDragMove(e);
    }

	void draw(const DrawArgs& args) override {
		NVGcolor backgroundColor = nvgRGB(0x16, 0x2e, 0x40);
		NVGcolor borderColor = nvgRGB(0x10, 0x10, 0x10);
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.0, 0.0, box.size.x, box.size.y, 4.0);
		nvgFillColor(args.vg, backgroundColor);
		nvgFill(args.vg);
		nvgStrokeWidth(args.vg, 1.5);
		nvgStrokeColor(args.vg, borderColor);
		nvgStroke(args.vg);
	}
    
    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer == 1) {
            std::shared_ptr<Font> font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Segment7Standard.ttf"));
            if (font) {
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
        }
        Widget::drawLayer(args, layer);
    }
};

struct RatioParamDisplayWidget : ParamDisplayWidget {
	void get_display_string(std::stringstream& to_display) override {
        ParamQuantity* paramQuantity = getParamQuantity();
		if (paramQuantity)
			to_display << std::right << std::setw(7) << std::setprecision(7) << std::round(paramQuantity->getValue());
	}
};

struct CentsParamDisplayWidget : ParamDisplayWidget {
	void get_display_string(std::stringstream& to_display) override {
        ParamQuantity* paramQuantity = getParamQuantity();
		if (paramQuantity)
			to_display << std::right << std::setw(7) << std::setprecision(1) << std::fixed << (paramQuantity->getValue());
	}
};

struct TransposeParamDisplayWidget : ParamDisplayWidget {
	void get_display_string(std::stringstream& to_display) override {
        ParamQuantity* paramQuantity = getParamQuantity();
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
		
		addParam(createParam<CKSS>(mm2px(Vec(10., 19.6)), module, Interval::INTERVAL_MODE_PARAM));
        
        displayNum = createParam<RatioParamDisplayWidget>(Vec(9, 85), module, Interval::RATIO_NUM_PARAM);
		displayNum->box.size = Vec(87, 20);
        displayNum->speed = 0.00001;
        addParam(displayNum);
		
        displayDen = createParam<RatioParamDisplayWidget>(Vec(9, 110), module, Interval::RATIO_DEN_PARAM);
		displayDen->box.size = Vec(87, 20);
        displayDen->speed = 0.00001;
		addParam(displayDen);
		
        displayCents = createParam<CentsParamDisplayWidget>(Vec(9, 85), module, Interval::CENTS_PARAM);
		displayCents->box.size = Vec(87, 20);
        displayCents->speed = 0.1;
		addParam(displayCents);
		
        TransposeParamDisplayWidget* displayTranspose = createParam<TransposeParamDisplayWidget>(Vec(35, 166), module, Interval::TRANSPOSE_PARAM);
		displayTranspose->box.size = Vec(61, 20);
        displayTranspose->speed = 0.001;
        displayTranspose->textX = 1.f;
		addParam(displayTranspose);
		
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
