/***********************************************************************************************************************
*                                                                                                                      *
* libscopehal v0.1                                                                                                     *
*                                                                                                                      *
* Copyright (c) 2012-2024 Andrew D. Zonenberg and contributors                                                         *
* All rights reserved.                                                                                                 *
*                                                                                                                      *
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that the     *
* following conditions are met:                                                                                        *
*                                                                                                                      *
*    * Redistributions of source code must retain the above copyright notice, this list of conditions, and the         *
*      following disclaimer.                                                                                           *
*                                                                                                                      *
*    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the       *
*      following disclaimer in the documentation and/or other materials provided with the distribution.                *
*                                                                                                                      *
*    * Neither the name of the author nor the names of any contributors may be used to endorse or promote products     *
*      derived from this software without specific prior written permission.                                           *
*                                                                                                                      *
* THIS SOFTWARE IS PROVIDED BY THE AUTHORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED   *
* TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL *
* THE AUTHORS BE HELD LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES        *
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR       *
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT *
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE       *
* POSSIBILITY OF SUCH DAMAGE.                                                                                          *
*                                                                                                                      *
***********************************************************************************************************************/

/**
	@file OwonHDS200.cpp
	@author Christian Antila
	@brief TODO
 */

#include "scopehal.h"
#include "OwonHDS200.h"
#include "MultimeterChannel.h"
#include "Oscilloscope.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

OwonHDS200::OwonHDS200(SCPITransport* transport)
	: SCPIDevice(transport)
	, SCPIInstrument(transport)
	, m_sampleDepthValid(false)
	, m_sampleDepth(0)
	, m_hasAWG(false)
	, m_AWGEnabled(false)
	, m_AWGAmplitude(0.5)
	, m_AWGFrequency(1000)
	, m_AWGDutyCycle(0.5)
	, m_AWGOffset(0)
	, m_AWGShape(FunctionGenerator::SHAPE_SINE)
	, m_cached_mode(std::bind(&OwonHDS200::ModeGetter, this))
	, m_triggerArmed(false)
	, m_triggerOneShot(false)
{
	//prefetch operating mode
//	GetMeterMode();

	// Create DMM channel
	m_channels.push_back(new MultimeterChannel(this, "VIN", "#808080", 0));

	// TODO: Yellow
	m_channels.push_back(new OscilloscopeChannel(this, string("CH1"), "#808000", Unit(Unit::UNIT_FS), Unit(Unit::UNIT_VOLTS), Stream::STREAM_TYPE_ANALOG, 1));

	// TODO: Cyan
	m_channels.push_back(new OscilloscopeChannel(this, string("CH2"), "#004080", Unit(Unit::UNIT_FS), Unit(Unit::UNIT_VOLTS), Stream::STREAM_TYPE_ANALOG, 2));

	// Figure out if there is an AWG
	if(m_model.length() > 0)
	{
		string last = m_model.substr(m_model.length()-1);
		if(last == "S")
		{
			m_hasAWG = true;
			m_channels.push_back(new FunctionGeneratorChannel(this, "AWG", "#808080", 3));
		}
	}

	// We need to enable rate limiting to be able to use settle_time
	// Keep it to something low as it is not really used
	m_transport->EnableRateLimiting(std::chrono::milliseconds(1));
}

OwonHDS200::~OwonHDS200()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Device info

string OwonHDS200::GetDriverNameInternal()
{
	return "owon_hds200";
}

unsigned int OwonHDS200::GetInstrumentTypes() const
{
	if(m_hasAWG)
	{
		return INST_DMM | INST_OSCILLOSCOPE | INST_FUNCTION;
	}
	else
	{
		return INST_DMM | INST_OSCILLOSCOPE;
	}
}

uint32_t OwonHDS200::GetInstrumentTypesForChannel(size_t i) const
{
	switch(i)
	{
		case 0:
			return INST_DMM;
		case 1:
			return INST_OSCILLOSCOPE;
		case 2:
			return INST_OSCILLOSCOPE;
		case 3:
			return INST_FUNCTION;
	}

	return 0; // TODO
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DMM

unsigned int OwonHDS200::GetMeasurementTypes()
{
	return AC_RMS_AMPLITUDE | DC_VOLTAGE | DC_CURRENT | AC_CURRENT | RESISTANCE | CAPACITANCE | CONTINUITY | DIODE;
}

Multimeter::MeasurementTypes OwonHDS200::GetMeterMode()
{
	return m_cached_mode.Get();
}

/**
	@breif Selects a mode on the instrument
	@param type The mode the instrument should be set to
 */
void OwonHDS200::SetMeterMode(Multimeter::MeasurementTypes type)
{
	// 300 is a bit unreliable
	// 350 seems to work great
	// Use 400 just to be sure
	const std::chrono::milliseconds settle_time = std::chrono::milliseconds(400);

	switch(type)
	{
		case AC_RMS_AMPLITUDE:
			m_transport->SendCommandImmediate(":DMM:CONF:VOLT AC", settle_time);
			break;

		case DC_VOLTAGE:
			m_transport->SendCommandImmediate(":DMM:CONF:VOLT DC", settle_time);
			break;

		case DC_CURRENT:
			m_transport->SendCommandImmediate(":DMM:CONF:CURR DC", settle_time);
			break;

		case AC_CURRENT:
			m_transport->SendCommandImmediate(":DMM:CONF:CURR AC", settle_time);
			break;

		case RESISTANCE:
			m_transport->SendCommandImmediate(":DMM:CONF RES", settle_time);
			break;

		case CAPACITANCE:
			m_transport->SendCommandImmediate(":DMM:CONF CAP", settle_time);
			break;

		case CONTINUITY:
			m_transport->SendCommandImmediate(":DMM:CONF CONT", settle_time);
			break;

		case DIODE:
			m_transport->SendCommandImmediate(":DMM:CONF DIOD", settle_time);
			break;

		// Other modes are not supported
		default:
			LogWarning("OwonHDS200::SetMeterMode() was called with a type that is not supported by the driver: %s\n", ModeToText(type).c_str());
			return;
	}

	// Save the mode in the cache
	m_cached_mode.Set(type);
}

/**
	@breif Gets the mode from the instrument and return it. This is a getter function called by the CachedVariable class
	@return The selected mode
 */
Multimeter::MeasurementTypes OwonHDS200::ModeGetter()
{
	MeasurementTypes mode;
	string result = m_transport->SendCommandImmediateWithReply(":DMM:CONF?");

	// For some reason OWON decided that a request should not return the same values we use to set the mode...
	if(result == "RS")
	{
		mode = CONTINUITY;
	}
	else if(result == "R")
	{
		mode = RESISTANCE;
	}
	else if(result == "C")
	{
		mode = CAPACITANCE;
	}
	else if(result == "DIODE")
	{
		mode = DIODE;
	}
	else
	{
		// We need to call three different commands to be able to get the mode...
		result = m_transport->SendCommandImmediateWithReply(":DMM:CONF:VOLT?");
		if(result == "DCV")
		{
			mode = DC_VOLTAGE;
		}
		else if(result == "ACV")
		{
			mode = AC_RMS_AMPLITUDE;
		}
		else
		{
			result = m_transport->SendCommandImmediateWithReply(":DMM:CONF:CURR?");
			if(result == "DCA")
			{
				mode = DC_CURRENT;
			}
			else if(result == "ACA")
			{
				mode = AC_CURRENT;
			}
			else
			{
				// Oh noes! This amazing code have failed!
				// This should never happen, this means that all three queries have failed.
				// Just set the mode to something random
				mode = DC_VOLTAGE;
				// TODO: Maybe we should have an INVALID_MODE?
			}
		}
	}

	return mode;
}

bool OwonHDS200::GetMeterAutoRange()
{
	// Note: There is no way to ask the instrument if auto mode is enabled or not.
	// This means that if the caching of range is turned off, this method till always return false
	// See more about this in the GetMeterRange() method
	return (GetMeterRange() == "AUTO");
}

void OwonHDS200::SetMeterAutoRange(bool enable)
{
	if(enable)
	{
		SetMeterRange("AUTO");
	}
	else
	{
		// No way to disable auto range
		// It can only be disabled by selecting a manual range
	}
}

/**
	@breif Return a list of all available ranges for the specified mode. Or a list one one element ##none## if no range
	       select is possible on this mode.
	@param mode The mode we should get the ranges for
	@return A list of ranges
 */
std::vector<string> OwonHDS200::GetMeterRanges(Multimeter::MeasurementTypes mode)
{
	// TODO: Should the ranges be strings or numbers?

	if(mode == DC_VOLTAGE)
	{
		std::vector<string> result;
		result.push_back("AUTO");
		result.push_back("200m");
		result.push_back("2");
		result.push_back("20");
		result.push_back("200");
		result.push_back("1000");
		return result;
	}
	else if(mode == AC_RMS_AMPLITUDE)
	{
		std::vector<string> result;
		result.push_back("AUTO");
		result.push_back("200m");
		result.push_back("2");
		result.push_back("20");
		result.push_back("200");
		result.push_back("750");
		return result;
	}
	else if(mode == DC_CURRENT)
	{
		std::vector<string> result;
		result.push_back("200m");
		result.push_back("10");
		return result;
	}
	else if(mode == AC_CURRENT)
	{
		std::vector<string> result;
		result.push_back("200m");
		result.push_back("10");
		return result;
	}
	else if(mode == RESISTANCE)
	{
		std::vector<string> result;
		result.push_back("AUTO");
		result.push_back("200");
		result.push_back("2k");
		result.push_back("20k");
		result.push_back("200k");
		result.push_back("200k");
		result.push_back("2M");
		result.push_back("20M");
		result.push_back("100M");
		return result;
	}
	else
	{
		std::vector<string> result;
		result.push_back("##none##");
		return result;
	}
}

/**
	@breif TODO
	@return TODO
 */
string OwonHDS200::GetMeterRange()
{
	// TODO: Implement

	string reply = m_transport->SendCommandImmediateWithReply(":DMM:RANGE?");

	// Remove the last character "V"
	string stripped = reply.substr(0, reply.length()-1);

	// TODO: mA, A, mV & V kommer behöva en strängjämförelse
	// TODO: Övriga modes borde funka genom att ta bort sista bokstaven
	// TODO: Det är omöjligt att fråga instrumentet om auto range är igång

	return stripped;
}

/**
	@breif Selects a range on the DMM
	$param select_range The range to be set
 */
void OwonHDS200::SetMeterRange(string select_range)
{
	Multimeter::MeasurementTypes mode = m_cached_mode.Get();

	// Check that the selected range is valid
	std::vector<string> valid_ranges = GetMeterRanges(mode);
	auto it = std::find(valid_ranges.begin(), valid_ranges.end(), select_range);
	if(it == valid_ranges.end())
	{
		LogError("SetMeterRange() received an unkown range: %s\n", select_range.c_str());
		return;
	}

	if(select_range == "AUTO")
	{
		// Auto range is not supported in the 200 mV AC/DC range
		// Make sure we're in V range if the user selects AUTO
		if(mode == DC_VOLTAGE || mode == AC_RMS_AMPLITUDE)
		{
			m_transport->SendCommandImmediate(":DMM:RANGE V", std::chrono::milliseconds(780));
		}

		// Enable the auto range.
		// There is no way to figure out if the auto range is on, so we just block for 800 ms and hope is enough.
		m_transport->SendCommandImmediate(":DMM:AUTO ON", std::chrono::milliseconds(800));
	}
	else if(mode == DC_VOLTAGE || mode == AC_RMS_AMPLITUDE)
	{
		// The HDS200 have two diffeent modes for voltage measurement: mV and V.
		// A relay switches between the modes.
		// The mV mode does only have one range (200mV).
		// The V mode can be toggled between 2V, 20V, 200V and 1000V.
		// In the V mode it is also possible to select auto range

		// Switching between V and mV is slow
		// 770 ms does not work
		// 775 ms works
		// Use 850 ms to be sure

		if(select_range == "200m")
		{
			printf("Select mV\n");
			m_transport->SendCommandImmediate(":DMM:RANGE mV", std::chrono::milliseconds(780));
		}
		else
		{
			printf("Select V\n");
			m_transport->SendCommandImmediate(":DMM:RANGE V", std::chrono::milliseconds(780));

			_meterCycleRangeUntilValid(select_range);
		}
	}
	else if(mode == DC_CURRENT || mode == AC_CURRENT)
	{
		// The current range have two sets of inputs, one for 200 mA and one for 10 A
		if(select_range == "200m")
		{
			m_transport->SendCommandImmediate(":DMM:RANGE mA", std::chrono::milliseconds(780));
		}
		else
		{
			m_transport->SendCommandImmediate(":DMM:RANGE A", std::chrono::milliseconds(780));
		}
	}
	else if(mode == RESISTANCE)
	{
		_meterCycleRangeUntilValid(select_range);
	}

	// Note: It is not possible to manually select range in the capacitance mode
}

/**
	@breif Cycles the range and check the result until we find the correct one. Try cycling for 20 times before aborting.
	@param range The range we should select
	@return true on success
 */
bool OwonHDS200::_meterCycleRangeUntilValid(string range)
{
	// There is no way to explicitly select a range.
	// The range can only be changed by toggling between the different ranges.
	// We have to ask what range got selected and see if it is correct or if we should try again.
	// Try cycling for 20 times and then abort

	// Note: It does seem to be possible to quickly send X amount of range switches, then wait for a while and check that the correct one got selected. If we know for sure which one we have already selected then this might be a faster option.
	for(int i = 0; i <= 20; i++)
	{
		string reply = GetMeterRange();

		printf("Got reply: %s\n", reply.c_str());
		if(reply == range)
		{
			printf("Modes matches\n");
			return true;
		}

		printf("Sending range switch\n");

		// We need to rate limit very hard
		// 300 ms to short
		// 350 ms unreliable
		// 350 ms works
		// Use 400 ms to be safe
		// (Even 400 ms have failed at least once)
		m_transport->SendCommandImmediate(":DMM:RANGE ON", std::chrono::milliseconds(400));
	}

	return false;
}

int OwonHDS200::GetMeterDigits()
{
	return 6; // TODO
}

double OwonHDS200::GetMeterValue()
{
	return stod(m_transport->SendCommandImmediateWithReply(":DMM:MEAS?"));
}

int OwonHDS200::GetCurrentMeterChannel()
{
	// Only one channel
	return 0;
}

void OwonHDS200::SetCurrentMeterChannel(int chan)
{
	// Only one channel
	(void) chan;
}

void OwonHDS200::StartMeter()
{
	// Cannot be started or stopped
}

void OwonHDS200::StopMeter()
{
	// Cannot be started or stopped
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Oscilloscope

bool OwonHDS200::IsChannelEnabled(size_t i)
{
	// Only the scope channels, ignore DMM and AWG
	if(i != 1 && i != 2)
	{
		return false;
	}

	return (m_transport->SendCommandImmediateWithReply(":CH" + to_string(i) + ":DISP?") == "ON");
}

void OwonHDS200::EnableChannel(size_t i)
{
	// Only the scope channels, ignore DMM and AWG
	if(i != 1 && i != 2)
	{
		return;
	}

	m_transport->SendCommandImmediate(":CH" + to_string(i) + ":DISP ON"/*, std::chrono::milliseconds(400)*/);
	// TODO: Timing
}

void OwonHDS200::DisableChannel(size_t i)
{
	// Only the scope channels, ignore DMM and AWG
	if(i != 1 && i != 2)
	{
		return;
	}

	m_transport->SendCommandImmediate(":CH" + to_string(i) + ":DISP OFF"/*, std::chrono::milliseconds(400)*/);
	// TODO: Timing
}

std::vector<OscilloscopeChannel::CouplingType> OwonHDS200::GetAvailableCouplings(size_t i)
{
	(void) i;

	vector<OscilloscopeChannel::CouplingType> ret;
	ret.push_back(OscilloscopeChannel::COUPLE_AC_1M);
	ret.push_back(OscilloscopeChannel::COUPLE_DC_1M);
	ret.push_back(OscilloscopeChannel::COUPLE_GND);
	return ret;
}

OscilloscopeChannel::CouplingType OwonHDS200::GetChannelCoupling(size_t i)
{
	string reply = m_transport->SendCommandImmediateWithReply(":CH" + to_string(i) + ":COUP?");
	if(reply == "DC")
	{
		return OscilloscopeChannel::COUPLE_DC_1M;
	}
	else if(reply == "AC")
	{
		return OscilloscopeChannel::COUPLE_AC_1M;
	}
	else
	{
		return OscilloscopeChannel::COUPLE_GND;
	}
}

void OwonHDS200::SetChannelCoupling(size_t i, OscilloscopeChannel::CouplingType type)
{
	// TODO: Timing

	if(type == OscilloscopeChannel::COUPLE_DC_1M)
	{
		m_transport->SendCommandImmediate(":CH" + to_string(i) + ":COUP DC"/*, std::chrono::milliseconds(400)*/);
	}
	else if(type == OscilloscopeChannel::COUPLE_AC_1M)
	{
		m_transport->SendCommandImmediate(":CH" + to_string(i) + ":COUP AC"/*, std::chrono::milliseconds(400)*/);
	}
	else
	{
		m_transport->SendCommandImmediate(":CH" + to_string(i) + ":COUP GND"/*, std::chrono::milliseconds(400)*/);
	}
}

double OwonHDS200::GetChannelAttenuation(size_t i)
{
	// Only the scope channels, ignore DMM and AWG
	if(i != 1 && i != 2)
	{
		return 0.0;
	}

	string reply = m_transport->SendCommandImmediateWithReply(":CH" + to_string(i) + ":PROB?");
	if(reply == "1X")
	{
		return 1.0;
	}
	else if(reply == "10X")
	{
		return 10.0;
	}
	else if(reply == "100X")
	{
		return 100.0;
	}
	else if(reply == "1000X")
	{
		return 1000.0;
	}
	else
	{
		return 0.0;
	}
}

void OwonHDS200::SetChannelAttenuation(size_t i, double atten)
{
	// Only the scope channels, ignore DMM and AWG
	if(i != 1 && i != 2)
	{
		return;
	}

	// TODO: Timing

	if(atten == 1.0)
	{
		m_transport->SendCommandImmediate(":CH" + to_string(i) + ":PROB 1X"/*, std::chrono::milliseconds(400)*/);
	}
	else if(atten == 10.0)
	{
		m_transport->SendCommandImmediate(":CH" + to_string(i) + ":PROB 10X"/*, std::chrono::milliseconds(400)*/);
	}
	else if(atten == 100.0)
	{
		m_transport->SendCommandImmediate(":CH" + to_string(i) + ":PROB 100X"/*, std::chrono::milliseconds(400)*/);
	}
	else if(atten == 1000.0)
	{
		m_transport->SendCommandImmediate(":CH" + to_string(i) + ":PROB 1000X"/*, std::chrono::milliseconds(400)*/);
	}
}

unsigned int OwonHDS200::GetChannelBandwidthLimit(size_t i)
{
	(void) i;
	// TODO: It doesn't seem like it is possible to control bandwidth limit via SCPI
	return 0;
}

void OwonHDS200::SetChannelBandwidthLimit(size_t i, unsigned int limit_mhz)
{
	(void) i;
	(void) limit_mhz;
	// TODO: It doesn't seem like it is possible to control bandwidth limit via SCPI
}

float OwonHDS200::GetChannelVoltageRange(size_t i, size_t stream)
{
	(void) i;
	(void) stream;
	// TODO: Implement
	return 1.0f;
}

void OwonHDS200::SetChannelVoltageRange(size_t i, size_t stream, float range)
{
	(void) i;
	(void) stream;
	(void) range;
	// TODO: Implement
}

OscilloscopeChannel* OwonHDS200::GetExternalTrigger()
{
	// TODO: Implement
	return NULL;
}

float OwonHDS200::GetChannelOffset(size_t i, size_t stream)
{
	(void) i;
	(void) stream;
	// TODO: Implement
	return 0.0f;
}

void OwonHDS200::SetChannelOffset(size_t i, size_t stream, float offset)
{
	(void) i;
	(void) stream;
	(void) offset;
	// TODO: Implement
}

// Oscilloscope triggering
Oscilloscope::TriggerMode OwonHDS200::PollTrigger()
{
	if(m_triggerArmed)
	{
		return TRIGGER_MODE_TRIGGERED;
	}
	else
	{
		return TRIGGER_MODE_STOP;
	}
}

bool OwonHDS200::AcquireData()
{
	printf("AcquireData()\n");

	// TODO: Implement
	string reply;
//	string s;

	size_t len;
	unsigned char *data;
	data = (unsigned char*)m_transport->SendCommandImmediateWithRawBlockReply(":DAT:WAV:SCR:HEAD?", len);
	printf("Header: %li bytes: %s\n", len, data);
	// TODO: Free data

	data = (unsigned char*)m_transport->SendCommandImmediateWithRawBlockReply(":DAT:WAV:SCR:CH1?", len);
//	printf("CH1: %li bytes: %s\n", len, data);

/*
//	reply = m_transport->SendCommandImmediateWithReply(":DAT:WAV:SCR:CH2?");
//	printf("CH2: %s\n", reply.c_str());

	// From documentation: The data point is recorded as 8-bit, a point uses two bytes, little-endian byte order.

	return false;
*/

	//cap waveform rate at 50 wfm/s to avoid saturating cpu
//	std::this_thread::sleep_for(std::chrono::microseconds(20 * 1000));

	//Sweeping frequency
//	m_sweepFreq += 1e6;
//	if(m_sweepFreq > 1.5e9)
//		m_sweepFreq = 1.1e9;
//	float sweepPeriod = FS_PER_SECOND / m_sweepFreq;

	//Generate waveforms
//	auto depth = GetSampleDepth();
	size_t depth = 600;
	double m_rate = 50e3;
	int64_t sampleperiod = FS_PER_SECOND / m_rate;
	WaveformBase* waveforms[4] = {NULL};


	auto ret = new UniformAnalogWaveform("Step");
	ret->m_timescale = sampleperiod;
	ret->Resize(depth);

	printf("Depth: %li\n", depth);
	for(size_t i=0; i<depth; i++)
	{

		int16_t tmp = 0;
		memcpy(&tmp, &data[i*2], 2);
		ret->m_samples[i] =  tmp / 65535.0 * 0.5;

/*
		int8_t tmp = 0;
		memcpy(&tmp, &data[i+1], 1);
		ret->m_samples[i] =  tmp / 255.0 * 0.5;

		tmp = 0;
		memcpy(&tmp, &data[i], 1);
		ret->m_samples[i+1] =  tmp / 255.0 * 0.5;

		i++;
*/
	}





	waveforms[0] = ret;
	waveforms[0]->MarkModifiedFromCpu();


	SequenceSet s;
	s[GetOscilloscopeChannel(1)] = waveforms[0];
//	s[GetOscilloscopeChannel(2)] = waveforms[1];

	//Timestamp the waveform(s)
	double now = GetTime();
	time_t start = now;
	double tfrac = now - start;
	int64_t fs = tfrac * FS_PER_SECOND;
	for(auto it : s)
	{
		auto wfm = it.second;
		if(!wfm)
			continue;

		wfm->m_startTimestamp = start;
		wfm->m_startFemtoseconds = fs;
		wfm->m_triggerPhase = 0;
	}

	m_pendingWaveformsMutex.lock();
	m_pendingWaveforms.push_back(s);
	m_pendingWaveformsMutex.unlock();

	if(m_triggerOneShot)
	{
		m_triggerArmed = false;
	}


	return true;
}

void OwonHDS200::Start()
{
	m_triggerArmed = true;
}

void OwonHDS200::StartSingleTrigger()
{
	m_triggerArmed = true;
	m_triggerOneShot = true;
}

void OwonHDS200::Stop()
{
	m_triggerArmed = false;
}

void OwonHDS200::ForceTrigger()
{
	StartSingleTrigger();
}

bool OwonHDS200::IsTriggerArmed()
{
	// TODO: Implement
	return true;
}

void OwonHDS200::PushTrigger()
{
	// TODO: Implement
}

void OwonHDS200::PullTrigger()
{
	// TODO: Implement
}

void OwonHDS200::SetTriggerOffset(int64_t offset)
{
	(void) offset;
	// TODO: Implement
}

int64_t OwonHDS200::GetTriggerOffset()
{
	// TODO: Implement
	return 0;
}


// Other oscilloscope stuff
std::vector<uint64_t> OwonHDS200::GetSampleRatesNonInterleaved()
{
	// The sample rate will be set automatically from the timebase and memory depth. There is no way for the user to control it.

	vector<uint64_t> ret;
	return ret;
}

std::vector<uint64_t> OwonHDS200::GetSampleRatesInterleaved()
{
	// HDS200 does not support interleaving
	return GetSampleRatesNonInterleaved();
}

std::set<Oscilloscope::InterleaveConflict> OwonHDS200::GetInterleaveConflicts()
{
	set<Oscilloscope::InterleaveConflict> ret;

	// HDS200 have no interleaving
	// Every channel conflicts with itself

	// TODO: Implement
#if 0
	for(size_t i=0; i<m_analogChannelCount; i++)
		ret.emplace(InterleaveConflict(GetOscilloscopeChannel(i), GetOscilloscopeChannel(i)));
#endif

	return ret;
}

std::vector<uint64_t> OwonHDS200::GetSampleDepthsNonInterleaved()
{
	vector<uint64_t> ret;

	ret.push_back(4000);
	ret.push_back(8000);

	return ret;
}

std::vector<uint64_t> OwonHDS200::GetSampleDepthsInterleaved()
{
	// HDS200 does not support interleaving
	return GetSampleDepthsNonInterleaved();
}

uint64_t OwonHDS200::GetSampleRate()
{
	// TODO: Implement

/*
@chille
The OWON HDS200 scope does not allow the user to set the samplerate. It seems to be controlled automatically based in the timebase and memory depth. This means it will be problematic to do a correct implementation of Oscilloscope::GetSampleRatesNonInterleaved(). Will this cause a problem somewhere in the application?

@azonenberg
This is how most scopes work
GetSampleRates* just returns the list of valid sample rates the scope provides
then SetSampleRate() will need to set time/div based on memory depth and the desired rate
and something like that
or sometimes its the opposite you contrl the rate and time/div controls depth
Pico is one of the few that actually gives you explicit control over both
*/

	return 0;
}

void OwonHDS200::SetSampleRate(uint64_t rate)
{
	(void) rate;
	// HDS200 does not let the user control the samplerate
	// TODO: Do we need to implement this anyway? See GetSampleRate()
}

uint64_t OwonHDS200::GetSampleDepth()
{
	if(!m_sampleDepthValid)
	{
		m_sampleDepth = stos(m_transport->SendCommandImmediateWithReply(":ACQ:DEPM?"));
		m_sampleDepth *= 1000;
		m_sampleDepthValid = true;
	}

	return m_sampleDepth;
}

void OwonHDS200::SetSampleDepth(uint64_t depth)
{
	// TODO: Do we need to "Update the cache" as in TektronixOscilloscope::SetSampleDepth()?
	if(depth == 8000)
	{
		m_transport->SendCommandImmediate(string(":ACQ:DEPM? 8K"));
	}
	else
	{
		m_transport->SendCommandImmediate(string(":ACQ:DEPM? 4K"));
	}
}

bool OwonHDS200::IsInterleaving()
{
	// HDS200 have no interleaving
	return false;
}

bool OwonHDS200::SetInterleaving(bool combine)
{
	// HDS200 have no interleaving
	(void) combine;
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// AWG

bool OwonHDS200::GetFunctionChannelActive(int chan)
{
	(void) chan;
	return m_AWGEnabled;
}

void OwonHDS200::SetFunctionChannelActive(int chan, bool on)
{
	(void) chan;

	m_AWGEnabled = on;

	// TODO: Do we really need this?
//	lock_guard<recursive_mutex> lock(m_mutex);

	if(on)
	{
		m_transport->SendCommandImmediate(":CHAN ON");
	}
	else
	{
		m_transport->SendCommandImmediate(":CHAN OFF");
	}
}

float OwonHDS200::GetFunctionChannelAmplitude(int chan)
{
	(void) chan;
	return m_AWGAmplitude;
}

void OwonHDS200::SetFunctionChannelAmplitude(int chan, float amplitude)
{
	(void) chan;

	m_AWGAmplitude = amplitude;

	// TODO: Do we really need this?
//	lock_guard<recursive_mutex> lock(m_mutex);

	m_transport->SendCommandImmediate(string(":FUNC:AMP ") + to_string(amplitude));
}

float OwonHDS200::GetFunctionChannelOffset(int chan)
{
	(void) chan;
	return m_AWGOffset;
}

void OwonHDS200::SetFunctionChannelOffset(int chan, float offset)
{
	(void) chan;

	m_AWGOffset = offset;

	// TODO: Do we really need this?
//	lock_guard<recursive_mutex> lock(m_mutex);

	m_transport->SendCommandImmediate(string(":FUNC:OFF ") + to_string(offset));
}

float OwonHDS200::GetFunctionChannelFrequency(int chan)
{
	(void) chan;
	return m_AWGFrequency;
}

void OwonHDS200::SetFunctionChannelFrequency(int chan, float hz)
{
	(void) chan;
	m_AWGFrequency = hz;

	// TODO: Do we really need this?
//	lock_guard<recursive_mutex> lock(m_mutex);

	m_transport->SendCommandImmediate(string(":FUNC:FREQ ") + to_string(hz));
}

float OwonHDS200::GetFunctionChannelDutyCycle(int chan)
{
	(void) chan;
	return m_AWGShape == SHAPE_PULSE ? m_AWGDutyCycle : 0;
}

void OwonHDS200::SetFunctionChannelDutyCycle(int chan, float duty)
{
	(void) chan;

	m_AWGDutyCycle = duty;

	if (m_AWGShape != SHAPE_PULSE)
		return;

	// TODO: :FUNC:DTY does seem to be the correct way of doing it? Ignore :FUNC:WIDT

	// The pulse width is specified in seconds
	// So we have to calculate a pulse width from the duty cycle and frequency
//	float value = 1.0 / m_AWGFrequency * duty;

	// TODO: Is this really needed?
//	lock_guard<recursive_mutex> lock(m_mutex);

//	m_transport->SendCommandImmediate(string(":FUNC:WIDT ") + to_string(value));
	m_transport->SendCommandImmediate(string(":FUNC:DTY ") + to_string(duty));
}

std::vector<FunctionGenerator::WaveShape> OwonHDS200::GetAvailableWaveformShapes(int)
{
	vector<WaveShape> ret;
	ret.push_back(FunctionGenerator::SHAPE_SINE);
	ret.push_back(FunctionGenerator::SHAPE_SQUARE);
	ret.push_back(FunctionGenerator::SHAPE_TRIANGLE);
	ret.push_back(FunctionGenerator::SHAPE_PULSE);
	// TODO: AmpALT
	// TODO: AttALT
	ret.push_back(FunctionGenerator::SHAPE_STAIRCASE_DOWN);    // StairDn
	ret.push_back(FunctionGenerator::SHAPE_STAIRCASE_UP);      // StairUp
	ret.push_back(FunctionGenerator::SHAPE_STAIRCASE_UP_DOWN); // StairUD
	// TODO: Besselj
	// TODO: Bessely
	ret.push_back(FunctionGenerator::SHAPE_SINC);

	return ret;
}

FunctionGenerator::WaveShape OwonHDS200::GetFunctionChannelShape(int chan)
{
	(void) chan;
	return m_AWGShape;
}

void OwonHDS200::SetFunctionChannelShape(int chan, WaveShape shape)
{
	(void) chan;

	m_AWGShape = shape;

	// TODO: What is this?
//	lock_guard<recursive_mutex> lock(m_mutex);

	switch(shape)
	{
		case SHAPE_SINE:
			m_transport->SendCommandImmediate(string(":FUNC SINE"));
			break;

		case SHAPE_SQUARE:
			m_transport->SendCommandImmediate(string(":FUNC SQU"));
			break;

		case SHAPE_TRIANGLE:
			m_transport->SendCommandImmediate(string(":FUNC RAMP"));
			break;

		case SHAPE_PULSE:
			m_transport->SendCommandImmediate(string(":FUNC PULS"));
			break;

		// TODO: AmpALT
		// TODO: AttALT

		case SHAPE_STAIRCASE_DOWN:
			m_transport->SendCommandImmediate(string(":FUNC StairDn"));
			break;

		case SHAPE_STAIRCASE_UP:
			m_transport->SendCommandImmediate(string(":FUNC StairUp"));
			break;

		case SHAPE_STAIRCASE_UP_DOWN:
			m_transport->SendCommandImmediate(string(":FUNC StairUD"));
			break;

		// TODO: Besselj
		// TODO: Bessely
		// TODO: Sinc

		default:
			break;
	}
}

bool OwonHDS200::HasFunctionRiseFallTimeControls(int)
{
	// TODO: The HDS200 does not have this? If so, then just return false
	return false;
}