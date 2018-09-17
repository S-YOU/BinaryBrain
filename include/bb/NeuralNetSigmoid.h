// --------------------------------------------------------------------------
//  Binary Brain  -- binary neural net framework
//
//                                     Copyright (C) 2018 by Ryuji Fuchikami
//                                     https://github.com/ryuz
//                                     ryuji.fuchikami@nifty.com
// --------------------------------------------------------------------------



#pragma once

//#ifndef EIGEN_MPL2_ONLY
//#define EIGEN_MPL2_ONLY
//#endif
#include <Eigen/Core>

#include "NeuralNetLayerBuf.h"


namespace bb {


// シグモイド(活性化関数)
template <typename T = float, typename INDEX = size_t>
class NeuralNetSigmoid : public NeuralNetLayerBuf<T, INDEX>
{
protected:
	typedef Eigen::Matrix<T, -1, -1, Eigen::ColMajor>	Matrix;

	INDEX		m_mux_size = 1;
	INDEX		m_frame_size = 1;
	INDEX		m_node_size = 0;

public:
	NeuralNetSigmoid() {}

	NeuralNetSigmoid(INDEX node_size)
	{
		Resize(node_size);
	}

	~NeuralNetSigmoid() {}		// デストラクタ

	void Resize(INDEX node_size)
	{
		m_node_size = node_size;
	}

	void  SetMuxSize(INDEX mux_size) { m_mux_size = mux_size; }
	void  SetBatchSize(INDEX batch_size) { m_frame_size = batch_size * m_mux_size; }

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
		Eigen::Map<Matrix> x((T*)m_input_signal_buffer.GetBuffer(), m_input_signal_buffer.GetFrameStride() / sizeof(T), m_node_size);
		Eigen::Map<Matrix> y((T*)m_output_signal_buffer.GetBuffer(), m_output_signal_buffer.GetFrameStride() / sizeof(T), m_node_size);

		y = ((x * -1).array().exp() + 1.0).inverse();
	}

	void Backward(void)
	{
		Eigen::Map<Matrix> y((T*)m_output_signal_buffer.GetBuffer(), m_output_signal_buffer.GetFrameStride() / sizeof(T), m_node_size);
		Eigen::Map<Matrix> dy((T*)m_output_error_buffer.GetBuffer(), m_output_signal_buffer.GetFrameStride() / sizeof(T), m_node_size);
		Eigen::Map<Matrix> dx((T*)m_input_error_buffer.GetBuffer(), m_output_signal_buffer.GetFrameStride() / sizeof(T), m_node_size);

		dx = dy.array() * (-y.array() + 1) * y.array();
	}

	void Update(double learning_rate)
	{
	}

};

}
