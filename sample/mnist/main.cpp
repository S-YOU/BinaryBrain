#include <iostream>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <random>
#include <chrono>

#include <cereal/cereal.hpp>
#include <cereal/types/array.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/archives/json.hpp>

#include "bb/NeuralNet.h"
#include "bb/NeuralNetUtility.h"

#include "bb/NeuralNetBinaryMultiplex.h"
#include "bb/NeuralNetRealToBinary.h"
#include "bb/NeuralNetBinaryToReal.h"
#include "bb/NeuralNetBinaryLut6.h"
#include "bb/NeuralNetBinaryLut6VerilogXilinx.h"

#include "bb/NeuralNetSigmoid.h"
#include "bb/NeuralNetReLU.h"
#include "bb/NeuralNetSoftmax.h"

#include "bb/NeuralNetAffine.h"
#include "bb/NeuralNetSparseAffine.h"
#include "bb/NeuralNetSparseAffineSigmoid.h"
#include "bb/NeuralNetDenseAffineSigmoid.h"

#include "bb/NeuralNetBatchNormalization.h"
#include "bb/NeuralNetBatchNormalizationAvx.h"

#include "bb/NeuralNetConvolution.h"
#include "bb/NeuralNetConvolutionPack.h"
#include "bb/NeuralNetMaxPooling.h"

#include "bb/NeuralNetOptimizerSgd.h"
#include "bb/NeuralNetOptimizerAdam.h"

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
	
	// �ő�o�b�`�T�C�Y
	size_t m_max_batch_size = 1024;
	
	// ����
	std::mt19937_64		m_mt;

	// ���O�p�o��
	std::ofstream		m_ofs_log;
	bb::ostream_tee		m_log;


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

		// ����������
		m_mt.seed(1);

		// �����Ń��O�t�@�C���I�[�v��
		time_t time_now = time(NULL);
		struct tm tm;
		localtime_s(&tm, &time_now);
		std::stringstream ss;
		ss << "log_";
		ss << std::setfill('0') << std::setw(4) << tm.tm_year + 1900;
		ss << std::setfill('0') << std::setw(2) << tm.tm_mon + 1;
		ss << std::setfill('0') << std::setw(2) << tm.tm_mday;
		ss << std::setfill('0') << std::setw(2) << tm.tm_hour;
		ss << std::setfill('0') << std::setw(2) << tm.tm_min;
		ss << std::setfill('0') << std::setw(2) << tm.tm_sec;
		ss << ".txt";
		m_ofs_log.open(ss.str());
		if (m_ofs_log.is_open()) { 
			m_log.add(m_ofs_log);
		}
		m_log.add(std::cout);
	}
	

protected:
	
	// �w�K�f�[�^�V���b�t��
	void ShuffleTrainData(void)
	{
		bb::ShuffleDataSet(m_mt(), m_train_images, m_train_labels, m_train_onehot);
	}


	// ���Ԍv��
	std::chrono::system_clock::time_point m_base_time;
	void reset_time(void) { m_base_time = std::chrono::system_clock::now(); }
	double get_time(void)
	{
		auto now_time = std::chrono::system_clock::now();
		return std::chrono::duration_cast<std::chrono::milliseconds>(now_time - m_base_time).count() / 1000.0;
	}

	// �i���\��
	void PrintProgress(float loss, size_t progress, size_t size)
	{
		size_t rate = progress * 100 / size;
		std::cout << "[" << rate << "% (" << progress << "/" << size << ")] loss : " << loss << "\r" << std::flush;
	}

	void ClearProgress(void) {
		std::cout << "                                                                \r" << std::flush;
	}


	// �l�b�g�̐��𗦕]��
	double CalcAccuracy(bb::NeuralNet<>& net, std::vector< std::vector<float> >& images, std::vector<std::uint8_t>& labels)
	{
		int ok_count = 0;
		for (size_t x_index = 0; x_index < images.size(); x_index += m_max_batch_size) {
			// �����̃o�b�`�T�C�Y�N���b�v
			size_t batch_size = std::min(m_max_batch_size, images.size() - x_index);

			// �f�[�^�Z�b�g
			net.SetBatchSize(batch_size);
			for (size_t frame = 0; frame < batch_size; ++frame) {
				net.SetInputSignal(frame, images[x_index + frame]);
			}

			// �]�����{
			net.Forward(false);

			// ���ʏW�v
			for (size_t frame = 0; frame < batch_size; ++frame) {
				auto out_val = net.GetOutputSignal(frame);
				for (size_t i = 10; i < out_val.size(); i++) {
					out_val[i % 10] += out_val[i];
				}
				out_val.resize(10);
				int max_idx = bb::argmax<float>(out_val);
				ok_count += ((max_idx % 10) == (int)labels[x_index + frame] ? 1 : 0);
			}
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
	// LUT6���͂̃o�C�i���ł̗͋Z�w�K
	void RunBinaryLut6WithBbruteForce(int epoc_size, size_t max_batch_size, int max_iteration=-1)
	{
		m_log << "start [RunBinaryLut6WithBbruteForce]" << std::endl;
		reset_time();

		std::mt19937_64 mt(1);

		// �w�K���ƕ]�����ő��d����(������ς��ĕ������ʂ��ďW�v�ł���悤�ɂ���)��ς���
		int train_mux_size = 1;
		int test_mux_size = 16;

		// �w�\����`
		size_t input_node_size = 28 * 28;
		size_t output_node_size = 10;
		size_t input_hmux_size = 1;
		size_t output_hmux_size = 8;

		size_t layer0_node_size = 360 * 2;
		size_t layer1_node_size = 60 * 4;
		size_t layer2_node_size = output_node_size * output_hmux_size;

		// �o�C�i���l�b�g�̑��d��Group�쐬
		bb::NeuralNetBinaryLut6<>	mux0_lut(input_node_size*input_hmux_size, layer0_node_size);
		bb::NeuralNetBinaryLut6<>	mux1_lut(layer0_node_size, layer1_node_size);
		bb::NeuralNetBinaryLut6<>	mux2_lut(layer1_node_size, layer2_node_size);
		bb::NeuralNetGroup<>		mux_group;
		mux_group.AddLayer(&mux0_lut);
		mux_group.AddLayer(&mux1_lut);
		mux_group.AddLayer(&mux2_lut);

		// ���C���[��`
		bb::NeuralNetBinaryMultiplex<>	bin_mux(&mux_group, input_node_size, output_node_size, input_hmux_size, output_hmux_size);

		// �l�b�g�\�z
		bb::NeuralNet<> net;
		net.AddLayer(&bin_mux);
		

		// �w�K���[�v
		int iteration = 0;
		for (int epoc = 0; epoc < epoc_size; ++epoc) {
			// �w�K�󋵕]��
			bin_mux.SetMuxSize(test_mux_size);
			m_log << get_time() << "s " << "epoc[" << epoc << "] accuracy : " << CalcAccuracy(net) << std::endl;

			for (size_t x_index = 0; x_index < m_train_images.size(); x_index += max_batch_size) {
				// �����̃o�b�`�T�C�Y�N���b�v
				size_t batch_size = std::min(max_batch_size, m_train_images.size() - x_index);

				// �o�b�`�w�K�f�[�^�̍쐬
				std::vector< std::vector<float> >	batch_images(m_train_images.begin() + x_index, m_train_images.begin() + x_index + batch_size);
				std::vector< std::uint8_t >			batch_labels(m_train_labels.begin() + x_index, m_train_labels.begin() + x_index + batch_size);

				// �f�[�^�Z�b�g
				bin_mux.SetMuxSize(train_mux_size);
				net.SetBatchSize(batch_size);
				for (size_t frame = 0; frame < batch_size; ++frame) {
					net.SetInputSignal(frame, batch_images[frame]);
				}

				// �\��
				net.Forward();

				// �o�C�i���Ńt�B�[�h�o�b�N(�͋Z�w�K)
				while (bin_mux.Feedback(bin_mux.GetOutputOnehotLoss<std::uint8_t, 10>(batch_labels)))
					;

				// ���ԕ\��
				bin_mux.SetMuxSize(test_mux_size);
				m_log << get_time() << "s " << "epoc[" << epoc << "] accuracy : " << CalcAccuracy(net) << std::endl;

				iteration++;
				if (max_iteration > 0 && iteration >= max_iteration) {
					goto loop_end;
				}
			}
		}
	loop_end:


		{
			std::ofstream ofs("lut_net.v");
			bb::NeuralNetBinaryLut6VerilogXilinx(ofs, mux0_lut, "lutnet_layer0");
			bb::NeuralNetBinaryLut6VerilogXilinx(ofs, mux1_lut, "lutnet_layer1");
			bb::NeuralNetBinaryLut6VerilogXilinx(ofs, mux2_lut, "lutnet_layer2");
		}

		m_log << "end\n" << std::endl;
	}


	// ����(float)�̑S�ڑ��w�ŁA�t���b�g�ȃl�b�g��]��
	void RunSimpleDenseAffine(int epoc_size, size_t max_batch_size, bool binary_mode)
	{
		m_log << "start [SimpleDenseAffine]" << std::endl;
		reset_time();

		// ������NET�\�z
		bb::NeuralNetAffine<>  layer0_affine(28 * 28, 256);
		bb::NeuralNetSigmoid<> layer0_activation(256);
		bb::NeuralNetAffine<>  layer1_affine(256, 256);
		bb::NeuralNetSigmoid<> layer1_activation(256);
		bb::NeuralNetAffine<>  layer2_affine(256, 10);
		bb::NeuralNetSoftmax<> layer2_activation(10);

		bb::NeuralNet<> net;
		net.AddLayer(&layer0_affine);
		net.AddLayer(&layer0_activation);
		net.AddLayer(&layer1_affine);
		net.AddLayer(&layer1_activation);
		net.AddLayer(&layer2_affine);
		net.AddLayer(&layer2_activation);

		// �I�v�e�B�}�C�U�ݒ�
		net.SetOptimizer(&bb::NeuralNetOptimizerAdam<>(0.001f, 0.9f, 0.999f));

		net.SetBinaryMode(binary_mode);
		m_log << "binary mode : " << binary_mode << std::endl;

		// �w�K���[�v
		for (int epoc = 0; epoc < epoc_size; ++epoc) {

			// �w�K�󋵕]��
			m_log << get_time() << "s " << "epoc[" << epoc << "] accuracy : " << CalcAccuracy(net) << std::endl;
			
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
				net.Update();
			}

			// Shuffle
			ShuffleTrainData();
		}
		m_log << "end\n" << std::endl;
	}

	// ����(float)�̑S�ڑ��w�ŁA�t���b�g�ȃl�b�g��]��
	void RunSimpleDenseAffineBinary(int epoc_size, size_t max_batch_size)
	{
		m_log << "start [SimpleDenseAffineBinary]" << std::endl;
		reset_time();

		std::mt19937_64 mt(1);

		// ������NET�\�z
		size_t input_node_size = 1 * 28 * 28;
		size_t layer0_node_size = 10800;
		size_t layer1_node_size = 10800;
		size_t layer2_node_size = 3600;
		size_t layer3_node_size = 600;
		size_t layer4_node_size = 100;
		size_t output_node_size = 10;
		bb::NeuralNetBatchNormalization<>	input_batch_norm(input_node_size);
		bb::NeuralNetSigmoid<>				input_activation(input_node_size);

		bb::NeuralNetAffine<>				layer0_affine(input_node_size, layer0_node_size);
		bb::NeuralNetBatchNormalization<>	layer0_batch_norm(layer0_node_size);
		bb::NeuralNetSigmoid<>				layer0_activation(layer0_node_size);

		bb::NeuralNetAffine<>				layer1_affine(layer0_node_size, layer1_node_size);
		bb::NeuralNetBatchNormalization<>	layer1_batch_norm(layer1_node_size);
		bb::NeuralNetSigmoid<>				layer1_activation(layer1_node_size);

		bb::NeuralNetAffine<>				layer2_affine(layer1_node_size, layer2_node_size);
		bb::NeuralNetBatchNormalization<>	layer2_batch_norm(layer2_node_size);
		bb::NeuralNetSigmoid<>				layer2_activation(layer2_node_size);

		bb::NeuralNetAffine<>				layer3_affine(layer2_node_size, layer3_node_size);
		bb::NeuralNetBatchNormalization<>	layer3_batch_norm(layer3_node_size);
		bb::NeuralNetSigmoid<>				layer3_activation(layer3_node_size);

		bb::NeuralNetAffine<>				layer4_affine(layer3_node_size, layer4_node_size);
		bb::NeuralNetBatchNormalization<>	layer4_batch_norm(layer4_node_size);
		bb::NeuralNetSigmoid<>				layer4_activation(layer4_node_size);

		bb::NeuralNetBinaryToReal<float>	output_bin2real(layer4_node_size, output_node_size);
		bb::NeuralNetSoftmax<>				output_softmax(output_node_size);

		bb::NeuralNet<> net;
		net.AddLayer(&input_batch_norm);
		net.AddLayer(&input_activation);
		net.AddLayer(&layer0_affine);
		net.AddLayer(&layer0_batch_norm);
		net.AddLayer(&layer0_activation);
		net.AddLayer(&layer1_affine);
		net.AddLayer(&layer1_batch_norm);
		net.AddLayer(&layer1_activation);
		net.AddLayer(&layer2_affine);
		net.AddLayer(&layer2_batch_norm);
		net.AddLayer(&layer2_activation);
		net.AddLayer(&layer3_affine);
		net.AddLayer(&layer3_batch_norm);
		net.AddLayer(&layer3_activation);
		net.AddLayer(&layer4_affine);
		net.AddLayer(&layer4_batch_norm);
		net.AddLayer(&layer4_activation);
		net.AddLayer(&output_bin2real);
		net.AddLayer(&output_softmax);

		// �I�v�e�B�}�C�U�ݒ�
		net.SetOptimizer(&bb::NeuralNetOptimizerAdam<>(0.001f, 0.9f, 0.999f));

		// �o�C�i���ݒ�
		net.SetBinaryMode(true);

		// �w�K���[�v
		for (int epoc = 0; epoc < epoc_size; ++epoc) {

			// �w�K�󋵕]��
			m_log << get_time() << "s " << "epoc[" << epoc << "] accuracy : " << CalcAccuracy(net) << std::endl;

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
				float loss = 0;
				for (size_t frame = 0; frame < batch_size; ++frame) {
					auto signals = net.GetOutputSignal(frame);
					for (size_t node = 0; node < signals.size(); ++node) {
						signals[node] -= m_train_onehot[x_index + frame][node];
						loss += signals[node] * signals[node];
						signals[node] /= (float)batch_size;
					}
					net.SetOutputError(frame, signals);
				}
				loss = sqrt(loss / batch_size);
				net.Backward();

				// �X�V
				net.Update();

				// �i���\��
				PrintProgress(loss, x_index + batch_size, m_train_images.size());
			}
			ClearProgress();

			// Shuffle
			ShuffleTrainData();
		}
		m_log << "end\n" << std::endl;
	}
	

	// ����(float)��6���͂ɐ��������m�[�h�őw���`�����ăl�b�g��]��
	void RunSimpleSparseAffine(int epoc_size, size_t max_batch_size, bool binary_mode)
	{
		m_log << "start [SimpleSparseAffine]" << std::endl;
		reset_time();

		// ������NET�\�z
		bb::NeuralNet<> net;
		size_t input_node_size = 28 * 28;
		size_t layer0_node_size = 10 * 6 * 6 * 6 * 3;
		size_t layer1_node_size = 10 * 6 * 6 * 6 * 3;
		size_t layer2_node_size = 10 * 6 * 6 * 6;
		size_t layer3_node_size = 10 * 6 * 6;
		size_t layer4_node_size = 10 * 6;
		size_t layer5_node_size = 10;
		size_t output_node_size = 10;
		bb::NeuralNetSparseAffine<>			layer0_affine(28 * 28, layer0_node_size);
		bb::NeuralNetBatchNormalization<>	layer0_norm(layer0_node_size);
		bb::NeuralNetSigmoid<>				layer0_sigmoid(layer0_node_size);
		bb::NeuralNetSparseAffine<>			layer1_affine(layer0_node_size, layer1_node_size);
		bb::NeuralNetBatchNormalization<>	layer1_norm(layer1_node_size);
		bb::NeuralNetSigmoid<>				layer1_sigmoid(layer1_node_size);
		bb::NeuralNetSparseAffine<>			layer2_affine(layer1_node_size, layer2_node_size);
		bb::NeuralNetSigmoid<>				layer2_sigmoid(layer2_node_size);
		bb::NeuralNetSparseAffine<>			layer3_affine(layer2_node_size, layer3_node_size);
		bb::NeuralNetSigmoid<>				layer3_sigmoid(layer3_node_size);
		bb::NeuralNetSparseAffine<>			layer4_affine(layer3_node_size, layer4_node_size);
		bb::NeuralNetSigmoid<>				layer4_sigmoid(layer4_node_size);
		bb::NeuralNetSparseAffine<>			layer5_affine(layer4_node_size, layer5_node_size);
		bb::NeuralNetSoftmax<>				layer5_softmax(layer5_node_size);
		net.AddLayer(&layer0_affine);
		net.AddLayer(&layer0_norm);
		net.AddLayer(&layer0_sigmoid);
		net.AddLayer(&layer1_affine);
		net.AddLayer(&layer1_norm);
		net.AddLayer(&layer1_sigmoid);
		net.AddLayer(&layer2_affine);
		net.AddLayer(&layer2_sigmoid);
		net.AddLayer(&layer3_affine);
		net.AddLayer(&layer3_sigmoid);
		net.AddLayer(&layer4_affine);
		net.AddLayer(&layer4_sigmoid);
		net.AddLayer(&layer5_affine);
		net.AddLayer(&layer5_softmax);

		// �I�v�e�B�}�C�U�ݒ�
		net.SetOptimizer(&bb::NeuralNetOptimizerAdam<>(0.001f, 0.9f, 0.999f));

		// �o�C�i���ݒ�
		m_log << "binary mode : " << binary_mode << std::endl;
		net.SetBinaryMode(binary_mode);

		// �w�K���[�v
		for (int epoc = 0; epoc < epoc_size; ++epoc) {

			// �w�K�󋵕]��
			auto accuracy = CalcAccuracy(net);
			m_log << get_time() << "s " << "epoc[" << epoc << "] accuracy : " << accuracy << std::endl;
			
			// �~�j�o�b�`�w�K
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
				float loss = 0;
				for (size_t frame = 0; frame < batch_size; ++frame) {
					auto signals = net.GetOutputSignal(frame);
					for (size_t node = 0; node < signals.size(); ++node) {
						signals[node] -= m_train_onehot[x_index + frame][node];
						loss += signals[node] * signals[node];
						signals[node] /= (float)batch_size;
					}
					net.SetOutputError(frame, signals);
				}
				loss = sqrt(loss/batch_size);
				net.Backward();
				
				// �X�V
				net.Update();

				// �i���\��
				PrintProgress(loss, x_index + batch_size, m_train_images.size());
			}
			ClearProgress();

			// Shuffle
			ShuffleTrainData();
		}
		m_log << "end\n" << std::endl;
	}


	// ���������_�Ŋw�K�����ăo�C�i���ɃR�s�[
	void RunRealToBinary(int real_epoc_size, size_t real_max_batch_size, int bin_epoc_size, int bin_max_iteration, size_t bin_max_batch_size)
	{
		m_log << "start [RunRealToBinary]" << std::endl;
		reset_time();

		std::mt19937_64 mt(1);

		int train_mux_size = 1;
		int test_mux_size = 16;


		// �w�\��
		size_t input_node_size = 28 * 28;
		size_t output_node_size = 10;
		size_t input_hmux_size = 1;
		size_t output_hmux_size = 8;

		size_t layer0_node_size = 360 * 2;
		size_t layer1_node_size = 60 * 4;
		size_t layer2_node_size = output_node_size * output_hmux_size;

		// �o�C�i���l�b�g�̑��d��Group�쐬
		bb::NeuralNetSparseAffineSigmoid<6>	real_mux0_affine(input_node_size*input_hmux_size, layer0_node_size);
		bb::NeuralNetSparseAffineSigmoid<6>	real_mux1_affine(layer0_node_size, layer1_node_size);
		bb::NeuralNetSparseAffineSigmoid<6>	real_mux2_affine(layer1_node_size, layer2_node_size);
		bb::NeuralNetGroup<>				real_mux_group;
		real_mux_group.AddLayer(&real_mux0_affine);
		real_mux_group.AddLayer(&real_mux1_affine);
		real_mux_group.AddLayer(&real_mux2_affine);

		// ���C���[��`
		bb::NeuralNetBinaryMultiplex<float>	real_mux(&real_mux_group, input_node_size, output_node_size, input_hmux_size, output_hmux_size);
		bb::NeuralNetSoftmax<>				real_softmax(output_node_size);

		// �l�b�g�\�z
		bb::NeuralNet<> real_net;
		real_net.AddLayer(&real_mux);
		real_net.AddLayer(&real_softmax);

		// ���d���Ȃ�
		real_mux.SetMuxSize(1);

		// �I�v�e�B�}�C�Y�ݒ�
		real_net.SetOptimizer(&bb::NeuralNetOptimizerAdam<>());

		// �o�C�i�����[�h
		real_net.SetBinaryMode(true);


		// �����ŋt�`�d�Ŋw�K
		for (int epoc = 0; epoc < real_epoc_size; ++epoc) {

			// �w�K�󋵕]��
			//		real_mux.SetMuxSize(test_mux_size);
			auto real_accuracy = CalcAccuracy(real_net);
			m_log << get_time() << "s " << "epoc[" << epoc << "] real_net accuracy : " << real_accuracy << std::endl;

			for (size_t x_index = 0; x_index < m_train_images.size(); x_index += real_max_batch_size) {
				// �����̃o�b�`�T�C�Y�N���b�v
				size_t batch_size = std::min(real_max_batch_size, m_train_images.size() - x_index);

				// ���̓f�[�^�ݒ�
				//			real_mux.SetMuxSize(train_mux_size);
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
				real_net.Update();
			}
		}

		// �ŏI���ʕ\��
		m_log << get_time() << "s " << "real_net final accuracy : " << CalcAccuracy(real_net) << std::endl;


		// �o�C�i����NET�\�z
		bb::NeuralNetBinaryLut6<>	bin_mux0_lut(input_node_size*input_hmux_size, layer0_node_size);
		bb::NeuralNetBinaryLut6<>	bin_mux1_lut(layer0_node_size, layer1_node_size);
		bb::NeuralNetBinaryLut6<>	bin_mux2_lut(layer1_node_size, layer2_node_size);
		bb::NeuralNetGroup<>		bin_mux_group;
		bin_mux_group.AddLayer(&bin_mux0_lut);
		bin_mux_group.AddLayer(&bin_mux1_lut);
		bin_mux_group.AddLayer(&bin_mux2_lut);
		bb::NeuralNetBinaryMultiplex<>	bin_mux(&bin_mux_group, input_node_size, output_node_size, input_hmux_size, output_hmux_size);

		bb::NeuralNet<> bin_net;
		bin_net.AddLayer(&bin_mux);


		// �p�����[�^���R�s�[
		m_log << "[parameter copy] real-net -> binary-net" << std::endl;
		bin_mux0_lut.ImportLayer(real_mux0_affine);
		bin_mux1_lut.ImportLayer(real_mux1_affine);
		bin_mux2_lut.ImportLayer(real_mux2_affine);

		// �o�C�i���ŕ]��
		bin_mux.SetMuxSize(test_mux_size);

		// �w�K���[�v
		int iteration = 0;
		for (int epoc = 0; epoc < bin_epoc_size; ++epoc) {
			// �w�K�󋵕]��
			bin_mux.SetMuxSize(test_mux_size);
			m_log << get_time() << "s " << "epoc[" << epoc << "] bin_net accuracy : " << CalcAccuracy(bin_net) << std::endl;

			for (size_t x_index = 0; x_index < m_train_images.size(); x_index += bin_max_batch_size) {
				// �����̃o�b�`�T�C�Y�N���b�v
				size_t batch_size = std::min(bin_max_batch_size, m_train_images.size() - x_index);

				// �o�b�`�w�K�f�[�^�̍쐬
				std::vector< std::vector<float> >	batch_images(m_train_images.begin() + x_index, m_train_images.begin() + x_index + batch_size);
				std::vector< std::uint8_t >			batch_labels(m_train_labels.begin() + x_index, m_train_labels.begin() + x_index + batch_size);

				// �f�[�^�Z�b�g
				bin_mux.SetMuxSize(train_mux_size);
				bin_net.SetBatchSize(batch_size);
				for (size_t frame = 0; frame < batch_size; ++frame) {
					bin_net.SetInputSignal(frame, batch_images[frame]);
				}

				// �\��
				bin_net.Forward();

				// �o�C�i���Ńt�B�[�h�o�b�N(�͋Z�w�K)
				while (bin_mux.Feedback(bin_mux.GetOutputOnehotLoss<std::uint8_t, 10>(batch_labels)))
					;

				// ���ԕ\��
				bin_mux.SetMuxSize(test_mux_size);
				m_log << get_time() << "s " << "epoc[" << epoc << "] bin_net accuracy : " << CalcAccuracy(bin_net) << std::endl;

				iteration++;
				if (bin_max_iteration > 0 && iteration >= bin_max_iteration) {
					goto loop_end;
				}
			}
		}
	loop_end:

		{
			std::ofstream ofs("lut_net.v");
			bb::NeuralNetBinaryLut6VerilogXilinx(ofs, bin_mux0_lut, "lutnet_layer0");
			bb::NeuralNetBinaryLut6VerilogXilinx(ofs, bin_mux1_lut, "lutnet_layer1");
			bb::NeuralNetBinaryLut6VerilogXilinx(ofs, bin_mux2_lut, "lutnet_layer2");
		}

		m_log << "end\n" << std::endl;
	}


	// ����(float)�̑S�ڑ��w�ŁA�t���b�g�ȃl�b�g��]��
	void RunSimpleConvolution(int epoc_size, size_t max_batch_size, bool binary_mode)
	{
		m_log << "start [SimpleConvolution]" << std::endl;
		reset_time();

		std::mt19937_64 mt(1);

		// ������NET�\�z
		bb::NeuralNetBatchNormalization<>	input_batch_norm(1 * 28 * 28);
		bb::NeuralNetSigmoid<>				input_activation(1 * 28 * 28);

		bb::NeuralNetConvolution<>			layer0_convolution(1, 28, 28, 16, 3, 3);
		bb::NeuralNetBatchNormalization<>	layer0_batch_norm(16 * 26 * 26);
		bb::NeuralNetSigmoid<>				layer0_activation(16 * 26 * 26);

		bb::NeuralNetConvolution<>			layer1_convolution(16, 26, 26, 16, 3, 3);
		bb::NeuralNetBatchNormalization<>	layer1_batch_norm(16 * 24 * 24);
		bb::NeuralNetSigmoid<>				layer1_activation(16 * 24 * 24);

		bb::NeuralNetMaxPooling<>			layer2_pooling(16, 24, 24, 2, 2);

		bb::NeuralNetAffine<>				layer3_affine(16 * 12 * 12, 10);
		bb::NeuralNetBatchNormalization<>	layer3_batch_norm(10);
		bb::NeuralNetSigmoid<>				layer3_activation(10);

		bb::NeuralNetSoftmax<>				output_softmax(10);

		bb::NeuralNet<> net;
		net.AddLayer(&input_batch_norm);
		net.AddLayer(&input_activation);
		net.AddLayer(&layer0_convolution);
		net.AddLayer(&layer0_batch_norm);
		net.AddLayer(&layer0_activation);
		net.AddLayer(&layer1_convolution);
		net.AddLayer(&layer1_batch_norm);
		net.AddLayer(&layer1_activation);
		net.AddLayer(&layer2_pooling);
		net.AddLayer(&layer3_affine);
		net.AddLayer(&layer3_batch_norm);
		net.AddLayer(&layer3_activation);
		net.AddLayer(&output_softmax);

		// �I�v�e�B�}�C�U�ݒ�
		net.SetOptimizer(&bb::NeuralNetOptimizerAdam<>(0.001f, 0.9f, 0.999f));

		// �o�C�i���ݒ�
		m_log << "binary mode : " << binary_mode << std::endl;
		net.SetBinaryMode(binary_mode);

		// �w�K���[�v
		for (int epoc = 0; epoc < epoc_size; ++epoc) {

			// �w�K�󋵕]��
			m_log << get_time() << "s " << "epoc[" << epoc << "] accuracy : " << CalcAccuracy(net) << std::endl;

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
				float loss = 0;
				for (size_t frame = 0; frame < batch_size; ++frame) {
					auto signals = net.GetOutputSignal(frame);
					for (size_t node = 0; node < signals.size(); ++node) {
						signals[node] -= m_train_onehot[x_index + frame][node];
						loss += signals[node] * signals[node];
						signals[node] /= (float)batch_size;
					}
					net.SetOutputError(frame, signals);
				}
				loss = sqrt(loss/batch_size);
				net.Backward();

				// �X�V
				net.Update();

				// �i���\��
				PrintProgress(loss, x_index + batch_size, m_train_images.size());
			}
			ClearProgress();

			// Shuffle
			bb::ShuffleDataSet(mt(), m_train_images, m_train_onehot);
		}
		m_log << "end\n" << std::endl;
	}

	void RunSparseFullyCnn(int epoc_size, size_t max_batch_size, bool binary_mode)
	{
		m_log << "start [SparseFullyCnn]" << std::endl;
		reset_time();

		// Conv�psub�l�b�g�\�z (3x3)
		bb::NeuralNetSparseAffineSigmoid<>	real_sub0_affine(1 * 3 * 3, 30);
		bb::NeuralNetGroup<>				real_sub0_net;
		real_sub0_net.AddLayer(&real_sub0_affine);

		// Conv�psub�l�b�g�\�z (3x3)
		bb::NeuralNetSparseAffineSigmoid<>	real_sub1_affine0(30 * 3 * 3, 180);
		bb::NeuralNetSparseAffineSigmoid<>	real_sub1_affine1(180, 30);
		bb::NeuralNetGroup<>				real_sub1_net;
		real_sub1_net.AddLayer(&real_sub1_affine0);
		real_sub1_net.AddLayer(&real_sub1_affine1);

		// Conv�psub�l�b�g�\�z (3x3)
		bb::NeuralNetSparseAffineSigmoid<>	real_sub3_affine0(30 * 3 * 3, 180);
		bb::NeuralNetSparseAffineSigmoid<>	real_sub3_affine1(180, 30);
		bb::NeuralNetGroup<>				real_sub3_net;
		real_sub3_net.AddLayer(&real_sub3_affine0);
		real_sub3_net.AddLayer(&real_sub3_affine1);

		// Conv�psub�l�b�g�\�z (3x3)
		bb::NeuralNetSparseAffineSigmoid<>	real_sub4_affine0(30 * 3 * 3, 180);
		bb::NeuralNetSparseAffineSigmoid<>	real_sub4_affine1(180, 30);
		bb::NeuralNetGroup<>				real_sub4_net;
		real_sub4_net.AddLayer(&real_sub4_affine0);
		real_sub4_net.AddLayer(&real_sub4_affine1);

		// Conv�psub�l�b�g�\�z (3x3)
		bb::NeuralNetSparseAffineSigmoid<>	real_sub6_affine0(30 * 3 * 3, 180);
		bb::NeuralNetSparseAffineSigmoid<>	real_sub6_affine1(180, 30);
		bb::NeuralNetGroup<>				real_sub6_net;
		real_sub6_net.AddLayer(&real_sub6_affine0);
		real_sub6_net.AddLayer(&real_sub6_affine1);

		// Conv�psub�l�b�g�\�z (3x3)
		bb::NeuralNetSparseAffineSigmoid<>	real_sub7_affine0(30 * 2 * 2, 180);
		bb::NeuralNetSparseAffineSigmoid<>	real_sub7_affine1(180, 30);
		bb::NeuralNetGroup<>				real_sub7_net;
		real_sub7_net.AddLayer(&real_sub7_affine0);
		real_sub7_net.AddLayer(&real_sub7_affine1);

		// ������NET�\�z
		bb::NeuralNetConvolutionPack<>		real_layer0_conv(&real_sub0_net, 1, 28, 28, 30, 3, 3);
		bb::NeuralNetConvolutionPack<>		real_layer1_conv(&real_sub1_net, 30, 26, 26, 30, 3, 3);
		bb::NeuralNetMaxPooling<>			real_layer2_maxpol(30, 24, 24, 2, 2);
		bb::NeuralNetConvolutionPack<>		real_layer3_conv(&real_sub3_net, 30, 12, 12, 30, 3, 3);
		bb::NeuralNetConvolutionPack<>		real_layer4_conv(&real_sub4_net, 30, 10, 10, 30, 3, 3);
		bb::NeuralNetMaxPooling<>			real_layer5_maxpol(30, 8, 8, 2, 2);
		bb::NeuralNetConvolutionPack<>		real_layer6_conv(&real_sub6_net, 30, 4, 4, 30, 3, 3);
		bb::NeuralNetConvolutionPack<>		real_layer7_conv(&real_sub7_net, 30, 2, 2, 30, 2, 2);
		bb::NeuralNetBinaryToReal<float>	real_layer8_bin2rel(30, 10);
		bb::NeuralNetSoftmax<>				real_layer9_softmax(10);

		bb::NeuralNet<>		real_net;
		real_net.AddLayer(&real_layer0_conv);
		real_net.AddLayer(&real_layer1_conv);
		real_net.AddLayer(&real_layer2_maxpol);
		real_net.AddLayer(&real_layer3_conv);
		real_net.AddLayer(&real_layer4_conv);
		real_net.AddLayer(&real_layer5_maxpol);
		real_net.AddLayer(&real_layer6_conv);
		real_net.AddLayer(&real_layer7_conv);
		real_net.AddLayer(&real_layer8_bin2rel);
		real_net.AddLayer(&real_layer9_softmax);

		real_net.SetOptimizer(&bb::NeuralNetOptimizerAdam<>(0.001f, 0.9f, 0.999f));
		
		real_net.SetBinaryMode(binary_mode);
		m_log << "binary mode : " << binary_mode << std::endl;

		for (int epoc = 0; epoc < epoc_size; ++epoc) {

			// �w�K�󋵕]��
			m_log << get_time() << "s " << "epoc[" << epoc << "] accuracy : " << CalcAccuracy(real_net) << std::endl;

			size_t current_batch_size = 0;
			for (size_t x_index = 0; x_index < m_train_images.size(); x_index += max_batch_size) {
				// �����̃o�b�`�T�C�Y�N���b�v
				size_t batch_size = std::min(max_batch_size, m_train_images.size() - x_index);
				if (current_batch_size != batch_size) {
					real_net.SetBatchSize(batch_size);
					current_batch_size = batch_size;
				}

				// �f�[�^�Z�b�g
				for (size_t frame = 0; frame < batch_size; ++frame) {
					real_net.SetInputSignal(frame, m_train_images[x_index + frame]);
				}

				// �\��
				real_net.Forward();

				// �덷�t�`�d
				float loss = 0;
				for (size_t frame = 0; frame < batch_size; ++frame) {
					auto signals = real_net.GetOutputSignal(frame);
					for (size_t node = 0; node < signals.size(); ++node) {
						signals[node] -= m_train_onehot[x_index + frame][node];
						loss += signals[node] * signals[node];
						signals[node] /= (float)batch_size;
					}
					real_net.SetOutputError(frame, signals);
				}
				loss = sqrt(loss/batch_size);
				real_net.Backward();

				// �X�V
				real_net.Update();

				// �i���\��
				PrintProgress(loss, x_index + batch_size, m_train_images.size());
			}
			ClearProgress();

			// Shuffle
			ShuffleTrainData();
		}
		m_log << "end\n" << std::endl;
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
#else
	int train_max_size = -1;
	int test_max_size = -1;
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
	eva_mnist.RunSimpleDenseAffine(16, 256, false);
#endif

#if 1
	// �������ڑ�����(�ڑ�����LUT�I�ɂ��Ē��g�̃m�[�h�͎���)
	eva_mnist.RunSimpleSparseAffine(16, 256, true);
#endif

#if 1
	// �ڑ������̎����Ŋw�K������Ńo�C�i���ɃR�s�[
	eva_mnist.RunRealToBinary(16, 256, 16, 16, 8192);
#endif


#if 0
	eva_mnist.RunSimpleConvolution(1000, 256, true);
#endif

#if 0
	eva_mnist.RunSparseFullyCnn(1000, 256, true);
#endif


	getchar();

	return 0;
}


