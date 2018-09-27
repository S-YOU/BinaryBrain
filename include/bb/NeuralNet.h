// --------------------------------------------------------------------------
//  Binary Brain  -- binary neural net framework
//
//                                     Copyright (C) 2018 by Ryuji Fuchikami
//                                     https://github.com/ryuz
//                                     ryuji.fuchikami@nifty.com
// --------------------------------------------------------------------------


#pragma once

#include <vector>
#include <intrin.h>
#include <assert.h>
#include "NeuralNetGroup.h"


namespace bb {


// NeuralNet �ŏ�ʍ\���p�N���X
template <typename T = float, typename INDEX = size_t>
class NeuralNet : public NeuralNetGroup<T, INDEX>
{
protected:
	INDEX						m_mux_size = 0;
	INDEX						m_batch_size = 0;

	NeuralNetBuffer<T, INDEX>	m_input_signal_buffers;
	NeuralNetBuffer<T, INDEX>	m_output_signal_buffers;
	NeuralNetBuffer<T, INDEX>	m_input_error_buffers;
	NeuralNetBuffer<T, INDEX>	m_output_error_buffers;
	
public:
	// �R���X�g���N�^
	NeuralNet()
	{
	}
	
	// �f�X�g���N�^
	~NeuralNet() {
	}

	void SetMuxSize(INDEX mux_size)
	{
//		if (m_mux_size == mux_size) {
//			return;
//		}

		// �e�N���X�Ăяo��
		NeuralNetGroup<T, INDEX>::SetMuxSize(mux_size);
		m_batch_size = 0;
		m_mux_size = mux_size;
	}

	void SetBatchSize(INDEX batch_size)
	{
		// �T�C�Y�ύX��������΂��̂܂�
//		if (m_batch_size == batch_size) {
//			return;
//		}
		
		// �e�N���X�Ăяo��
		NeuralNetGroup<T, INDEX>::SetBatchSize(batch_size);
		m_batch_size = batch_size;

		// ���o�͂̃o�b�t�@������
		m_input_signal_buffers = m_firstLayer->CreateInputSignalBuffer();
		m_input_error_buffers = m_firstLayer->CreateInputErrorBuffer();
		m_output_signal_buffers = m_lastLayer->CreateOutputSignalBuffer();
		m_output_error_buffers = m_lastLayer->CreateOutputErrorBuffer();
		m_firstLayer->SetInputSignalBuffer(m_input_signal_buffers);
		m_firstLayer->SetInputErrorBuffer(m_input_error_buffers);
		m_lastLayer->SetOutputSignalBuffer(m_output_signal_buffers);
		m_lastLayer->SetOutputErrorBuffer(m_output_error_buffers);
	}

	void Forward(bool train = true, INDEX start_layer = 0)
	{
		INDEX layer_size = m_layers.size();

		for (INDEX layer = start_layer; layer < layer_size; ++layer) {
			m_layers[layer]->Forward(train);
		}
	}

	void Backward(void)
	{
		for (auto layer = m_layers.rbegin(); layer != m_layers.rend(); ++layer) {
			(*layer)->Backward();
		}
	}

	void Update(void)
	{
		for (auto layer = m_layers.begin(); layer != m_layers.end(); ++layer) {
			(*layer)->Update();
		}
	}


	// ���o�̓f�[�^�ւ̃A�N�Z�X�⏕
	void SetInputSignal(INDEX frame, INDEX node, T signal) {
		return m_firstLayer->GetInputSignalBuffer().SetReal(frame, node, signal);
	}

	void SetInputSignal(INDEX frame, std::vector<T> signals) {
		for (INDEX node = 0; node < (INDEX)signals.size(); ++node) {
			SetInputSignal(frame, node, signals[node]);
		}
	}

	T GetOutputSignal(INDEX frame, INDEX node) {
		return m_lastLayer->GetOutputSignalBuffer().GetReal(frame, node);
	}

	std::vector<T> GetOutputSignal(INDEX frame) {
		std::vector<T> signals(m_lastLayer->GetOutputNodeSize());
		for (INDEX node = 0; node < (INDEX)signals.size(); ++node) {
			signals[node] = GetOutputSignal(frame, node);
		}
		return signals;
	}

	void SetOutputError(INDEX frame, INDEX node, T error) {
		m_lastLayer->GetOutputErrorBuffer().SetReal(frame, node, error);
	}

	void SetOutputError(INDEX frame, std::vector<T> errors) {
		for (INDEX node = 0; node < (INDEX)errors.size(); ++node) {
			SetOutputError(frame, node, errors[node]);
		}
	}
};


}