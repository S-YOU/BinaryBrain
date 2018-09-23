// --------------------------------------------------------------------------
//  Binary Brain  -- binary neural net framework
//
//                                     Copyright (C) 2018 by Ryuji Fuchikami
//                                     https://github.com/ryuz
//                                     ryuji.fuchikami@nifty.com
// --------------------------------------------------------------------------


#pragma once

#include <vector>

#include "cereal/types/array.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/archives/json.hpp"

#include "NeuralNetBuffer.h"
#include "NeuralNetOptimizer.h"


namespace bb {


// abstract neura-network layer class
template <typename T=float, typename INDEX = size_t>
class NeuralNetLayer
{
protected:
	std::string					m_layer_name;

public:
	// basic functions
	virtual ~NeuralNetLayer() {}												// �f�X�g���N�^

	virtual void  SetLayerName(const std::string name) {						// ���C���[���ݒ�
		m_layer_name = name;
	}
	virtual std::string GetLayerName(void)
	{
		return m_layer_name;
	}

	virtual void  Resize(std::vector<INDEX> size) {}							// �T�C�Y�ݒ�
	virtual void  InitializeCoeff(std::uint64_t seed) {}						// �����W���̗���������
	
	virtual void  SetOptimizer(const NeuralNetOptimizer<T, INDEX>* optimizer) {}	//�I�v�e�B�}�C�U�̐ݒ�

	virtual INDEX GetInputFrameSize(void) const = 0;							// ���͂̃t���[����
	virtual INDEX GetInputNodeSize(void) const = 0;								// ���͂̃m�[�h��
	virtual INDEX GetOutputFrameSize(void) const = 0;							// �o�͂̃t���[����
	virtual INDEX GetOutputNodeSize(void) const = 0;							// �o�͂̃m�[�h��
	
	virtual void  SetBinaryMode(bool enable) {}									// �o�C�i�����[�h��ݒ�

	virtual void  SetMuxSize(INDEX mux_size) = 0;								// ���d���T�C�Y�̐ݒ�
	virtual void  SetBatchSize(INDEX batch_size) = 0;							// �o�b�`�T�C�Y�̐ݒ�
	
	virtual T     CalcNode(INDEX node, std::vector<T> input_value) const { return input_value[0]; }	// 1�m�[�h�����ʌv�Z
	
	virtual	void  Forward(bool train=true) = 0;									// �\��
	virtual	void  Backward(void) = 0;											// �덷�t�`�d
	virtual	void  Update(void) = 0;												// �w�K
	virtual	bool  Feedback(const std::vector<double>& loss) { return false; }	// ���ڃt�B�[�h�o�b�N
	
	
	// forward propagation of signals
	virtual int   GetInputSignalDataType(void) const = 0;
	virtual int   GetOutputSignalDataType(void) const = 0;
	
	virtual void  SetInputSignalBuffer(NeuralNetBuffer<T, INDEX> buffer) = 0;
	virtual void  SetOutputSignalBuffer(NeuralNetBuffer<T, INDEX> buffer) = 0;
	
	virtual const NeuralNetBuffer<T, INDEX>& GetInputSignalBuffer(void) const = 0;
	virtual const NeuralNetBuffer<T, INDEX>& GetOutputSignalBuffer(void) const = 0;
	
	NeuralNetBuffer<T, INDEX> CreateInputSignalBuffer(void) { 
		return NeuralNetBuffer<T, INDEX>(GetInputFrameSize(), GetInputNodeSize(), GetInputSignalDataType());
	}
	NeuralNetBuffer<T, INDEX> CreateOutputSignalBuffer(void) {
		return NeuralNetBuffer<T, INDEX>(GetOutputFrameSize(), GetOutputNodeSize(), GetOutputSignalDataType());
	}
	

	// backward propagation of errors
	virtual int   GetInputErrorDataType(void) const = 0;
	virtual int   GetOutputErrorDataType(void) const = 0;

	virtual void  SetInputErrorBuffer(NeuralNetBuffer<T, INDEX> buffer) = 0;
	virtual void  SetOutputErrorBuffer(NeuralNetBuffer<T, INDEX> buffer) = 0;

	virtual const NeuralNetBuffer<T, INDEX>& GetInputErrorBuffer(void) const = 0;
	virtual const NeuralNetBuffer<T, INDEX>& GetOutputErrorBuffer(void) const = 0;

	NeuralNetBuffer<T, INDEX> CreateInputErrorBuffer(void) {
		return NeuralNetBuffer<T, INDEX>(GetInputFrameSize(), GetInputNodeSize(), GetInputErrorDataType());
	}
	NeuralNetBuffer<T, INDEX> CreateOutputErrorBuffer(void) {
		return NeuralNetBuffer<T, INDEX>(GetOutputFrameSize(), GetOutputNodeSize(), GetOutputErrorDataType());
	}
	
	
	// Serialize(CEREAL)
	template <class Archive>
	void save(Archive& archive, std::uint32_t const version) const
	{
		archive(cereal::make_nvp("layer_name", m_layer_name));
	}

	template <class Archive>
	void load(Archive& archive, std::uint32_t const version)
	{
		archive(cereal::make_nvp("layer_name", m_layer_name));
	}

	virtual void Save(cereal::JSONOutputArchive& archive) const
	{
		archive(cereal::make_nvp("NeuralNetLayer", *this));
	}

	virtual void Load(cereal::JSONInputArchive& archive)
	{
		archive(cereal::make_nvp("NeuralNetLayer", *this));
	}
};


}