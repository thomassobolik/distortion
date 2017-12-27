/*
 ____  _____ _        _    
| __ )| ____| |      / \   
|  _ \|  _| | |     / _ \  
| |_) | |___| |___ / ___ \ 
|____/|_____|_____/_/   \_\

*/


// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
// Include any files or libraries we need 
#include <Bela.h>
#include <cmath>
#include "Mu45LFO.h"
#include "math.h"
#include <BiQuad.h>
#include <Mu45FilterCalc.h>

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
// Global variables 

// Set the analog input channels to read from
int gSensorInputGain = 0;		// analog in channel for volume potentiometer
int gSensorInputBPFWidth = 1;		// analog in channel for LFO potentiometer
int gSensorInputBPFCutoff = 2;

// Variables for audio processing and parameters
int gAudioFramesPerAnalogFrame;

stk::BiQuad hiPass, loPass;

// Variables for debugging
int gPCount = 0;				// counter for printing
float gPrintInterval = 0.5;		// how often to print, in seconds

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
bool setup(BelaContext *context, void *userData)
{
	// --  --  --  --  --  --  --  --  --  --  --  --  --  --  -- 
	// Here is where we do any initializations and calculations needed before processing audio
	gAudioFramesPerAnalogFrame = context->audioFrames / context->analogFrames;
	
	// You can use rt_print for figuring out what's going on and debugging
	rt_printf("distortion is running!\n");
    rt_printf("Audio sample rate is %f\n", context->audioSampleRate);
    rt_printf("Digital sample rate is %f\n", context->digitalSampleRate);
    rt_printf("Analog sample rate is %f\n", context->analogSampleRate);

	return true;
}

// clipping algorithm from the internet
// source: https://dsp.stackexchange.com/questions/13142/digital-distortion-effect-algorithm
float nonlinearity(float x) {
	if (x > 0) {
	 	return 1 - exp(-x);
	}
	else return -1 + exp(x);
}

float NN2Hz(float NN) {
	return 440.0 * powf(2, (NN-60.0) / 12.0);
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
// This function gets called for each block of input data
void render(BelaContext *context, void *userData)
{
	// Declare any local variables you need
    float monoIn;
    float preGain;
    float baseFreq, width, LPFreq;
    float filtQ = 3;
    float HPcoeffs[5];
    float LPcoeffs[5];
        
    // Step through each frame in the audio buffer
    for(unsigned int n = 0; n < context->audioFrames; n++) 
    {
    	// --  --  --  --  --  --  --  --  --  --  --  --  --  --  -- 
		// Read the data from analog sensors, and calculate any alg params
		if(!(n % gAudioFramesPerAnalogFrame)) 
		{
			baseFreq = analogRead(context, n/gAudioFramesPerAnalogFrame, gSensorInputBPFCutoff);	
			baseFreq = map(baseFreq, 0.0001, 0.827, 27, 123);
			baseFreq = NN2Hz(baseFreq);
			
			width = analogRead(context, n/gAudioFramesPerAnalogFrame, gSensorInputBPFWidth);
			width = map(width, 0.0001, 0.827, 20, 3000);
			
			preGain = analogRead(context, n/gAudioFramesPerAnalogFrame, gSensorInputGain);
			preGain = map(preGain, 0.0001, 0.827, 0.5, 8);
			
			// wrap LPF to frequency range 
			if(baseFreq + width > 19900) {
				LPFreq = 19900;
			}
			else LPFreq = baseFreq + width;
			
			Mu45FilterCalc::calcCoeffsHPF (HPcoeffs, baseFreq, filtQ, context->audioSampleRate);
			hiPass.setCoefficients(HPcoeffs[0], HPcoeffs[1], HPcoeffs[2], HPcoeffs[3], HPcoeffs[4]);
			
			// calculate and set the coefficients for the lo pass filter
			Mu45FilterCalc::calcCoeffsLPF (LPcoeffs, LPFreq, filtQ, context->audioSampleRate);
			loPass.setCoefficients(LPcoeffs[0], LPcoeffs[1], LPcoeffs[2], LPcoeffs[3], LPcoeffs[4]);
		}
		
		monoIn = (audioRead(context, n, 0) + audioRead(context, n, 1)) / 2;
		monoIn *= preGain;
		
		// filter the signal before distortion
		monoIn = hiPass.tick(monoIn);
		monoIn = loPass.tick(monoIn);
		
		// woah some non-linear shit goin down here
		monoIn = nonlinearity(monoIn);

		audioWrite(context, n, 0, monoIn);
		audioWrite(context, n, 1, monoIn);
	}
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
// Cleanup
void cleanup(BelaContext *context, void *userData)
{

}