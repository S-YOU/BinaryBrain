#include <iostream>
#include <fstream>
#include <numeric>
#include <random>
#include <chrono>

#include <cereal/cereal.hpp>
#include <cereal/types/array.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/archives/json.hpp>

#include "bb/NeuralNet.h"
#include "bb/NeuralNetUtility.h"
#include "bb/NeuralNetAffine.h"
#include "bb/NeuralNetSigmoid.h"
#include "bb/NeuralNetSoftmax.h"
#include "bb/NeuralNetSparseAffine.h"
#include "bb/NeuralNetSparseBinaryAffine.h"
#include "bb/NeuralNetRealToBinary.h"
#include "bb/NeuralNetBinaryToReal.h"
#include "bb/NeuralNetBinaryLut6.h"
#include "bb/NeuralNetBinaryLut6VerilogXilinx.h"
#include "bb/NeuralNetBinaryFilter.h"
#include "bb/ShuffleSet.h"

#include "mnist_read.h"


// MNIST�f�[�^���g�����]���p�N���X
class EvaluateMnist
{
protected:
	// �]���p�f�[�^�Z�b�g
	std::vector< std::vector<float> >	m_test_images;
	std::vector< std::uint8_t >			m_test_labels;
	std::vector< std::vector<float> >	m_test_onehot;

	// �w�K�p�f�[�^�Z�b�g
	std::vector< std::vector<float> >	m_train_images;
	std::vector< std::uint8_t >			m_train_labels;
	std::vector< std::vector<float> >	m_train_onehot;
	
	// ���Ԍv��
	std::chrono::system_clock::time_point m_base_time;
	void reset_time(void) {	m_base_time = std::chrono::system_clock::now(); }
	double get_time(void)
	{
		auto now_time = std::chrono::system_clock::now();
		return std::chrono::duration_cast<std::chrono::milliseconds>(now_time - m_base_time).count() / 1000.0;
	}

	// �l�b�g�̐��𗦕]��
	double CalcAccuracy(bb::NeuralNet<>& net, std::vector< std::vector<float> >& images, std::vector<std::uint8_t>& labels)
	{
		// �]���T�C�Y�ݒ�
		net.SetBatchSize(images.size());

		// �]���摜�ݒ�
		for (size_t frame = 0; frame < images.size(); ++frame) {
			net.SetInputSignal(frame, images[frame]);
		}

		// �]�����{
		net.Forward();

		// ���ʏW�v
		int ok_count = 0;
		for (size_t frame = 0; frame < images.size(); ++frame) {
			int max_idx = bb::argmax<float>(net.GetOutputSignal(frame));
			ok_count += (max_idx == (int)labels[frame] ? 1 : 0);
		}

		// ���𗦂�Ԃ�
		return (double)ok_count / (double)images.size();
	}

	// �]���p�f�[�^�Z�b�g�Ő��𗦕]��
	double CalcAccuracy(bb::NeuralNet<>& net)
	{
		return CalcAccuracy(net, m_test_images, m_test_labels);
	}


public:
	// �R���X�g���N�^
	EvaluateMnist(int train_max_size = -1, int test_max_size = -1)
	{
		// MNIST�w�K�p�f�[�^�ǂݍ���
		m_train_images = mnist_read_images_real<float>("train-images-idx3-ubyte", train_max_size);
		m_train_labels = mnist_read_labels("train-labels-idx1-ubyte", train_max_size);

		// MNIST�]���p�f�[�^�ǂݍ���
		m_test_images = mnist_read_images_real<float>("t10k-images-idx3-ubyte", test_max_size);
		m_test_labels = mnist_read_labels("t10k-labels-idx1-ubyte", test_max_size);

		// ���f�[�^�����x���Ȃ̂ŁA���Ғl���� 1.0 �ƂȂ�N���X�G���g���s�[�p�̃f�[�^�����
		m_train_onehot = bb::LabelToOnehot<std::uint8_t, float>(m_train_labels, 10);
		m_test_onehot = bb::LabelToOnehot<std::uint8_t, float>(m_test_labels, 10);
	}
	

	// LUT6���͂̃o�C�i���ł̗͋Z�w�K
	void RunBinaryLut6WithBbruteForce(int epoc_size, size_t max_batch_size, int max_iteration=-1)
	{
		std::cout << "start [RunBinaryLut6WithBbruteForce]" << std::endl;
		reset_time();

		std::mt19937_64 mt(1);

		// �w�K���ƕ]�����ő��d����(������ς��ĕ������ʂ��ďW�v�ł���悤�ɂ���)��ς���
		int train_mux_size = 1;
		int test_mux_size = 16;

		// �w�\����`
		size_t input_node_size = 28 * 28;
		size_t layer0_node_size = 360 * 2;
		size_t layer1_node_size = 60 * 4;
		size_t layer2_node_size = 10 * 8;
		size_t output_node_size = 10;

		// �w�K�pNET�\�z
		bb::NeuralNet<> net;
		bb::NeuralNetRealToBinary<> layer_real2bin(input_node_size, input_node_size, mt());
		bb::NeuralNetBinaryLut6<>	layer_lut0(input_node_size, layer0_node_size, mt());
		bb::NeuralNetBinaryLut6<>	layer_lut1(layer0_node_size, layer1_node_size, mt());
		bb::NeuralNetBinaryLut6<>	layer_lut2(layer1_node_size, layer2_node_size, mt());
		bb::NeuralNetBinaryToReal<>	layer_bin2real(layer2_node_size, output_node_size, mt());
		auto last_lut_layer = &layer_lut2;
		net.AddLayer(&layer_real2bin);
		net.AddLayer(&layer_lut0);
		net.AddLayer(&layer_lut1);
		net.AddLayer(&layer_lut2);
		//	net.AddLayer(&layer_unbinarize);	// �w�K����unbinarize�s�v
		
		// �]���pNET�\�z(�m�[�h�͋��L)
		bb::NeuralNet<> net_eva;
		net_eva.AddLayer(&layer_real2bin);
		net_eva.AddLayer(&layer_lut0);
		net_eva.AddLayer(&layer_lut1);
		net_eva.AddLayer(&layer_lut2);
		net_eva.AddLayer(&layer_bin2real);

		// �w�K���[�v
		int iteration = 0;
		for (int epoc = 0; epoc < epoc_size; ++epoc) {
			// �w�K�󋵕]��
			layer_real2bin.InitializeCoeff(1);
			layer_bin2real.InitializeCoeff(1);
			net_eva.SetMuxSize(test_mux_size);
			std::cout << get_time() << "s " << "epoc[" << epoc << "] accuracy : " << CalcAccuracy(net_eva) << std::endl;

			for (size_t x_index = 0; x_index < m_train_images.size(); x_index += max_batch_size) {
				// �����̃o�b�`�T�C�Y�N���b�v
				size_t batch_size = std::min(max_batch_size, m_train_images.size() - x_index);

				// �o�b�`�w�K�f�[�^�̍쐬
				std::vector< std::vector<float> >	batch_images(m_train_images.begin() + x_index, m_train_images.begin() + x_index + batch_size);
				std::vector< std::uint8_t >			batch_labels(m_train_labels.begin() + x_index, m_train_labels.begin() + x_index + batch_size);

				// �f�[�^�Z�b�g
				net.SetMuxSize(train_mux_size);
				net.SetBatchSize(batch_size);
				for (size_t frame = 0; frame < batch_size; ++frame) {
					net.SetInputSignal(frame, batch_images[frame]);
				}

				// �\��
				net.Forward();

				// �o�C�i���Ńt�B�[�h�o�b�N(�͋Z�w�K)
				while (net.Feedback(last_lut_layer->GetOutputOnehotLoss<std::uint8_t, 10>(batch_labels)))
					;

				// ���ԕ\��()
				layer_real2bin.InitializeCoeff(1);
				layer_bin2real.InitializeCoeff(1);
				net_eva.SetMuxSize(test_mux_size);
				std::cout << get_time() << "s " << "epoc[" << epoc << "] accuracy : " << CalcAccuracy(net_eva) << std::endl;

				iteration++;
				if (max_iteration > 0 && iteration >= max_iteration) {
					goto loop_end;
				}
			}
		}
	loop_end:


		{
			std::ofstream ofs("lut_net.v");
			bb::NeuralNetBinaryLut6VerilogXilinx(ofs, layer_lut0, "lutnet_layer0");
			bb::NeuralNetBinaryLut6VerilogXilinx(ofs, layer_lut1, "lutnet_layer1");
			bb::NeuralNetBinaryLut6VerilogXilinx(ofs, layer_lut2, "lutnet_layer2");
		}

		std::cout << "end\n" << std::endl;
	}


	// ����(float)�̑S�ڑ��w�ŁA�t���b�g�ȃl�b�g��]��
	void RunDenseAffine(int epoc_size, size_t max_batch_size)
	{
		std::cout << "start [RunDenseAffine]" << std::endl;
		reset_time();

		// ������NET�\�z
		bb::NeuralNet<> net;
		bb::NeuralNetAffine<>  layer0_affine(28 * 28, 100);
		bb::NeuralNetSigmoid<> layer0_sigmoid(100);
		bb::NeuralNetAffine<>  layer1_affine(100, 10);
		bb::NeuralNetSoftmax<> layer1_softmax(10);
		net.AddLayer(&layer0_affine);
		net.AddLayer(&layer0_sigmoid);
		net.AddLayer(&layer1_affine);
		net.AddLayer(&layer1_softmax);

		for (int epoc = 0; epoc < epoc_size; ++epoc) {

			// �w�K�󋵕]��
			std::cout << get_time() << "s " << "epoc[" << epoc << "] accuracy : " << CalcAccuracy(net) << std::endl;


			for (size_t x_index = 0; x_index < m_train_images.size(); x_index += max_batch_size) {
				// �����̃o�b�`�T�C�Y�N���b�v
				size_t batch_size = std::min(max_batch_size, m_train_images.size() - x_index);

				// �f�[�^�Z�b�g
				net.SetBatchSize(batch_size);
				for (size_t frame = 0; frame < batch_size; ++frame) {
					net.SetInputSignal(frame, m_train_images[x_index + frame]);
				}

				// �\��
				net.Forward();

				// �덷�t�`�d
				for (size_t frame = 0; frame < batch_size; ++frame) {
					auto signals = net.GetOutputSignal(frame);
					for (size_t node = 0; node < signals.size(); ++node) {
						signals[node] -= m_train_onehot[x_index + frame][node];
						signals[node] /= (float)batch_size;
					}
					net.SetOutputError(frame, signals);
				}
				net.Backward();

				// �X�V
				net.Update(0.2);
			}
		}
		std::cout << "end\n" << std::endl;
	}


	// ����(float)��6���͂ɐ��������m�[�h�őw���`�����ăl�b�g��]��
	void RunSparseAffine(int epoc_size, size_t max_batch_size)
	{
		std::cout << "start [RunSparseAffine]" << std::endl;
		reset_time();

		// ������NET�\�z
		bb::NeuralNet<> net;
		size_t input_node_size = 28 * 28;
		size_t layer0_node_size = 10 * 6 * 6 * 3;
		size_t layer1_node_size = 10 * 6 * 6;
		size_t layer2_node_size = 10 * 6;
		size_t layer3_node_size = 10;
		size_t output_node_size = 10;
		bb::NeuralNetSparseAffine<>	layer0_affine(28 * 28, layer0_node_size);
		bb::NeuralNetSigmoid<>		layer0_sigmoid(layer0_node_size);
		bb::NeuralNetSparseAffine<> layer1_affine(layer0_node_size, layer1_node_size);
		bb::NeuralNetSigmoid<>		layer1_sigmoid(layer1_node_size);
		bb::NeuralNetSparseAffine<> layer2_affine(layer1_node_size, layer2_node_size);
		bb::NeuralNetSigmoid<>		layer2_sigmoid(layer2_node_size);
		bb::NeuralNetSparseAffine<> layer3_affine(layer2_node_size, layer3_node_size);
		bb::NeuralNetSoftmax<>		layer3_softmax(layer3_node_size);
		net.AddLayer(&layer0_affine);
		net.AddLayer(&layer0_sigmoid);
		net.AddLayer(&layer1_affine);
		net.AddLayer(&layer1_sigmoid);
		net.AddLayer(&layer2_affine);
		net.AddLayer(&layer2_sigmoid);
		net.AddLayer(&layer3_affine);
		net.AddLayer(&layer3_softmax);

		for (int epoc = 0; epoc < epoc_size; ++epoc) {

			// �w�K�󋵕]��
			std::cout << get_time() << "s " << "epoc[" << epoc << "] accuracy : " << CalcAccuracy(net) << std::endl;
			
			for (size_t x_index = 0; x_index < m_train_images.size(); x_index += max_batch_size) {
				// �����̃o�b�`�T�C�Y�N���b�v
				size_t batch_size = std::min(max_batch_size, m_train_images.size() - x_index);

				// �f�[�^�Z�b�g
				net.SetBatchSize(batch_size);
				for (size_t frame = 0; frame < batch_size; ++frame) {
					net.SetInputSignal(frame, m_train_images[x_index + frame]);
				}

				// �\��
				net.Forward();

				// �덷�t�`�d
				for (size_t frame = 0; frame < batch_size; ++frame) {
					auto signals = net.GetOutputSignal(frame);
					for (size_t node = 0; node < signals.size(); ++node) {
						signals[node] -= m_train_onehot[x_index + frame][node];
						signals[node] /= (float)batch_size;
					}
					net.SetOutputError(frame, signals);
				}
				net.Backward();
				
				// �X�V
				net.Update(1.0);
			}
		}
		std::cout << "end\n" << std::endl;
	}


	// ���������_�Ŋw�K�����ăo�C�i���ɃR�s�[
	void RunRealToBinary(int epoc_size, size_t max_batch_size)
	{
		std::cout << "start [RunRealToBinary]" << std::endl;
		reset_time();

		std::mt19937_64 mt(1);

		int train_mux_size = 1;
		int test_mux_size = 16;


		// �w�\��
		size_t input_node_size = 28 * 28;
		size_t layer0_node_size = 10 * 6 * 6 * 3;
		size_t layer1_node_size = 10 * 6 * 6;
		size_t layer2_node_size = 10 * 6;
		size_t output_node_size = 10;

		// ������NET�\�z
		bb::NeuralNet<> real_net;
		bb::NeuralNetRealToBinary<float>	real_real2bin(input_node_size, input_node_size);
		bb::NeuralNetSparseBinaryAffine<6>	real_affine0(input_node_size, layer0_node_size);
		bb::NeuralNetSparseBinaryAffine<6>	real_affine1(layer0_node_size, layer1_node_size);
		bb::NeuralNetSparseBinaryAffine<6>	real_affine2(layer1_node_size, layer2_node_size);
		bb::NeuralNetBinaryToReal<float>	real_bin2real(layer2_node_size, output_node_size);
		real_net.AddLayer(&real_real2bin);
		real_net.AddLayer(&real_affine0);
		real_net.AddLayer(&real_affine1);
		real_net.AddLayer(&real_affine2);
		real_net.AddLayer(&real_bin2real);

		// �����ŋt�`�d�Ŋw�K
		for (int epoc = 0; epoc < epoc_size; ++epoc) {

			// �w�K�󋵕]��
			real_net.SetMuxSize(test_mux_size);
			auto real_accuracy = CalcAccuracy(real_net);
			std::cout << get_time() << "s " << "epoc[" << epoc << "] real_net accuracy : " << real_accuracy << std::endl;

			for (size_t x_index = 0; x_index < m_train_images.size(); x_index += max_batch_size) {
				// �����̃o�b�`�T�C�Y�N���b�v
				size_t batch_size = std::min(max_batch_size, m_train_images.size() - x_index);

				// ���̓f�[�^�ݒ�
				real_net.SetMuxSize(train_mux_size);
				real_net.SetBatchSize(batch_size);
				for (size_t frame = 0; frame < batch_size; ++frame) {
					real_net.SetInputSignal(frame, m_train_images[frame + x_index]);
				}

				// �\��
				real_net.Forward();

				// �덷�t�`�d
				for (size_t frame = 0; frame < batch_size; ++frame) {
					auto signals = real_net.GetOutputSignal(frame);
					for (size_t node = 0; node < signals.size(); ++node) {
						signals[node] -= m_train_onehot[frame + x_index][node % 10];
						signals[node] /= (float)batch_size;
					}
					real_net.SetOutputError(frame, signals);
				}
				real_net.Backward();

				// �X�V
				real_net.Update(1.0);
			}
		}


		// �o�C�i����NET�\�z
		bb::NeuralNet<>	bin_net;
		bb::NeuralNetRealToBinary<> bin_layer_real2bin(input_node_size, input_node_size);
		bb::NeuralNetBinaryLut6<>	bin_layer_lut0(input_node_size, layer0_node_size);
		bb::NeuralNetBinaryLut6<>	bin_layer_lut1(layer0_node_size, layer1_node_size);
		bb::NeuralNetBinaryLut6<>	bin_layer_lut2(layer1_node_size, layer2_node_size);
		bb::NeuralNetBinaryToReal<>	bin_layer_bin2real(layer2_node_size, output_node_size);
		bin_net.AddLayer(&bin_layer_real2bin);
		bin_net.AddLayer(&bin_layer_lut0);
		bin_net.AddLayer(&bin_layer_lut1);
		bin_net.AddLayer(&bin_layer_lut2);
		//		bin_net.AddLayer(&bin_layer_bin2real);

		// �o�C�i���]���p
		bb::NeuralNet<>	bin_net_eva;
		bin_net_eva.AddLayer(&bin_layer_real2bin);
		bin_net_eva.AddLayer(&bin_layer_lut0);
		bin_net_eva.AddLayer(&bin_layer_lut1);
		bin_net_eva.AddLayer(&bin_layer_lut2);
		bin_net_eva.AddLayer(&bin_layer_bin2real);

		// �p�����[�^���R�s�[
		std::cout << "[parameter copy] real-net -> binary-net" << std::endl;
		bin_layer_lut0.ImportLayer(real_affine0);
		bin_layer_lut1.ImportLayer(real_affine1);
		bin_layer_lut2.ImportLayer(real_affine2);

		// �o�C�i���ŕ]��
		bin_net.SetMuxSize(test_mux_size);
	
		// �w�K���[�v
		max_batch_size = 8192;
		int max_iteration = 8;
		int iteration = 0;
		for (int epoc = 0; epoc < epoc_size; ++epoc) {
			// �w�K�󋵕]��
			bin_layer_real2bin.InitializeCoeff(1);
			bin_layer_bin2real.InitializeCoeff(1);
			bin_net_eva.SetMuxSize(test_mux_size);
			std::cout << get_time() << "s " << "epoc[" << epoc << "] bin_net accuracy : " << CalcAccuracy(bin_net_eva) << std::endl;

			for (size_t x_index = 0; x_index < m_train_images.size(); x_index += max_batch_size) {
				// �����̃o�b�`�T�C�Y�N���b�v
				size_t batch_size = std::min(max_batch_size, m_train_images.size() - x_index);

				// �o�b�`�w�K�f�[�^�̍쐬
				std::vector< std::vector<float> >	batch_images(m_train_images.begin() + x_index, m_train_images.begin() + x_index + batch_size);
				std::vector< std::uint8_t >			batch_labels(m_train_labels.begin() + x_index, m_train_labels.begin() + x_index + batch_size);

				// �f�[�^�Z�b�g
				bin_net.SetMuxSize(train_mux_size);
				bin_net.SetBatchSize(batch_size);
				for (size_t frame = 0; frame < batch_size; ++frame) {
					bin_net.SetInputSignal(frame, batch_images[frame]);
				}

				// �\��
				bin_net.Forward();

				// �o�C�i���Ńt�B�[�h�o�b�N(�͋Z�w�K)
				while (bin_net.Feedback(bin_layer_lut2.GetOutputOnehotLoss<std::uint8_t, 10>(batch_labels)))
					;

				// ���ԕ\��()
				bin_layer_real2bin.InitializeCoeff(1);
				bin_layer_bin2real.InitializeCoeff(1);
				bin_net_eva.SetMuxSize(test_mux_size);
				std::cout << get_time() << "s " << "epoc[" << epoc << "] bin_net accuracy : " << CalcAccuracy(bin_net_eva) << std::endl;

				iteration++;
				if (max_iteration > 0 && iteration >= max_iteration) {
					goto loop_end;
				}
			}
		}
	loop_end:

		{
			std::ofstream ofs("lut_net.v");
			bb::NeuralNetBinaryLut6VerilogXilinx(ofs, bin_layer_lut0, "lutnet_layer0");
			bb::NeuralNetBinaryLut6VerilogXilinx(ofs, bin_layer_lut1, "lutnet_layer1");
			bb::NeuralNetBinaryLut6VerilogXilinx(ofs, bin_layer_lut2, "lutnet_layer2");
		}

		std::cout << "end\n" << std::endl;
	}
};



// ���C���֐�
int main()
{
	omp_set_num_threads(6);

#ifdef _DEBUG
	std::cout << "!!! Debug Version !!!" << std::endl;
	int train_max_size = 128;
	int test_max_size = 128;
	int epoc_size = 16;
#else
	int train_max_size = -1;
	int test_max_size = -1;
	int epoc_size = 100;
#endif


	// �]���p�N���X���쐬
	EvaluateMnist	eva_mnist(train_max_size, test_max_size);

	// �ȉ��]�����������̂�K���ɐ؂�ւ��Ă��g�p��������

#if 1
	// �o�C�i��6����LUT�Ŋw�K����(�d���ł�)
	eva_mnist.RunBinaryLut6WithBbruteForce(2, 8192, 8);
#endif

#if 1
	// �������S�ڑ�(������ÓT�I�ȃj���[�����l�b�g)
	eva_mnist.RunDenseAffine(16, 256);
#endif

#if 1
	// �������ڑ�����(�ڑ�����LUT�I�ɂ��Ē��g�̃m�[�h�͎���)
	eva_mnist.RunSparseAffine(16, 256);
#endif

#if 1
	// �ڑ������̎����Ŋw�K������Ńo�C�i���ɃR�s�[
	eva_mnist.RunRealToBinary(16, 256);
#endif

	return 0;
}


