

#pragma once

#include <vector>

// NeuralNet�̒��ۃN���X
template <typename INDEX=size_t>
class NeuralNetLayer
{
public:
	virtual ~NeuralNetLayer() {}							// �f�X�g���N�^
	
	virtual void  SetInputValuePtr(const void* ptr) = 0;	// ���͑��l�A�h���X�ݒ�
	virtual void  SetOutputValuePtr(void* ptr) = 0;			// �o�͑��l�A�h���X�ݒ�
	virtual void  SetOutputErrorPtr(const void* ptr) = 0;	// �o�͑��덷�A�h���X�ݒ�
	virtual void  SetInputErrorPtr(void* ptr) = 0;			// ���͑��덷�A�h���X�ݒ�

	virtual INDEX GetInputFrameSize(void) const = 0;		// ���͂̃t���[����
	virtual INDEX GetInputNodeSize(void) const = 0;			// ���͂̃m�[�h��
	virtual INDEX GetOutputFrameSize(void) const = 0;		// �o�͂̃t���[����
	virtual INDEX GetOutputNodeSize(void) const = 0;		// �o�͂̃m�[�h��
	
	virtual int   GetInputValueBitSize(void) const = 0;		// ���͒l�̃T�C�Y
	virtual int   GetInputErrorBitSize(void) const = 0;		// �o�͒l�̃T�C�Y
	virtual int   GetOutputValueBitSize(void) const = 0;	// ���͒l�̃T�C�Y
	virtual int   GetOutputErrorBitSize(void) const = 0;	// ���͒l�̃T�C�Y

	virtual	void  Forward(void) = 0;						// �\��
	virtual	void  Backward(void) = 0;						// �덷�t�`�d
	virtual	void  Update(double learning_rate) {};			// �w�K
};

