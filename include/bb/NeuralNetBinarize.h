﻿// --------------------------------------------------------------------------
//  Binary Brain  -- binary neural net framework
//
//                                     Copyright (C) 2018 by Ryuji Fuchikami
//                                     https://github.com/ryuz
//                                     ryuji.fuchikami@nifty.com
// --------------------------------------------------------------------------



#pragma once

#include <Eigen/Core>

#include "bb/NeuralNetLayerBuf.h"


namespace bb {


// バイナリ化(活性化関数)
template <typename T = float>
class NeuralNetBinarize : public NeuralNetLayerBuf<T>
{
protected:
	INDEX		m_frame_size = 1;
	INDEX		m_node_size = 0;
	bool		m_enable = true;

public:
	NeuralNetBinarize() {}

	NeuralNetBinarize(INDEX node_size)
	{
		Resize(node_size);
	}

	~NeuralNetBinarize() {}

	std::string GetClassName(void) const { return "NeuralNetBinarize"; }

	void Resize(INDEX node_size)
	{
		m_node_size = node_size;
	}

	void  SetBatchSize(INDEX batch_size) { m_frame_size = batch_size; }

	INDEX GetInputFrameSize(void) const { return m_frame_size; }
	INDEX GetInputNodeSize(void) const { return m_node_size; }
	INDEX GetOutputFrameSize(void) const { return m_frame_size; }
	INDEX GetOutputNodeSize(void) const { return m_node_size; }

	int   GetInputSignalDataType(void) const { return NeuralNetType<T>::type; }
	int   GetInputErrorDataType(void) const { return NeuralNetType<T>::type; }
	int   GetOutputSignalDataType(void) const { return NeuralNetType<T>::type; }
	int   GetOutputErrorDataType(void) const { return NeuralNetType<T>::type; }

	void Forward(bool train = true)
	{
		auto x = this->GetInputSignalBuffer();
		auto y = this->GetOutputSignalBuffer();

		// binarize
		for (INDEX node = 0; node < m_node_size; ++node) {
			for (INDEX frame = 0; frame < m_frame_size; ++frame) {
				y.template Set<T>(frame, node, x.template Get<T>(frame, node) > (T)0.0 ? (T)1.0 : (T)0.0);
			}
		}
	}

	void Backward(void)
	{
		auto dx = this->GetInputErrorBuffer();
		auto dy = this->GetOutputErrorBuffer();
		auto x = this->GetInputSignalBuffer();
		auto y = this->GetOutputSignalBuffer();

		// hard-tanh
		for (INDEX node = 0; node < m_node_size; ++node) {
			for (INDEX frame = 0; frame < m_frame_size; ++frame) {
				auto err = dy.template Get<T>(frame, node);
				auto sig = x.template Get<T>(frame, node);
				dx.template Set<T>(frame, node, (sig >= (T)-1.0 && sig <= (T)1.0) ? err : 0);
			}
		}
	}

	void Update(void)
	{
	}
};

}
