// --------------------------------------------------------------------------
//  Binary Brain  -- binary neural net framework
//
//                                     Copyright (C) 2018 by Ryuji Fuchikami
//                                     https://github.com/ryuz
//                                     ryuji.fuchikami@nifty.com
// --------------------------------------------------------------------------



#pragma once

#include <random>
#include "NeuralNetLayerBuf.h"


namespace bb {


// NeuralNet�̒��ۃN���X
template <typename BT = Bit, typename T = float, typename INDEX = size_t>
class NeuralNetRealToBinary : public NeuralNetLayerBuf<T, INDEX>
{
protected:
	std::mt19937_64		m_mt;

	INDEX				m_input_node_size = 0;
	INDEX				m_output_node_size = 0;
	INDEX				m_batch_size = 1;
	INDEX				m_mux_size = 1;

	T					m_real_range_lo = (T)0.0;
	T					m_real_range_hi = (T)1.0;

public:
	NeuralNetRealToBinary() {}

	NeuralNetRealToBinary(INDEX input_node_size, INDEX output_node_size, std::uint64_t seed = 1)
	{
		Resize(input_node_size, output_node_size);
		InitializeCoeff(seed);
	}

	~NeuralNetRealToBinary() {}

	void Resize(INDEX input_node_size, INDEX output_node_size)
	{
		m_input_node_size = input_node_size;
		m_output_node_size = output_node_size;
	}

	void  InitializeCoeff(std::uint64_t seed)
	{
		m_mt.seed(seed);
	}

	void  SetMuxSize(INDEX mux_size) { m_mux_size = mux_size; }
	INDEX GetMuxSize(void) { return m_mux_size; }
	void  SetBatchSize(INDEX batch_size) { m_batch_size = batch_size; }

	INDEX GetInputFrameSize(void) const { return m_batch_size; }
	INDEX GetInputNodeSize(void) const { return m_input_node_size; }
	INDEX GetOutputFrameSize(void) const { return m_batch_size * m_mux_size; }
	INDEX GetOutputNodeSize(void) const { return m_output_node_size; }

	int   GetInputSignalDataType(void) const { return NeuralNetType<T>::type; }
	int   GetOutputSignalDataType(void) const { return NeuralNetType<BT>::type; }
	int   GetInputErrorDataType(void) const { return NeuralNetType<T>::type; }
	int   GetOutputErrorDataType(void) const { return NeuralNetType<T>::type; }

	void Forward(bool train = true)
	{
		auto in_val = GetInputSignalBuffer();
		auto out_val = GetOutputSignalBuffer();

		std::uniform_real_distribution<T>	rand(m_real_range_lo, m_real_range_hi);

		INDEX node_size = std::max(m_input_node_size, m_output_node_size);
		for (INDEX frame = 0; frame < m_batch_size; frame++) {
			std::vector<T>		vec_v(m_output_node_size, (T)0.0);
			std::vector<int>	vec_n(m_output_node_size, 0);
			for (INDEX node = 0; node < node_size; node++) {
				vec_v[node % m_output_node_size] += in_val.Get<T>(frame, node % m_input_node_size);
				vec_n[node % m_output_node_size] += 1;
			}

			for (INDEX node = 0; node < m_output_node_size; node++) {
				T		realVal = vec_v[node] / (T)vec_n[node];
				Binary	binVal = (realVal > rand(m_mt));
				for (INDEX i = 0; i < m_mux_size; i++) {
					out_val.Set<BT>(frame*m_mux_size + i, node, binVal);
				}
			}
		}
	}

	void Backward(void)
	{
		auto in_err = GetInputErrorBuffer();
		auto out_err = GetOutputErrorBuffer();

		in_err.Clear();
		for (INDEX node = 0; node < m_output_node_size; node++) {
			for (INDEX frame = 0; frame < m_batch_size; ++frame) {
				auto err = in_err.Get<T>(frame, node % m_input_node_size);
				for (INDEX i = 0; i < m_mux_size; i++) {
					err += out_err.Get<T>(frame*m_mux_size + i, node);
				}
				in_err.Set<T>(frame, node % m_input_node_size, err);
			}
		}
	}

	void Update(double learning_rate)
	{
	}

};

}