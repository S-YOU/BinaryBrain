// --------------------------------------------------------------------------
//  Binary Brain  -- binary neural net framework
//
//                                     Copyright (C) 2018 by Ryuji Fuchikami
//                                     https://github.com/ryuz
//                                     ryuji.fuchikami@nifty.com
// --------------------------------------------------------------------------



#pragma once

#include <cstdint>
#include "NeuralNetLayer.h"
#include "NeuralNetRealToBinary.h"
#include "NeuralNetBinaryToReal.h"


namespace bb {


// �o�C�i���𑽏d�����ĕ]��
template <typename BT = bool, typename T = float, typename INDEX = size_t>
class NeuralNetBinaryMultiplex : public NeuralNetLayer<T, INDEX>
{
protected:
	// 3�w�ō\��
	NeuralNetRealToBinary<BT, T, INDEX>		m_real2bin;
	NeuralNetLayer<T, INDEX>*				m_layer;
	NeuralNetBinaryToReal<BT, T, INDEX>		m_bin2real;
	
	INDEX	m_batch_size = 0;
	INDEX	m_mux_size   = 0;
	
public:
	NeuralNetBinaryMultiplex() {}
	
	NeuralNetBinaryMultiplex(NeuralNetLayer<T, INDEX>* layer, INDEX input_node_size, INDEX output_node_size, INDEX input_hmux_size=1, INDEX output_hmux_size=1)
		: m_real2bin(input_node_size, input_node_size*input_hmux_size), m_bin2real(output_node_size*output_hmux_size, output_node_size)
	{
		m_layer = layer;
	}
	
	~NeuralNetBinaryMultiplex() {}
	
	std::string GetClassName(void) const { return "NeuralNetBinaryMultiplex"; }


	void InitializeCoeff(std::uint64_t seed)
	{
		std::mt19937_64 mt(seed);
		m_real2bin.InitializeCoeff(mt());
		m_layer->InitializeCoeff(mt());
		m_bin2real.InitializeCoeff(mt());
	}
	
	void SetBinaryMode(bool enable)
	{
		m_real2bin.SetBinaryMode(enable);
		m_layer->SetBinaryMode(enable);
		m_bin2real.SetBinaryMode(enable);
	}
	
	void  SetMuxSize(INDEX mux_size)
	{
		if (m_mux_size == mux_size) {
			return;
		}
		m_mux_size = mux_size;
		m_real2bin.SetMuxSize(mux_size);
		m_bin2real.SetMuxSize(mux_size);

		m_batch_size = 0;
	}
	
	void SetOptimizer(const NeuralNetOptimizer<T, INDEX>* optimizer)
	{
		m_real2bin.SetOptimizer(optimizer);
		m_layer->SetOptimizer(optimizer);
		m_bin2real.SetOptimizer(optimizer);
	}

	void  SetBatchSize(INDEX batch_size)
	{
		m_real2bin.SetBatchSize(batch_size);
		m_layer->SetBatchSize(batch_size * m_mux_size);
		m_bin2real.SetBatchSize(batch_size);
		
		if (m_batch_size == batch_size) {
			return;
		}
		m_batch_size = batch_size;
		
		
		// �`�F�b�N
		CheckConnection(m_real2bin, *m_layer);
		CheckConnection(*m_layer, m_bin2real);

		m_real2bin.SetOutputSignalBuffer(m_real2bin.CreateOutputSignalBuffer());
		m_real2bin.SetOutputErrorBuffer(m_real2bin.CreateOutputErrorBuffer());
		m_layer->SetInputSignalBuffer(m_real2bin.GetOutputSignalBuffer());
		m_layer->SetInputErrorBuffer(m_real2bin.GetOutputErrorBuffer());
		
		m_layer->SetOutputSignalBuffer(m_layer->CreateOutputSignalBuffer());
		m_layer->SetOutputErrorBuffer(m_layer->CreateOutputErrorBuffer());
		m_bin2real.SetInputSignalBuffer(m_layer->GetOutputSignalBuffer());
		m_bin2real.SetInputErrorBuffer(m_layer->GetOutputErrorBuffer());
	}

	
	// ���o�̓o�b�t�@
	void  SetInputSignalBuffer(NeuralNetBuffer<T, INDEX> buffer) { m_real2bin.SetInputSignalBuffer(buffer); }
	void  SetInputErrorBuffer(NeuralNetBuffer<T, INDEX> buffer) { m_real2bin.SetInputErrorBuffer(buffer); }
	void  SetOutputSignalBuffer(NeuralNetBuffer<T, INDEX> buffer) { m_bin2real.SetOutputSignalBuffer(buffer); }
	void  SetOutputErrorBuffer(NeuralNetBuffer<T, INDEX> buffer) { m_bin2real.SetOutputErrorBuffer(buffer); }

	const NeuralNetBuffer<T, INDEX>& GetInputSignalBuffer(void) const { return m_real2bin.GetInputSignalBuffer(); }
	const NeuralNetBuffer<T, INDEX>& GetInputErrorBuffer(void) const { return m_real2bin.GetInputErrorBuffer(); }
	const NeuralNetBuffer<T, INDEX>& GetOutputSignalBuffer(void) const { return m_bin2real.GetOutputSignalBuffer(); }
	const NeuralNetBuffer<T, INDEX>& GetOutputErrorBuffer(void) const { return m_bin2real.GetOutputErrorBuffer(); }


	INDEX GetInputFrameSize(void) const { return m_real2bin.GetInputFrameSize(); }
	INDEX GetInputNodeSize(void) const { return m_real2bin.GetInputNodeSize(); }
	int   GetInputSignalDataType(void) const { return m_real2bin.GetInputSignalDataType(); }
	int   GetInputErrorDataType(void) const { return m_real2bin.GetInputErrorDataType(); }

	INDEX GetOutputFrameSize(void) const { return m_bin2real.GetOutputFrameSize(); }
	INDEX GetOutputNodeSize(void) const { return m_bin2real.GetOutputNodeSize(); }
	int   GetOutputSignalDataType(void) const { return m_bin2real.GetOutputSignalDataType(); }
	int   GetOutputErrorDataType(void) const { return m_bin2real.GetOutputErrorDataType(); }


public:

	void Forward(bool train = true)
	{
		m_real2bin.Forward(train);
		m_layer->Forward(train);
		m_bin2real.Forward(train);
	}

	void Backward(void)
	{
		m_bin2real.Backward();
		m_layer->Backward();
		m_real2bin.Backward();
	}

	void Update(void)
	{
		m_bin2real.Update();
		m_layer->Update();
		m_real2bin.Update();
	}
	
	bool Feedback(const std::vector<double>& loss)
	{
		return m_layer->Feedback(loss);
	}


public:

	// �o�͂̑����֐�
	template <typename LT, int LABEL_SIZE>
	std::vector<double> GetOutputOnehotLoss(std::vector<LT> label)
	{
		auto buf = m_layer->GetOutputSignalBuffer();
		INDEX frame_size = m_layer->GetOutputFrameSize();
		INDEX node_size = m_layer->GetOutputNodeSize();

		std::vector<double> vec_loss_x(frame_size);
		double* vec_loss = &vec_loss_x[0];

#pragma omp parallel for
		for (int frame = 0; frame < (int)frame_size; ++frame) {
			vec_loss[frame] = 0;
			for (size_t node = 0; node < node_size; ++node) {
				if (label[frame / m_mux_size] == (node % LABEL_SIZE)) {
					vec_loss[frame] += (buf.Get<bool>(frame, node) ? 0.0 : +1.0);
				}
				else {
					vec_loss[frame] += (buf.Get<bool>(frame, node) ? +(1.0 / LABEL_SIZE) : -(0.0 / LABEL_SIZE));
				}
			}
		}

		return vec_loss_x;
	}


public:
	// Serialize
	template <class Archive>
	void save(Archive &archive, std::uint32_t const version) const
	{
	}

	template <class Archive>
	void load(Archive &archive, std::uint32_t const version)
	{
	}
	
	virtual void Save(cereal::JSONOutputArchive& archive) const
	{
		m_layer->Save(archive);
	}

	virtual void Load(cereal::JSONInputArchive& archive)
	{
		m_layer->Load(archive);
	}

};


}
