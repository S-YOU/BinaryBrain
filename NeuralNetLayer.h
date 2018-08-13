


#pragma once


#include <vector>


// NeuralNet�̒��ۃN���X
template <typename T=float, typename ET = float, typename INDEX=size_t>
class NeuralNetLayer
{
public:
	virtual ~NeuralNetLayer() {}								// �f�X�g���N�^
	
	// ��{����
	virtual INDEX GetInputFrameSize(void) const = 0;	// ���͂̃t���[����
	virtual INDEX GetInputNodeSize(void) const = 0;		// ���͂̃m�[�h��
	virtual INDEX GetOutputFrameSize(void) const = 0;	// �o�͂̃t���[����
	virtual INDEX GetOutputNodeSize(void) const = 0;	// �o�͂̃m�[�h��
	
	virtual	void  Forward(void) = 0;
	virtual	void  Backward(void) = 0;
	
};

