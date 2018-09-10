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
	NeuralNetBuffer<T, INDEX>	m_input_value_buffers;
	NeuralNetBuffer<T, INDEX>	m_input_error_buffers;
	NeuralNetBuffer<T, INDEX>	m_output_value_buffers;
	NeuralNetBuffer<T, INDEX>	m_output_error_buffers;

public:
	// �R���X�g���N�^
	NeuralNet()
	{
	}

	// �f�X�g���N�^
	~NeuralNet() {
	}

	void SetBatchSize(INDEX batch_size)
	{
		// �e�N���X�Ăяo��
		NeuralNetGroup<T, INDEX>::SetBatchSize(batch_size);

		// ���o�͂̃o�b�t�@������
		m_input_value_buffers = m_firstLayer->CreateInputValueBuffer();
		m_input_error_buffers = m_firstLayer->CreateInputErrorBuffer();
		m_output_value_buffers = m_lastLayer->CreateOutputValueBuffer();
		m_output_error_buffers = m_lastLayer->CreateOutputErrorBuffer();
		m_firstLayer->SetInputValueBuffer(m_input_value_buffers);
		m_firstLayer->SetInputErrorBuffer(m_input_error_buffers);
		m_lastLayer->SetOutputValueBuffer(m_output_value_buffers);
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

	void Update(double learning_rate)
	{
		for (auto layer = m_layers.begin(); layer != m_layers.end(); ++layer) {
			(*layer)->Update(learning_rate);
		}
	}


	// ���o�̓f�[�^�ւ̃A�N�Z�X�⏕
	void SetInputValue(INDEX frame, INDEX node, T value) {
		return m_firstLayer->GetInputValueBuffer().SetReal(frame, node, value);
	}

	void SetInputValue(INDEX frame, std::vector<T> values) {
		for (INDEX node = 0; node < (INDEX)values.size(); ++node) {
			SetInputValue(frame, node, values[node]);
		}
	}

	T GetOutputValue(INDEX frame, INDEX node) {
		return m_lastLayer->GetOutputValueBuffer().GetReal(frame, node);
	}

	std::vector<T> GetOutputValue(INDEX frame) {
		std::vector<T> values(m_lastLayer->GetOutputNodeSize());
		for (INDEX node = 0; node < (INDEX)values.size(); ++node) {
			values[node] = GetOutputValue(frame, node);
		}
		return values;
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