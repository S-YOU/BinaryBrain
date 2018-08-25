// --------------------------------------------------------------------------
//  Binary Brain  -- binary neural net framework
//
//                                     Copyright (C) 2018 by Ryuji Fuchikami
//                                     https://github.com/ryuz
//                                     ryuji.fuchikami@nifty.com
// --------------------------------------------------------------------------


#pragma once

#include <vector>
#include "NeuralNetBuffer.h"


namespace bb {


// NeuralNet�̒��ۃN���X
template <typename T=float, typename INDEX = size_t>
class NeuralNetLayer
{
public:
	// ��{�@�\
	virtual ~NeuralNetLayer() {}											// �f�X�g���N�^

	virtual void  Resize(std::vector<INDEX> size) {};						// �T�C�Y�ݒ�
	virtual void  InitializeCoeff(std::uint64_t seed) {}					// �����W���̗���������
	
	virtual INDEX GetInputFrameSize(void) const = 0;						// ���͂̃t���[����
	virtual INDEX GetInputNodeSize(void) const = 0;							// ���͂̃m�[�h��
	virtual INDEX GetOutputFrameSize(void) const = 0;						// �o�͂̃t���[����
	virtual INDEX GetOutputNodeSize(void) const = 0;						// �o�͂̃m�[�h��
	virtual int   GetInputValueDataType(void) const = 0;					// ���͒l�̃T�C�Y
	virtual int   GetInputErrorDataType(void) const = 0;					// �o�͒l�̃T�C�Y
	virtual int   GetOutputValueDataType(void) const = 0;					// ���͒l�̃T�C�Y
	virtual int   GetOutputErrorDataType(void) const = 0;					// ���͒l�̃T�C�Y

	virtual void  SetMuxSize(INDEX mux_size) {}								// ���d���T�C�Y�̐ݒ�
	virtual void  SetBatchSize(INDEX batch_size) = 0;						// �o�b�`�T�C�Y�̐ݒ�
	virtual	void  Forward(void) = 0;										// �\��
	virtual	void  Backward(void) = 0;										// �덷�t�`�d
	virtual	void  Update(double learning_rate) {};							// �w�K
	virtual	bool  Feedback(const std::vector<double>& loss) { return false; }	// ���ڃt�B�[�h�o�b�N
	

protected:
	// �o�b�t�@���
	NeuralNetBuffer<T, INDEX>	m_input_value_buffer;
	NeuralNetBuffer<T, INDEX>	m_output_value_buffer;
	NeuralNetBuffer<T, INDEX>	m_input_error_buffer;
	NeuralNetBuffer<T, INDEX>	m_output_error_buffer;

public:
	// �o�b�t�@�ݒ�
	virtual void  SetInputValueBuffer(NeuralNetBuffer<T, INDEX> buffer)  { m_input_value_buffer = buffer; }
	virtual void  SetOutputValueBuffer(NeuralNetBuffer<T, INDEX> buffer) { m_output_value_buffer = buffer; }
	virtual void  SetInputErrorBuffer(NeuralNetBuffer<T, INDEX> buffer)  { m_input_error_buffer = buffer; }
	virtual void  SetOutputErrorBuffer(NeuralNetBuffer<T, INDEX> buffer) { m_output_error_buffer = buffer; }
	
	// �o�b�t�@�擾
	NeuralNetBuffer<T, INDEX>& GetInputValueBuffer(void) { return m_input_value_buffer; }
	NeuralNetBuffer<T, INDEX>& GetOutputValueBuffer(void) { return m_output_value_buffer; }
	NeuralNetBuffer<T, INDEX>& GetInputErrorBuffer(void) { return m_input_error_buffer; }
	NeuralNetBuffer<T, INDEX>& GetOutputErrorBuffer(void) { return m_output_error_buffer; }

	// �o�b�t�@�����⏕
	NeuralNetBuffer<T, INDEX> CreateInputValueBuffer(void) { 
		return NeuralNetBuffer<T, INDEX>(GetInputFrameSize(), GetInputNodeSize(), GetInputValueDataType());
	}
	NeuralNetBuffer<T, INDEX> CreateOutputValueBuffer(void) {
		return NeuralNetBuffer<T, INDEX>(GetOutputFrameSize(), GetOutputNodeSize(), GetOutputValueDataType());
	}
	NeuralNetBuffer<T, INDEX> CreateInputErrorBuffer(void) {
		return NeuralNetBuffer<T, INDEX>(GetInputFrameSize(), GetInputNodeSize(), GetInputErrorDataType());
	}
	NeuralNetBuffer<T, INDEX> CreateOutputErrorBuffer(void) {
		return NeuralNetBuffer<T, INDEX>(GetOutputFrameSize(), GetOutputNodeSize(), GetOutputErrorDataType());
	}
};


}