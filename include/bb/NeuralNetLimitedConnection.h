// --------------------------------------------------------------------------
//  Binary Brain  -- binary neural net framework
//
//                                     Copyright (C) 2018 by Ryuji Fuchikami
//                                     https://github.com/ryuz
//                                     ryuji.fuchikami@nifty.com
// --------------------------------------------------------------------------



#pragma once

#include <array>
#include <vector>
#include "NeuralNetLayerBuf.h"
#include "ShuffleSet.h"

namespace bb {


// ���͐ڑ����ɐ����̂���l�b�g
template <typename T = float, typename INDEX = size_t>
class NeuralNetLimitedConnection : public NeuralNetLayerBuf<T, INDEX>
{
	typedef NeuralNetLayer<T, INDEX> super;
	
protected:
	INDEX					m_input_node_size;
	INDEX					m_output_node_size;

public:
	// ���o�̓T�C�Y�̊Ǘ�
	INDEX GetInputNodeSize(void) const { return m_input_node_size; }
	INDEX GetOutputNodeSize(void) const { return m_output_node_size; }
	
	// �m�[�h�̐ڑ��Ǘ��̒�`
	virtual int   GetNodeInputSize(INDEX node) const = 0;
	virtual void  SetNodeInput(INDEX node, int input_index, INDEX input_node) = 0;
	virtual INDEX GetNodeInput(INDEX node, int input_index) const = 0;
	
	virtual void Resize(INDEX input_node_size, INDEX output_node_size)
	{
		m_input_node_size = input_node_size;
		m_output_node_size = output_node_size;
	}
	
	virtual void InitializeCoeff(std::uint64_t seed)
	{
		std::mt19937_64                     mt(seed);
		std::uniform_int_distribution<int>	distribution(0, 1);
		
		// �ڑ�����V���b�t��
		ShuffleSet	ss(GetInputNodeSize(), mt());
		
		INDEX node_size = GetOutputNodeSize();
		for (INDEX node = 0; node < node_size; ++node) {
			// ���͂������_���ڑ�
			int  input_size = GetNodeInputSize(node);
			auto random_set = ss.GetRandomSet(input_size);
			for (int i = 0; i < input_size; ++i) {
				SetNodeInput(node, i, random_set[i]);
			}
		}
	}
};


}

