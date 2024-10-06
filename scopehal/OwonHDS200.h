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
	@file OwonHDS200.h
	@author Christian Antila
	@brief TODO
 */

#ifndef OwonHDS200_h
#define OwonHDS200_h

using std::string;

/**
	@breif This class will handle caching for one variable.

	       If the cache is enabled then the Get() method will either return the cached value, or call a getter provided
	       by the user. This getter will ask the instrument for a value and return it. The value will then be cached
	       and can be used again if caching is enabled.

	       The Set() method will store data in the cache.
 */
template <typename T> class CachedVariable
{
public:
	T m_value;
	bool m_is_valid;
	bool m_cache_enabled;
	std::function<T()> m_getter;

	CachedVariable(
			std::function<T()> const& lambda_getter
		)
		: m_is_valid(false)
		, m_cache_enabled(true)
		, m_getter(lambda_getter)
	{
	}

	T Get()
	{
		if(!m_cache_enabled || !m_is_valid)
		{
//			printf("No value cached, calling getter\n");
			m_value = m_getter();
			m_is_valid = true;
		}

		return m_value;
	}

	void Set(T value)
	{
		m_value = value;
		m_is_valid = true;
	}
};

/**
	@brief The OWON HDS200 series is a series of handheld instruments that combines an oscilloscope, a multimeter (DMM)
	       and an arbitrary waveform generator (AWG) into one package. All three functions can be used at the same time.

	       This code has been tested on a HDS2102S with firmware V3.0.1
 */
class OwonHDS200
	: public virtual SCPIMultimeter
	, public virtual SCPIOscilloscope
	, public virtual SCPIFunctionGenerator
{
public:
	OwonHDS200(SCPITransport* transport);
	virtual ~OwonHDS200();

	// Generic stuff
	virtual unsigned int GetInstrumentTypes() const override;
	virtual uint32_t GetInstrumentTypesForChannel(size_t i) const override;
//	virtual unsigned int GetSecondaryMeasurementTypes() override;

	// DMM
	virtual unsigned int GetMeasurementTypes() override;
	virtual MeasurementTypes GetMeterMode() override;
	virtual void SetMeterMode(MeasurementTypes type) override;
	virtual void SetMeterAutoRange(bool enable) override;
	virtual bool GetMeterAutoRange() override;
	virtual string GetMeterRange(); // TODO: Add to class Multimeter and override here
	virtual void SetMeterRange(string select_range); // TODO: Add to class Multimeter and override here
	virtual int GetMeterDigits() override;
	virtual double GetMeterValue() override;
	virtual int GetCurrentMeterChannel() override;
	virtual void SetCurrentMeterChannel(int chan) override;
	virtual void StartMeter() override;
	virtual void StopMeter() override;

	// Oscilloscope
	virtual bool IsChannelEnabled(size_t i) override;
	virtual void EnableChannel(size_t i) override;
//	virtual bool CanEnableChannel(size_t i) override;
	virtual void DisableChannel(size_t i) override;
	virtual std::vector<OscilloscopeChannel::CouplingType> GetAvailableCouplings(size_t i) override;
	virtual OscilloscopeChannel::CouplingType GetChannelCoupling(size_t i) override;
	virtual void SetChannelCoupling(size_t i, OscilloscopeChannel::CouplingType type) override;
	virtual double GetChannelAttenuation(size_t i) override;
	virtual void SetChannelAttenuation(size_t i, double atten) override;
	virtual unsigned int GetChannelBandwidthLimit(size_t i) override;
	virtual void SetChannelBandwidthLimit(size_t i, unsigned int limit_mhz) override;
	virtual float GetChannelVoltageRange(size_t i, size_t stream) override;
	virtual void SetChannelVoltageRange(size_t i, size_t stream, float range) override;
	virtual OscilloscopeChannel* GetExternalTrigger() override;
	virtual float GetChannelOffset(size_t i, size_t stream) override;
	virtual void SetChannelOffset(size_t i, size_t stream, float offset) override;
//	virtual std::string GetChannelDisplayName(size_t i) override;
//	virtual void SetChannelDisplayName(size_t i, std::string name) override;
//	virtual std::vector<unsigned int> GetChannelBandwidthLimiters(size_t i) override;
//	Unit GetYAxisUnit(size_t i);
//	virtual bool CanDegauss(size_t i) override;
//	virtual bool ShouldDegauss(size_t i) override;
//	virtual void Degauss(size_t i) override;

	// Oscilloscope triggering
	virtual TriggerMode PollTrigger() override;
//	virtual bool PeekTriggerArmed() override;
	virtual bool AcquireData() override;
	virtual void Start() override;
	virtual void StartSingleTrigger() override;
	virtual void Stop() override;
	virtual void ForceTrigger() override;
	virtual bool IsTriggerArmed() override;
	virtual void PushTrigger() override;
	virtual void PullTrigger() override;
	virtual void SetTriggerOffset(int64_t offset) override;
	virtual int64_t GetTriggerOffset() override;
//	std::vector<std::string> GetTriggerTypes() override;

	// Other oscilloscope stuff
	virtual std::vector<uint64_t> GetSampleRatesNonInterleaved() override;
	virtual std::vector<uint64_t> GetSampleRatesInterleaved() override;
	virtual std::set<InterleaveConflict> GetInterleaveConflicts() override;
	virtual std::vector<uint64_t> GetSampleDepthsNonInterleaved() override;
	virtual std::vector<uint64_t> GetSampleDepthsInterleaved() override;
	virtual uint64_t GetSampleRate() override;
	virtual void SetSampleRate(uint64_t rate) override;
	virtual uint64_t GetSampleDepth() override;
	virtual void SetSampleDepth(uint64_t depth) override;
	virtual bool IsInterleaving() override;
	virtual bool SetInterleaving(bool combine) override;

	// AWG
	virtual bool GetFunctionChannelActive(int) override;
	virtual void SetFunctionChannelActive(int, bool) override;
	virtual float GetFunctionChannelAmplitude(int) override;
	virtual void SetFunctionChannelAmplitude(int, float) override;
	virtual float GetFunctionChannelOffset(int) override;
	virtual void SetFunctionChannelOffset(int, float) override;
	virtual float GetFunctionChannelFrequency(int) override;
	virtual void SetFunctionChannelFrequency(int, float) override;
	virtual float GetFunctionChannelDutyCycle(int chan) override;
	virtual void SetFunctionChannelDutyCycle(int chan, float duty) override;
	virtual std::vector<WaveShape> GetAvailableWaveformShapes(int) override;
	virtual WaveShape GetFunctionChannelShape(int) override;
	virtual void SetFunctionChannelShape(int, WaveShape) override;
	virtual bool HasFunctionRiseFallTimeControls(int) override;

	// TODO: Experimental
	bool _meterCycleRangeUntilValid(string range);
	std::vector<string> GetMeterRanges(Multimeter::MeasurementTypes mode);

protected:
	// Oscilloscope
	bool m_sampleDepthValid;
	uint64_t m_sampleDepth;

	// AWG
	bool m_hasAWG;
	bool m_AWGEnabled;
	float m_AWGAmplitude;
	float m_AWGFrequency;
	float m_AWGDutyCycle;
	float m_AWGOffset;
	FunctionGenerator::WaveShape m_AWGShape;

	CachedVariable<Multimeter::MeasurementTypes> m_cached_mode;

	void ModeSetter(Multimeter::MeasurementTypes input);
	Multimeter::MeasurementTypes ModeGetter();

	bool m_triggerArmed;
	bool m_triggerOneShot;

public:
	static std::string GetDriverNameInternal();
//	METER_INITPROC(OwonHDS200)
//	GENERATOR_INITPROC(OwonHDS200)
	OSCILLOSCOPE_INITPROC(OwonHDS200)
};

#endif