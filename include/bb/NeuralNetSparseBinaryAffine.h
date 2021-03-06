﻿// --------------------------------------------------------------------------
//  Binary Brain  -- binary neural net framework
//
//                                     Copyright (C) 2018 by Ryuji Fuchikami
//                                     https://github.com/ryuz
//                                     ryuji.fuchikami@nifty.com
// --------------------------------------------------------------------------



#pragma once

#include <cstdint>

#include "bb/NeuralNetSparseLayer.h"
#include "bb/NeuralNetSparseAffine.h"
#include "bb/NeuralNetBatchNormalization.h"
#include "bb/NeuralNetBinarize.h"


namespace bb {


// 入力数制限Affine Binary Connect版
template <int N = 6, typename T = float>
class NeuralNetSparseBinaryAffine : public NeuralNetSparseLayer<T>
{
protected:
	// 3層で構成
	NeuralNetSparseAffine<N, T>		m_affine;
	NeuralNetBatchNormalization<T>	m_norm;
	NeuralNetBinarize<T>			m_binarize;
		
public:
	NeuralNetSparseBinaryAffine() {}

	NeuralNetSparseBinaryAffine(INDEX input_node_size, INDEX output_node_size, std::uint64_t seed = 1,
		const NeuralNetOptimizer<T>* optimizer = nullptr)
		: m_affine(input_node_size, output_node_size, seed, optimizer),
		m_norm(output_node_size),
		m_binarize(output_node_size)
	{
	}

	~NeuralNetSparseBinaryAffine() {}

	std::string GetClassName(void) const { return "NeuralNetSparseBinaryAffine"; }

	std::vector<T> CalcNode(INDEX node, std::vector<T> input_value) const
	{
		auto vec_affine = m_affine.CalcNode(node, input_value);
		auto vec_norm = m_norm.CalcNode(node, vec_affine);
		return m_binarize.CalcNode(node, vec_norm);
	}


	void InitializeCoeff(std::uint64_t seed)
	{
		m_affine.InitializeCoeff(seed);
		m_norm.InitializeCoeff(seed);
		m_binarize.InitializeCoeff(seed);
	}

	void  SetBinaryMode(bool enable)
	{
		m_affine.SetBinaryMode(enable);
		m_norm.SetBinaryMode(enable);
		m_binarize.SetBinaryMode(enable);
	}

	int   GetNodeInputSize(INDEX node) const { return m_affine.GetNodeInputSize(node); }
	void  SetNodeInput(INDEX node, int input_index, INDEX input_node) { m_affine.SetNodeInput(node, input_index, input_node); }
	INDEX GetNodeInput(INDEX node, int input_index) const { return m_affine.GetNodeInput(node, input_index); }

	void  SetBatchSize(INDEX batch_size) {
		m_affine.SetBatchSize(batch_size);
		m_norm.SetBatchSize(batch_size);
		m_binarize.SetBatchSize(batch_size);

		m_affine.SetOutputSignalBuffer(m_affine.CreateOutputSignalBuffer());
		m_affine.SetOutputErrorBuffer(m_affine.CreateOutputErrorBuffer());
		m_norm.SetInputSignalBuffer(m_affine.GetOutputSignalBuffer());
		m_norm.SetInputErrorBuffer(m_affine.GetOutputErrorBuffer());

		m_norm.SetOutputSignalBuffer(m_norm.CreateOutputSignalBuffer());
		m_norm.SetOutputErrorBuffer(m_norm.CreateOutputErrorBuffer());
		m_binarize.SetInputSignalBuffer(m_norm.GetOutputSignalBuffer());
		m_binarize.SetInputErrorBuffer(m_norm.GetOutputErrorBuffer());
	}

	
	// 入出力バッファ
	void  SetInputSignalBuffer(NeuralNetBuffer<T> buffer) { m_affine.SetInputSignalBuffer(buffer); }
	void  SetOutputSignalBuffer(NeuralNetBuffer<T> buffer) { m_binarize.SetOutputSignalBuffer(buffer); }
	void  SetInputErrorBuffer(NeuralNetBuffer<T> buffer) { m_affine.SetInputErrorBuffer(buffer); }
	void  SetOutputErrorBuffer(NeuralNetBuffer<T> buffer) { m_binarize.SetOutputErrorBuffer(buffer); }

	const NeuralNetBuffer<T>& GetInputSignalBuffer(void) const { return m_affine.GetInputSignalBuffer(); }
	const NeuralNetBuffer<T>& GetOutputSignalBuffer(void) const { return m_binarize.GetOutputSignalBuffer(); }
	const NeuralNetBuffer<T>& GetInputErrorBuffer(void) const { return m_affine.GetInputErrorBuffer(); }
	const NeuralNetBuffer<T>& GetOutputErrorBuffer(void) const { return m_binarize.GetOutputErrorBuffer(); }


	INDEX GetInputFrameSize(void) const { return m_affine.GetInputFrameSize(); }
	INDEX GetOutputFrameSize(void) const { return m_binarize.GetOutputFrameSize(); }

	INDEX GetInputNodeSize(void) const { return m_affine.GetInputNodeSize(); }
	INDEX GetOutputNodeSize(void) const { return m_binarize.GetOutputNodeSize(); }

	int   GetInputSignalDataType(void) const { return m_affine.GetInputSignalDataType(); }
	int   GetInputErrorDataType(void) const { return m_affine.GetInputErrorDataType(); }
	int   GetOutputSignalDataType(void) const { return m_binarize.GetOutputSignalDataType(); }
	int   GetOutputErrorDataType(void) const { return m_binarize.GetOutputErrorDataType(); }


public:

	void Forward(bool train = true)
	{
		m_affine.Forward(train);
		m_norm.Forward(train);
		m_binarize.Forward(train);
	}

	void Backward(void)
	{
		m_binarize.Backward();
		m_norm.Backward();
		m_affine.Backward();
	}

	void Update(void)
	{
		m_affine.Update();
		m_norm.Update();
		m_binarize.Update();
	}


public:
	// Serialize
	template <class Archive>
	void save(Archive &archive, std::uint32_t const version) const
	{
		archive(cereal::make_nvp("affine",      m_affine));
		archive(cereal::make_nvp("batch_norm",  m_norm));
		archive(cereal::make_nvp("binarize",    m_binarize));
	}

	template <class Archive>
	void load(Archive &archive, std::uint32_t const version)
	{
		archive(cereal::make_nvp("affine", m_affine));
		archive(cereal::make_nvp("batch_norm", m_norm));
		archive(cereal::make_nvp("binarize", m_binarize));
	}


	virtual void Save(cereal::JSONOutputArchive& archive) const
	{
		archive(cereal::make_nvp("SparseBinaryAffine", *this));
	}

	virtual void Load(cereal::JSONInputArchive& archive)
	{
		archive(cereal::make_nvp("SparseBinaryAffine", *this));
	}
};


}